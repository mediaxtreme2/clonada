<?php
/**
 * Clonada License Server - API Router
 */

require_once __DIR__ . '/config.php';

// Security headers
header('X-Content-Type-Options: nosniff');
header('X-Frame-Options: DENY');
header('X-XSS-Protection: 1; mode=block');
header('Cache-Control: no-store, no-cache, must-revalidate');

// Rate limiting
$clientIp = $_SERVER['HTTP_X_FORWARDED_FOR'] ?? $_SERVER['REMOTE_ADDR'] ?? '0.0.0.0';
if (!checkRateLimit($clientIp)) {
    jsonResponse(['error' => 'Rate limit exceeded. Try again later.'], 429);
}

// Parse request
$method = $_SERVER['REQUEST_METHOD'];
$uri = $_SERVER['REQUEST_URI'];
$path = parse_url($uri, PHP_URL_PATH);

// Remove base path prefix
$path = preg_replace('#^/api#', '', $path);
$path = '/' . ltrim($path, '/');

// Get JSON input for POST
$input = [];
if ($method === 'POST') {
    $raw = file_get_contents('php://input');
    $input = json_decode($raw, true) ?: [];
}

// Router
switch (true) {
    case $method === 'POST' && $path === '/activate':
        handleActivate($input);
        break;
    case $method === 'POST' && $path === '/validate':
        handleValidate($input);
        break;
    case $method === 'POST' && $path === '/deactivate':
        handleDeactivate($input);
        break;
    case $method === 'GET' && $path === '/status':
        handleStatus();
        break;
    default:
        jsonResponse(['error' => 'Endpoint not found'], 404);
}

function handleActivate(array $input): void {
    $required = ['license_key', 'hardware_fingerprint', 'machine_name', 'os_info'];
    foreach ($required as $field) {
        if (empty($input[$field])) {
            jsonResponse(['error' => "Missing required field: $field"], 400);
        }
    }

    $key = trim($input['license_key']);
    $fingerprint = trim($input['hardware_fingerprint']);
    $machineName = trim($input['machine_name']);
    $osInfo = trim($input['os_info']);
    $clientIp = $_SERVER['HTTP_X_FORWARDED_FOR'] ?? $_SERVER['REMOTE_ADDR'] ?? '0.0.0.0';

    $db = getDB();

    // Find license
    $stmt = $db->prepare('SELECT * FROM licenses WHERE license_key = ?');
    $stmt->execute([$key]);
    $license = $stmt->fetch();

    if (!$license) {
        jsonResponse(['error' => 'Invalid license key'], 404);
    }

    if ($license['status'] !== 'active') {
        jsonResponse(['error' => 'License is ' . $license['status']], 403);
    }

    // Check expiry
    if ($license['expires_at'] && strtotime($license['expires_at']) < time()) {
        // Mark as expired
        $db->prepare('UPDATE licenses SET status = "expired" WHERE id = ?')->execute([$license['id']]);
        jsonResponse(['error' => 'License has expired'], 403);
    }

    // Check if this fingerprint is already activated
    $stmt = $db->prepare('SELECT * FROM activations WHERE license_id = ? AND hardware_fingerprint = ? AND is_active = 1');
    $stmt->execute([$license['id'], $fingerprint]);
    $existing = $stmt->fetch();

    if ($existing) {
        // Already activated, return success
        $features = $license['tier'] === 'advanced' ? FEATURES_ADVANCED : FEATURES_BASIC;
        jsonResponse([
            'status' => 'activated',
            'tier' => $license['tier'],
            'expires_at' => $license['expires_at'],
            'features' => $features,
        ]);
    }

    // Check activation limit
    if ($license['activations_count'] >= $license['max_activations']) {
        jsonResponse(['error' => 'Maximum activations reached (' . $license['max_activations'] . ')'], 403);
    }

    // Create activation
    $now = date('Y-m-d H:i:s');
    $stmt = $db->prepare('INSERT INTO activations (license_id, hardware_fingerprint, machine_name, os_info, ip_address, activated_at, last_validated) VALUES (?, ?, ?, ?, ?, ?, ?)');
    $stmt->execute([$license['id'], $fingerprint, $machineName, $osInfo, $clientIp, $now, $now]);

    // Increment count
    $db->prepare('UPDATE licenses SET activations_count = activations_count + 1, updated_at = ? WHERE id = ?')->execute([$now, $license['id']]);

    $features = $license['tier'] === 'advanced' ? FEATURES_ADVANCED : FEATURES_BASIC;
    jsonResponse([
        'status' => 'activated',
        'tier' => $license['tier'],
        'expires_at' => $license['expires_at'],
        'features' => $features,
    ]);
}

