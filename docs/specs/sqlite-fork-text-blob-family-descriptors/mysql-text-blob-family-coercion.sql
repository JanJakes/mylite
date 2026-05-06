DROP DATABASE IF EXISTS mylite_text_blob_family_coercion;
CREATE DATABASE mylite_text_blob_family_coercion CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci;
USE mylite_text_blob_family_coercion;
SET NAMES utf8mb4;

CREATE TABLE text_blob_basic (
  id INT PRIMARY KEY,
  tiny_text TINYTEXT NOT NULL,
  text_value TEXT NOT NULL,
  tiny_blob TINYBLOB NOT NULL,
  blob_value BLOB NOT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

INSERT INTO text_blob_basic VALUES
  (1, REPEAT('a', 255), 'alpha', REPEAT('b', 255), 'beta'),
  (2, REPEAT('é', 127), REPEAT('é', 100), REPEAT('é', 127), REPEAT('é', 100)),
  (3, 65, 123456, 65, 123456),
  (4, 'short', 'medium', X'00FF', X'414243');
SELECT 'after-insert', id,
  LENGTH(tiny_text), CHAR_LENGTH(tiny_text), LENGTH(text_value), CHAR_LENGTH(text_value),
  LENGTH(tiny_blob), LEFT(HEX(tiny_blob), 8), LENGTH(blob_value), LEFT(HEX(blob_value), 8)
FROM text_blob_basic ORDER BY id;

UPDATE text_blob_basic
SET tiny_text = REPEAT('z', 254), text_value = REPEAT('w', 300),
    tiny_blob = REPEAT('q', 254), blob_value = REPEAT('r', 300)
WHERE id = 1;
SELECT 'after-update', id,
  LENGTH(tiny_text), CHAR_LENGTH(tiny_text), LENGTH(text_value), CHAR_LENGTH(text_value),
  LENGTH(tiny_blob), LEFT(HEX(tiny_blob), 8), LENGTH(blob_value), LEFT(HEX(blob_value), 8)
FROM text_blob_basic ORDER BY id;

INSERT INTO text_blob_basic VALUES (2, 'dup', 'duptext', 'uv', 'wx')
ON DUPLICATE KEY UPDATE
  tiny_text = VALUES(tiny_text), text_value = VALUES(text_value),
  tiny_blob = VALUES(tiny_blob), blob_value = VALUES(blob_value);
SELECT 'after-duplicate-update', id,
  LENGTH(tiny_text), CHAR_LENGTH(tiny_text), LENGTH(text_value), CHAR_LENGTH(text_value),
  LENGTH(tiny_blob), LEFT(HEX(tiny_blob), 8), LENGTH(blob_value), LEFT(HEX(blob_value), 8)
FROM text_blob_basic ORDER BY id;

REPLACE INTO text_blob_basic VALUES
  (5, REPEAT('s', 255), REPEAT('t', 512), REPEAT('u', 255), REPEAT('v', 512));
SELECT 'after-replace', id,
  LENGTH(tiny_text), CHAR_LENGTH(tiny_text), LENGTH(text_value), CHAR_LENGTH(text_value),
  LENGTH(tiny_blob), LEFT(HEX(tiny_blob), 8), LENGTH(blob_value), LEFT(HEX(blob_value), 8)
FROM text_blob_basic ORDER BY id;

INSERT INTO text_blob_basic VALUES (6, REPEAT('a', 256), 'ok', 'ok', 'ok');
SELECT 'after-too-long-tinytext', COUNT(*) FROM text_blob_basic;
INSERT INTO text_blob_basic VALUES (6, REPEAT('é', 128), 'ok', 'ok', 'ok');
SELECT 'after-too-long-tinytext-multibyte', COUNT(*) FROM text_blob_basic;
INSERT INTO text_blob_basic VALUES (6, 'ok', 'ok', REPEAT('b', 256), 'ok');
SELECT 'after-too-long-tinyblob', COUNT(*) FROM text_blob_basic;
