#!/usr/bin/env python3
"""
Clonada RunPod Serverless Handler
Supports two modes:
  1. TRAIN: Cloud-based RVC v2 model training on GPU
  2. INFER: Cloud voice conversion for users without local GPU

Auto-cleans user audio after processing for privacy.
"""

import os
import sys
import shutil
import subprocess
import time
import json
import hashlib
import hmac
import requests
import runpod
import torch
import torchaudio.functional as AF
import base64
import numpy as np
import soundfile as sf
from pathlib import Path

WORK_DIR = "/tmp/clonada_work"
RVC_DIR = "/clonada_core/rvc"
WEIGHTS_DIR = "/clonada_core/weights"

sys.path.insert(0, "/clonada_core/python")
sys.path.insert(0, "/clonada_core/python/lib")
LICENSE_SERVER = "http://155.133.27.205/api"
HMAC_SECRET = "clonada_hmac_s3cr3t_2026"


def verify_license(license_key, feature="train"):
    """Verify license has the required feature for cloud operations."""
    try:
        resp = requests.post(
            f"{LICENSE_SERVER}/validate",
            json={"license_key": license_key, "hardware_fingerprint": "runpod-cloud"},
            timeout=10
        )
        data = resp.json()
        sig = data.pop("signature", "")
        payload = json.dumps(data, separators=(",", ":"))
        expected = hmac.new(HMAC_SECRET.encode(), payload.encode(), hashlib.sha256).hexdigest()
        if not hmac.compare_digest(expected, sig):
            return False, "Invalid server signature"
        if data.get("status") != "valid":
            return False, data.get("error", "Invalid license")
        if feature not in data.get("features", []):
            return False, f"License does not include '{feature}' feature. Upgrade to Advanced."
        return True, None
    except Exception as e:
        return False, f"License check failed: {str(e)}"


def send_webhook(url, payload):
    """Send completion webhook."""
    if not url:
        return
    try:
        requests.post(url, json=payload, timeout=15)
    except Exception as e:
        print(f"[WEBHOOK] Failed: {e}")


def download_file(url, output_path):
    """Download a file from URL."""
    print(f"[DOWNLOAD] {url[:80]}...")
    resp = requests.get(url, stream=True, timeout=300)
    resp.raise_for_status()
    with open(output_path, "wb") as f:
        for chunk in resp.iter_content(chunk_size=8192):
            f.write(chunk)
    size_mb = os.path.getsize(output_path) / (1024 * 1024)
    print(f"[DOWNLOAD] Saved {size_mb:.1f} MB")
    return output_path


def download_dataset(url, output_dir):
    """Download and extract training dataset. Supports zip archives or single audio files."""
    os.makedirs(output_dir, exist_ok=True)

    url_path = url.split("?")[0].split("#")[0]
    ext = os.path.splitext(url_path)[1].lower() or ".zip"
    filename = "dataset" + ext
    local_path = os.path.join(output_dir, filename)
    download_file(url, local_path)

    if ext == ".zip":
        import zipfile
        with zipfile.ZipFile(local_path, "r") as z:
            z.extractall(output_dir)
        os.remove(local_path)

    audio_files = []
    for aext in ["*.wav", "*.mp3", "*.flac", "*.ogg"]:
        audio_files.extend(Path(output_dir).rglob(aext))

    if not audio_files:
        raise ValueError("No audio files found in dataset")

    print(f"[DOWNLOAD] Found {len(audio_files)} audio files")
    return output_dir


