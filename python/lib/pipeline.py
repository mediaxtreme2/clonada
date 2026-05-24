"""
Clonada RVC v2 Inference Pipeline
Complete voice conversion: audio -> HuBERT -> pitch -> RVC -> output
"""

import os
import numpy as np
import torch
import torch.nn.functional as F
import torchaudio
import soundfile as sf
from scipy.signal import butter, sosfilt
from pathlib import Path

from .models import SynthesizerTrnMs768NSFsid


class HuBERTFeatureExtractor:
    """Extract features using HuBERT model."""

    def __init__(self, model_path, device="cpu"):
        self.device = device
        self.model = None
        self.model_path = model_path

    def load(self):
        if self.model is not None:
            return True

        try:
            from transformers import HubertModel
            self.model = HubertModel.from_pretrained("facebook/hubert-base-ls960")
            self.model = self.model.to(self.device)
            self.model.eval()
            self._use_transformers = True
            print("[OK] HuBERT loaded via transformers (facebook/hubert-base-ls960)")
            return True
        except Exception as e:
            print(f"[WARN] Transformers HuBERT failed: {e}, trying fairseq...")

        if not os.path.exists(self.model_path):
            print(f"[ERROR] HuBERT weights not found: {self.model_path}")
            return False

        try:
            from fairseq import checkpoint_utils
            models, _, _ = checkpoint_utils.load_model_ensemble_and_task(
                [self.model_path], suffix=""
            )
            self.model = models[0].to(self.device)
            self.model.eval()
            print(f"[OK] HuBERT loaded from {self.model_path}")
            return True
        except Exception as e:
            print(f"[ERROR] HuBERT load failed: {e}")
            return False

    def extract(self, audio_16k, chunk_seconds=30):
        """
        Extract features from 16kHz audio with chunking for long files.
        Returns: [1, T/320, 768] tensor
        """
        if self.model is None:
            raise RuntimeError("HuBERT model not loaded")

        chunk_samples = chunk_seconds * 16000
        total_samples = len(audio_16k)

        if total_samples <= chunk_samples:
            return self._extract_chunk(audio_16k)

        all_feats = []
        for start in range(0, total_samples, chunk_samples):
            end = min(start + chunk_samples, total_samples)
            chunk = audio_16k[start:end]
            feats = self._extract_chunk(chunk)
            all_feats.append(feats)

        return torch.cat(all_feats, dim=1)

    def _extract_chunk(self, audio_16k):
        with torch.no_grad():
            audio_tensor = torch.from_numpy(audio_16k).float().unsqueeze(0).to(self.device)

            if hasattr(self, '_use_transformers') and self._use_transformers:
                outputs = self.model(audio_tensor, output_hidden_states=True)
                feats = outputs.hidden_states[12]
            else:
                padding_mask = torch.zeros(audio_tensor.shape, dtype=torch.bool, device=self.device)
                inputs = {"source": audio_tensor, "padding_mask": padding_mask, "output_layer": 12}
                logits = self.model.extract_features(**inputs)
                feats = logits[0]

            feats = F.interpolate(feats.transpose(1, 2), scale_factor=2).transpose(1, 2)

        return feats


