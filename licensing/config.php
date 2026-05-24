<?php
/**
 * Clonada License Server - Configuration
 */

define('DB_HOST', '127.0.0.1');
define('DB_NAME', 'clonada_license');
define('DB_USER', 'clonada');
define('DB_PASS', 'Cl0nada_Lic2026!');

define('HMAC_SECRET', 'clonada_hmac_s3cr3t_2026');

define('RATE_LIMIT_REQUESTS', 30);
define('RATE_LIMIT_WINDOW', 60); // seconds
define('RATE_LIMIT_DIR', '/tmp/clonada_rate_limit');

define('FEATURES_BASIC', ['swap', 'separate']);
define('FEATURES_ADVANCED', ['swap', 'separate', 'train', 'batch', 'realtime']);

define('NEXT_CHECK_SECONDS', 2592000); // 30 days

function getDB(): PDO {
    static $pdo = null;
    if ($pdo === null) {
        $pdo = new PDO(
            'mysql:host=' . DB_HOST . ';dbname=' . DB_NAME . ';charset=utf8mb4',
            DB_USER,
            DB_PASS,
            [
                PDO::ATTR_ERRMODE => PDO::ERRMODE_EXCEPTION,
                PDO::ATTR_DEFAULT_FETCH_MODE => PDO::FETCH_ASSOC,
                PDO::ATTR_EMULATE_PREPARES => false,
            ]
        );
    }
    return $pdo;
}

function signResponse(array $data): array {
    $json = json_encode($data, JSON_UNESCAPED_SLASHES);
    $data['signature'] = hash_hmac('sha256', $json, HMAC_SECRET);
    return $data;
}

function jsonResponse(array $data, int $code = 200): void {
    http_response_code($code);
    header('Content-Type: application/json');
    $signed = signResponse($data);
    echo json_encode($signed, JSON_UNESCAPED_SLASHES | JSON_PRETTY_PRINT);
    exit;
}

function checkRateLimit(string $ip): bool {
    if (!is_dir(RATE_LIMIT_DIR)) {
        @mkdir(RATE_LIMIT_DIR, 0777, true);
    }
    $file = RATE_LIMIT_DIR . '/' . md5($ip) . '.json';
    $now = time();
    $data = [];

    if (file_exists($file)) {
        $content = file_get_contents($file);
        $data = json_decode($content, true) ?: [];
    }

    // Remove entries older than window
    $data = array_filter($data, fn($ts) => $ts > ($now - RATE_LIMIT_WINDOW));

    if (count($data) >= RATE_LIMIT_REQUESTS) {
        return false;
    }

    $data[] = $now;
    file_put_contents($file, json_encode($data));
    return true;
}