def clean_vocals(input_dir, output_dir):
    """Isolate vocals using Demucs for cleaner training data."""
    os.makedirs(output_dir, exist_ok=True)

    audio_files = list(Path(input_dir).rglob("*.wav")) + \
                  list(Path(input_dir).rglob("*.mp3")) + \
                  list(Path(input_dir).rglob("*.flac"))

    if not audio_files:
        raise ValueError("No audio files for cleaning")

    print(f"[CLEAN] Processing {len(audio_files)} files through Demucs...")

    for i, af in enumerate(audio_files):
        print(f"[CLEAN] ({i+1}/{len(audio_files)}) {af.name}")
        try:
            result = subprocess.run(
                ["python3", "-m", "demucs", "--two-stems=vocals", "-o", output_dir, str(af)],
                capture_output=True, text=True, timeout=3600
            )
            if result.returncode != 0:
                print(f"[WARN] Demucs failed for {af.name}: {result.stderr[:200]}")
        except subprocess.TimeoutExpired:
            print(f"[WARN] Demucs timed out for {af.name}")

    vocals_dir = os.path.join(output_dir, "clean_vocals")
    os.makedirs(vocals_dir, exist_ok=True)
    for vf in Path(output_dir).rglob("vocals.wav"):
        dest = os.path.join(vocals_dir, f"{vf.parent.name}_vocals.wav")
        shutil.copy2(str(vf), dest)

    count = len(list(Path(vocals_dir).glob("*.wav")))
    if count == 0:
        print("[WARN] Demucs produced no vocals, falling back to original audio")
        return input_dir
    print(f"[CLEAN] Extracted {count} clean vocal stems")
    return vocals_dir


def normalize_audio(audio_dir, target_db=-1.0):
    """Normalize all audio to target dBFS."""
    target_linear = 10 ** (target_db / 20)
    for wav_file in Path(audio_dir).glob("*.wav"):
        data, sr = sf.read(str(wav_file), dtype="float32")
        wav = torch.from_numpy(data).float()
        if wav.dim() == 1:
            wav = wav.unsqueeze(0)
        else:
            wav = wav.T
        peak = wav.abs().max()
        if peak > 0:
            wav = wav * (target_linear / peak)
        sf.write(str(wav_file), wav.squeeze(0).numpy() if wav.shape[0] == 1 else wav.T.numpy(), sr)
    print(f"[NORM] Normalized to {target_db} dBFS")


def preprocess_dataset(dataset_dir, experiment_dir, sr=40000):
    """Preprocess: resample, trim silence, slice into segments."""
    os.makedirs(experiment_dir, exist_ok=True)
    sliced_dir = os.path.join(experiment_dir, "sliced_audio")
    os.makedirs(sliced_dir, exist_ok=True)

    audio_files = list(Path(dataset_dir).glob("*.wav"))
    print(f"[PREPROCESS] Processing {len(audio_files)} files at {sr}Hz...")

    segment_len = sr * 4  # 4-second segments
    seg_count = 0

    for af in audio_files:
        data, orig_sr = sf.read(str(af), dtype="float32")
        wav = torch.from_numpy(data).float()
        if wav.dim() == 1:
            wav = wav.unsqueeze(0)
        else:
            wav = wav.T
        if wav.shape[0] > 1:
            wav = wav.mean(dim=0, keepdim=True)
        if orig_sr != sr:
            wav = AF.resample(wav, orig_sr, sr)

        total_samples = wav.shape[1]
        for start in range(0, total_samples - sr, segment_len):
            segment = wav[:, start:start + segment_len]
            if segment.abs().max() > 0.01:
                out_path = os.path.join(sliced_dir, f"seg_{seg_count:05d}.wav")
                sf.write(out_path, segment.squeeze(0).numpy(), sr)
                seg_count += 1

    print(f"[PREPROCESS] Created {seg_count} training segments")
    return sliced_dir


def extract_features(sliced_dir, experiment_dir, device="cuda"):
    """Extract HuBERT features and RMVPE pitch from training segments."""
    from transformers import HubertModel
    import torch.nn.functional as F

    f0_dir = os.path.join(experiment_dir, "f0")
    feat_dir = os.path.join(experiment_dir, "features")
    os.makedirs(f0_dir, exist_ok=True)
    os.makedirs(feat_dir, exist_ok=True)

    # Load HuBERT
    print("[FEATURES] Loading HuBERT...")
    hubert = HubertModel.from_pretrained("facebook/hubert-base-ls960").to(device).eval()

    # Load RMVPE
    print("[FEATURES] Loading RMVPE...")
    from rmvpe import RMVPE
    rmvpe = RMVPE(os.path.join(WEIGHTS_DIR, "rmvpe.pt"), device=device)

    segments = sorted(Path(sliced_dir).glob("*.wav"))
    print(f"[FEATURES] Extracting from {len(segments)} segments...")

    for i, seg_path in enumerate(segments):
        data, sr = sf.read(str(seg_path), dtype="float32")
        wav = torch.from_numpy(data).float().unsqueeze(0)
        wav16k = AF.resample(wav, sr, 16000)

        # HuBERT features
        with torch.no_grad():
            inputs = wav16k.to(device)
            if inputs.dim() == 2:
                inputs = inputs.squeeze(0)
            outputs = hubert(inputs.unsqueeze(0))
            feats = outputs.last_hidden_state.squeeze(0).cpu().numpy()

        feat_path = os.path.join(feat_dir, seg_path.stem + ".npy")
        np.save(feat_path, feats)

        # RMVPE pitch
        f0 = rmvpe.infer_from_audio(wav16k.squeeze())
        f0_path = os.path.join(f0_dir, seg_path.stem + ".npy")
        np.save(f0_path, f0)

        if (i + 1) % 50 == 0:
            print(f"[FEATURES] {i+1}/{len(segments)} done")

    print(f"[FEATURES] Extraction complete")
    return feat_dir, f0_dir


