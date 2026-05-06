DROP DATABASE IF EXISTS mylite_enum_coercion;
CREATE DATABASE mylite_enum_coercion CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci;
USE mylite_enum_coercion;
SET NAMES utf8mb4;

CREATE TABLE enum_basic (
  id INT PRIMARY KEY,
  status ENUM('draft','published','archived') NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

INSERT INTO enum_basic VALUES (1, 'draft');
INSERT INTO enum_basic VALUES (2, 2);
INSERT INTO enum_basic VALUES (3, '2');
INSERT INTO enum_basic VALUES (4, 3.9);
INSERT INTO enum_basic VALUES (5, NULL);
SELECT 'after-insert', id, status, status + 0, CAST(status AS CHAR), status IS NULL
FROM enum_basic ORDER BY id;

UPDATE enum_basic SET status = 'archived' WHERE id = 1;
UPDATE enum_basic SET status = '+2' WHERE id = 2;
UPDATE enum_basic SET status = '02' WHERE id = 3;
SELECT 'after-update', id, status, status + 0, CAST(status AS CHAR)
FROM enum_basic ORDER BY id;

INSERT INTO enum_basic VALUES (2, 'draft')
ON DUPLICATE KEY UPDATE status = VALUES(status);
SELECT 'after-duplicate-update', id, status, status + 0, CAST(status AS CHAR)
FROM enum_basic ORDER BY id;

REPLACE INTO enum_basic VALUES (6, 1.9);
SELECT 'after-replace', id, status, status + 0, CAST(status AS CHAR)
FROM enum_basic ORDER BY id;

CREATE TABLE enum_numeric (
  id INT PRIMARY KEY,
  value ENUM('0','1','2') NOT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
INSERT INTO enum_numeric VALUES (1, 2), (2, '2'), (3, '3'), (4, 1), (5, '1'), (6, '0');
SELECT 'numeric-labels', id, value, value + 0, CAST(value AS CHAR)
FROM enum_numeric ORDER BY id;

CREATE TABLE enum_empty (
  id INT PRIMARY KEY,
  value ENUM('', 'a') NOT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
INSERT INTO enum_empty VALUES (1, ''), (2, 1), (3, '1');
SELECT 'empty-label', id, value, value + 0, CAST(value AS CHAR)
FROM enum_empty ORDER BY id;

INSERT INTO enum_basic VALUES (7, 0);
SELECT 'after-zero-index', COUNT(*) FROM enum_basic;
INSERT INTO enum_basic VALUES (7, 'missing');
SELECT 'after-missing-label', COUNT(*) FROM enum_basic;
INSERT INTO enum_basic VALUES (7, '2.9');
SELECT 'after-decimal-text', COUNT(*) FROM enum_basic;
INSERT INTO enum_basic VALUES (7, '2x');
SELECT 'after-junk-text', COUNT(*) FROM enum_basic;
