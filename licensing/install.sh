#!/bin/bash
# Clonada License Server - Deployment Script
set -e

echo "=== Clonada License Server Deployment ==="
echo ""

DEPLOY_DIR="/var/www/clonada-license"
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"

# 1. Create MariaDB database and user
echo "[1/7] Setting up MariaDB database..."
mysql -u root <<'EOSQL'
CREATE DATABASE IF NOT EXISTS clonada_license CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci;
CREATE USER IF NOT EXISTS 'clonada'@'127.0.0.1' IDENTIFIED BY 'Cl0nada_Lic2026!';
CREATE USER IF NOT EXISTS 'clonada'@'localhost' IDENTIFIED BY 'Cl0nada_Lic2026!';
GRANT ALL PRIVILEGES ON clonada_license.* TO 'clonada'@'127.0.0.1';
GRANT ALL PRIVILEGES ON clonada_license.* TO 'clonada'@'localhost';
FLUSH PRIVILEGES;
EOSQL
echo "  Database and user created."

# 2. Import schema
echo "[2/7] Importing schema..."
mysql -u root clonada_license < "$SCRIPT_DIR/schema.sql" 2>/dev/null || true
echo "  Schema imported."

# 3. Insert default admin user
echo "[3/7] Creating default admin user..."
ADMIN_HASH=$(php -r "echo password_hash('Clonada_Admin2026!', PASSWORD_BCRYPT);")
mysql -u root clonada_license <<EOSQL
INSERT IGNORE INTO admin_users (username, password_hash, created_at)
VALUES ('admin', '$ADMIN_HASH', NOW());
EOSQL
echo "  Admin user created (admin / Clonada_Admin2026!)"

# 4. Copy files to deployment directory
echo "[4/7] Deploying files to $DEPLOY_DIR..."
rm -rf "$DEPLOY_DIR"
mkdir -p "$DEPLOY_DIR/admin"

cp "$SCRIPT_DIR/config.php" "$DEPLOY_DIR/"
cp "$SCRIPT_DIR/api.php" "$DEPLOY_DIR/"
cp "$SCRIPT_DIR/admin/"*.php "$DEPLOY_DIR/admin/"

echo "  Files deployed."

# 5. Set permissions
echo "[5/7] Setting permissions..."
chown -R www-data:www-data "$DEPLOY_DIR"
chmod -R 755 "$DEPLOY_DIR"
chmod 640 "$DEPLOY_DIR/config.php"
chown www-data:www-data "$DEPLOY_DIR/config.php"

# Create rate limit directory
mkdir -p /tmp/clonada_rate_limit
chmod 777 /tmp/clonada_rate_limit

echo "  Permissions set."

# 6. Create Nginx server block
echo "[6/7] Configuring Nginx..."
cat > /etc/nginx/sites-available/clonada-license <<'NGINX'
server {
    listen 80 default_server;
    listen [::]:80 default_server;
    server_name _;

    root /var/www/clonada-license;
    index index.php;

    # Security headers
    add_header X-Content-Type-Options "nosniff" always;
    add_header X-Frame-Options "SAMEORIGIN" always;
    add_header X-XSS-Protection "1; mode=block" always;
    add_header Referrer-Policy "strict-origin-when-cross-origin" always;

    # API routing
    location /api/ {
        try_files $uri /api.php$is_args$args;
    }

    location /api.php {
        include fastcgi_params;
        fastcgi_pass unix:/run/php/php8.3-fpm.sock;
        fastcgi_param SCRIPT_FILENAME $document_root/api.php;
        fastcgi_param REQUEST_URI $request_uri;
    }

    # Admin panel
    location /admin/ {
        try_files $uri $uri/ /admin/index.php$is_args$args;
    }

    location ~ ^/admin/.*\.php$ {
        include fastcgi_params;
        fastcgi_pass unix:/run/php/php8.3-fpm.sock;
        fastcgi_param SCRIPT_FILENAME $document_root$fastcgi_script_name;
    }

    # Block access to config
    location = /config.php {
        deny all;
        return 404;
    }

    # Block dotfiles
    location ~ /\. {
        deny all;
        return 404;
    }

    # Default PHP handling
    location ~ \.php$ {
        include fastcgi_params;
        fastcgi_pass unix:/run/php/php8.3-fpm.sock;
        fastcgi_param SCRIPT_FILENAME $document_root$fastcgi_script_name;
    }
}
NGINX

# Remove default site if exists
rm -f /etc/nginx/sites-enabled/default

# Enable our site
ln -sf /etc/nginx/sites-available/clonada-license /etc/nginx/sites-enabled/clonada-license

echo "  Nginx configured."

# 7. Test and restart Nginx
echo "[7/7] Restarting services..."
nginx -t
systemctl restart php8.3-fpm
systemctl restart nginx

echo ""
echo "=== Deployment Complete ==="
echo ""
echo "Admin Panel: http://155.133.27.205/admin/"
echo "API Base:    http://155.133.27.205/api/"
echo ""
echo "Admin Login: admin / Clonada_Admin2026!"
echo ""
