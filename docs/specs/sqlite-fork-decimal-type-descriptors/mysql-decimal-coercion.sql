DROP DATABASE IF EXISTS mylite_decimal_probe;
CREATE DATABASE mylite_decimal_probe CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci;
USE mylite_decimal_probe;

CREATE TABLE decimal_basic (
  id INT PRIMARY KEY,
  amount DECIMAL(5,2) NOT NULL,
  whole DECIMAL(4,0) NOT NULL,
  unsigned_amount DECIMAL(5,2) UNSIGNED NOT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

INSERT INTO decimal_basic VALUES
  (1, 1.234, 12.5, 0),
  (2, -1.235, -12.5, 9.995),
  (3, '001.2', '9.5', '99.999');
SELECT 'after-insert', id, amount, CAST(amount AS CHAR), whole, CAST(whole AS CHAR),
       unsigned_amount, CAST(unsigned_amount AS CHAR)
FROM decimal_basic
ORDER BY id;

UPDATE decimal_basic SET amount = 999.994, whole = -0.5, unsigned_amount = 0.004
WHERE id = 1;
SELECT 'after-update', id, amount, CAST(amount AS CHAR), whole, CAST(whole AS CHAR),
       unsigned_amount, CAST(unsigned_amount AS CHAR)
FROM decimal_basic
ORDER BY id;

INSERT INTO decimal_basic VALUES (2, 2.225, 1.5, 1.555)
ON DUPLICATE KEY UPDATE
  amount = VALUES(amount),
  whole = VALUES(whole),
  unsigned_amount = VALUES(unsigned_amount);
SELECT 'after-duplicate-update', id, amount, CAST(amount AS CHAR), whole, CAST(whole AS CHAR),
       unsigned_amount, CAST(unsigned_amount AS CHAR)
FROM decimal_basic
ORDER BY id;

REPLACE INTO decimal_basic VALUES (4, 3.335, -2.5, 2.225);
SELECT 'after-replace', id, amount, CAST(amount AS CHAR), whole, CAST(whole AS CHAR),
       unsigned_amount, CAST(unsigned_amount AS CHAR)
FROM decimal_basic
ORDER BY id;