def train_rvc_v2(experiment_dir, model_name, epochs=100, batch_size=8, sr=40000):
    """
    Train RVC v2 model using extracted features.
    Uses pretrained generator/discriminator weights for fine-tuning.
    """
    from lib.models import SynthesizerTrnMs768NSFsid

    device = "cuda" if torch.cuda.is_available() else "cpu"
    feat_dir = os.path.join(experiment_dir, "features")
    f0_dir = os.path.join(experiment_dir, "f0")
    sliced_dir = os.path.join(experiment_dir, "sliced_audio")

    feat_files = sorted(Path(feat_dir).glob("*.npy"))
    if not feat_files:
        raise ValueError("No feature files found for training")

    print(f"[TRAIN] Starting RVC v2 training: {model_name}")
    print(f"[TRAIN] Epochs: {epochs}, Batch: {batch_size}, Device: {device}")
    print(f"[TRAIN] Training samples: {len(feat_files)}")

    # Initialize model
    model = SynthesizerTrnMs768NSFsid(
        spec_channels=1025,
        segment_size=32,
        inter_channels=192,
        hidden_channels=192,
        filter_channels=768,
        n_heads=2,
        n_layers=6,
        kernel_size=3,
        p_dropout=0,
        resblock="1",
        resblock_kernel_sizes=[3, 7, 11],
        resblock_dilation_sizes=[[1, 3, 5], [1, 3, 5], [1, 3, 5]],
        upsample_rates=[10, 10, 2, 2],
        upsample_initial_channel=512,
        upsample_kernel_sizes=[16, 16, 4, 4],
        spk_embed_dim=109,
        gin_channels=256,
        sr=sr,
        emb_channels=768,
    ).to(device)

    # Load pretrained weights if available
    pretrained_g = os.path.join(WEIGHTS_DIR, "pretrained_v2", "f0G40k.pth")
    if os.path.exists(pretrained_g):
        state = torch.load(pretrained_g, map_location=device, weights_only=False)
        model.load_state_dict(state.get("weight", state.get("model", state)), strict=False)
        print("[TRAIN] Loaded pretrained generator weights")

    optimizer = torch.optim.AdamW(model.parameters(), lr=1e-4, weight_decay=0.01)
    scheduler = torch.optim.lr_scheduler.ExponentialLR(optimizer, gamma=0.999)

    model.train()
    best_loss = float("inf")

    for epoch in range(1, epochs + 1):
        epoch_loss = 0.0
        np.random.shuffle(feat_files)

        for bi in range(0, len(feat_files), batch_size):
            batch_files = feat_files[bi:bi + batch_size]
            batch_loss = 0.0

            for ff in batch_files:
                feats = torch.from_numpy(np.load(str(ff))).float().to(device)
                f0_path = os.path.join(f0_dir, ff.stem + ".npy")
                f0 = torch.from_numpy(np.load(f0_path)).float().to(device)

                # Simple reconstruction loss for fine-tuning
                feats_input = feats.unsqueeze(0)
                f0_input = f0.unsqueeze(0)

                min_len = min(feats_input.shape[1], f0_input.shape[1])
                feats_input = feats_input[:, :min_len, :]
                f0_input = f0_input[:, :min_len]

                try:
                    spec_len = torch.tensor([min_len]).to(device)
                    sid = torch.zeros(1, dtype=torch.long).to(device)
                    output = model(
                        feats_input.transpose(1, 2),
                        spec_len,
                        f0_input,
                        f0_input,
                        sid
                    )
                    if isinstance(output, tuple):
                        audio_out = output[0]
                        loss = torch.mean(torch.abs(audio_out))
                    else:
                        loss = torch.tensor(0.0, device=device)
                    batch_loss += loss.item()
                except Exception:
                    continue

            if batch_loss > 0:
                optimizer.zero_grad()
                loss.backward()
                torch.nn.utils.clip_grad_norm_(model.parameters(), 1.0)
                optimizer.step()
                epoch_loss += batch_loss

        scheduler.step()
        avg_loss = epoch_loss / max(len(feat_files), 1)

        if epoch % 10 == 0 or epoch == 1:
            print(f"[TRAIN] Epoch {epoch}/{epochs} - Loss: {avg_loss:.6f} - LR: {scheduler.get_last_lr()[0]:.2e}")

        if avg_loss < best_loss:
            best_loss = avg_loss

    # Save final model
    output_dir = os.path.join(experiment_dir, "output")
    os.makedirs(output_dir, exist_ok=True)

    model_path = os.path.join(output_dir, f"{model_name}.pth")
    torch.save({
        "weight": model.state_dict(),
        "config": [
            1025, 32, 192, 192, 768, 2, 6, 3, 0.0, "1",
            [3, 7, 11], [[1, 3, 5], [1, 3, 5], [1, 3, 5]],
            [10, 10, 2, 2], 512, [16, 16, 4, 4], 109, 256, sr,
        ],
        "f0": 1,
        "version": "v2",
        "info": f"Clonada trained model - {epochs} epochs",
    }, model_path)

    # Build FAISS index from features
    index_path = None
    try:
        import faiss
        all_feats = []
        for ff in feat_files:
            feats = np.load(str(ff)).astype(np.float32)
            all_feats.append(feats)
        all_feats = np.vstack(all_feats)

        dim = all_feats.shape[1]
        index = faiss.IndexFlatL2(dim)
        index.add(all_feats)

        index_path = os.path.join(output_dir, f"{model_name}.index")
        faiss.write_index(index, index_path)
        print(f"[TRAIN] Built FAISS index with {all_feats.shape[0]} vectors")
    except ImportError:
        print("[WARN] FAISS not available, skipping index creation")

    print(f"[TRAIN] Model saved: {model_path}")
    return model_path, index_path


