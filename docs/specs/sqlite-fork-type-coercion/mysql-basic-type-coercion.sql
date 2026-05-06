DROP DATABASE IF EXISTS mylite_type_coercion;
CREATE DATABASE mylite_type_coercion CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci;
USE mylite_type_coercion;

CREATE TABLE coercion_basic (
    id INT PRIMARY KEY,
    tiny TINYINT NOT NULL,
    unsigned_id INT UNSIGNED NOT NULL,
    label VARCHAR(4) NOT NULL,
    score DOUBLE NOT NULL,
    optional VARCHAR(4) NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

INSERT INTO coercion_basic VALUES
    (1, '42', '7', 123, '3.25', NULL),
    ('2', -5, 0, 'éé', 4, 'ok');

SELECT 'after-insert', id, tiny, unsigned_id, label, score + 0, optional IS NULL
FROM coercion_basic
ORDER BY id;

UPDATE coercion_basic
SET tiny = '12', unsigned_id = '8', label = 99, score = '6.5'
WHERE id = '2';

SELECT 'after-update', id, tiny, unsigned_id, label, score + 0, optional IS NULL
FROM coercion_basic
ORDER BY id;
