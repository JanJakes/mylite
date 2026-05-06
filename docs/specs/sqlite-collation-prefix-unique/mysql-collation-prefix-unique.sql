DROP DATABASE IF EXISTS mylite_collation_probe;
CREATE DATABASE mylite_collation_probe CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci;
USE mylite_collation_probe;

CREATE TABLE prefix_ci (
  id INT AUTO_INCREMENT PRIMARY KEY,
  slug VARCHAR(20) COLLATE utf8mb4_unicode_ci NOT NULL,
  UNIQUE KEY uq_slug4 (slug(4))
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
INSERT INTO prefix_ci(slug) VALUES ('Post Alpha');
INSERT IGNORE INTO prefix_ci(slug) VALUES ('post Beta');
SELECT 'prefix-ci', ROW_COUNT(), @@warning_count, COUNT(*),
       GROUP_CONCAT(slug ORDER BY id SEPARATOR '|')
FROM prefix_ci;

CREATE TABLE prefix_bin (
  id INT AUTO_INCREMENT PRIMARY KEY,
  slug VARCHAR(20) COLLATE utf8mb4_bin NOT NULL,
  UNIQUE KEY uq_slug4 (slug(4))
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
INSERT INTO prefix_bin(slug) VALUES ('Post Alpha');
INSERT IGNORE INTO prefix_bin(slug) VALUES ('post Beta');
SELECT 'prefix-bin', ROW_COUNT(), @@warning_count, COUNT(*),
       GROUP_CONCAT(slug ORDER BY id SEPARATOR '|')
FROM prefix_bin;

CREATE TABLE pad_space_unique (
  id INT AUTO_INCREMENT PRIMARY KEY,
  value VARCHAR(20) COLLATE utf8mb4_unicode_ci NOT NULL UNIQUE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
INSERT INTO pad_space_unique(value) VALUES ('trail');
INSERT IGNORE INTO pad_space_unique(value) VALUES ('trail ');
SELECT 'pad-space', ROW_COUNT(), @@warning_count, COUNT(*),
       GROUP_CONCAT(CONCAT(value, ':', LENGTH(value)) ORDER BY id SEPARATOR '|')
FROM pad_space_unique;

CREATE TABLE no_pad_unique (
  id INT AUTO_INCREMENT PRIMARY KEY,
  value VARCHAR(20) COLLATE utf8mb4_0900_ai_ci NOT NULL UNIQUE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci;
INSERT INTO no_pad_unique(value) VALUES ('trail');
INSERT IGNORE INTO no_pad_unique(value) VALUES ('trail ');
SELECT 'no-pad', ROW_COUNT(), @@warning_count, COUNT(*),
       GROUP_CONCAT(CONCAT(value, ':', LENGTH(value)) ORDER BY id SEPARATOR '|')
FROM no_pad_unique;
