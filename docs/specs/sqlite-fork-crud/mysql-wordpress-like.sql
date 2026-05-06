DROP DATABASE IF EXISTS mylite_fork_crud;
CREATE DATABASE mylite_fork_crud CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci;
USE mylite_fork_crud;

CREATE TABLE wp_posts_like (
  ID BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
  post_author BIGINT UNSIGNED NOT NULL DEFAULT 0,
  post_date DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,
  post_title TEXT NOT NULL,
  post_name VARCHAR(200) NOT NULL DEFAULT '',
  post_status VARCHAR(20) NOT NULL DEFAULT 'publish',
  comment_count BIGINT NOT NULL DEFAULT 0,
  PRIMARY KEY (ID),
  KEY post_name (post_name),
  KEY post_status_date (post_status, post_date)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

CREATE TABLE wp_postmeta_like (
  meta_id BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
  post_id BIGINT UNSIGNED NOT NULL DEFAULT 0,
  meta_key VARCHAR(255) DEFAULT NULL,
  meta_value LONGTEXT,
  PRIMARY KEY (meta_id),
  KEY post_id (post_id),
  KEY meta_key (meta_key)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

INSERT INTO wp_posts_like
  (post_author, post_date, post_title, post_name, post_status, comment_count)
VALUES
  (1, '2026-05-06 09:15:00', 'Hello MyLite', 'hello-mylite', 'publish', 2),
  (2, '2026-05-06 10:00:00', 'Draft Notes', 'draft-notes', 'draft', 0),
  (1, '2026-05-07 08:30:00', 'SQLite Fork Plan', 'sqlite-fork-plan', 'publish', 1);

INSERT INTO wp_postmeta_like (post_id, meta_key, meta_value) VALUES
  (1, '_edit_lock', '1714994100:1'),
  (1, '_thumbnail_id', '99'),
  (3, '_wp_page_template', 'default');

UPDATE wp_posts_like
SET post_status = 'publish', comment_count = comment_count + 1
WHERE post_name = 'draft-notes';

DELETE FROM wp_postmeta_like WHERE meta_key = '_edit_lock';

SELECT 'posts-before-truncate', COUNT(*), MIN(ID), MAX(ID), SUM(comment_count)
FROM wp_posts_like;

SELECT 'published', ID, post_name, post_status, comment_count
FROM wp_posts_like
WHERE post_status = 'publish'
ORDER BY ID;

SELECT
  'meta-before-truncate',
  COUNT(*),
  GROUP_CONCAT(CONCAT(post_id, ':', meta_key, '=', meta_value)
               ORDER BY meta_id SEPARATOR '|')
FROM wp_postmeta_like;

TRUNCATE TABLE wp_postmeta_like;

SELECT 'meta-after-truncate', COUNT(*), COALESCE(MAX(meta_id), 0)
FROM wp_postmeta_like;

INSERT INTO wp_postmeta_like (post_id, meta_key, meta_value)
VALUES (2, '_restored', 'yes');

SELECT 'meta-after-reinsert', meta_id, post_id, meta_key, meta_value
FROM wp_postmeta_like;

DROP TABLE wp_postmeta_like;

SELECT 'remaining-tables', COUNT(*)
FROM information_schema.tables
WHERE table_schema = DATABASE()
  AND table_name IN ('wp_posts_like', 'wp_postmeta_like');

DROP DATABASE mylite_fork_crud;