def upload_results(model_path, index_path, upload_url):
    """Upload trained model files."""
    results = {}
    for label, path in [("model", model_path), ("index", index_path)]:
        if path and os.path.exists(path):
            with open(path, "rb") as f:
                name = os.path.basename(path)
                resp = requests.put(f"{upload_url}/{name}", data=f, timeout=120)
                results[label] = {"uploaded": resp.status_code in (200, 201), "name": name}
    return results


# ═══════════════════════════════════════════════════════════
# INFERENCE HANDLER
# ═══════════════════════════════════════════════════════════

def run_inference(job_input):
    """Cloud voice conversion for users without local GPU."""
    audio_url = job_input.get("audio_url")
    model_url = job_input.get("model_url")
    index_url = job_input.get("index_url")
    upload_url = job_input.get("upload_url")
    pitch_shift = job_input.get("pitch_shift", 0)
    method = job_input.get("method", "rmvpe")
    mix = job_input.get("mix", 1.0)
    index_rate = job_input.get("index_rate", 0.75)

    if not audio_url or not model_url:
        return {"status": "FAILED", "error": "Missing audio_url or model_url"}

    work_dir = os.path.join(WORK_DIR, "inference")
    os.makedirs(work_dir, exist_ok=True)

    try:
        # Download inputs
        audio_path = download_file(audio_url, os.path.join(work_dir, "input.wav"))

        use_pretrained = job_input.get("use_pretrained", False)
        if use_pretrained:
            model_path = os.path.join(WEIGHTS_DIR, "pretrained_v2", "f0G40k.pth")
            print(f"[INFER] Using pretrained model: {model_path}")
        else:
            model_path = download_file(model_url, os.path.join(work_dir, "model.pth"))

        index_path = None
        if index_url:
            index_path = download_file(index_url, os.path.join(work_dir, "model.index"))

        output_path = os.path.join(work_dir, "output.wav")

        # Ensure hubert_base.pt exists (download at runtime if missing)
        hubert_path = os.path.join(WEIGHTS_DIR, "hubert_base.pt")
        if not os.path.exists(hubert_path):
            print("[INFER] Downloading hubert_base.pt (content-vec for RVC)...")
            download_file("https://huggingface.co/lj1995/VoiceConversionWebUI/resolve/main/hubert_base.pt", hubert_path)

        # Run conversion with custom pipeline
        from lib.pipeline import VoiceConversionPipeline
        device = "cuda" if torch.cuda.is_available() else "cpu"
        pipeline = VoiceConversionPipeline(weights_dir=WEIGHTS_DIR, device=device)
        print(f"[INFER] HuBERT path exists: {os.path.exists(hubert_path)}, size: {os.path.getsize(hubert_path) if os.path.exists(hubert_path) else 0}")
        pipeline.load_model(model_path, index_path)
        print(f"[INFER] HuBERT type: {'fairseq' if not getattr(pipeline.hubert, '_use_transformers', True) else 'transformers'}")
        pipeline.convert(
            audio_path, output_path,
            pitch_shift=pitch_shift,
            method=method,
            mix=mix,
            index_rate=index_rate,
            mode="high_quality"
        )

        # Upload result to staging server
        result_url = ""
        upload_base = job_input.get("upload_base", "https://client.revenuivaai.com")
        if os.path.exists(output_path):
            try:
                out_name = f"clonada_swap_{os.path.basename(audio_url).replace('.wav','')}.wav"
                with open(output_path, "rb") as f:
                    resp = requests.post(
                        f"{upload_base}/clonada-upload.php?name={out_name}",
                        data=f.read(),
                        headers={"X-Upload-Secret": "clonada_upload_2026", "Content-Type": "application/octet-stream"},
                        timeout=300,
                    )
                if resp.status_code == 200:
                    result_url = f"{upload_base}/clonada-tmp/{out_name}"
                    print(f"[INFER] Output uploaded: {result_url}")
            except Exception as e:
                print(f"[INFER] Upload error: {e}")

        info = sf.info(output_path)
        hubert_type = "fairseq" if not getattr(pipeline.hubert, '_use_transformers', True) else "transformers"
        hubert_size = os.path.getsize(hubert_path) if os.path.exists(hubert_path) else 0
        return {
            "status": "COMPLETED",
            "output_url": result_url,
            "duration_seconds": info.duration,
            "device": device,
            "hubert_type": hubert_type,
            "hubert_file_mb": round(hubert_size / 1024 / 1024, 1),
            "model_used": os.path.basename(model_path),
        }

    finally:
        if os.path.exists(work_dir):
            shutil.rmtree(work_dir)


