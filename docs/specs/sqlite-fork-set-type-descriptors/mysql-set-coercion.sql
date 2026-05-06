DROP DATABASE IF EXISTS mylite_set_coercion;
CREATE DATABASE mylite_set_coercion CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci;
USE mylite_set_coercion;
SET NAMES utf8mb4;

CREATE TABLE set_basic (
  id INT PRIMARY KEY,
  flags SET('a','b','c','d') NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

INSERT INTO set_basic VALUES (1, 'a');
INSERT INTO set_basic VALUES (2, 'a,b');
INSERT INTO set_basic VALUES (3, 'b,a');
INSERT INTO set_basic VALUES (4, 'a,a');
INSERT INTO set_basic VALUES (5, '');
INSERT INTO set_basic VALUES (6, 9);
INSERT INTO set_basic VALUES (7, 3.9);
INSERT INTO set_basic VALUES (8, NULL);
INSERT INTO set_basic VALUES (9, '3');
INSERT INTO set_basic VALUES (10, '+3');
INSERT INTO set_basic VALUES (11, '03');
INSERT INTO set_basic VALUES (12, ' 3');
SELECT 'after-insert', id, flags, flags + 0, CAST(flags AS CHAR), flags IS NULL
FROM set_basic ORDER BY id;

UPDATE set_basic SET flags = 'd,a,d' WHERE id = 1;
UPDATE set_basic SET flags = 15 WHERE id = 2;
SELECT 'after-update', id, flags, flags + 0, CAST(flags AS CHAR)
FROM set_basic WHERE id IN (1, 2) ORDER BY id;

INSERT INTO set_basic VALUES (2, 'a')
ON DUPLICATE KEY UPDATE flags = VALUES(flags);
SELECT 'after-duplicate-update', id, flags, flags + 0, CAST(flags AS CHAR)
FROM set_basic WHERE id IN (1, 2) ORDER BY id;

REPLACE INTO set_basic VALUES (13, 1.9);
SELECT 'after-replace', id, flags, flags + 0, CAST(flags AS CHAR)
FROM set_basic WHERE id IN (1, 2, 13) ORDER BY id;

CREATE TABLE set_numeric (
  id INT PRIMARY KEY,
  flags SET('0','1','2') NOT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
INSERT INTO set_numeric VALUES
  (1, 2), (2, '2'), (3, '3'), (4, 1),
  (5, '1'), (6, '0'), (7, '0,2'), (8, '2,0');
SELECT 'numeric-labels', id, flags, flags + 0, CAST(flags AS CHAR)
FROM set_numeric ORDER BY id;

CREATE TABLE set_empty (
  id INT PRIMARY KEY,
  flags SET('', 'a') NOT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
INSERT INTO set_empty VALUES (1, ''), (2, 1), (3, '1'), (4, 'a'), (5, 2), (6, 3);
SELECT 'empty-label', id, flags, flags + 0, CAST(flags AS CHAR)
FROM set_empty ORDER BY id;

CREATE TABLE set_wide (
  id INT PRIMARY KEY,
  flags SET(
    'v1','v2','v3','v4','v5','v6','v7','v8',
    'v9','v10','v11','v12','v13','v14','v15','v16',
    'v17','v18','v19','v20','v21','v22','v23','v24',
    'v25','v26','v27','v28','v29','v30','v31','v32',
    'v33','v34','v35','v36','v37','v38','v39','v40',
    'v41','v42','v43','v44','v45','v46','v47','v48',
    'v49','v50','v51','v52','v53','v54','v55','v56',
    'v57','v58','v59','v60','v61','v62','v63','v64'
  ) NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
INSERT INTO set_wide VALUES (1, '9223372036854775808');
SELECT 'wide-mask', id, flags, CAST(flags AS CHAR), flags IS NULL
FROM set_wide ORDER BY id;

INSERT INTO set_basic VALUES (20, 16);
SELECT 'after-wide-mask', COUNT(*) FROM set_basic;
INSERT INTO set_basic VALUES (21, 'missing');
SELECT 'after-missing-label', COUNT(*) FROM set_basic;
INSERT INTO set_basic VALUES (22, 'a,missing,b');
SELECT 'after-mixed-label', COUNT(*) FROM set_basic;
INSERT INTO set_basic VALUES (23, '3 ');
SELECT 'after-trailing-space', COUNT(*) FROM set_basic;
INSERT INTO set_basic VALUES (24, '3.9');
SELECT 'after-decimal-text', COUNT(*) FROM set_basic;
