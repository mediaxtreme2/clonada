<?php
/**
 * Clonada Admin - License Generation
 */

require_once __DIR__ . '/auth.php';
requireAuth();

$db = getDB();
$generatedKeys = [];
$message = '';
$messageType = '';

function generateLicenseKey(): string {
    $chars = 'ABCDEFGHJKLMNPQRSTUVWXYZ23456789'; // no I, O, 0, 1 for readability
    $segments = [];
    for ($i = 0; $i < 4; $i++) {
        $seg = '';
        for ($j = 0; $j < 4; $j++) {
            $seg .= $chars[random_int(0, strlen($chars) - 1)];
        }
        $segments[] = $seg;
    }
    return 'CLON-' . implode('-', $segments);
}

if ($_SERVER['REQUEST_METHOD'] === 'POST') {
    $quantity = max(1, min(100, (int)($_POST['quantity'] ?? 1)));
    $tier = in_array($_POST['tier'] ?? '', ['basic', 'advanced']) ? $_POST['tier'] : 'basic';
    $email = trim($_POST['email'] ?? '');
    $expiresAt = !empty($_POST['expires_at']) ? $_POST['expires_at'] . ' 23:59:59' : null;
    $notes = trim($_POST['notes'] ?? '') ?: null;

    $maxActivations = $tier === 'advanced' ? 5 : 2;

    if (empty($email)) {
        $message = 'Email is required.';
        $messageType = 'error';
    } else {
        $stmt = $db->prepare("INSERT INTO licenses (license_key, tier, email, max_activations, status, expires_at, notes, created_at, updated_at) VALUES (?, ?, ?, ?, 'active', ?, ?, NOW(), NOW())");

        for ($i = 0; $i < $quantity; $i++) {
            $attempts = 0;
            do {
                $key = generateLicenseKey();
                $attempts++;
                try {
                    $stmt->execute([$key, $tier, $email, $maxActivations, $expiresAt, $notes]);
                    $generatedKeys[] = $key;
                    break;
                } catch (PDOException $e) {
                    if ($attempts > 10) {
                        $message = 'Error generating unique key after multiple attempts.';
                        $messageType = 'error';
                        break 2;
                    }
                }
            } while (true);
        }

        if (empty($message)) {
            $message = count($generatedKeys) . ' license(s) generated successfully.';
            $messageType = 'success';
        }
    }
}

include __DIR__ . '/header.php';
?>

<div class="page-header">
    <h2>Generate Licenses</h2>
    <p>Create new license keys for distribution</p>
</div>

<?php if ($message): ?>
    <div class="alert alert-<?= $messageType ?>"><?= htmlspecialchars($message) ?></div>
<?php endif; ?>

<?php if (!empty($generatedKeys)): ?>
<div class="card">
    <h3 style="margin-bottom: 12px; font-size: 1rem; color: var(--text-primary);">Generated Keys</h3>
    <div class="license-keys-output"><?= implode("\n", array_map('htmlspecialchars', $generatedKeys)) ?></div>
    <p class="text-muted mt-2" style="font-size: 0.8rem;"><?= count($generatedKeys) ?> key(s) ready to distribute</p>
</div>
<?php endif; ?>

<div class="card">
    <form method="POST">
        <div class="form-row">
            <div class="form-group">
                <label>Tier</label>
                <select name="tier" class="form-control">
                    <option value="basic">Basic (2 activations)</option>
                    <option value="advanced">Advanced (5 activations)</option>
                </select>
            </div>
            <div class="form-group">
                <label>Quantity</label>
                <input type="number" name="quantity" class="form-control" value="1" min="1" max="100">
            </div>
        </div>

        <div class="form-group">
            <label>Customer Email</label>
            <input type="email" name="email" class="form-control" placeholder="customer@email.com" required>
        </div>

        <div class="form-row">
            <div class="form-group">
                <label>Expires At (leave blank for lifetime)</label>
                <input type="date" name="expires_at" class="form-control">
            </div>
            <div class="form-group">
                <label>Notes (optional)</label>
                <input type="text" name="notes" class="form-control" placeholder="Internal notes...">
            </div>
        </div>

        <button type="submit" class="btn btn-primary">Generate License(s)</button>
    </form>
</div>

<?php include __DIR__ . '/footer.php'; ?>
