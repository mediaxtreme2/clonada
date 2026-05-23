#!/usr/bin/env python3
"""
Clonada RunPod Serverless Handler
Manages cloud-based RVC model training on GPU instances.
Auto-cleans user audio after training for privacy.
"""

import os
import sys
import shutil
import subprocess
import time
import json
import hashlib
import requests
import runpod
import torch
import torchaudio
import numpy as np
from pathlib import Path

WORK_DIR = "/tmp/clonada_training"
MODELS_OUTPUT = "/tmp/clonada_models"
CLEANED_DIR = "/tmp/clonada_cleaned"


def download_dataset(url, output_dir):
    """Download and extract training dataset from S3/R2 URL."""
    os.makedirs(output_dir, exist_ok=True)
    local_path = os.path.join(output_dir, "dataset.zip")

    print(f"[DOWNLOAD] Fetching dataset from {url[:60]}...")
    resp = requests.get(url, stream=True, timeout=300)
    resp.raise_for_status()

    total = int(resp.headers.get("content-length", 0))
    downloaded = 0

    with open(local_path, "wb") as f:
        for chunk in resp.iter_content(chunk_size=8192):
            f.write(chunk)
            downloaded += len(chunk)

    # Extract if zip
    if local_path.endswith(".zip"):
        import zipfile
        with zipfile.ZipFile(local_path, "r") as z:
            z.extractall(output_dir)
        os.remove(local_path)

    audio_files = []
    for ext in ["*.wav", "*.mp3", "*.flac", "*.ogg"]:
        audio_files.extend(Path(output_dir).rglob(ext))

    print(f"[DOWNLOAD] Found {len(audio_files)} audio files")
    return output_dir


def run_vocal_cleaning(input_dir):
    """
    Deep de-reverb and stem isolation using Demucs.
    Strips background, reverb, bleed from raw recordings.
    """
    os.makedirs(CLEANED_DIR, exist_ok=True)

    audio_files = []
    for ext in ["*.wav", "*.mp3", "*.flac"]:
        audio_files.extend(Path(input_dir).rglob(ext))

    if not audio_files:
        raise ValueError("No audio files found in dataset")

    print(f"[CLEAN] Processing {len(audio_files)} files through Demucs...")

    for i, audio_file in enumerate(audio_files):
        print(f"[CLEAN] ({i+1}/{len(audio_files)}) {audio_file.name}")

        cmd = [
            "python3", "-m", "demucs",
            "--two-stems=vocals",
            "-o", CLEANED_DIR,
            str(audio_file)
        ]

        result = subprocess.run(cmd, capture_output=True, text=True, timeout=600)
        if result.returncode != 0:
            print(f"[WARN] Demucs failed for {audio_file.name}: {result.stderr[:200]}")

    # Collect vocal stems
    vocal_dir = os.path.join(CLEANED_DIR, "vocals_only")
    os.makedirs(vocal_dir, exist_ok=True)

    for vocals_file in Path(CLEANED_DIR).rglob("vocals.wav"):
        dest = os.path.join(vocal_dir, f"{vocals_file.parent.name}_vocals.wav")
        shutil.copy2(str(vocals_file), dest)

    vocal_count = len(list(Path(vocal_dir).glob("*.wav")))
    print(f"[CLEAN] Extracted {vocal_count} clean vocal stems")

    return vocal_dir


def normalize_audio(audio_dir, target_db=-1.0):
    """Normalize all audio files to target dBFS."""
    target_linear = 10 ** (target_db / 20)

    for wav_file in Path(audio_dir).glob("*.wav"):
        wav, sr = torchaudio.load(str(wav_file))
        peak = wav.abs().max()
        if peak > 0:
            wav = wav * (target_linear / peak)
        torchaudio.save(str(wav_file), wav, sr)

    print(f"[NORM] Normalized to {target_db} dBFS")