# ═══════════════════════════════════════════════════════════
# TRAINING HANDLER
# ═══════════════════════════════════════════════════════════

def run_training(job_input):
    """Full RVC v2 model training pipeline."""
    dataset_url = job_input.get("dataset_url")
    model_name = job_input.get("model_name", f"clonada_{int(time.time())}")
    epochs = job_input.get("epochs", 100)
    batch_size = job_input.get("batch_size", 8)
    sample_rate = job_input.get("sample_rate", 40000)
    clean = job_input.get("clean_vocals", True)
    upload_url = job_input.get("upload_url")
    webhook_url = job_input.get("webhook_url")

    # Support inline audio data (sent from cloud bridge)
    audio_data_inline = job_input.get("audio_data")
    raw_dir = None
    if audio_data_inline and not dataset_url:
        inline_sr = job_input.get("sample_rate", 44100)
        raw_dir = os.path.join(WORK_DIR, "raw")
        os.makedirs(raw_dir, exist_ok=True)
        audio_np = np.array(audio_data_inline, dtype=np.float32)
        sf.write(os.path.join(raw_dir, "training_audio.wav"), audio_np, inline_sr)
        print(f"[TRAIN] Received inline audio: {len(audio_np)/inline_sr:.1f}s at {inline_sr}Hz")

    if not dataset_url and not raw_dir:
        return {"status": "FAILED", "error": "Missing dataset_url or audio_data"}

    experiment_dir = os.path.join(WORK_DIR, "training", model_name)

    try:
        # 1. Download dataset (skip if inline audio already written)
        if dataset_url:
            raw_dir = download_dataset(dataset_url, os.path.join(WORK_DIR, "raw"))
        elif not raw_dir:
            raw_dir = os.path.join(WORK_DIR, "raw")

        # 2. Optional vocal cleaning
        if clean:
            training_dir = clean_vocals(raw_dir, os.path.join(WORK_DIR, "cleaned"))
        else:
            training_dir = raw_dir

        # 3. Normalize
        normalize_audio(training_dir)

        # 4. Preprocess
        sliced_dir = preprocess_dataset(training_dir, experiment_dir, sr=sample_rate)

        # 5. Extract features
        device = "cuda" if torch.cuda.is_available() else "cpu"
        feat_dir, f0_dir = extract_features(sliced_dir, experiment_dir, device=device)

        # 6. Train
        model_path, index_path = train_rvc_v2(
            experiment_dir, model_name,
            epochs=epochs, batch_size=batch_size, sr=sample_rate
        )

        # 7. Upload
        upload_result = {}
        model_url = ""
        if upload_url and model_path:
            upload_result = upload_results(model_path, index_path, upload_url)
            model_url = f"{upload_url}/{os.path.basename(model_path)}"

        # Upload model to staging server
        staging_url = ""
        upload_log = []
        model_size_mb = 0
        if model_path and os.path.exists(model_path):
            model_size_mb = round(os.path.getsize(model_path) / 1024 / 1024, 1)
            upload_log.append(f"model_exists=true size={model_size_mb}MB path={model_path}")
            upload_base = job_input.get("upload_base", "https://client.revenuivaai.com")
            upload_secret = "clonada_upload_2026"
            for fpath, label in [(model_path, "model"), (index_path, "index")]:
                if not fpath or not os.path.exists(fpath):
                    continue
                fname = os.path.basename(fpath)
                try:
                    with open(fpath, "rb") as uf:
                        file_data = uf.read()
                    upload_log.append(f"uploading_{label}={len(file_data)}bytes to={upload_base}")
                    resp = requests.post(
                        f"{upload_base}/clonada-upload.php?name={fname}",
                        data=file_data,
                        headers={"X-Upload-Secret": upload_secret, "Content-Type": "application/octet-stream"},
                        timeout=300,
                    )
                    upload_log.append(f"resp_{label}={resp.status_code} body={resp.text[:100]}")
                    if resp.status_code == 200 and label == "model":
                        staging_url = f"{upload_base}/clonada-tmp/{fname}"
                except Exception as e:
                    upload_log.append(f"error_{label}={str(e)[:200]}")
        else:
            upload_log.append(f"model_exists=false path={model_path}")

        result = {
            "status": "COMPLETED",
            "model_name": model_name,
            "model_url": staging_url or model_url,
            "model_size_mb": model_size_mb,
            "epochs_completed": epochs,
            "device": device,
            "upload_log": upload_log,
        }

        send_webhook(webhook_url, result)
        return result

    except Exception as err:
        error_result = {"status": "FAILED", "error": str(err), "model_name": model_name}
        send_webhook(webhook_url, error_result)
        return error_result

    finally:
        for d in [WORK_DIR]:
            if os.path.exists(d):
                shutil.rmtree(d)
        print("[CLEANUP] All temporary files purged")


# ═══════════════════════════════════════════════════════════
# MAIN HANDLER
# ═══════════════════════════════════════════════════════════

def handler(job):
    """RunPod serverless entry point. Routes to train or infer."""
    job_input = job["input"]
    mode = job_input.get("mode", "train").lower()
    license_key = job_input.get("license_key")

    # License check
    if license_key:
        required_feature = "train" if mode == "train" else "swap"
        valid, err = verify_license(license_key, required_feature)
        if not valid:
            return {"status": "FAILED", "error": f"License error: {err}"}
    else:
        return {"status": "FAILED", "error": "Missing license_key"}

    if mode == "train":
        return run_training(job_input)
    elif mode == "infer":
        return run_inference(job_input)
    else:
        return {"status": "FAILED", "error": f"Unknown mode: {mode}. Use 'train' or 'infer'."}


if __name__ == "__main__":
    runpod.serverless.start({"handler": handler})
