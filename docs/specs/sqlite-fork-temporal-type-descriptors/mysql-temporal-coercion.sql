DROP DATABASE IF EXISTS mylite_temporal_probe;
CREATE DATABASE mylite_temporal_probe CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci;
USE mylite_temporal_probe;

CREATE TABLE temporal_basic (
  id INT PRIMARY KEY,
  d DATE NOT NULL,
  dt DATETIME NOT NULL,
  dt3 DATETIME(3) NOT NULL,
  dt6 DATETIME(6) NOT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

INSERT INTO temporal_basic VALUES
  (1, '2024-02-29', '2024-02-29 12:34:56',
   '2024-02-29 12:34:56.7896', '2024-02-29 12:34:56.1234567'),
  (2, '20240229', '20240229123456', '20240229123456.1', '20240229123456.123'),
  (3, 20240229, 20240229123456, '20240229123456.789', '20240229123456.123456');
SELECT 'after-insert', id, d, CAST(d AS CHAR), dt, CAST(dt AS CHAR),
       dt3, CAST(dt3 AS CHAR), dt6, CAST(dt6 AS CHAR)
FROM temporal_basic
ORDER BY id;

UPDATE temporal_basic
SET d = '2024-03-01',
    dt = '2024-03-01',
    dt3 = '2024-03-01 01:02:03.9999',
    dt6 = '2024-03-01 01:02:03.0000019'
WHERE id = 1;
SELECT 'after-update', id, d, CAST(d AS CHAR), dt, CAST(dt AS CHAR),
       dt3, CAST(dt3 AS CHAR), dt6, CAST(dt6 AS CHAR)
FROM temporal_basic
ORDER BY id;

INSERT INTO temporal_basic VALUES
  (2, '2024-04-05 06:07:08', '2024-04-05 06:07:08.9',
   '2024-04-05 06:07:08.9876', '2024-04-05 06:07:08.987654')
ON DUPLICATE KEY UPDATE
  d = VALUES(d),
  dt = VALUES(dt),
  dt3 = VALUES(dt3),
  dt6 = VALUES(dt6);
SELECT 'after-duplicate-update', id, d, CAST(d AS CHAR), dt, CAST(dt AS CHAR),
       dt3, CAST(dt3 AS CHAR), dt6, CAST(dt6 AS CHAR)
FROM temporal_basic
ORDER BY id;

REPLACE INTO temporal_basic VALUES
  (4, '1999-12-31', '1999-12-31 23:59:59.8',
   '1999-12-31 23:59:59.8765', '1999-12-31 23:59:59.876543');
SELECT 'after-replace', id, d, CAST(d AS CHAR), dt, CAST(dt AS CHAR),
       dt3, CAST(dt3 AS CHAR), dt6, CAST(dt6 AS CHAR)
FROM temporal_basic
ORDER BY id;
