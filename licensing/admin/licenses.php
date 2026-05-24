<?php
/**
 * Clonada Admin - License Management
 */

require_once __DIR__ . '/auth.php';
requireAuth();

$db = getDB();
$message = '';
$messageType = '';

// Handle actions
if ($_SERVER['REQUEST_METHOD'] === 'POST') {
    $action = $_POST['action'] ?? '';

    if ($action === 'revoke' && !empty($_POST['license_id'])) {
        $stmt = $db->prepare("UPDATE licenses SET status = 'revoked', updated_at = NOW() WHERE id = ?");
        $stmt->execute([$_POST['license_id']]);
        // Deactivate all activations
        $stmt = $db->prepare("UPDATE activations SET is_active = 0 WHERE license_id = ?");
        $stmt->execute([$_POST['license_id']]);
        $message = 'License revoked successfully.';
        $messageType = 'success';
    }

    if ($action === 'reactivate' && !empty($_POST['license_id'])) {
        $stmt = $db->prepare("UPDATE licenses SET status = 'active', updated_at = NOW() WHERE id = ?");
        $stmt->execute([$_POST['license_id']]);
        $message = 'License reactivated.';
        $messageType = 'success';
    }

    if ($action === 'update' && !empty($_POST['license_id'])) {
        $email = trim($_POST['email'] ?? '');
        $tier = $_POST['tier'] ?? 'basic';
        $maxAct = (int)($_POST['max_activations'] ?? 2);
        $expiresAt = !empty($_POST['expires_at']) ? $_POST['expires_at'] : null;
        $notes = trim($_POST['notes'] ?? '') ?: null;

        $stmt = $db->prepare("UPDATE licenses SET email = ?, tier = ?, max_activations = ?, expires_at = ?, notes = ?, updated_at = NOW() WHERE id = ?");
        $stmt->execute([$email, $tier, $maxAct, $expiresAt, $notes, $_POST['license_id']]);
        $message = 'License updated.';
        $messageType = 'success';
    }
}

// Search and filter
$search = trim($_GET['search'] ?? '');
$filterStatus = $_GET['status'] ?? '';
$filterTier = $_GET['tier'] ?? '';

$where = [];
$params = [];

if ($search) {
    $where[] = "(license_key LIKE ? OR email LIKE ?)";
    $params[] = "%$search%";
    $params[] = "%$search%";
}
if ($filterStatus) {
    $where[] = "status = ?";
    $params[] = $filterStatus;
}
if ($filterTier) {
    $where[] = "tier = ?";
    $params[] = $filterTier;
}

$sql = "SELECT * FROM licenses";
if ($where) {
    $sql .= " WHERE " . implode(' AND ', $where);
}
$sql .= " ORDER BY created_at DESC LIMIT 200";

$stmt = $db->prepare($sql);
$stmt->execute($params);
$licenses = $stmt->fetchAll();

include __DIR__ . '/header.php';
?>

<div class="page-header">
    <h2>Licenses</h2>
    <p>Manage all license keys</p>
</div>

<?php if ($message): ?>
    <div class="alert alert-<?= $messageType ?>"><?= htmlspecialchars($message) ?></div>
<?php endif; ?>

<div class="card">
    <form method="GET" class="search-bar">
        <input type="text" name="search" class="form-control" placeholder="Search by key or email..." value="<?= htmlspecialchars($search) ?>">
        <select name="status" class="form-control" style="max-width: 160px;">
            <option value="">All Status</option>
            <option value="active" <?= $filterStatus === 'active' ? 'selected' : '' ?>>Active</option>
            <option value="revoked" <?= $filterStatus === 'revoked' ? 'selected' : '' ?>>Revoked</option>
            <option value="expired" <?= $filterStatus === 'expired' ? 'selected' : '' ?>>Expired</option>
        </select>
        <select name="tier" class="form-control" style="max-width: 160px;">
            <option value="">All Tiers</option>
            <option value="basic" <?= $filterTier === 'basic' ? 'selected' : '' ?>>Basic</option>
            <option value="advanced" <?= $filterTier === 'advanced' ? 'selected' : '' ?>>Advanced</option>
        </select>
        <button type="submit" class="btn btn-primary">Filter</button>
    </form>

    <?php if (empty($licenses)): ?>
        <div class="empty-state">
            <p>No licenses found.</p>
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
                    <th>Expires</th>
                    <th>Actions</th>
                </tr>
            </thead>
            <tbody>
                <?php foreach ($licenses as $lic): ?>
                <tr>
                    <td class="mono"><?= htmlspecialchars($lic['license_key']) ?></td>
                    <td><span class="badge badge-<?= $lic['tier'] ?>"><?= $lic['tier'] ?></span></td>
                    <td><?= htmlspecialchars($lic['email']) ?></td>
                    <td><span class="badge badge-<?= $lic['status'] ?>"><?= $lic['status'] ?></span></td>
                    <td><?= $lic['activations_count'] ?>/<?= $lic['max_activations'] ?></td>
                    <td class="text-muted"><?= $lic['expires_at'] ? date('j M Y', strtotime($lic['expires_at'])) : 'Lifetime' ?></td>
                    <td>
                        <?php if ($lic['status'] === 'active'): ?>
                            <form method="POST" style="display:inline;" onsubmit="return confirm('Revoke this license?')">
                                <input type="hidden" name="action" value="revoke">
                                <input type="hidden" name="license_id" value="<?= $lic['id'] ?>">
                                <button type="submit" class="btn btn-danger btn-sm">Revoke</button>
                            </form>
                        <?php else: ?>
                            <form method="POST" style="display:inline;">
                                <input type="hidden" name="action" value="reactivate">
                                <input type="hidden" name="license_id" value="<?= $lic['id'] ?>">
                                <button type="submit" class="btn btn-primary btn-sm">Reactivate</button>
                            </form>
                        <?php endif; ?>
                    </td>
                </tr>
                <?php endforeach; ?>
            </tbody>
        </table>
    <?php endif; ?>
</div>

<?php include __DIR__ . '/footer.php'; ?>
