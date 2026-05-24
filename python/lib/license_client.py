"""
Clonada License Client
Validates license with the Clonada License Server.
Hardware fingerprint generation + HMAC response verification.
"""

import os
import json
import hashlib
import hmac
import platform
import subprocess
import time
import urllib.request
import urllib.error
from pathlib import Path

LICENSE_SERVER = "http://155.133.27.205/api"
HMAC_SECRET = "clonada_hmac_s3cr3t_2026"
CACHE_DIR = os.path.join(str(Path.home()), ".clonada")
CACHE_FILE = os.path.join(CACHE_DIR, "license.cache")
CACHE_TTL = 30 * 24 * 3600  # 30 days


def get_hardware_fingerprint():
    """Generate a stable hardware fingerprint from CPU + motherboard + disk identifiers."""
    parts = []

    system = platform.system()
    parts.append(platform.node())

    if system == "Windows":
        try:
            r = subprocess.run(
                ["wmic", "cpu", "get", "ProcessorId"],
                capture_output=True, text=True, timeout=5
            )
            for line in r.stdout.strip().split("\n"):
                line = line.strip()
                if line and line != "ProcessorId":
                    parts.append(line)
                    break
        except Exception:
            pass
        try:
            r = subprocess.run(
                ["wmic", "baseboard", "get", "SerialNumber"],
                capture_output=True, text=True, timeout=5
            )
            for line in r.stdout.strip().split("\n"):
                line = line.strip()
                if line and line != "SerialNumber":
                    parts.append(line)
                    break
        except Exception:
            pass
    elif system == "Darwin":
        try:
            r = subprocess.run(
                ["ioreg", "-rd1", "-c", "IOPlatformExpertDevice"],
                capture_output=True, text=True, timeout=5
            )
            for line in r.stdout.split("\n"):
                if "IOPlatformSerialNumber" in line:
                    parts.append(line.split('"')[-2])
                    break
        except Exception:
            pass
    else:
        try:
            with open("/etc/machine-id", "r") as f:
                parts.append(f.read().strip())
        except Exception:
            pass
        try:
            r = subprocess.run(
                ["cat", "/sys/class/dmi/id/board_serial"],
                capture_output=True, text=True, timeout=5
            )
            if r.returncode == 0 and r.stdout.strip():
                parts.append(r.stdout.strip())
        except Exception:
            pass

    raw = "|".join(parts) if parts else platform.node() + platform.machine()
    return hashlib.sha256(raw.encode()).hexdigest()


def verify_signature(data, signature):
    """Verify HMAC-SHA256 signature from server response."""
    payload = json.dumps(data, sort_keys=True, separators=(",", ":"))
    expected = hmac.new(HMAC_SECRET.encode(), payload.encode(), hashlib.sha256).hexdigest()
    return hmac.compare_digest(expected, signature)


def _api_request(endpoint, data):
    """Make a POST request to the license server."""
    url = f"{LICENSE_SERVER}/{endpoint}"
    body = json.dumps(data).encode()
    req = urllib.request.Request(
        url,
        data=body,
        headers={"Content-Type": "application/json"},
        method="POST"
    )
    try:
        with urllib.request.urlopen(req, timeout=15) as resp:
            return json.loads(resp.read().decode())
    except urllib.error.HTTPError as e:
        try:
            return json.loads(e.read().decode())
        except Exception:
            return {"error": f"HTTP {e.code}"}
    except Exception as e:
        return {"error": str(e)}


def _load_cache():
    """Load cached license validation."""
    try:
        if os.path.exists(CACHE_FILE):
            with open(CACHE_FILE, "r") as f:
                cache = json.load(f)
            if time.time() - cache.get("timestamp", 0) < CACHE_TTL:
                return cache
    except Exception:
        pass
    return None


def _save_cache(data):
    """Save license validation to cache."""
    try:
        os.makedirs(CACHE_DIR, exist_ok=True)
        data["timestamp"] = time.time()
        with open(CACHE_FILE, "w") as f:
            json.dump(data, f)
    except Exception:
        pass


def activate(license_key):
    """Activate this machine with a license key. Returns (success, tier, features, error)."""
    fingerprint = get_hardware_fingerprint()
    machine = platform.node()
    os_info = f"{platform.system()} {platform.release()}"

    result = _api_request("activate", {
        "license_key": license_key,
        "hardware_fingerprint": fingerprint,
        "machine_name": machine,
        "os_info": os_info,
    })

    if "error" in result:
        return False, None, [], result["error"]

    sig = result.pop("signature", "")
    if not verify_signature(result, sig):
        return False, None, [], "Invalid server signature"

    if result.get("status") == "activated":
        _save_cache({
            "license_key": license_key,
            "tier": result["tier"],
            "features": result["features"],
            "fingerprint": fingerprint,
        })
        return True, result["tier"], result["features"], None

    return False, None, [], result.get("error", "Activation failed")


def validate(license_key=None):
    """
    Validate the current license. Uses cache if within 30-day window.
    Returns (valid, tier, features, error).
    """
    cache = _load_cache()

    if cache and not license_key:
        license_key = cache.get("license_key")

    if not license_key:
        return False, None, [], "No license key found"

    if cache and cache.get("license_key") == license_key:
        return True, cache["tier"], cache["features"], None

    fingerprint = get_hardware_fingerprint()
    result = _api_request("validate", {
        "license_key": license_key,
        "hardware_fingerprint": fingerprint,
    })

    if "error" in result:
        if cache:
            return True, cache["tier"], cache["features"], None
        return False, None, [], result["error"]

    sig = result.pop("signature", "")
    if not verify_signature(result, sig):
        if cache:
            return True, cache["tier"], cache["features"], None
        return False, None, [], "Invalid server signature"

    if result.get("status") == "valid":
        _save_cache({
            "license_key": license_key,
            "tier": result["tier"],
            "features": result["features"],
            "fingerprint": fingerprint,
        })
        return True, result["tier"], result["features"], None

    return False, None, [], result.get("error", "Validation failed")


def deactivate(license_key=None):
    """Deactivate this machine."""
    cache = _load_cache()
    if not license_key and cache:
        license_key = cache.get("license_key")
    if not license_key:
        return False, "No license key found"

    fingerprint = get_hardware_fingerprint()
    result = _api_request("deactivate", {
        "license_key": license_key,
        "hardware_fingerprint": fingerprint,
    })

    if result.get("status") == "deactivated":
        try:
            os.remove(CACHE_FILE)
        except Exception:
            pass
        return True, None

    return False, result.get("error", "Deactivation failed")


def has_feature(feature_name):
    """Check if current license includes a specific feature."""
    cache = _load_cache()
    if not cache:
        return False
    return feature_name in cache.get("features", [])
