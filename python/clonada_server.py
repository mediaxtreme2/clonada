#!/usr/bin/env python3
"""
Clonada Local AI Engine - ZeroMQ Sidecar Server
Handles voice conversion (RVC), stem separation (Demucs), and model management.
Communicates with REAPER Lua script and C++/JUCE plugin via ZeroMQ TCP.
"""

import sys
import os
import argparse
import json
import signal
import time
import threading
import numpy as np
import zmq
import torch
import torchaudio
import soundfile as sf
from pathlib import Path

# ═══════════════════════════════════════════════════════════
# GPU DETECTION
# ═══════════════════════════════════════════════════════════

def detect_device():
    if torch.cuda.is_available():
        gpu_name = torch.cuda.get_device_name(0)
        vram = torch.cuda.get_device_properties(0).total_mem / (1024**3)
        print(f"[GPU] CUDA detected: {gpu_name} ({vram:.1f} GB VRAM)")
        return "cuda"
    elif hasattr(torch.backends, "mps") and torch.backends.mps.is_available():
        print("[GPU] Apple Metal (MPS) detected")
        return "mps"
    else:
        print("[CPU] No GPU detected, using CPU inference")
        return "cpu"


# ═══════════════════════════════════════════════════════════
# RVC INFERENCE ENGINE
# ═══════════════════════════════════════════════════════════

class RVCInferenceEngine:
    """Handles voice conversion using RVC v2 models."""

    def __init__(self, device="cpu"):
        self.device = device
        self.loaded_model = None
        self.loaded_model_path = None
        self.hubert_model = None
        self.sample_rate = 16000

    def load_hubert(self, weights_path):
        """Load HuBERT base model for feature extraction."""
        if not os.path.exists(weights_path):
            print(f"[WARN] HuBERT weights not found at {weights_path}")
            return False

        try:
            from fairseq import checkpoint_utils
            models, _, _ = checkpoint_utils.load_model_ensemble_and_task(
                [weights_path], suffix=""
            )
            self.hubert_model = models[0].to(self.device)
            self.hubert_model.eval()
            print(f"[OK] HuBERT model loaded from {weights_path}")
            return True
        except ImportError:
            print("[WARN] fairseq not installed, HuBERT loading skipped")
            return False
        except Exception as e:
            print(f"[ERROR] Failed to load HuBERT: {e}")
            return False

    def load_model(self, model_path):
        """Load an RVC .pth voice model."""
        if self.loaded_model_path == model_path and self.loaded_model is not None:
            return True

        if not os.path.exists(model_path):
            print(f"[ERROR] Model not found: {model_path}")
            return False

        try:
            checkpoint = torch.load(model_path, map_location=self.device, weights_only=False)
            self.loaded_model = checkpoint
            self.loaded_model_path = model_path
            print(f"[OK] RVC model loaded: {os.path.basename(model_path)}")
            return True
        except Exception as e:
            print(f"[ERROR] Failed to load model: {e}")
            return False

    def infer(self, audio_data, pitch_shift=0, formant_shift=0, method="rmvpe", mix=1.0, mode="low_latency"):
        """
        Run voice conversion inference.

        Args:
            audio_data: numpy float32 array of audio samples
            pitch_shift: semitones to shift (-12 to 12)
            formant_shift: formant adjustment (-5 to 5)
            method: pitch extraction method (rmvpe, crepe, harvest, fcpe)
            mix: dry/wet mix (0.0 = dry, 1.0 = wet)
            mode: low_latency or high_quality

        Returns:
            numpy float32 array of processed audio
        """
        if self.loaded_model is None:
            raise RuntimeError("No model loaded")

        audio_tensor = torch.from_numpy(audio_data).float().to(self.device)

        if mode == "low_latency":
            block_size = 256
        else:
            block_size = 16384

        # Process in blocks
        output_chunks = []
        total_samples = len(audio_tensor)

        for start in range(0, total_samples, block_size):
            end = min(start + block_size, total_samples)
            chunk = audio_tensor[start:end]

            with torch.no_grad():
                # Pitch shift via resampling approximation
                if pitch_shift != 0:
                    ratio = 2 ** (pitch_shift / 12.0)
                    chunk = torchaudio.functional.resample(
                        chunk.unsqueeze(0),
                        orig_freq=int(self.sample_rate),
                        new_freq=int(self.sample_rate * ratio)
                    ).squeeze(0)

                # TODO: Full RVC pipeline integration
                # For now, apply basic transformation
                processed = chunk

            output_chunks.append(processed)

        output = torch.cat(output_chunks)

        # Apply dry/wet mix
        if mix < 1.0:
            dry = torch.from_numpy(audio_data[:len(output)]).float().to(self.device)
            output = dry * (1.0 - mix) + output * mix

        return output.cpu().numpy()


# ═══════════════════════════════════════════════════════════
# DEMUCS STEM SEPARATOR
# ═══════════════════════════════════════════════════════════

