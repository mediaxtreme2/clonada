<?php
/**
 * Clonada Admin - Activation Viewer
 */

require_once __DIR__ . '/auth.php';
requireAuth();

$db = getDB();
$message = '';
$messageType = '';

// Handle deactivation
if ($_SERVER['REQUEST_METHOD'] === 'POST' && ($_POST['action'] ?? '') === 'deactivate') {
    $activationId = (int)($_POST['activation_id'] ?? 0);
    if ($activationId) {
        // Get license_id before deactivating
        $stmt = $db->prepare('SELECT license_id FROM activations WHERE id = ? AND is_active = 1');
        $stmt->execute([$activationId]);
        $act = $stmt->fetch();

        if ($act) {
            $db->prepare('UPDATE activations SET is_active = 0 WHERE id = ?')->execute([$activationId]);
            $db->prepare('UPDATE licenses SET activations_count = GREATEST(activations_count - 1, 0), updated_at = NOW() WHERE id = ?')->execute([$act['license_id']]);
            $message = 'Activation deactivated successfully.';
            $messageType = 'success';
        }
    }
}

// Fetch activations with license info
$search = trim($_GET['search'] ?? '');
$sql = "SELECT a.*, l.license_key, l.tier, l.email
        FROM activations a
        JOIN licenses l ON a.license_id = l.id";

$params = [];
if ($search) {
    $sql .= " WHERE (l.license_key LIKE ? OR a.machine_name LIKE ? OR a.hardware_fingerprint LIKE ?)";
    $params = ["%$search%", "%$search%", "%$search%"];
}
$sql .= " ORDER BY a.activated_at DESC LIMIT 200";

$stmt = $db->prepare($sql);
$stmt->execute($params);
$activations = $stmt->fetchAll();

include __DIR__ . '/header.php';
?>

<div class="page-header">
    <h2>Activations</h2>
    <p>View and manage device activations</p>
</div>

<?php if ($message): ?>
    <div class="alert alert-<?= $messageType ?>"><?= htmlspecialchars($message) ?></div>
<?php endif; ?>

<div class="card">
    <form method="GET" class="search-bar">
        <input type="text" name="search" class="form-control" placeholder="Search by key, machine name, or fingerprint..." value="<?= htmlspecialchars($search) ?>">
        <button type="submit" class="btn btn-primary">Search</button>
    </form>

    <?php if (empty($activations)): ?>
        <div class="empty-state">
            <p>No activations found.</p>
        </div>
    <?php else: ?>
        <table>
            <thead>
                <tr>
                    <th>License Key</th>
                    <th>Machine</th>
                    <th>OS</th>
                    <th>Fingerprint</th>
                    <th>IP</th>
                    <th>Activated</th>
                    <th>Last Check</th>
                    <th>Status</th>
                    <th>Action</th>
                </tr>
            </thead>
            <tbody>
                <?php foreach ($activations as $act): ?>
                <tr>
                    <td class="mono" style="font-size: 0.75rem;"><?= htmlspecialchars($act['license_key']) ?></td>
                    <td><?= htmlspecialchars($act['machine_name']) ?></td>
                    <td class="text-muted" style="font-size: 0.8rem;"><?= htmlspecialchars($act['os_info']) ?></td>
                    <td class="mono text-muted" style="font-size: 0.7rem;" title="<?= htmlspecialchars($act['hardware_fingerprint']) ?>"><?= substr($act['hardware_fingerprint'], 0, 12) ?>...</td>
                    <td class="text-muted"><?= htmlspecialchars($act['ip_address']) ?></td>
                    <td class="text-muted"><?= date('j M Y', strtotime($act['activated_at'])) ?></td>
                    <td class="text-muted"><?= date('j M Y', strtotime($act['last_validated'])) ?></td>
                    <td>
                        <?php if ($act['is_active']): ?>
                            <span class="badge badge-active">Active</span>
                        <?php else: ?>
                            <span class="badge badge-revoked">Inactive</span>
                        <?php endif; ?>
                    </td>
                    <td>
                        <?php if ($act['is_active']): ?>
                            <form method="POST" style="display:inline;" onsubmit="return confirm('Deactivate this device?')">
                                <input type="hidden" name="action" value="deactivate">
                                <input type="hidden" name="activation_id" value="<?= $act['id'] ?>">
                                <button type="submit" class="btn btn-danger btn-sm">Deactivate</button>
                            </form>
                        <?php else: ?>
                            <span class="text-muted">-</span>
                        <?php endif; ?>
                    </td>
                </tr>
                <?php endforeach; ?>
            </tbody>
        </table>
    <?php endif; ?>
</div>

<?php include __DIR__ . '/footer.php'; ?>
