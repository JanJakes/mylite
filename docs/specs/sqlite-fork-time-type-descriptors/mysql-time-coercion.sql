DROP DATABASE IF EXISTS mylite_time_probe;
CREATE DATABASE mylite_time_probe CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci;
USE mylite_time_probe;

CREATE TABLE time_basic (
  id INT PRIMARY KEY,
  t TIME NOT NULL,
  t3 TIME(3) NOT NULL,
  t6 TIME(6) NOT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

INSERT INTO time_basic VALUES
  (1, '12:34:56', '12:34:56.7896', '12:34:56.1234567'),
  (2, '-12:34:56', '-12:34:56.7896', '-12:34:56.1234567'),
  (3, '1 02:03:04', '1 02:03:04.5678', '1 02:03:04.123456'),
  (4, 123456, 123456.789, 123456.123456),
  (5, 1234, 1234.5, 1234.123456),
  (6, 12, 12.9, 12.123456),
  (7, '34:56', '34:56.7894', '34:56.789456'),
  (8, ':12', '1:2:3.4567', '-00:00:00.0000004'),
  (9, '838:59:58.5', '838:59:58.9995', '838:59:58.9999995');
SELECT 'after-insert', id, t, CAST(t AS CHAR), t3, CAST(t3 AS CHAR), t6, CAST(t6 AS CHAR)
FROM time_basic ORDER BY id;

UPDATE time_basic
SET t = '23:59:59.9', t3 = '-23:59:59.9994', t6 = '838:59:58.999999'
WHERE id = 1;
SELECT 'after-update', id, t, CAST(t AS CHAR), t3, CAST(t3 AS CHAR), t6, CAST(t6 AS CHAR)
FROM time_basic ORDER BY id;

INSERT INTO time_basic VALUES
  (2, '00:00:00.5', '12:34.9876', '-838:59:58.9999995')
ON DUPLICATE KEY UPDATE
  t = VALUES(t),
  t3 = VALUES(t3),
  t6 = VALUES(t6);
SELECT 'after-duplicate-update', id, t, CAST(t AS CHAR), t3, CAST(t3 AS CHAR), t6, CAST(t6 AS CHAR)
FROM time_basic ORDER BY id;

REPLACE INTO time_basic VALUES
  (10, '-838:59:58.5', '-00:00:00.0004', '123456.9999995');
SELECT 'after-replace', id, t, CAST(t AS CHAR), t3, CAST(t3 AS CHAR), t6, CAST(t6 AS CHAR)
FROM time_basic ORDER BY id;