class StemSeparator:
    """Handles vocal isolation using Demucs."""

    def __init__(self, device="cpu"):
        self.device = device
        self.model = None
        self.model_name = "htdemucs"

    def load_model(self):
        """Load Demucs model."""
        try:
            from demucs.pretrained import get_model
            from demucs.apply import apply_model
            self.model = get_model(self.model_name)
            self.model.to(self.device)
            self.model.eval()
            self.apply_fn = apply_model
            print(f"[OK] Demucs model loaded: {self.model_name}")
            return True
        except ImportError:
            print("[WARN] Demucs not installed")
            return False
        except Exception as e:
            print(f"[ERROR] Failed to load Demucs: {e}")
            return False

    def separate(self, audio_path, output_dir):
        """
        Separate stems from audio file.

        Returns dict with paths to separated stems:
        {vocals, drums, bass, other}
        """
        if self.model is None:
            if not self.load_model():
                raise RuntimeError("Demucs model not available")

        wav, sr = torchaudio.load(audio_path)
        wav = wav.to(self.device)

        # Ensure stereo
        if wav.shape[0] == 1:
            wav = wav.repeat(2, 1)

        ref = wav.mean(0)
        wav = (wav - ref.mean()) / ref.std()

        with torch.no_grad():
            sources = self.apply_fn(self.model, wav.unsqueeze(0), device=self.device)

        sources = sources.squeeze(0)
        sources = sources * ref.std() + ref.mean()

        os.makedirs(output_dir, exist_ok=True)
        stem_names = ["drums", "bass", "other", "vocals"]
        result = {}

        for i, name in enumerate(stem_names):
            stem_path = os.path.join(output_dir, f"{name}.wav")
            torchaudio.save(stem_path, sources[i].cpu(), sr)
            result[name] = stem_path

        return result


# ═══════════════════════════════════════════════════════════
# MAIN SERVER
# ═══════════════════════════════════════════════════════════

