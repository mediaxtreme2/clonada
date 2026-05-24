#!/usr/bin/env python3
"""
Clonada CLI Bridge - Connects REAPER Lua scripts to the Python AI Engine.
Handles command serialization, file I/O, and progress reporting.
Designed to be compiled with PyInstaller for distribution.
"""

import sys
import os
import argparse
import json
import hashlib
import time
import zmq


def send_command(port, command_data, timeout=300000):
    """Send a command to the Clonada engine and return response."""
    context = zmq.Context()
    socket = context.socket(zmq.REQ)
    socket.setsockopt(zmq.RCVTIMEO, timeout)
    socket.setsockopt(zmq.SNDTIMEO, 10000)
    socket.connect(f"tcp://127.0.0.1:{port}")

    try:
        socket.send_json(command_data)
        response = socket.recv_json()
        return response
    except zmq.Again:
        return {"status": "ERROR", "message": "Engine timeout - is clonada_server.py running?"}
    except zmq.ZMQError as e:
        return {"status": "ERROR", "message": f"Connection error: {e}"}
    finally:
        socket.close()
        context.term()


def write_progress(progress_file, value, text):
    """Write progress update for Lua script to read."""
    if progress_file:
        try:
            with open(progress_file, "w") as f:
                f.write(f"{value}|{text}")
        except:
            pass


def cmd_swap(args):
    """Execute voice swap command."""
    write_progress(args.progress, 0.1, "Connecting to engine...")

    command = {
        "version": "1.0.0",
        "command": "SWAP_FILE",
        "input": args.input,
        "model": args.model,
        "pitch": args.pitch,
        "formant": args.formant,
        "method": args.method,
        "mix": args.mix,
        "mode": args.mode,
        "progress": args.progress or "",
    }

    response = send_command(args.port, command, timeout=600000)

    if response.get("status") == "SUCCESS":
        write_progress(args.progress, 1.0, "Voice swap complete")
        print(f"SUCCESS|{response.get('output', '')}")
    else:
        write_progress(args.progress, 0.0, f"Error: {response.get('message', 'Unknown')}")
        print(f"ERROR|{response.get('message', 'Unknown error')}", file=sys.stderr)
        sys.exit(1)


def cmd_separate(args):
    """Execute stem separation command."""
    write_progress(args.progress, 0.1, "Connecting to engine...")

    command = {
        "version": "1.0.0",
        "command": "SEPARATE",
        "input": args.input,
        "output_dir": args.output_dir or os.path.dirname(args.input),
        "progress": args.progress or "",
    }

    response = send_command(args.port, command, timeout=600000)

    if response.get("status") == "SUCCESS":
        write_progress(args.progress, 1.0, "Separation complete")
        stems = response.get("stems", {})
        print(f"SUCCESS|vocals={stems.get('vocals', '')}|drums={stems.get('drums', '')}|bass={stems.get('bass', '')}|other={stems.get('other', '')}")
    else:
        write_progress(args.progress, 0.0, f"Error: {response.get('message', 'Unknown')}")
        print(f"ERROR|{response.get('message', '')}", file=sys.stderr)
        sys.exit(1)


def cmd_train(args):
    """Execute cloud training command (RunPod)."""
    write_progress(args.progress, 0.05, "Preparing dataset...")

    # TODO: Implement RunPod training submission
    # 1. Export track audio from REAPER project
    # 2. Upload to RunPod via API
    # 3. Monitor training progress
    # 4. Download trained model

    import requests

    headers = {
        "Authorization": f"Bearer {args.runpod_key}",
        "Content-Type": "application/json"
    }

    write_progress(args.progress, 0.1, "Submitting training job to RunPod...")

    # RunPod serverless endpoint
    endpoint_url = f"https://api.runpod.ai/v2/{args.runpod_endpoint or 'lmxzg81itmh3on'}/run"

    payload = {
        "input": {
            "epochs": args.epochs,
            "batch_size": args.batch_size,
            "sample_rate": args.sample_rate,
            "cleanup_after_training": True,
        }
    }

    try:
        resp = requests.post(endpoint_url, json=payload, headers=headers, timeout=30)
        resp.raise_for_status()
        job = resp.json()
        job_id = job.get("id", "")

        write_progress(args.progress, 0.2, f"Job submitted: {job_id}")

        # Poll for completion
        endpoint_id = args.runpod_endpoint or 'lmxzg81itmh3on'
        status_url = f"https://api.runpod.ai/v2/{endpoint_id}/status/{job_id}"
        while True:
            time.sleep(10)
            status_resp = requests.get(status_url, headers=headers, timeout=30)
            status_data = status_resp.json()
            job_status = status_data.get("status", "")

            if job_status == "COMPLETED":
                output = status_data.get("output", {})
                model_url = output.get("model_url", "")
                write_progress(args.progress, 0.9, "Downloading trained model...")

                # Download model
                if model_url and args.models_dir:
                    model_name = f"clonada_model_{int(time.time())}.pth"
                    model_path = os.path.join(args.models_dir, model_name)
                    model_resp = requests.get(model_url, timeout=120)
                    with open(model_path, "wb") as f:
                        f.write(model_resp.content)
                    write_progress(args.progress, 1.0, f"Training complete: {model_name}")
                    print(f"SUCCESS|{model_path}")
                else:
                    write_progress(args.progress, 1.0, "Training complete")
                    print("SUCCESS|")
                break

            elif job_status == "FAILED":
                error = status_data.get("error", "Unknown error")
                write_progress(args.progress, 0.0, f"Training failed: {error}")
                print(f"ERROR|{error}", file=sys.stderr)
                sys.exit(1)

            elif job_status == "IN_PROGRESS":
                # Estimate progress
                elapsed = status_data.get("executionTime", 0)
                est_progress = min(0.2 + (elapsed / 3600) * 0.6, 0.85)
                write_progress(args.progress, est_progress, f"Training in progress ({elapsed}s elapsed)...")

    except requests.RequestException as e:
        write_progress(args.progress, 0.0, f"RunPod error: {e}")
        print(f"ERROR|{e}", file=sys.stderr)
        sys.exit(1)


