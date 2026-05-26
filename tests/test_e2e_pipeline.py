#!/usr/bin/env python3
"""End-to-end test for Clonada AI Vocal Suite.

Validates the full pipeline:
1. License validation against the server
2. Python engine startup and ZMQ connectivity
3. Voice swap inference (RVC v2 pipeline)
4. Audio quality checks on output
"""

import sys
import os
import time
import json
import struct
import hashlib
import hmac
import subprocess
import signal
import numpy as np

sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', 'python'))

WEIGHTS_DIR = os.path.join(os.path.dirname(__file__), '..', 'weights')
MODELS_DIR = os.path.join(os.path.dirname(__file__), '..', 'models')
TEST_AUDIO = os.path.join(os.path.dirname(__file__), '..', 'test_vocal.wav')
LICENSE_KEY = "CLON-8YNK-R2EL-QH97-82KR"
LICENSE_SERVER = "http://155.133.27.205/api"
HMAC_SECRET = "clonada_hmac_s3cr3t_2026"
ZMQ_PORT = 5051  # Use different port to avoid conflicts


def test_license_server():
    """Test 1: Verify license server is reachable and key validates."""
    print("\n[TEST 1] License Server Validation")
    print("-" * 40)

    import urllib.request

    hardware_id = hashlib.sha256(b"test_e2e_hardware").hexdigest()
    payload = f"{LICENSE_KEY}|{hardware_id}"
    signature = hmac.HMAC(
        HMAC_SECRET.encode(), payload.encode(), hashlib.sha256
    ).hexdigest()

    data = json.dumps({
        "license_key": LICENSE_KEY,
        "hardware_fingerprint": hardware_id,
        "machine_name": "e2e-test",
        "os_info": "Linux Test",
        "hmac": signature
    }).encode()

    # Try activation first
    req = urllib.request.Request(
        f"{LICENSE_SERVER}/activate",
        data=data,
        headers={"Content-Type": "application/json"}
    )

    try:
        with urllib.request.urlopen(req, timeout=10) as resp:
            result = json.loads(resp.read())
            if result.get("status") in ("activated", "active", "already_active"):
                print(f"  [PASS] License activated, tier: {result.get('tier', 'unknown')}")
                print(f"         Features: {result.get('features', [])}")
                return True
            else:
                print(f"  [WARN] Unexpected: {result}")
                return False
    except urllib.error.HTTPError as e:
        body = e.read().decode()
        if "already" in body.lower() or "active" in body.lower():
            print(f"  [PASS] License already active on this device")
            return True
        print(f"  [WARN] License server error: HTTP {e.code}: {body[:100]}")
        return False
    except Exception as e:
        print(f"  [WARN] License server unreachable: {e}")
        print("  (This is OK for offline testing)")
        return False


def test_engine_startup():
    """Test 2: Start the Python engine and verify ZMQ connectivity."""
    print("\n[TEST 2] Engine Startup & ZMQ Connectivity")
    print("-" * 40)

    try:
        import zmq
    except ImportError:
        print("  [SKIP] pyzmq not installed")
        return None, None

    engine_script = os.path.join(os.path.dirname(__file__), '..', 'python', 'clonada_server.py')
    if not os.path.exists(engine_script):
        print(f"  [SKIP] Engine script not found: {engine_script}")
        return None, None

    # Start engine process
    env = os.environ.copy()
    env['CLONADA_PORT'] = str(ZMQ_PORT)

    proc = subprocess.Popen(
        [sys.executable, engine_script, '--port', str(ZMQ_PORT),
         '--weights-dir', WEIGHTS_DIR, '--models-dir', MODELS_DIR],
        stdout=subprocess.PIPE, stderr=subprocess.PIPE, env=env
    )

    # Wait for engine to start
    time.sleep(3)

    if proc.poll() is not None:
        stderr = proc.stderr.read().decode()
        print(f"  [FAIL] Engine exited immediately: {stderr[:200]}")
        return None, None

    # Try ZMQ connection
    ctx = zmq.Context()
    sock = ctx.socket(zmq.REQ)
    sock.setsockopt(zmq.RCVTIMEO, 5000)
    sock.setsockopt(zmq.SNDTIMEO, 5000)
    sock.connect(f"tcp://127.0.0.1:{ZMQ_PORT}")

    # Send health check (engine uses "command" field, uppercase)
    sock.send_json({"command": "HEALTH"})
    try:
        resp = sock.recv_json()
        if resp.get("status") in ("OK", "ok", "HEALTHY"):
            print(f"  [PASS] Engine running, ZMQ connected on port {ZMQ_PORT}")
            return proc, (ctx, sock)
        else:
            print(f"  [WARN] Unexpected health response: {resp}")
            return proc, (ctx, sock)
    except zmq.error.Again:
        print("  [FAIL] ZMQ timeout - engine not responding")
        proc.kill()
        return None, None


