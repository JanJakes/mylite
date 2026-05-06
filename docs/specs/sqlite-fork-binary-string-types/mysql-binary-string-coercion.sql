DROP DATABASE IF EXISTS mylite_binary_probe;
CREATE DATABASE mylite_binary_probe CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci;
USE mylite_binary_probe;

CREATE TABLE binary_basic (
  id INT PRIMARY KEY,
  fixed BINARY(3) NOT NULL,
  variable VARBINARY(4) NOT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

INSERT INTO binary_basic VALUES (1, 'a', 'é'), (2, 65, 1234);
SELECT 'after-insert', id, HEX(fixed), LENGTH(fixed), HEX(variable), LENGTH(variable)
FROM binary_basic
ORDER BY id;

UPDATE binary_basic SET fixed = 'xy', variable = 'z' WHERE id = 2;
SELECT 'after-update', id, HEX(fixed), LENGTH(fixed), HEX(variable), LENGTH(variable)
FROM binary_basic
ORDER BY id;

INSERT INTO binary_basic VALUES (2, 'uv', 'wx')
ON DUPLICATE KEY UPDATE fixed = VALUES(fixed), variable = VALUES(variable);
SELECT 'after-duplicate-update', id, HEX(fixed), LENGTH(fixed), HEX(variable), LENGTH(variable)
FROM binary_basic
ORDER BY id;

REPLACE INTO binary_basic VALUES (3, 'r', 'st');
SELECT 'after-replace', id, HEX(fixed), LENGTH(fixed), HEX(variable), LENGTH(variable)
FROM binary_basic
ORDER BY id;