def cmd_activate(args):
    """Validate license key against license server."""
    sys.path.insert(0, os.path.join(os.path.dirname(__file__), "lib"))
    from license_client import activate

    ok, tier, features, err = activate(args.key)
    if ok:
        print(f"SUCCESS|TIER:{tier}|FEATURES:{','.join(features)}")
    else:
        print(f"ERROR|{err or 'Activation failed'}", file=sys.stderr)
        sys.exit(1)


def cmd_shutdown(args):
    """Send shutdown command to engine."""
    response = send_command(args.port, {"version": "1.0.0", "command": "SHUTDOWN"})
    print(f"{response.get('status', 'ERROR')}")


def cmd_health(args):
    """Check engine health."""
    response = send_command(args.port, {"version": "1.0.0", "command": "HEALTH"}, timeout=5000)
    if response.get("status") == "SUCCESS":
        tier = response.get("license_tier", "demo")
        features = ",".join(response.get("license_features", []))
        print(f"SUCCESS|device={response.get('device')}|gpu={response.get('gpu_available')}|tier={tier}|features={features}")
    else:
        print(f"ERROR|{response.get('message', 'Engine not responding')}")
        sys.exit(1)


def main():
    parser = argparse.ArgumentParser(description="Clonada CLI Bridge")
    parser.add_argument("--command", required=True,
                       choices=["SWAP", "SEPARATE", "TRAIN", "ACTIVATE", "SHUTDOWN", "HEALTH"],
                       help="Command to execute")
    parser.add_argument("--port", type=int, default=5050, help="Engine ZeroMQ port")
    parser.add_argument("--progress", type=str, default="", help="Progress file path")

    # Swap args
    parser.add_argument("--input", type=str, help="Input audio file path")
    parser.add_argument("--model", type=str, help="Voice model file path")
    parser.add_argument("--pitch", type=int, default=0, help="Pitch shift in semitones")
    parser.add_argument("--formant", type=int, default=0, help="Formant shift")
    parser.add_argument("--method", type=str, default="rmvpe", help="Pitch extraction method")
    parser.add_argument("--mix", type=float, default=1.0, help="Dry/wet mix")
    parser.add_argument("--mode", type=str, default="low_latency", choices=["low_latency", "high_quality"])
    parser.add_argument("--output_dir", type=str, help="Output directory for stems")

    # Train args
    parser.add_argument("--track", type=int, default=0, help="REAPER track index")
    parser.add_argument("--epochs", type=int, default=100, help="Training epochs")
    parser.add_argument("--batch_size", type=int, default=8)
    parser.add_argument("--sample_rate", type=int, default=40000)
    parser.add_argument("--runpod_key", type=str, default="", help="RunPod API key")
    parser.add_argument("--runpod_endpoint", type=str, default="lmxzg81itmh3on", help="RunPod endpoint ID")
    parser.add_argument("--models_dir", type=str, default="", help="Models directory")

    # Activation args
    parser.add_argument("--key", type=str, help="License key")
    parser.add_argument("--machine", type=str, help="Machine ID")

    args = parser.parse_args()

    commands = {
        "SWAP": cmd_swap,
        "SEPARATE": cmd_separate,
        "TRAIN": cmd_train,
        "ACTIVATE": cmd_activate,
        "SHUTDOWN": cmd_shutdown,
        "HEALTH": cmd_health,
    }

    handler = commands.get(args.command)
    if handler:
        handler(args)
    else:
        print(f"ERROR|Unknown command: {args.command}", file=sys.stderr)
        sys.exit(1)


if __name__ == "__main__":
    main()
