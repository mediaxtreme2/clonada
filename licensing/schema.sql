-- Clonada License Server - Database Schema

CREATE DATABASE IF NOT EXISTS clonada_license CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci;
USE clonada_license;

CREATE TABLE IF NOT EXISTS licenses (
    id INT AUTO_INCREMENT PRIMARY KEY,
    license_key VARCHAR(29) NOT NULL UNIQUE,
    tier ENUM('basic', 'advanced') NOT NULL DEFAULT 'basic',
    email VARCHAR(255) NOT NULL,
    max_activations INT NOT NULL DEFAULT 2,
    activations_count INT NOT NULL DEFAULT 0,
    status ENUM('active', 'revoked', 'expired') NOT NULL DEFAULT 'active',
    created_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,
    updated_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
    expires_at DATETIME DEFAULT NULL,
    notes TEXT DEFAULT NULL,
    INDEX idx_status (status),
    INDEX idx_tier (tier),
    INDEX idx_email (email)
) ENGINE=InnoDB;

CREATE TABLE IF NOT EXISTS activations (
    id INT AUTO_INCREMENT PRIMARY KEY,
    license_id INT NOT NULL,
    hardware_fingerprint VARCHAR(64) NOT NULL,
    machine_name VARCHAR(100) NOT NULL,
    os_info VARCHAR(100) NOT NULL,
    ip_address VARCHAR(45) NOT NULL,
    activated_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,
    last_validated DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,
    is_active BOOLEAN NOT NULL DEFAULT 1,
    FOREIGN KEY (license_id) REFERENCES licenses(id) ON DELETE CASCADE,
    INDEX idx_license_fingerprint (license_id, hardware_fingerprint),
    INDEX idx_active (is_active)
) ENGINE=InnoDB;

CREATE TABLE IF NOT EXISTS admin_users (
    id INT AUTO_INCREMENT PRIMARY KEY,
    username VARCHAR(50) NOT NULL UNIQUE,
    password_hash VARCHAR(255) NOT NULL,
    created_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP
) ENGINE=InnoDB;