class ClonadaLocalEngine:
    """ZeroMQ server handling all AI operations."""

    VERSION = "1.0.0"

    def __init__(self, port=5050, models_dir=None, weights_dir=None):
        self.port = port
        self.context = zmq.Context()
        self.socket = self.context.socket(zmq.REP)
        self.socket.bind(f"tcp://127.0.0.1:{port}")
        self.is_running = True

        self.device = detect_device()
        self.rvc_engine = RVCInferenceEngine(self.device)
        self.stem_separator = StemSeparator(self.device)

        self.models_dir = models_dir or os.path.join(os.path.dirname(__file__), "..", "models")
        self.weights_dir = weights_dir or os.path.join(os.path.dirname(__file__), "..", "weights")

        os.makedirs(self.models_dir, exist_ok=True)
        os.makedirs(self.weights_dir, exist_ok=True)

        # Load HuBERT base weights if available
        hubert_path = os.path.join(self.weights_dir, "hubert_base.pt")
        self.rvc_engine.load_hubert(hubert_path)

    def handle_swap(self, message):
        """Process voice swap request."""
        try:
            audio_data = np.array(message["audio"], dtype=np.float32)
            model_path = message.get("model", "")
            pitch = message.get("pitch", 0)
            formant = message.get("formant", 0)
            method = message.get("method", "rmvpe")
            mix = message.get("mix", 1.0)
            mode = message.get("mode", "low_latency")

            if not self.rvc_engine.load_model(model_path):
                return {"status": "ERROR", "message": f"Failed to load model: {model_path}"}

            result = self.rvc_engine.infer(
                audio_data,
                pitch_shift=pitch,
                formant_shift=formant,
                method=method,
                mix=mix,
                mode=mode
            )

            return {
                "status": "SUCCESS",
                "audio": result.tolist(),
                "sample_rate": self.rvc_engine.sample_rate
            }
        except Exception as e:
            return {"status": "ERROR", "message": str(e)}

    def handle_swap_file(self, message):
        """Process voice swap from file path (more efficient for large audio)."""
        try:
            input_path = message["input"]
            output_path = message.get("output", input_path.replace(".wav", "_clonada.wav"))
            model_path = message["model"]
            pitch = message.get("pitch", 0)
            formant = message.get("formant", 0)
            method = message.get("method", "rmvpe")
            mix = message.get("mix", 1.0)
            mode = message.get("mode", "low_latency")
            progress_file = message.get("progress", "")

            # Load audio
            audio_data, sr = sf.read(input_path, dtype="float32")
            if len(audio_data.shape) > 1:
                audio_data = audio_data.mean(axis=1)

            self.rvc_engine.sample_rate = sr

            if not self.rvc_engine.load_model(model_path):
                return {"status": "ERROR", "message": f"Failed to load model: {model_path}"}

            # Write progress
            if progress_file:
                with open(progress_file, "w") as f:
                    f.write("0.3|Loading model...")

            result = self.rvc_engine.infer(
                audio_data, pitch_shift=pitch, formant_shift=formant,
                method=method, mix=mix, mode=mode
            )

            if progress_file:
                with open(progress_file, "w") as f:
                    f.write("0.8|Saving output...")

            sf.write(output_path, result, sr)

            if progress_file:
                with open(progress_file, "w") as f:
                    f.write("1.0|Complete")

            return {
                "status": "SUCCESS",
                "output": output_path,
                "duration": len(result) / sr
            }
        except Exception as e:
            return {"status": "ERROR", "message": str(e)}

    def handle_separate(self, message):
        """Process stem separation request."""
        try:
            input_path = message["input"]
            output_dir = message.get("output_dir", os.path.dirname(input_path))
            progress_file = message.get("progress", "")

            if progress_file:
                with open(progress_file, "w") as f:
                    f.write("0.1|Loading Demucs model...")

            stems = self.stem_separator.separate(input_path, output_dir)

            if progress_file:
                with open(progress_file, "w") as f:
                    f.write("1.0|Separation complete")

            return {"status": "SUCCESS", "stems": stems}
        except Exception as e:
            return {"status": "ERROR", "message": str(e)}

    def handle_list_models(self, message):
        """List available voice models."""
        models = []
        models_dir = message.get("models_dir", self.models_dir)

        if os.path.isdir(models_dir):
            for f in os.listdir(models_dir):
                if f.endswith(".pth"):
                    path = os.path.join(models_dir, f)
                    size_mb = os.path.getsize(path) / (1024 * 1024)
                    models.append({
                        "name": f,
                        "path": path,
                        "size_mb": round(size_mb, 1)
                    })

        return {"status": "SUCCESS", "models": models}

    def handle_health(self, message):
        """Health check endpoint."""
        return {
            "status": "SUCCESS",
            "version": self.VERSION,
            "device": self.device,
            "gpu_available": self.device != "cpu",
            "models_dir": self.models_dir,
            "port": self.port
        }

    def process_stream(self):
        """Main event loop."""
        print(f"\n{'='*60}")
        print(f"  CLONADA LOCAL AI ENGINE v{self.VERSION}")
        print(f"  Listening on tcp://127.0.0.1:{self.port}")
        print(f"  Device: {self.device}")
        print(f"  Models: {self.models_dir}")
        print(f"{'='*60}\n")

        handlers = {
            "SWAP": self.handle_swap,
            "SWAP_FILE": self.handle_swap_file,
            "SEPARATE": self.handle_separate,
            "LIST_MODELS": self.handle_list_models,
            "HEALTH": self.handle_health,
        }

        while self.is_running:
            try:
                message = self.socket.recv_json()

                # Version check
                msg_version = message.get("version", self.VERSION)
                if msg_version != self.VERSION:
                    self.socket.send_json({
                        "status": "ERROR",
                        "message": f"Version mismatch: expected {self.VERSION}, got {msg_version}"
                    })
                    continue

                command = message.get("command", "").upper()

                if command == "SHUTDOWN":
                    print("[CMD] Shutdown requested")
                    self.socket.send_json({"status": "TERMINATED"})
                    self.is_running = False
                    break

                handler = handlers.get(command)
                if handler:
                    print(f"[CMD] {command}")
                    response = handler(message)
                    self.socket.send_json(response)
                else:
                    self.socket.send_json({
                        "status": "ERROR",
                        "message": f"Unknown command: {command}"
                    })

            except zmq.ZMQError as e:
                if e.errno == zmq.ETERM:
                    break
                print(f"[ZMQ ERROR] {e}")
            except json.JSONDecodeError as e:
                self.socket.send_json({"status": "ERROR", "message": f"Invalid JSON: {e}"})
            except Exception as e:
                print(f"[ERROR] {e}")
                try:
                    self.socket.send_json({"status": "ERROR", "message": str(e)})
                except:
                    pass

        self.cleanup()

    def cleanup(self):
        """Clean shutdown."""
        print("[SHUTDOWN] Cleaning up...")
        self.socket.close()
        self.context.term()
        print("[SHUTDOWN] Engine terminated.")


def main():
    parser = argparse.ArgumentParser(description="Clonada Local AI Engine")
    parser.add_argument("--port", type=int, default=5050, help="ZeroMQ TCP port")
    parser.add_argument("--models-dir", type=str, default=None, help="Path to voice models directory")
    parser.add_argument("--weights-dir", type=str, default=None, help="Path to base weights directory")
    parser.add_argument("--device", type=str, default=None, choices=["cpu", "cuda", "mps"], help="Force device")
    args = parser.parse_args()

    engine = ClonadaLocalEngine(
        port=args.port,
        models_dir=args.models_dir,
        weights_dir=args.weights_dir
    )

    if args.device:
        engine.device = args.device
        engine.rvc_engine.device = args.device
        engine.stem_separator.device = args.device

    signal.signal(signal.SIGINT, lambda *_: setattr(engine, 'is_running', False))
    signal.signal(signal.SIGTERM, lambda *_: setattr(engine, 'is_running', False))

    engine.process_stream()


if __name__ == "__main__":
    main()
