DROP DATABASE IF EXISTS mylite_year_coercion;
CREATE DATABASE mylite_year_coercion CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci;
USE mylite_year_coercion;
SET NAMES utf8mb4;

CREATE TABLE year_basic (
  id INT PRIMARY KEY,
  y YEAR NOT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

INSERT INTO year_basic VALUES
  (1, 0),
  (2, '0'),
  (3, '00'),
  (4, '0000'),
  (5, 1),
  (6, '1'),
  (7, 69),
  (8, '69'),
  (9, 70),
  (10, '70'),
  (11, 99),
  (12, '99'),
  (13, 1901),
  (14, '1901'),
  (15, 2155),
  (16, '2155'),
  (17, 1901.4),
  (18, 1901.5);
SELECT 'after-insert', id, y, y + 0, CAST(y AS CHAR)
FROM year_basic ORDER BY id;

UPDATE year_basic SET y = '2' WHERE id = 1;
UPDATE year_basic SET y = 98 WHERE id = 2;
SELECT 'after-update', id, y, y + 0, CAST(y AS CHAR)
FROM year_basic ORDER BY id;

INSERT INTO year_basic VALUES (2, '2012')
ON DUPLICATE KEY UPDATE y = VALUES(y);
SELECT 'after-duplicate-update', id, y, y + 0, CAST(y AS CHAR)
FROM year_basic ORDER BY id;

REPLACE INTO year_basic VALUES (19, '68');
SELECT 'after-replace', id, y, y + 0, CAST(y AS CHAR)
FROM year_basic ORDER BY id;

INSERT INTO year_basic VALUES (20, 100);
SELECT 'after-100', COUNT(*) FROM year_basic;
INSERT INTO year_basic VALUES (20, 1900);
SELECT 'after-1900', COUNT(*) FROM year_basic;
INSERT INTO year_basic VALUES (20, 2156);
SELECT 'after-2156', COUNT(*) FROM year_basic;
INSERT INTO year_basic VALUES (20, 'bad');
SELECT 'after-bad', COUNT(*) FROM year_basic;
INSERT INTO year_basic VALUES (20, '');
SELECT 'after-empty', COUNT(*) FROM year_basic;
