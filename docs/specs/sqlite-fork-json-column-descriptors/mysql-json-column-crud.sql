DROP DATABASE IF EXISTS mylite_json_column_crud;
CREATE DATABASE mylite_json_column_crud CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci;
USE mylite_json_column_crud;

CREATE TABLE wp_options_json_like (
  option_id BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
  option_name VARCHAR(64) NOT NULL,
  option_value JSON NOT NULL,
  autoload VARCHAR(20) NOT NULL DEFAULT 'yes',
  PRIMARY KEY (option_id),
  UNIQUE KEY option_name (option_name)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

INSERT INTO wp_options_json_like (option_name, option_value, autoload) VALUES
  ('site_meta', '{"blog_id":1,"active":true,"tags":["cms","blog"]}', 'yes'),
  ('theme_mods', '{"color":"blue","layout":{"columns":2}}', 'no'),
  ('empty_flags', '[]', 'yes');

UPDATE wp_options_json_like
SET option_value = '{"color":"green","layout":{"columns":3},"enabled":true}'
WHERE option_name = 'theme_mods';

DELETE FROM wp_options_json_like WHERE option_name = 'empty_flags';

SELECT 'after-delete', option_id, option_name, JSON_TYPE(option_value),
       JSON_UNQUOTE(JSON_EXTRACT(option_value, '$.color')),
       JSON_EXTRACT(option_value, '$.layout.columns'),
       JSON_LENGTH(option_value), JSON_VALID(option_value), autoload
FROM wp_options_json_like
ORDER BY option_id;

SELECT 'summary', COUNT(*), SUM(JSON_VALID(option_value)),
       GROUP_CONCAT(option_name ORDER BY option_id SEPARATOR ',')
FROM wp_options_json_like;

INSERT INTO wp_options_json_like(option_name, option_value) VALUES ('bad_text', 'not json');
SHOW WARNINGS;

INSERT INTO wp_options_json_like(option_name, option_value) VALUES ('bad_int', 1);
SHOW WARNINGS;

TRUNCATE TABLE wp_options_json_like;
SELECT 'after-truncate', COUNT(*), COALESCE(MAX(option_id), 0) FROM wp_options_json_like;

INSERT INTO wp_options_json_like(option_name, option_value)
VALUES ('restored', '[{"id":1},{"id":2}]');

SELECT 'after-reinsert', option_id, option_name, JSON_TYPE(option_value),
       JSON_LENGTH(option_value), JSON_EXTRACT(option_value, '$[1].id')
FROM wp_options_json_like;

DROP TABLE wp_options_json_like;

SELECT 'remaining-tables', COUNT(*)
FROM information_schema.tables
WHERE table_schema = DATABASE()
  AND table_name = 'wp_options_json_like';
