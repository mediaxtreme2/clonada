#!/usr/bin/env python3
"""
Clonada Cloud Bridge Engine
Lightweight ZeroMQ server that proxies all AI operations to RunPod cloud.
No PyTorch, no Miniconda, no heavy dependencies -- just requests + zmq.
Speaks the exact same protocol as the full local engine so the plugin
sees "Engine Online" immediately.
"""

import sys
import os
import argparse
import json
import signal
import time
import tempfile
import threading
import hashlib
import hmac
import numpy as np
import zmq
import soundfile as sf
import requests
from pathlib import Path

sys.path.insert(0, os.path.dirname(__file__))
from lib.license_client import validate, activate, deactivate, has_feature

RUNPOD_ENDPOINT_ID = "lmxzg81itmh3on"
RUNPOD_BASE = f"https://api.runpod.ai/v2/{RUNPOD_ENDPOINT_ID}"
LICENSE_SERVER = "http://155.133.27.205/api"


def _load_runpod_key():
    """Load RunPod API key from config file, environment, or license server."""
    env_key = os.environ.get("RUNPOD_API_KEY", "")
    if env_key:
        return env_key
    config_path = os.path.join(os.path.expanduser("~"), "Clonada", "config.json")
    if os.path.exists(config_path):
        try:
            with open(config_path) as f:
                cfg = json.load(f)
                if cfg.get("runpod_api_key"):
                    return cfg["runpod_api_key"]
        except:
            pass
    # Fetch from license server
    try:
        resp = requests.get(f"{LICENSE_SERVER}/cloud-config", timeout=10)
        if resp.status_code == 200:
            data = resp.json()
            key = data.get("runpod_api_key", "")
            if key:
                os.makedirs(os.path.dirname(config_path), exist_ok=True)
                with open(config_path, "w") as f:
                    json.dump({"runpod_api_key": key}, f)
                return key
    except:
        pass
    return ""


RUNPOD_API_KEY = _load_runpod_key()


