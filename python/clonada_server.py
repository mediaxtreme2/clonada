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

sys.path.insert(0, os.path.dirname(__file__))
from lib.pipeline import VoiceConversionPipeline
from lib.license_client import validate, activate, deactivate, has_feature

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

    def __init__(self, port=5050, models_dir=None, weights_dir=None, license_key=None):
        self.port = port
        self.context = zmq.Context()
        self.socket = self.context.socket(zmq.REP)
        self.socket.bind(f"tcp://127.0.0.1:{port}")
        self.is_running = True

        self.license_tier = None
        self.license_features = []
        self._validate_license(license_key)

        self.device = detect_device()
        self.pipeline = VoiceConversionPipeline(
            weights_dir=weights_dir or os.path.join(os.path.dirname(__file__), "..", "weights"),
            device=self.device
        )
        self.stem_separator = StemSeparator(self.device)

        self.models_dir = models_dir or os.path.join(os.path.dirname(__file__), "..", "models")
        self.weights_dir = weights_dir or os.path.join(os.path.dirname(__file__), "..", "weights")

        os.makedirs(self.models_dir, exist_ok=True)
        os.makedirs(self.weights_dir, exist_ok=True)

        # Pre-load HuBERT if available
        self.pipeline.hubert.load()

    def _validate_license(self, license_key=None):
        """Check license on startup."""
        if license_key:
            ok, tier, features, err = activate(license_key)
            if not ok:
                ok, tier, features, err = validate(license_key)
        else:
            ok, tier, features, err = validate()

        if ok:
            self.license_tier = tier
            self.license_features = features
            print(f"[LICENSE] Valid - Tier: {tier}, Features: {', '.join(features)}")
        else:
            print(f"[LICENSE] WARNING: {err or 'No valid license'}")
            print("[LICENSE] Running in demo mode (limited functionality)")
            self.license_tier = "demo"
            self.license_features = []

    def _check_feature(self, feature):
        """Check if current license allows a feature."""
        if self.license_tier == "demo":
            return False
        return feature in self.license_features

    def handle_activate(self, message):
        """Activate license."""
        key = message.get("license_key", "")
        if not key:
            return {"status": "ERROR", "message": "Missing license_key"}
        ok, tier, features, err = activate(key)
        if ok:
            self.license_tier = tier
            self.license_features = features
            return {"status": "SUCCESS", "tier": tier, "features": features}
        return {"status": "ERROR", "message": err}

    def handle_deactivate(self, message):
        """Deactivate license."""
        ok, err = deactivate()
        if ok:
            self.license_tier = "demo"
            self.license_features = []
            return {"status": "SUCCESS"}
        return {"status": "ERROR", "message": err}

    def handle_swap(self, message):
        """Process voice swap from raw audio data."""
        if not self._check_feature("swap"):
            return {"status": "ERROR", "message": "License required for voice swap. Activate with a valid license key."}
        try:
            audio_data = np.array(message["audio"], dtype=np.float32)
            model_path = message.get("model", "")
            index_path = message.get("index", "")

            # Write temp input file
            tmp_in = "/tmp/clonada_swap_in.wav"
            tmp_out = "/tmp/clonada_swap_out.wav"
            sf.write(tmp_in, audio_data, message.get("sample_rate", 44100))

            self.pipeline.load_model(model_path, index_path or None)
            self.pipeline.convert(
                tmp_in, tmp_out,
                pitch_shift=message.get("pitch", 0),
                formant_shift=message.get("formant", 0),
                method=message.get("method", "rmvpe"),
                mix=message.get("mix", 1.0),
                mode=message.get("mode", "low_latency"),
                index_rate=message.get("index_rate", 0.75)
            )

            result, sr = sf.read(tmp_out, dtype="float32")
            os.remove(tmp_in)
            os.remove(tmp_out)

            return {"status": "SUCCESS", "audio": result.tolist(), "sample_rate": sr}
        except Exception as e:
            return {"status": "ERROR", "message": str(e)}

    def handle_swap_file(self, message):
        """Process voice swap from file path (efficient for large audio)."""
        if not self._check_feature("swap"):
            return {"status": "ERROR", "message": "License required for voice swap."}
        try:
            input_path = message["input"]
            output_path = message.get("output", input_path.replace(".wav", "_clonada.wav"))
            model_path = message["model"]
            index_path = message.get("index", "")
            progress_file = message.get("progress", "")

            def write_progress(val, text):
                if progress_file:
                    try:
                        with open(progress_file, "w") as f:
                            f.write(f"{val}|{text}")
                    except:
                        pass

            self.pipeline.load_model(model_path, index_path or None)
            self.pipeline.convert(
                input_path, output_path,
                pitch_shift=message.get("pitch", 0),
                formant_shift=message.get("formant", 0),
                method=message.get("method", "rmvpe"),
                mix=message.get("mix", 1.0),
                mode=message.get("mode", "low_latency"),
                index_rate=message.get("index_rate", 0.75),
                progress_callback=write_progress
            )

            audio_info = sf.info(output_path)
            return {
                "status": "SUCCESS",
                "output": output_path,
                "duration": audio_info.duration
            }
        except Exception as e:
            return {"status": "ERROR", "message": str(e)}

    def handle_separate(self, message):
        """Process stem separation request."""
        if not self._check_feature("separate"):
            return {"status": "ERROR", "message": "License required for stem separation."}
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
            "port": self.port,
            "license_tier": self.license_tier,
            "license_features": self.license_features,
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
            "ACTIVATE": self.handle_activate,
            "DEACTIVATE": self.handle_deactivate,
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
    parser.add_argument("--license-key", type=str, default=None, help="License key to activate")
    args = parser.parse_args()

    engine = ClonadaLocalEngine(
        port=args.port,
        models_dir=args.models_dir,
        weights_dir=args.weights_dir,
        license_key=args.license_key
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