class RMVPEPitchExtractor:
    """RMVPE (Robust Model for Vocal Pitch Estimation) pitch extraction."""

    def __init__(self, model_path, device="cpu"):
        self.device = device
        self.model_path = model_path
        self.model = None

    def load(self):
        if self.model is not None:
            return True

        if not os.path.exists(self.model_path):
            print(f"[WARN] RMVPE weights not found: {self.model_path}")
            return False

        try:
            from .rmvpe import RMVPE
            self.model = RMVPE(self.model_path, is_half=False, device=self.device)
            print(f"[OK] RMVPE loaded from {self.model_path}")
            return True
        except Exception as e:
            print(f"[ERROR] RMVPE load failed: {e}, using fallback")
            return False

    def extract(self, audio_16k, threshold=0.03, chunk_seconds=30):
        """Extract f0 from 16kHz audio with chunking. Returns Hz array at 10ms hop."""
        if self.model is None:
            return self._fallback_f0(audio_16k)

        chunk_samples = chunk_seconds * 16000
        total_samples = len(audio_16k)

        if total_samples <= chunk_samples:
            return self.model.infer_from_audio(
                torch.from_numpy(audio_16k).float().to(self.device),
                thred=threshold
            )

        all_f0 = []
        for start in range(0, total_samples, chunk_samples):
            end = min(start + chunk_samples, total_samples)
            chunk = audio_16k[start:end]
            f0_chunk = self.model.infer_from_audio(
                torch.from_numpy(chunk).float().to(self.device),
                thred=threshold
            )
            all_f0.append(f0_chunk)

        return np.concatenate(all_f0)

    def _fallback_f0(self, audio_16k):
        """Simple autocorrelation-based f0 extraction as fallback."""
        hop = 160  # 10ms at 16kHz
        frame_len = 1024
        n_frames = len(audio_16k) // hop

        f0 = np.zeros(n_frames)
        for i in range(n_frames):
            start = i * hop
            end = min(start + frame_len, len(audio_16k))
            frame = audio_16k[start:end]

            if len(frame) < frame_len:
                frame = np.pad(frame, (0, frame_len - len(frame)))

            # Autocorrelation
            corr = np.correlate(frame, frame, mode='full')
            corr = corr[len(corr) // 2:]

            # Find first peak after minimum pitch period (50Hz = 320 samples at 16kHz)
            min_lag = int(16000 / 500)  # 500Hz max
            max_lag = int(16000 / 50)   # 50Hz min

            if max_lag > len(corr):
                max_lag = len(corr)

            segment = corr[min_lag:max_lag]
            if len(segment) > 0 and np.max(segment) > 0.3 * corr[0]:
                peak_idx = np.argmax(segment) + min_lag
                f0[i] = 16000.0 / peak_idx
            else:
                f0[i] = 0.0

        return f0


class VoiceConversionPipeline:
    """Complete RVC v2 voice conversion pipeline."""

    def __init__(self, weights_dir, device="cpu"):
        self.device = device
        self.weights_dir = weights_dir

        hubert_path = os.path.join(weights_dir, "hubert_base.pt")
        rmvpe_path = os.path.join(weights_dir, "rmvpe.pt")

        self.hubert = HuBERTFeatureExtractor(hubert_path, device)
        self.rmvpe = RMVPEPitchExtractor(rmvpe_path, device)

        self.net_g = None
        self.loaded_model_path = None
        self.model_sr = 40000
        self.index = None
        self.index_data = None

    def load_model(self, model_path, index_path=None):
        """Load RVC v2 .pth model and optional .index file."""
        if model_path == self.loaded_model_path and self.net_g is not None:
            return True

        if not os.path.exists(model_path):
            raise FileNotFoundError(f"Model not found: {model_path}")

        print(f"[LOAD] Loading model: {os.path.basename(model_path)}")
        cpt = torch.load(model_path, map_location="cpu", weights_only=False)

        config = cpt.get("config", [])
        f0_enabled = cpt.get("f0", 1)
        version = cpt.get("version", "v2")

        if version != "v2" or not f0_enabled:
            print(f"[WARN] Model is {version}/f0={f0_enabled}, expected v2/f0=1")

        # Unpack config into constructor args
        if len(config) >= 18:
            self.net_g = SynthesizerTrnMs768NSFsid(*config)
        else:
            # Default config for 40k model
            self.net_g = SynthesizerTrnMs768NSFsid(
                1025, 32, 192, 192, 768, 2, 6, 3, 0.0, "1",
                [3, 7, 11], [[1, 3, 5], [1, 3, 5], [1, 3, 5]],
                [10, 10, 2, 2], 512, [16, 16, 4, 4], 109, 256, 40000
            )

        self.net_g.load_state_dict(cpt["weight"], strict=False)
        self.net_g = self.net_g.to(self.device)
        self.net_g.eval()

        self.model_sr = config[-1] if config else 40000
        self.loaded_model_path = model_path

        # Load FAISS index if available
        if index_path and os.path.exists(index_path):
            self._load_index(index_path)

        print(f"[OK] Model loaded: sr={self.model_sr}, params={sum(p.numel() for p in self.net_g.parameters()):,}")
        return True

    def _load_index(self, index_path):
        """Load FAISS index for feature retrieval."""
        try:
            import faiss
            self.index = faiss.read_index(index_path)
            self.index_data = self.index.reconstruct_n(0, self.index.ntotal)
            print(f"[OK] FAISS index loaded: {self.index.ntotal} vectors")
        except ImportError:
            print("[WARN] faiss not installed, skipping index retrieval")
        except Exception as e:
            print(f"[WARN] Index load failed: {e}")

    def _retrieve_features(self, feats, index_rate=0.75):
        """Blend retrieved features from FAISS index."""
        if self.index is None or index_rate <= 0:
            return feats

        npy = feats[0].cpu().numpy()
        score, ix = self.index.search(npy, k=8)
        weight = np.square(1.0 / (score + 1e-6))
        weight /= weight.sum(axis=1, keepdims=True)
        retrieved = np.sum(self.index_data[ix] * np.expand_dims(weight, axis=2), axis=1)

        retrieved_tensor = torch.from_numpy(retrieved).unsqueeze(0).to(self.device).float()
        feats = feats * (1 - index_rate) + retrieved_tensor * index_rate
        return feats

    def convert(self, input_path, output_path, pitch_shift=0, formant_shift=0,
                method="rmvpe", mix=1.0, mode="low_latency", index_rate=0.75,
                progress_callback=None):
        """
        Full voice conversion pipeline.

        Args:
            input_path: Path to input WAV file
            output_path: Path to write converted audio
            pitch_shift: Semitones to shift (-12 to 12)
            formant_shift: Formant adjustment
            method: Pitch extraction method
            mix: Dry/wet ratio (0=dry, 1=wet)
            mode: low_latency or high_quality
            index_rate: Feature retrieval blend ratio
            progress_callback: fn(float, str) for progress updates
        """
        def progress(val, msg):
            if progress_callback:
                progress_callback(val, msg)
            print(f"[{val*100:.0f}%] {msg}")

        if self.net_g is None:
            raise RuntimeError("No model loaded. Call load_model() first.")

        # 1. Load and preprocess audio
        progress(0.05, "Loading audio...")
        audio, sr = sf.read(input_path, dtype="float32")
        if len(audio.shape) > 1:
            audio = audio.mean(axis=1)

        # Resample to 16kHz for feature extraction
        if sr != 16000:
            audio_16k = torchaudio.functional.resample(
                torch.from_numpy(audio).float(), sr, 16000
            ).numpy()
        else:
            audio_16k = audio

        # High-pass filter (48Hz Butterworth 5th order)
        sos = butter(5, 48, btype='high', fs=16000, output='sos')
        audio_16k = sosfilt(sos, audio_16k).astype(np.float32)

        # Pad with reflection
        t_pad = int(16000 * 0.04)  # 40ms padding
        audio_padded = np.pad(audio_16k, (t_pad, t_pad), mode='reflect')

        # 2. HuBERT feature extraction
        progress(0.15, "Extracting vocal features (HuBERT)...")
        if not self.hubert.load():
            raise RuntimeError("Failed to load HuBERT model")

        feats = self.hubert.extract(audio_padded)

        # 3. FAISS retrieval
        if self.index is not None:
            progress(0.25, "Retrieving voice characteristics...")
            feats = self._retrieve_features(feats, index_rate)

        # 4. Pitch extraction
        progress(0.30, f"Extracting pitch ({method})...")
        if not self.rmvpe.load():
            print("[WARN] Using fallback pitch extraction")

        f0 = self.rmvpe.extract(audio_padded)

        # Apply pitch shift
        if pitch_shift != 0:
            f0 = f0 * (2 ** (pitch_shift / 12.0))

        # Align lengths
        p_len = min(feats.shape[1], len(f0))
        feats = feats[:, :p_len, :]
        f0 = f0[:p_len]

        # Convert f0 to coarse (int) and fine (float) tensors
        f0_coarse = self._f0_to_coarse(f0)
        pitch = torch.LongTensor(f0_coarse).unsqueeze(0).to(self.device)
        pitchf = torch.FloatTensor(f0).unsqueeze(0).to(self.device)

        # 5. Voice conversion inference (chunked for long files)
        progress(0.50, "Running voice conversion...")
        sid = torch.LongTensor([0]).to(self.device)

        chunk_size = 3000  # ~30s of frames
        if p_len <= chunk_size:
            p_len_tensor = torch.LongTensor([p_len]).to(self.device)
            with torch.no_grad():
                audio_out = self.net_g.infer(
                    feats, p_len_tensor, pitch, pitchf, sid
                )[0][0, 0].cpu().numpy()
        else:
            audio_chunks = []
            for i in range(0, p_len, chunk_size):
                end = min(i + chunk_size, p_len)
                c_len = end - i
                c_feats = feats[:, i:end, :]
                c_pitch = pitch[:, i:end]
                c_pitchf = pitchf[:, i:end]
                c_len_t = torch.LongTensor([c_len]).to(self.device)
                with torch.no_grad():
                    chunk_out = self.net_g.infer(
                        c_feats, c_len_t, c_pitch, c_pitchf, sid
                    )[0][0, 0].cpu().numpy()
                audio_chunks.append(chunk_out)
                progress(0.50 + 0.30 * (end / p_len), f"Converting chunk {i//chunk_size + 1}...")
            audio_out = np.concatenate(audio_chunks)

        # 6. Trim padding
        t_pad_out = int(t_pad * (self.model_sr / 16000))
        if t_pad_out < len(audio_out):
            audio_out = audio_out[t_pad_out:-t_pad_out]

        # 7. Resample to original SR if needed
        if sr != self.model_sr:
            progress(0.80, "Resampling...")
            audio_out = torchaudio.functional.resample(
                torch.from_numpy(audio_out).float(), self.model_sr, sr
            ).numpy()

        # 8. Apply dry/wet mix
        if mix < 1.0:
            min_len = min(len(audio), len(audio_out))
            audio_out = audio[:min_len] * (1 - mix) + audio_out[:min_len] * mix

        # 9. RMS normalization to match input level
        in_rms = np.sqrt(np.mean(audio ** 2) + 1e-7)
        out_rms = np.sqrt(np.mean(audio_out ** 2) + 1e-7)
        if out_rms > 0:
            audio_out = audio_out * (in_rms / out_rms)

        # Clip to prevent clipping
        audio_out = np.clip(audio_out, -1.0, 1.0)

        # 10. Write output
        progress(0.95, "Saving output...")
        sf.write(output_path, audio_out, sr)

        progress(1.0, "Voice conversion complete")
        return output_path

    def _f0_to_coarse(self, f0):
        """Convert f0 Hz values to coarse integer indices (0-255)."""
        f0_min = 50.0
        f0_max = 1100.0
        f0_mel_min = 1127 * np.log(1 + f0_min / 700)
        f0_mel_max = 1127 * np.log(1 + f0_max / 700)

        f0_mel = np.where(f0 > 0, 1127 * np.log(1 + f0 / 700), 0)
        f0_mel = np.clip(f0_mel, f0_mel_min, f0_mel_max)

        coarse = np.round((f0_mel - f0_mel_min) * 254 / (f0_mel_max - f0_mel_min) + 1)
        coarse = np.where(f0 <= 0, 0, coarse)
        return coarse.astype(int)