def test_voice_swap(zmq_conn):
    """Test 3: Perform a voice swap through the engine."""
    print("\n[TEST 3] Voice Swap Inference")
    print("-" * 40)

    if zmq_conn is None:
        print("  [SKIP] No ZMQ connection")
        return False

    ctx, sock = zmq_conn

    # Generate a simple test signal (440Hz sine wave, 1 second)
    sr = 44100
    duration = 1.0
    t = np.linspace(0, duration, int(sr * duration), dtype=np.float32)
    test_audio = (np.sin(2 * np.pi * 440 * t) * 0.5).tolist()

    # Check if we have a test model
    model_files = []
    if os.path.isdir(MODELS_DIR):
        model_files = [f for f in os.listdir(MODELS_DIR) if f.endswith('.pth')]

    if not model_files:
        print("  [WARN] No voice models found in models/ - testing with no model")
        model_path = ""
    else:
        model_path = os.path.join(MODELS_DIR, model_files[0])
        print(f"  Using model: {model_files[0]}")

    # Send swap request (engine uses "command" field)
    request = {
        "command": "SWAP",
        "audio": test_audio,
        "sample_rate": sr,
        "pitch": 0,
        "formant": 0,
        "model": model_path,
        "mix": 1.0
    }

    start_time = time.time()
    sock.send_json(request)

    try:
        resp = sock.recv_json()
        elapsed = time.time() - start_time

        if resp.get("status") == "OK" and "audio" in resp:
            output_len = len(resp["audio"])
            print(f"  [PASS] Swap completed in {elapsed:.2f}s")
            print(f"         Input: {len(test_audio)} samples")
            print(f"         Output: {output_len} samples")

            # Basic quality check
            output = np.array(resp["audio"], dtype=np.float32)
            rms = np.sqrt(np.mean(output ** 2))
            peak = np.max(np.abs(output))
            print(f"         Output RMS: {rms:.4f}, Peak: {peak:.4f}")

            if rms > 0.001:
                print("  [PASS] Output contains audio (non-silent)")
                return True
            else:
                print("  [WARN] Output is nearly silent")
                return False
        elif resp.get("status") == "ERROR":
            msg = resp.get('message', 'unknown')
            print(f"  [WARN] Engine returned error: {msg}")
            if "model" in msg.lower() or "license" in msg.lower():
                print("  (Expected if no model/license configured in engine)")
            return False
        else:
            print(f"  [INFO] Response: {str(resp)[:200]}")
            return False
    except Exception as e:
        print(f"  [FAIL] {e}")
        return False


def test_audio_file():
    """Test 4: Verify test audio file exists and is valid WAV."""
    print("\n[TEST 4] Audio File Validation")
    print("-" * 40)

    if not os.path.exists(TEST_AUDIO):
        print(f"  [SKIP] Test audio not found: {TEST_AUDIO}")
        return False

    size = os.path.getsize(TEST_AUDIO)
    print(f"  File: {TEST_AUDIO}")
    print(f"  Size: {size:,} bytes")

    with open(TEST_AUDIO, 'rb') as f:
        header = f.read(44)
        if header[:4] == b'RIFF' and header[8:12] == b'WAVE':
            channels = struct.unpack('<H', header[22:24])[0]
            sample_rate = struct.unpack('<I', header[24:28])[0]
            bits = struct.unpack('<H', header[34:36])[0]
            print(f"  Format: {channels}ch, {sample_rate}Hz, {bits}-bit")
            print("  [PASS] Valid WAV file")
            return True
        else:
            print("  [FAIL] Not a valid WAV file")
            return False


def test_weights_present():
    """Test 5: Verify pretrained weights are available."""
    print("\n[TEST 5] Pretrained Weights Check")
    print("-" * 40)

    required_weights = {
        'hubert_base.pt': 180_000_000,
        'rmvpe.pt': 170_000_000,
    }

    all_present = True
    for name, min_size in required_weights.items():
        path = os.path.join(WEIGHTS_DIR, name)
        if os.path.exists(path):
            size = os.path.getsize(path)
            if size >= min_size:
                print(f"  [PASS] {name} ({size / 1e6:.0f} MB)")
            else:
                print(f"  [WARN] {name} too small ({size} bytes)")
                all_present = False
        else:
            print(f"  [MISS] {name} not found")
            all_present = False

    return all_present


def main():
    print("=" * 60)
    print("  CLONADA END-TO-END TEST SUITE")
    print("  Version 1.0.0")
    print("=" * 60)

    results = {}
    engine_proc = None
    zmq_conn = None

    try:
        # Test 1: License
        results['license'] = test_license_server()

        # Test 4: Audio file
        results['audio_file'] = test_audio_file()

        # Test 5: Weights
        results['weights'] = test_weights_present()

        # Test 2: Engine startup
        engine_proc, zmq_conn = test_engine_startup()
        results['engine'] = engine_proc is not None

        # Test 3: Voice swap
        results['swap'] = test_voice_swap(zmq_conn)

    finally:
        # Cleanup
        if zmq_conn:
            ctx, sock = zmq_conn
            sock.close()
            ctx.term()
        if engine_proc:
            engine_proc.terminate()
            engine_proc.wait(timeout=5)

    # Summary
    print("\n" + "=" * 60)
    print("  TEST RESULTS SUMMARY")
    print("=" * 60)

    passed = 0
    total = len(results)
    for name, result in results.items():
        status = "PASS" if result else ("SKIP" if result is None else "FAIL")
        icon = "+" if result else ("-" if result is None else "X")
        print(f"  [{icon}] {name}: {status}")
        if result:
            passed += 1

    print(f"\n  {passed}/{total} tests passed")
    print("=" * 60)

    return 0 if passed >= 3 else 1


if __name__ == "__main__":
    sys.exit(main())