def train_rvc_model(dataset_dir, epochs=100, batch_size=8, sample_rate=40000):
    """
    Train RVC v2 model on cleaned vocal dataset.
    Returns path to trained .pth and .index files.
    """
    os.makedirs(MODELS_OUTPUT, exist_ok=True)

    model_name = f"clonada_model_{int(time.time())}"
    experiment_dir = os.path.join(MODELS_OUTPUT, model_name)
    os.makedirs(experiment_dir, exist_ok=True)

    print(f"[TRAIN] Starting RVC training: {model_name}")
    print(f"[TRAIN] Epochs: {epochs}, Batch: {batch_size}, SR: {sample_rate}")

    # Step 1: Preprocess - extract features
    preprocess_cmd = [
        "python3", "trainset_preprocess_pipeline_print.py",
        dataset_dir,
        str(sample_rate),
        "2",  # number of CPU threads
        experiment_dir,
        "True"  # normalize
    ]

    # Step 2: Extract f0 features
    f0_cmd = [
        "python3", "extract_f0_print.py",
        experiment_dir,
        "rmvpe",  # f0 method
        "2"
    ]

    # Step 3: Extract feature embeddings
    feature_cmd = [
        "python3", "extract_feature_print.py",
        "cuda:0" if torch.cuda.is_available() else "cpu",
        "1",
        "0",
        "0",
        experiment_dir,
        "v2"
    ]

    # Step 4: Train
    train_cmd = [
        "python3", "train_nsf_sim_cache_sid_load_pretrain.py",
        "-e", model_name,
        "-sr", str(sample_rate),
        "-bs", str(batch_size),
        "-te", str(epochs),
        "-pg", "/clonada_core/weights/pretrained_v2/f0G40k.pth",
        "-pd", "/clonada_core/weights/pretrained_v2/f0D40k.pth",
        "-l", "1",
        "-c", "0",
        "-sw", "0",
        "-v", "v2"
    ]

    steps = [
        ("Preprocessing dataset", preprocess_cmd),
        ("Extracting pitch (F0)", f0_cmd),
        ("Extracting features", feature_cmd),
        ("Training model", train_cmd),
    ]

    for step_name, cmd in steps:
        print(f"[TRAIN] {step_name}...")
        try:
            result = subprocess.run(
                cmd,
                capture_output=True,
                text=True,
                timeout=7200,
                cwd="/clonada_core"
            )
            if result.returncode != 0:
                print(f"[WARN] {step_name} had issues: {result.stderr[:500]}")
        except subprocess.TimeoutExpired:
            print(f"[ERROR] {step_name} timed out")
            raise

    # Locate output files
    model_pth = None
    model_index = None

    for f in Path(experiment_dir).rglob("*.pth"):
        model_pth = str(f)
    for f in Path(experiment_dir).rglob("*.index"):
        model_index = str(f)

    return model_pth, model_index, model_name


def upload_model(model_pth, model_index, upload_url=None):
    """Upload trained model files to S3/R2 storage."""
    results = {}

    if upload_url:
        for label, path in [("pth", model_pth), ("index", model_index)]:
            if path and os.path.exists(path):
                with open(path, "rb") as f:
                    resp = requests.put(
                        f"{upload_url}/{os.path.basename(path)}",
                        data=f,
                        timeout=120
                    )
                    results[label] = resp.status_code == 200

    return results


def training_handler(job):
    """RunPod serverless handler entry point."""
    job_input = job["input"]

    dataset_url = job_input.get("dataset_url")
    epochs = job_input.get("epochs", 100)
    batch_size = job_input.get("batch_size", 8)
    sample_rate = job_input.get("sample_rate", 40000)
    cleanup = job_input.get("cleanup_after_training", True)
    upload_url = job_input.get("upload_url", None)

    try:
        # 1. Download dataset
        raw_dir = download_dataset(dataset_url, os.path.join(WORK_DIR, "raw"))

        # 2. Clean vocals with Demucs
        clean_dir = run_vocal_cleaning(raw_dir)

        # 3. Normalize
        normalize_audio(clean_dir)

        # 4. Train RVC model
        model_pth, model_index, model_name = train_rvc_model(
            clean_dir,
            epochs=epochs,
            batch_size=batch_size,
            sample_rate=sample_rate
        )

        # 5. Upload results
        model_url = ""
        if upload_url and model_pth:
            upload_model(model_pth, model_index, upload_url)
            model_url = f"{upload_url}/{os.path.basename(model_pth)}"

        # 6. Privacy cleanup
        if cleanup:
            for d in [WORK_DIR, CLEANED_DIR]:
                if os.path.exists(d):
                    shutil.rmtree(d)
            print("[CLEANUP] Raw audio and intermediate files purged")

        return {
            "status": "COMPLETED",
            "model_name": model_name,
            "model_url": model_url,
            "epochs_completed": epochs,
        }

    except Exception as err:
        # Always clean up on failure too
        for d in [WORK_DIR, CLEANED_DIR]:
            if os.path.exists(d):
                shutil.rmtree(d)

        return {
            "status": "FAILED",
            "error": str(err)
        }


if __name__ == "__main__":
    runpod.serverless.start({"handler": training_handler})
