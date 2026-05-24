<?php
/**
 * Clonada Admin - Shared Header
 */
$currentPage = basename($_SERVER['PHP_SELF'], '.php');
?>
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Clonada License Admin</title>
    <style>
        * { margin: 0; padding: 0; box-sizing: border-box; }

        :root {
            --bg-primary: #0a0a0c;
            --bg-secondary: #111114;
            --bg-tertiary: #1a1a1f;
            --bg-card: #15151a;
            --border: #2a2a32;
            --text-primary: #f0f0f5;
            --text-secondary: #9898a8;
            --text-muted: #6a6a7a;
            --primary: #6366f1;
            --primary-hover: #7c7ff7;
            --primary-bg: rgba(99, 102, 241, 0.1);
            --accent: #06b6d4;
            --accent-bg: rgba(6, 182, 212, 0.1);
            --success: #10b981;
            --success-bg: rgba(16, 185, 129, 0.1);
            --danger: #ef4444;
            --danger-bg: rgba(239, 68, 68, 0.1);
            --warning: #f59e0b;
            --warning-bg: rgba(245, 158, 11, 0.1);
            --radius: 8px;
            --radius-lg: 12px;
            --shadow: 0 4px 24px rgba(0, 0, 0, 0.4);
        }

        body {
            font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', 'Inter', sans-serif;
            background: var(--bg-primary);
            color: var(--text-primary);
            line-height: 1.6;
            min-height: 100vh;
        }

        .layout {
            display: flex;
            min-height: 100vh;
        }

        .sidebar {
            width: 260px;
            background: var(--bg-secondary);
            border-right: 1px solid var(--border);
            padding: 24px 0;
            position: fixed;
            top: 0;
            left: 0;
            bottom: 0;
            overflow-y: auto;
            z-index: 100;
        }

        .sidebar-brand {
            padding: 0 24px 24px;
            border-bottom: 1px solid var(--border);
            margin-bottom: 16px;
        }

        .sidebar-brand h1 {
            font-size: 1.4rem;
            font-weight: 700;
            background: linear-gradient(135deg, var(--primary), var(--accent));
            -webkit-background-clip: text;
            -webkit-text-fill-color: transparent;
            background-clip: text;
            letter-spacing: -0.5px;
        }

        .sidebar-brand span {
            font-size: 0.75rem;
            color: var(--text-muted);
            text-transform: uppercase;
            letter-spacing: 1px;
        }

        .sidebar-nav { list-style: none; padding: 0 12px; }

        .sidebar-nav li { margin-bottom: 2px; }

        .sidebar-nav a {
            display: flex;
            align-items: center;
            gap: 12px;
            padding: 10px 16px;
            color: var(--text-secondary);
            text-decoration: none;
            border-radius: var(--radius);
            font-size: 0.9rem;
            font-weight: 500;
            transition: all 0.15s ease;
        }

        .sidebar-nav a:hover {
            background: var(--bg-tertiary);
            color: var(--text-primary);
        }

        .sidebar-nav a.active {
            background: var(--primary-bg);
            color: var(--primary);
            border: 1px solid rgba(99, 102, 241, 0.2);
        }

        .sidebar-nav .nav-icon {
            width: 20px;
            height: 20px;
            display: flex;
            align-items: center;
            justify-content: center;
            font-size: 1rem;
        }

        .sidebar-footer {
            position: absolute;
            bottom: 0;
            left: 0;
            right: 0;
            padding: 16px 24px;
            border-top: 1px solid var(--border);
        }

        .sidebar-footer a {
            color: var(--text-muted);
            text-decoration: none;
            font-size: 0.85rem;
            transition: color 0.15s;
        }

        .sidebar-footer a:hover { color: var(--danger); }

        .main-content {
            flex: 1;
            margin-left: 260px;
            padding: 32px 40px;
            max-width: 1200px;
        }

        .page-header {
            margin-bottom: 32px;
        }

        .page-header h2 {
            font-size: 1.6rem;
            font-weight: 700;
            color: var(--text-primary);
            margin-bottom: 4px;
        }

        .page-header p {
            color: var(--text-muted);
            font-size: 0.9rem;
        }

        .card {
            background: var(--bg-card);
            border: 1px solid var(--border);
            border-radius: var(--radius-lg);
            padding: 24px;
            margin-bottom: 24px;
            box-shadow: var(--shadow);
        }

        .stats-grid {
            display: grid;
            grid-template-columns: repeat(auto-fit, minmax(220px, 1fr));
            gap: 16px;
            margin-bottom: 32px;
        }

        .stat-card {
            background: var(--bg-card);
            border: 1px solid var(--border);
            border-radius: var(--radius-lg);
            padding: 20px 24px;
            box-shadow: var(--shadow);
        }

        .stat-card .stat-label {
            font-size: 0.8rem;
            text-transform: uppercase;
            letter-spacing: 0.5px;
            color: var(--text-muted);
            margin-bottom: 8px;
        }

        .stat-card .stat-value {
            font-size: 1.8rem;
            font-weight: 700;
            color: var(--text-primary);
        }

        .stat-card.primary .stat-value { color: var(--primary); }
        .stat-card.accent .stat-value { color: var(--accent); }
        .stat-card.success .stat-value { color: var(--success); }
        .stat-card.warning .stat-value { color: var(--warning); }

        table {
            width: 100%;
            border-collapse: collapse;
            font-size: 0.88rem;
        }

        table th {
            text-align: left;
            padding: 12px 16px;
            font-size: 0.75rem;
            text-transform: uppercase;
            letter-spacing: 0.5px;
            color: var(--text-muted);
            border-bottom: 1px solid var(--border);
            font-weight: 600;
        }

        table td {
            padding: 12px 16px;
            border-bottom: 1px solid var(--border);
            color: var(--text-secondary);
        }

        table tr:hover td {
            background: var(--bg-tertiary);
        }

        table tr:last-child td { border-bottom: none; }

        .badge {
            display: inline-block;
            padding: 3px 10px;
            border-radius: 20px;
            font-size: 0.72rem;
            font-weight: 600;
            text-transform: uppercase;
            letter-spacing: 0.5px;
        }

        .badge-active { background: var(--success-bg); color: var(--success); }
        .badge-revoked { background: var(--danger-bg); color: var(--danger); }
        .badge-expired { background: var(--warning-bg); color: var(--warning); }
        .badge-basic { background: var(--primary-bg); color: var(--primary); }
        .badge-advanced { background: var(--accent-bg); color: var(--accent); }

        .btn {
            display: inline-flex;
            align-items: center;
            gap: 8px;
            padding: 9px 18px;
            border: none;
            border-radius: var(--radius);
            font-size: 0.85rem;
            font-weight: 600;
            cursor: pointer;
            transition: all 0.15s ease;
            text-decoration: none;
        }

        .btn-primary {
            background: var(--primary);
            color: #fff;
        }
        .btn-primary:hover { background: var(--primary-hover); }

        .btn-danger {
            background: var(--danger-bg);
            color: var(--danger);
            border: 1px solid rgba(239, 68, 68, 0.3);
        }
        .btn-danger:hover { background: rgba(239, 68, 68, 0.2); }

        .btn-sm {
            padding: 5px 12px;
            font-size: 0.78rem;
        }

        .form-group {
            margin-bottom: 20px;
        }

        .form-group label {
            display: block;
            font-size: 0.82rem;
            font-weight: 600;
            color: var(--text-secondary);
            margin-bottom: 6px;
            text-transform: uppercase;
            letter-spacing: 0.5px;
        }

        .form-control {
            width: 100%;
            padding: 10px 14px;
            background: var(--bg-primary);
            border: 1px solid var(--border);
            border-radius: var(--radius);
            color: var(--text-primary);
            font-size: 0.9rem;
            font-family: inherit;
            transition: border-color 0.15s;
        }

        .form-control:focus {
            outline: none;
            border-color: var(--primary);
            box-shadow: 0 0 0 3px var(--primary-bg);
        }

        select.form-control {
            appearance: none;
            background-image: url("data:image/svg+xml,%3Csvg xmlns='http://www.w3.org/2000/svg' width='12' height='12' viewBox='0 0 12 12'%3E%3Cpath fill='%236a6a7a' d='M6 8L1 3h10z'/%3E%3C/svg%3E");
            background-repeat: no-repeat;
            background-position: right 12px center;
            padding-right: 36px;
        }

        .form-row {
            display: grid;
            grid-template-columns: 1fr 1fr;
            gap: 16px;
        }

        .alert {
            padding: 12px 16px;
            border-radius: var(--radius);
            margin-bottom: 20px;
            font-size: 0.88rem;
            font-weight: 500;
        }

        .alert-success { background: var(--success-bg); color: var(--success); border: 1px solid rgba(16, 185, 129, 0.2); }
        .alert-error { background: var(--danger-bg); color: var(--danger); border: 1px solid rgba(239, 68, 68, 0.2); }
        .alert-info { background: var(--primary-bg); color: var(--primary); border: 1px solid rgba(99, 102, 241, 0.2); }

        .mono {
            font-family: 'JetBrains Mono', 'Fira Code', 'Consolas', monospace;
            font-size: 0.82rem;
        }

        .text-muted { color: var(--text-muted); }
        .text-right { text-align: right; }
        .mt-2 { margin-top: 8px; }
        .mt-4 { margin-top: 16px; }
        .mb-4 { margin-bottom: 16px; }

        .search-bar {
            display: flex;
            gap: 12px;
            margin-bottom: 20px;
        }

        .search-bar input {
            flex: 1;
        }

        .empty-state {
            text-align: center;
            padding: 48px 24px;
            color: var(--text-muted);
        }

        .empty-state p { margin-top: 8px; font-size: 0.9rem; }

        .license-keys-output {
            background: var(--bg-primary);
            border: 1px solid var(--border);
            border-radius: var(--radius);
            padding: 16px;
            font-family: 'JetBrains Mono', 'Fira Code', monospace;
            font-size: 0.82rem;
            line-height: 1.8;
            white-space: pre-wrap;
            word-break: break-all;
            max-height: 400px;
            overflow-y: auto;
            color: var(--accent);
        }
    </style>
</head>
<body>
<div class="layout">
    <aside class="sidebar">
        <div class="sidebar-brand">
            <h1>Clonada</h1>
            <span>License Server</span>
        </div>
        <ul class="sidebar-nav">
            <li><a href="/admin/" class="<?= $currentPage === 'index' ? 'active' : '' ?>"><span class="nav-icon">&#9632;</span> Dashboard</a></li>
            <li><a href="/admin/licenses.php" class="<?= $currentPage === 'licenses' ? 'active' : '' ?>"><span class="nav-icon">&#9895;</span> Licenses</a></li>
            <li><a href="/admin/generate.php" class="<?= $currentPage === 'generate' ? 'active' : '' ?>"><span class="nav-icon">&#10010;</span> Generate</a></li>
            <li><a href="/admin/activations.php" class="<?= $currentPage === 'activations' ? 'active' : '' ?>"><span class="nav-icon">&#9881;</span> Activations</a></li>
        </ul>
        <div class="sidebar-footer">
            <a href="/admin/?logout=1">Sign Out</a>
        </div>
    </aside>
    <main class="main-content">