class ClonadaCloudBridge:
    """ZeroMQ server that proxies AI requests to RunPod serverless."""

    VERSION = "1.0.0"

    def __init__(self, port=5050, models_dir=None, license_key=None):
        self.port = port
        self.context = zmq.Context()
        self.socket = self.context.socket(zmq.REP)
        self.socket.bind(f"tcp://127.0.0.1:{port}")
        self.is_running = True

        self.models_dir = models_dir or os.path.join(
            os.path.expanduser("~"), "Clonada", "models"
        )
        os.makedirs(self.models_dir, exist_ok=True)

        self.license_tier = None
        self.license_features = []
        self.license_key_stored = None
        self._validate_license(license_key)

    def _validate_license(self, license_key=None):
        if license_key:
            ok, tier, features, err = activate(license_key)
            if not ok:
                ok, tier, features, err = validate(license_key)
            self.license_key_stored = license_key
        else:
            ok, tier, features, err = validate()

        if ok:
            self.license_tier = tier
            self.license_features = features
            print(f"[LICENSE] Valid - Tier: {tier}, Features: {', '.join(features)}")
        else:
            print(f"[LICENSE] WARNING: {err or 'No valid license'}")
            print("[LICENSE] Running in demo mode")
            self.license_tier = "demo"
            self.license_features = []

    def _check_feature(self, feature):
        if self.license_tier == "demo":
            return False
        return feature in self.license_features

    def _get_license_key(self):
        if self.license_key_stored:
            return self.license_key_stored
        key_file = os.path.join(
            os.path.expanduser("~"),
            "Library", "Application Support", "Clonada", "license.key"
        )
        if os.path.exists(key_file):
            with open(key_file) as f:
                return f.read().strip()
        return ""

    def _runpod_submit(self, payload, timeout=300):
        """Submit job to RunPod and wait for result."""
        headers = {"Authorization": f"Bearer {RUNPOD_API_KEY}"}

        resp = requests.post(
            f"{RUNPOD_BASE}/runsync",
            json={"input": payload},
            headers=headers,
            timeout=timeout
        )

        if resp.status_code != 200:
            return None, f"RunPod request failed: {resp.status_code}"

        data = resp.json()
        if data.get("status") == "COMPLETED":
            return data.get("output"), None
        elif data.get("status") == "FAILED":
            return None, data.get("output", {}).get("error", "Unknown RunPod error")
        else:
            job_id = data.get("id")
            return self._poll_job(job_id, headers, timeout)

    def _poll_job(self, job_id, headers, timeout=300):
        """Poll RunPod job until completion."""
        start = time.time()
        while time.time() - start < timeout:
            resp = requests.get(
                f"{RUNPOD_BASE}/status/{job_id}",
                headers=headers,
                timeout=30
            )
            if resp.status_code != 200:
                time.sleep(2)
                continue

            data = resp.json()
            status = data.get("status")
            if status == "COMPLETED":
                return data.get("output"), None
            elif status in ("FAILED", "CANCELLED"):
                return None, data.get("output", {}).get("error", f"Job {status}")
            time.sleep(3)

        return None, "RunPod job timed out"

    def handle_health(self, message):
        return {
            "status": "SUCCESS",
            "version": self.VERSION,
            "device": "cloud-gpu",
            "gpu_available": True,
            "models_dir": self.models_dir,
            "port": self.port,
            "license_tier": self.license_tier,
            "license_features": self.license_features,
            "mode": "cloud",
        }

    def handle_activate(self, message):
        key = message.get("license_key", "")
        if not key:
            return {"status": "ERROR", "message": "Missing license_key"}
        ok, tier, features, err = activate(key)
        if ok:
            self.license_tier = tier
            self.license_features = features
            self.license_key_stored = key
            return {"status": "SUCCESS", "tier": tier, "features": features}
        return {"status": "ERROR", "message": err}

    def handle_deactivate(self, message):
        ok, err = deactivate()
        if ok:
            self.license_tier = "demo"
            self.license_features = []
            self.license_key_stored = None
            return {"status": "SUCCESS"}
        return {"status": "ERROR", "message": err}

    def handle_list_models(self, message):
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

    def handle_swap_file(self, message):
        """Voice conversion via RunPod cloud."""
        if not self._check_feature("swap"):
            return {"status": "ERROR", "message": "License required for voice swap."}

        input_path = message.get("input", "")
        output_path = message.get("output", "")
        model_path = message.get("model", "")
        index_path = message.get("index", "")
        progress_file = message.get("progress", "")

        if not input_path or not os.path.exists(input_path):
            return {"status": "ERROR", "message": f"Input file not found: {input_path}"}
        if not model_path or not os.path.exists(model_path):
            return {"status": "ERROR", "message": f"Model file not found: {model_path}"}

        if not output_path:
            output_path = input_path.replace(".wav", "_clonada.wav")

        def write_progress(val, text):
            if progress_file:
                try:
                    with open(progress_file, "w") as f:
                        f.write(f"{val}|{text}")
                except:
                    pass

        try:
            write_progress(0.1, "Uploading to cloud...")

            # Upload audio and model to RunPod via presigned URLs
            # For now, we send raw audio data in the job payload
            audio_data, sr = sf.read(input_path, dtype="float32")
            if len(audio_data.shape) > 1:
                audio_data = audio_data.mean(axis=1)

            write_progress(0.3, "Processing on cloud GPU...")

            payload = {
                "mode": "infer",
                "license_key": self._get_license_key(),
                "audio_data": audio_data.tolist(),
                "sample_rate": sr,
                "model_name": os.path.basename(model_path),
                "pitch_shift": message.get("pitch", 0),
                "formant_shift": message.get("formant", 0),
                "method": message.get("method", "rmvpe"),
                "mix": message.get("mix", 1.0),
                "index_rate": message.get("index_rate", 0.75),
            }

            result, err = self._runpod_submit(payload, timeout=300)
            if err:
                return {"status": "ERROR", "message": f"Cloud processing failed: {err}"}

            write_progress(0.8, "Downloading result...")

            if result and result.get("status") == "COMPLETED":
                output_audio = np.array(result.get("audio", []), dtype=np.float32)
                out_sr = result.get("sample_rate", sr)
                sf.write(output_path, output_audio, out_sr)
                write_progress(1.0, "Done")
                return {
                    "status": "SUCCESS",
                    "output": output_path,
                    "duration": len(output_audio) / out_sr
                }
            else:
                return {"status": "ERROR", "message": "Cloud returned no output"}

        except Exception as e:
            return {"status": "ERROR", "message": str(e)}

    def handle_swap(self, message):
        """Process voice swap from raw audio data via cloud."""
        if not self._check_feature("swap"):
            return {"status": "ERROR", "message": "License required for voice swap."}
        try:
            audio_data = np.array(message["audio"], dtype=np.float32)
            sr = message.get("sample_rate", 44100)

            # Write temp file and use file-based handler
            tmp_in = tempfile.mktemp(suffix=".wav")
            tmp_out = tempfile.mktemp(suffix="_out.wav")
            sf.write(tmp_in, audio_data, sr)

            result = self.handle_swap_file({
                "input": tmp_in,
                "output": tmp_out,
                "model": message.get("model", ""),
                "index": message.get("index", ""),
                "pitch": message.get("pitch", 0),
                "formant": message.get("formant", 0),
                "method": message.get("method", "rmvpe"),
                "mix": message.get("mix", 1.0),
                "index_rate": message.get("index_rate", 0.75),
            })

            if result.get("status") == "SUCCESS" and os.path.exists(tmp_out):
                out_audio, out_sr = sf.read(tmp_out, dtype="float32")
                os.remove(tmp_in)
                os.remove(tmp_out)
                return {"status": "SUCCESS", "audio": out_audio.tolist(), "sample_rate": out_sr}

            if os.path.exists(tmp_in):
                os.remove(tmp_in)
            return result

        except Exception as e:
            return {"status": "ERROR", "message": str(e)}

    def handle_separate(self, message):
        """Stem separation via RunPod cloud."""
        if not self._check_feature("separate"):
            return {"status": "ERROR", "message": "License required for stem separation."}

        input_path = message.get("input", "")
        output_dir = message.get("output_dir", os.path.dirname(input_path))
        progress_file = message.get("progress", "")

        if not input_path or not os.path.exists(input_path):
            return {"status": "ERROR", "message": f"Input file not found: {input_path}"}

        def write_progress(val, text):
            if progress_file:
                try:
                    with open(progress_file, "w") as f:
                        f.write(f"{val}|{text}")
                except:
                    pass

        try:
            write_progress(0.1, "Uploading for stem separation...")

            audio_data, sr = sf.read(input_path, dtype="float32")
            if len(audio_data.shape) > 1:
                audio_data = audio_data.mean(axis=1)

            payload = {
                "mode": "separate",
                "license_key": self._get_license_key(),
                "audio_data": audio_data.tolist(),
                "sample_rate": sr,
            }

            write_progress(0.3, "Separating stems on cloud GPU...")
            result, err = self._runpod_submit(payload, timeout=600)

            if err:
                return {"status": "ERROR", "message": f"Cloud separation failed: {err}"}

            write_progress(0.8, "Downloading stems...")

            os.makedirs(output_dir, exist_ok=True)
            stems = {}
            if result and result.get("status") == "COMPLETED":
                for stem_name in ["vocals", "drums", "bass", "other"]:
                    stem_data = result.get(f"{stem_name}_audio")
                    if stem_data:
                        stem_path = os.path.join(output_dir, f"{stem_name}.wav")
                        sf.write(stem_path, np.array(stem_data, dtype=np.float32), sr)
                        stems[stem_name] = stem_path

            write_progress(1.0, "Separation complete")
            return {"status": "SUCCESS", "stems": stems}

        except Exception as e:
            return {"status": "ERROR", "message": str(e)}

    def process_stream(self):
        """Main event loop."""
        print(f"\n{'='*60}")
        print(f"  CLONADA CLOUD ENGINE v{self.VERSION}")
        print(f"  Mode: Cloud GPU (RunPod)")
        print(f"  Listening on tcp://127.0.0.1:{self.port}")
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
        print("[SHUTDOWN] Cleaning up...")
        self.socket.close()
        self.context.term()
        print("[SHUTDOWN] Engine terminated.")


def main():
    parser = argparse.ArgumentParser(description="Clonada Cloud Bridge Engine")
    parser.add_argument("--port", type=int, default=5050)
    parser.add_argument("--models-dir", type=str, default=None)
    parser.add_argument("--license-key", type=str, default=None)
    args = parser.parse_args()

    engine = ClonadaCloudBridge(
        port=args.port,
        models_dir=args.models_dir,
        license_key=args.license_key
    )

    signal.signal(signal.SIGINT, lambda *_: setattr(engine, 'is_running', False))
    signal.signal(signal.SIGTERM, lambda *_: setattr(engine, 'is_running', False))

    engine.process_stream()


if __name__ == "__main__":
    main()