function handleValidate(array $input): void {
    $required = ['license_key', 'hardware_fingerprint'];
    foreach ($required as $field) {
        if (empty($input[$field])) {
            jsonResponse(['error' => "Missing required field: $field"], 400);
        }
    }

    $key = trim($input['license_key']);
    $fingerprint = trim($input['hardware_fingerprint']);

    $db = getDB();

    // Find license
    $stmt = $db->prepare('SELECT * FROM licenses WHERE license_key = ?');
    $stmt->execute([$key]);
    $license = $stmt->fetch();

    if (!$license) {
        jsonResponse(['error' => 'Invalid license key'], 404);
    }

    if ($license['status'] !== 'active') {
        jsonResponse(['error' => 'License is ' . $license['status'], 'status' => 'invalid'], 403);
    }

    // Check expiry
    if ($license['expires_at'] && strtotime($license['expires_at']) < time()) {
        $db->prepare('UPDATE licenses SET status = "expired" WHERE id = ?')->execute([$license['id']]);
        jsonResponse(['error' => 'License has expired', 'status' => 'expired'], 403);
    }

    // Check activation exists
    $stmt = $db->prepare('SELECT * FROM activations WHERE license_id = ? AND hardware_fingerprint = ? AND is_active = 1');
    $stmt->execute([$license['id'], $fingerprint]);
    $activation = $stmt->fetch();

    if (!$activation) {
        jsonResponse(['error' => 'No active activation found for this device', 'status' => 'invalid'], 403);
    }

    // Update last_validated
    $now = date('Y-m-d H:i:s');
    $db->prepare('UPDATE activations SET last_validated = ? WHERE id = ?')->execute([$now, $activation['id']]);

    $features = $license['tier'] === 'advanced' ? FEATURES_ADVANCED : FEATURES_BASIC;
    jsonResponse([
        'status' => 'valid',
        'tier' => $license['tier'],
        'features' => $features,
        'next_check_seconds' => NEXT_CHECK_SECONDS,
    ]);
}

function handleDeactivate(array $input): void {
    $required = ['license_key', 'hardware_fingerprint'];
    foreach ($required as $field) {
        if (empty($input[$field])) {
            jsonResponse(['error' => "Missing required field: $field"], 400);
        }
    }

    $key = trim($input['license_key']);
    $fingerprint = trim($input['hardware_fingerprint']);

    $db = getDB();

    // Find license
    $stmt = $db->prepare('SELECT * FROM licenses WHERE license_key = ?');
    $stmt->execute([$key]);
    $license = $stmt->fetch();

    if (!$license) {
        jsonResponse(['error' => 'Invalid license key'], 404);
    }

    // Find activation
    $stmt = $db->prepare('SELECT * FROM activations WHERE license_id = ? AND hardware_fingerprint = ? AND is_active = 1');
    $stmt->execute([$license['id'], $fingerprint]);
    $activation = $stmt->fetch();

    if (!$activation) {
        jsonResponse(['error' => 'No active activation found'], 404);
    }

    // Deactivate
    $now = date('Y-m-d H:i:s');
    $db->prepare('UPDATE activations SET is_active = 0 WHERE id = ?')->execute([$activation['id']]);
    $db->prepare('UPDATE licenses SET activations_count = GREATEST(activations_count - 1, 0), updated_at = ? WHERE id = ?')->execute([$now, $license['id']]);

    jsonResponse(['status' => 'deactivated']);
}

function handleStatus(): void {
    $key = $_GET['key'] ?? '';
    if (empty($key)) {
        jsonResponse(['error' => 'Missing key parameter'], 400);
    }

    $db = getDB();
    $stmt = $db->prepare('SELECT license_key, tier, max_activations, activations_count, status, expires_at FROM licenses WHERE license_key = ?');
    $stmt->execute([$key]);
    $license = $stmt->fetch();

    if (!$license) {
        jsonResponse(['error' => 'Invalid license key'], 404);
    }

    jsonResponse([
        'status' => $license['status'],
        'tier' => $license['tier'],
        'activations_used' => (int)$license['activations_count'],
        'activations_max' => (int)$license['max_activations'],
        'expires_at' => $license['expires_at'],
    ]);
}
