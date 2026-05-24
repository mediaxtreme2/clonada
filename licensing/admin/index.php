<?php
/**
 * Clonada Admin - Login & Dashboard
 */

require_once __DIR__ . '/auth.php';

// Handle logout
if (isset($_GET['logout'])) {
    logout();
}

// Handle login
$error = '';
if ($_SERVER['REQUEST_METHOD'] === 'POST' && !isLoggedIn()) {
    $username = trim($_POST['username'] ?? '');
    $password = $_POST['password'] ?? '';
    if (!attemptLogin($username, $password)) {
        $error = 'Invalid credentials';
    }
}

// Show login if not authenticated
if (!isLoggedIn()):
?>
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Clonada - Admin Login</title>
    <style>
        * { margin: 0; padding: 0; box-sizing: border-box; }
        body {
            font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', 'Inter', sans-serif;
            background: #0a0a0c;
            min-height: 100vh;
            display: flex;
            align-items: center;
            justify-content: center;
        }
        .login-container {
            width: 100%;
            max-width: 400px;
            padding: 20px;
        }
        .login-brand {
            text-align: center;
            margin-bottom: 40px;
        }
        .login-brand h1 {
            font-size: 2rem;
            font-weight: 700;
            background: linear-gradient(135deg, #6366f1, #06b6d4);
            -webkit-background-clip: text;
            -webkit-text-fill-color: transparent;
            background-clip: text;
            margin-bottom: 4px;
        }
        .login-brand p {
            color: #6a6a7a;
            font-size: 0.85rem;
            text-transform: uppercase;
            letter-spacing: 2px;
        }
        .login-card {
            background: #15151a;
            border: 1px solid #2a2a32;
            border-radius: 12px;
            padding: 32px;
            box-shadow: 0 8px 32px rgba(0, 0, 0, 0.5);
        }
        .login-card h2 {
            font-size: 1.1rem;
            color: #f0f0f5;
            margin-bottom: 24px;
            font-weight: 600;
        }
        .form-group {
            margin-bottom: 20px;
        }
        .form-group label {
            display: block;
            font-size: 0.78rem;
            font-weight: 600;
            color: #9898a8;
            margin-bottom: 6px;
            text-transform: uppercase;
            letter-spacing: 0.5px;
        }
        .form-group input {
            width: 100%;
            padding: 11px 14px;
            background: #0a0a0c;
            border: 1px solid #2a2a32;
            border-radius: 8px;
            color: #f0f0f5;
            font-size: 0.9rem;
            font-family: inherit;
            transition: border-color 0.15s;
        }
        .form-group input:focus {
            outline: none;
            border-color: #6366f1;
            box-shadow: 0 0 0 3px rgba(99, 102, 241, 0.1);
        }
        .btn-login {
            width: 100%;
            padding: 12px;
            background: #6366f1;
            color: #fff;
            border: none;
            border-radius: 8px;
            font-size: 0.9rem;
            font-weight: 600;
            cursor: pointer;
            transition: background 0.15s;
            margin-top: 8px;
        }
        .btn-login:hover { background: #7c7ff7; }
        .error-msg {
            background: rgba(239, 68, 68, 0.1);
            color: #ef4444;
            padding: 10px 14px;
            border-radius: 8px;
            font-size: 0.85rem;
            margin-bottom: 16px;
            border: 1px solid rgba(239, 68, 68, 0.2);
        }
    </style>
</head>
<body>
    <div class="login-container">
        <div class="login-brand">
            <h1>Clonada</h1>
            <p>License Server</p>
        </div>
        <div class="login-card">
            <h2>Sign in to Admin</h2>
            <?php if ($error): ?>
                <div class="error-msg"><?= htmlspecialchars($error) ?></div>
            <?php endif; ?>
            <form method="POST">
                <div class="form-group">
                    <label>Username</label>
                    <input type="text" name="username" required autofocus>
                </div>
                <div class="form-group">
                    <label>Password</label>
                    <input type="password" name="password" required>
                </div>
                <button type="submit" class="btn-login">Sign In</button>
            </form>
        </div>
    </div>
</body>
</html>
<?php
exit;
endif;

// Dashboard
$db = getDB();

$totalLicenses = $db->query('SELECT COUNT(*) FROM licenses')->fetchColumn();
$activeLicenses = $db->query("SELECT COUNT(*) FROM licenses WHERE status = 'active'")->fetchColumn();
$totalActivations = $db->query("SELECT COUNT(*) FROM activations WHERE is_active = 1")->fetchColumn();
$basicCount = $db->query("SELECT COUNT(*) FROM licenses WHERE tier = 'basic'")->fetchColumn();
$advancedCount = $db->query("SELECT COUNT(*) FROM licenses WHERE tier = 'advanced'")->fetchColumn();
$revokedCount = $db->query("SELECT COUNT(*) FROM licenses WHERE status = 'revoked'")->fetchColumn();

// Recent licenses
$recentLicenses = $db->query('SELECT * FROM licenses ORDER BY created_at DESC LIMIT 10')->fetchAll();

include __DIR__ . '/header.php';
?>

<div class="page-header">
    <h2>Dashboard</h2>
    <p>Clonada licensing overview</p>
</div>

<div class="stats-grid">
    <div class="stat-card primary">
        <div class="stat-label">Total Licenses</div>
        <div class="stat-value"><?= $totalLicenses ?></div>
    </div>
    <div class="stat-card success">
        <div class="stat-label">Active Licenses</div>
        <div class="stat-value"><?= $activeLicenses ?></div>
    </div>
    <div class="stat-card accent">
        <div class="stat-label">Active Devices</div>
        <div class="stat-value"><?= $totalActivations ?></div>
    </div>
    <div class="stat-card warning">
        <div class="stat-label">Advanced Tier</div>
        <div class="stat-value"><?= $advancedCount ?></div>
    </div>
</div>

<div class="stats-grid">
    <div class="stat-card">
        <div class="stat-label">Basic Licenses</div>
        <div class="stat-value"><?= $basicCount ?></div>
    </div>
    <div class="stat-card">
        <div class="stat-label">Revoked</div>
        <div class="stat-value" style="color: var(--danger);"><?= $revokedCount ?></div>
    </div>
</div>

<div class="card">
    <h3 style="margin-bottom: 16px; font-size: 1rem; color: var(--text-primary);">Recent Licenses</h3>
    <?php if (empty($recentLicenses)): ?>
        <div class="empty-state">
            <p>No licenses generated yet.</p>
        </div>
    <?php else: ?>
        <table>
            <thead>
                <tr>
                    <th>License Key</th>
                    <th>Tier</th>
                    <th>Email</th>
                    <th>Status</th>
                    <th>Activations</th>
                    <th>Created</th>
                </tr>
            </thead>
            <tbody>
                <?php foreach ($recentLicenses as $lic): ?>
                <tr>
                    <td class="mono"><?= htmlspecialchars($lic['license_key']) ?></td>
                    <td><span class="badge badge-<?= $lic['tier'] ?>"><?= $lic['tier'] ?></span></td>
                    <td><?= htmlspecialchars($lic['email']) ?></td>
                    <td><span class="badge badge-<?= $lic['status'] ?>"><?= $lic['status'] ?></span></td>
                    <td><?= $lic['activations_count'] ?>/<?= $lic['max_activations'] ?></td>
                    <td class="text-muted"><?= date('j M Y', strtotime($lic['created_at'])) ?></td>
                </tr>
                <?php endforeach; ?>
            </tbody>
        </table>
    <?php endif; ?>
</div>

<?php include __DIR__ . '/footer.php'; ?>
