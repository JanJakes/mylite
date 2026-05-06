DROP DATABASE IF EXISTS mylite_enum_set_sql_integration;
CREATE DATABASE mylite_enum_set_sql_integration CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci;
USE mylite_enum_set_sql_integration;

CREATE TABLE wp_like_posts (
  id BIGINT UNSIGNED NOT NULL PRIMARY KEY,
  post_status ENUM('draft','publish','trash') NOT NULL,
  feature_flags SET('sticky','featured','private') NULL,
  review_state ENUM('','0','1','approved') CHARACTER SET latin1 COLLATE latin1_bin NULL
);

SELECT COLUMN_NAME, DATA_TYPE, COLUMN_TYPE, CHARACTER_MAXIMUM_LENGTH,
       CHARACTER_OCTET_LENGTH, CHARACTER_SET_NAME, COLLATION_NAME
FROM information_schema.COLUMNS
WHERE TABLE_SCHEMA = 'mylite_enum_set_sql_integration'
  AND TABLE_NAME = 'wp_like_posts'
ORDER BY ORDINAL_POSITION;

SHOW COLUMNS FROM wp_like_posts;

INSERT INTO wp_like_posts VALUES
  (1, 'draft', 'sticky,featured', ''),
  (2, 2, 5, '0'),
  (3, '3', 'private,sticky', 3);

SELECT id, post_status, post_status + 0, feature_flags, feature_flags + 0,
       review_state, review_state + 0
FROM wp_like_posts ORDER BY id;

UPDATE wp_like_posts
SET post_status = 'trash', feature_flags = 'featured,sticky', review_state = 4
WHERE id = 1;

INSERT INTO wp_like_posts SET id = 4, post_status = '+2',
    feature_flags = '+3', review_state = '1';

INSERT INTO wp_like_posts VALUES (2, 'draft', 'sticky', '')
ON DUPLICATE KEY UPDATE post_status = 'publish', feature_flags = 'private';

REPLACE INTO wp_like_posts VALUES (5, 1.9, 6.9, 'approved');

SELECT id, post_status, post_status + 0, feature_flags, feature_flags + 0,
       review_state, review_state + 0
FROM wp_like_posts ORDER BY id;

DELETE FROM wp_like_posts WHERE feature_flags = 'featured,private';
SELECT 'after-delete', COUNT(*) FROM wp_like_posts;

TRUNCATE TABLE wp_like_posts;
SELECT 'after-truncate', COUNT(*) FROM wp_like_posts;

DROP TABLE wp_like_posts;
SHOW TABLES;
