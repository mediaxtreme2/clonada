<?php
/**
 * Clonada Admin - Authentication Middleware
 */

session_start();

require_once __DIR__ . '/../config.php';

function isLoggedIn(): bool {
    return isset($_SESSION['admin_id']) && isset($_SESSION['admin_user']);
}

function requireAuth(): void {
    if (!isLoggedIn()) {
        header('Location: /admin/');
        exit;
    }
}

function attemptLogin(string $username, string $password): bool {
    $db = getDB();
    $stmt = $db->prepare('SELECT * FROM admin_users WHERE username = ?');
    $stmt->execute([$username]);
    $user = $stmt->fetch();

    if ($user && password_verify($password, $user['password_hash'])) {
        $_SESSION['admin_id'] = $user['id'];
        $_SESSION['admin_user'] = $user['username'];
        return true;
    }
    return false;
}

function logout(): void {
    session_destroy();
    header('Location: /admin/');
    exit;
}
