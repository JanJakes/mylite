-- common selects
SELECT 1;
SELECT mylite_id, mylite_title FROM mylite_posts WHERE mylite_status = 'publish';
SELECT DISTINCT mylite_author_id AS author_id FROM mylite_posts ORDER BY author_id DESC LIMIT 10 OFFSET 5;
SELECT SQL_CALC_FOUND_ROWS * FROM mylite_posts WHERE mylite_title LIKE 'release\_%' ESCAPE '\\';
SELECT HIGH_PRIORITY STRAIGHT_JOIN p.mylite_id, m.mylite_key FROM mylite_posts AS p INNER JOIN mylite_postmeta AS m ON m.mylite_post_id = p.mylite_id;
SELECT p.*, mylite_schema.* FROM mylite_posts AS p CROSS JOIN mylite_schema;
SELECT mylite_id FROM mylite_posts WHERE mylite_id <=> NULL OR mylite_slug REGEXP '^[a-z0-9-]+$';
SELECT mylite_json->'$.title' AS json_title, mylite_json->>'$.slug' AS json_slug FROM mylite_documents;
SELECT !mylite_deleted, ~mylite_flags, mylite_flags & 7, mylite_flags | 8, mylite_flags ^ 3 FROM mylite_flags;
SELECT (mylite_score << 1) + (mylite_score >> 2) AS shifted_score FROM mylite_scores;
SELECT @mylite_seen := @mylite_seen + 1 AS sequence_number FROM mylite_posts;
SELECT @@session.sql_mode, @@global.time_zone, @@version_comment;
SELECT @@`default`.key_buffer_size, @@`default`.key_cache_block_size,
       @@`default`.key_cache_division_limit, @@`default`.key_cache_age_threshold;

-- literals and identifiers
SELECT 'single quoted', "double quoted string", 'two '' quotes', "two "" quotes";
SELECT 'backslash\nnewline', 'quote\'escape', 'nul\0marker', 'wildcard\%literal', 'under\_score';
SELECT N'national text', n'national lower', _utf8mb4'utf8 text' COLLATE utf8mb4_0900_ai_ci;
SELECT X'4D794C697465', x'00ff', 0x4d7953716c, _latin1 X'4D79';
SELECT b'101010', B'0101', 0b1101, b'' + 0;
SELECT 0, 42, 3.1415, .75, 3., 1e3, 1e+3, 1e-3, 2.34E0;
SELECT 1e + 3 AS identifier_like_number, 0XCAFE AS identifier_like_hex, 0B101 AS identifier_like_bits;
SELECT `select`, `mylite spaced name`, `mylite``tick`, mylite$inner, $mylite_leading;
SELECT @plain_user_var, @'dash-var', @"double-dash-var", @`tick-var`;

-- comments and hints
SELECT 1 # hash line comment
;
SELECT 1 -- dash line comment
;
SELECT 1 /* ordinary block comment */ + 2;
SELECT /*!80409 STRAIGHT_JOIN */ mylite_id FROM mylite_posts;
SELECT /*+ MAX_EXECUTION_TIME(1000) */ mylite_id FROM mylite_posts WHERE mylite_status = 'publish';
CREATE TABLE mylite_versioned_options (mylite_id BIGINT PRIMARY KEY) /*!80409 ENGINE=InnoDB */;

-- data definition
CREATE DATABASE IF NOT EXISTS mylite_app DEFAULT CHARACTER SET utf8mb4 COLLATE utf8mb4_0900_ai_ci;
CREATE TEMPORARY TABLE mylite_temp_ids (mylite_id BIGINT UNSIGNED NOT NULL PRIMARY KEY);
CREATE TABLE mylite_posts (
    mylite_id BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
    mylite_author_id BIGINT UNSIGNED NOT NULL DEFAULT 0,
    mylite_title VARCHAR(255) NOT NULL DEFAULT '',
    mylite_slug VARCHAR(200) NOT NULL,
    mylite_status ENUM('draft','publish','private') NOT NULL DEFAULT 'draft',
    mylite_flags SET('featured','sticky','archived') NOT NULL DEFAULT '',
    mylite_payload JSON,
    mylite_created_at DATETIME(6) NOT NULL DEFAULT CURRENT_TIMESTAMP(6),
    mylite_updated_at TIMESTAMP(6) NULL DEFAULT NULL ON UPDATE CURRENT_TIMESTAMP(6),
    mylite_visible TINYINT(1) NOT NULL DEFAULT 1,
    PRIMARY KEY (mylite_id),
    UNIQUE KEY mylite_slug_unique (mylite_slug),
    KEY mylite_author_status (mylite_author_id, mylite_status),
    FULLTEXT KEY mylite_title_search (mylite_title),
    CHECK (mylite_visible IN (0, 1))
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci COMMENT='mylite posts';
CREATE TABLE mylite_posts_copy LIKE mylite_posts;
CREATE TABLE mylite_recent_posts AS SELECT mylite_id, mylite_title FROM mylite_posts WHERE mylite_created_at >= CURRENT_DATE - INTERVAL 30 DAY;
CREATE INDEX mylite_posts_created_idx ON mylite_posts (mylite_created_at DESC) VISIBLE;
CREATE UNIQUE INDEX mylite_posts_author_slug_idx USING BTREE ON mylite_posts (mylite_author_id, mylite_slug);
ALTER TABLE mylite_posts ADD COLUMN mylite_excerpt TEXT NULL AFTER mylite_title;
ALTER TABLE mylite_posts MODIFY COLUMN mylite_title VARCHAR(320) NOT NULL;
ALTER TABLE mylite_posts RENAME COLUMN mylite_visible TO mylite_is_visible;
ALTER TABLE mylite_posts ALTER INDEX mylite_posts_created_idx INVISIBLE;
ALTER TABLE mylite_posts DROP CHECK mylite_posts_chk_1;
RENAME TABLE mylite_posts_copy TO mylite_posts_archive, mylite_recent_posts TO mylite_posts_recent;
TRUNCATE TABLE mylite_temp_ids;
DROP INDEX mylite_posts_author_slug_idx ON mylite_posts;
DROP TABLE IF EXISTS mylite_posts_archive, mylite_posts_recent;
DROP DATABASE IF EXISTS mylite_app;

-- views, routines, triggers, and events
CREATE OR REPLACE VIEW mylite_public_posts AS SELECT mylite_id, mylite_title FROM mylite_posts WHERE mylite_status = 'publish';
ALTER ALGORITHM=MERGE DEFINER=CURRENT_USER SQL SECURITY INVOKER VIEW mylite_public_posts AS SELECT mylite_id FROM mylite_posts;
CREATE TRIGGER mylite_posts_bi BEFORE INSERT ON mylite_posts FOR EACH ROW SET NEW.mylite_created_at = COALESCE(NEW.mylite_created_at, NOW(6));
CREATE FUNCTION mylite_identity(value BIGINT) RETURNS BIGINT DETERMINISTIC RETURN value;
CREATE PROCEDURE mylite_touch_post(IN post_id BIGINT) UPDATE mylite_posts SET mylite_updated_at = CURRENT_TIMESTAMP(6) WHERE mylite_id = post_id;
CREATE EVENT mylite_purge_drafts ON SCHEDULE EVERY 1 DAY DO DELETE FROM mylite_posts WHERE mylite_status = 'draft';
DROP TRIGGER IF EXISTS mylite_posts_bi;
DROP FUNCTION IF EXISTS mylite_identity;
DROP PROCEDURE IF EXISTS mylite_touch_post;
DROP EVENT IF EXISTS mylite_purge_drafts;
DROP VIEW IF EXISTS mylite_public_posts;

-- data manipulation
INSERT INTO mylite_posts (mylite_author_id, mylite_title, mylite_slug, mylite_status) VALUES
    (1, 'Hello MyLite', 'hello-mylite', 'publish'),
    (2, 'Draft note', 'draft-note', 'draft');
INSERT LOW_PRIORITY IGNORE INTO mylite_posts SET mylite_title = 'set form', mylite_slug = 'set-form', mylite_status = DEFAULT;
INSERT INTO mylite_posts (mylite_author_id, mylite_title, mylite_slug)
SELECT mylite_id, CONCAT('Author ', mylite_id), CONCAT('author-', mylite_id) FROM mylite_authors
ON DUPLICATE KEY UPDATE mylite_title = VALUES(mylite_title), mylite_updated_at = NOW();
REPLACE INTO mylite_options (mylite_name, mylite_value) VALUES ('siteurl', 'https://example.test');
UPDATE LOW_PRIORITY IGNORE mylite_posts SET mylite_status = 'private', mylite_flags = mylite_flags | 'archived' WHERE mylite_id IN (1, 2) ORDER BY mylite_id LIMIT 2;
UPDATE mylite_posts AS p JOIN mylite_authors AS a ON a.mylite_id = p.mylite_author_id SET p.mylite_title = CONCAT(a.mylite_name, ': ', p.mylite_title);
DELETE QUICK FROM mylite_posts WHERE mylite_status = 'draft' ORDER BY mylite_created_at LIMIT 50;
DELETE p FROM mylite_posts AS p INNER JOIN mylite_authors AS a ON a.mylite_id = p.mylite_author_id WHERE a.mylite_disabled = 1;
LOAD DATA LOCAL INFILE 'posts.tsv' INTO TABLE mylite_posts CHARACTER SET utf8mb4 FIELDS TERMINATED BY '\t' OPTIONALLY ENCLOSED BY '"' LINES TERMINATED BY '\n' IGNORE 1 LINES;
SELECT mylite_id, mylite_title INTO OUTFILE '/tmp/mylite-posts.tsv' FIELDS TERMINATED BY '\t' LINES TERMINATED BY '\n' FROM mylite_posts;

-- query expressions
WITH ranked_posts AS (
    SELECT mylite_id, mylite_author_id, ROW_NUMBER() OVER (PARTITION BY mylite_author_id ORDER BY mylite_created_at DESC) AS rn
    FROM mylite_posts
)
SELECT * FROM ranked_posts WHERE rn <= 3;
WITH RECURSIVE mylite_numbers(n) AS (
    SELECT 1
    UNION ALL
    SELECT n + 1 FROM mylite_numbers WHERE n < 5
)
SELECT n FROM mylite_numbers;
SELECT mylite_author_id, COUNT(*) AS total_posts FROM mylite_posts GROUP BY mylite_author_id WITH ROLLUP HAVING total_posts > 0;
SELECT mylite_id FROM mylite_posts WHERE EXISTS (SELECT 1 FROM mylite_postmeta WHERE mylite_post_id = mylite_posts.mylite_id);
SELECT mylite_id FROM mylite_posts WHERE (mylite_author_id, mylite_status) IN (SELECT mylite_author_id, mylite_status FROM mylite_post_acl);
SELECT mylite_id FROM mylite_posts FOR UPDATE NOWAIT;
SELECT mylite_id FROM mylite_posts UNION DISTINCT SELECT mylite_id FROM mylite_posts_archive;
SELECT mylite_id FROM mylite_posts INTERSECT SELECT mylite_post_id FROM mylite_featured_posts;
SELECT mylite_id FROM mylite_posts EXCEPT SELECT mylite_post_id FROM mylite_hidden_posts;
TABLE mylite_posts ORDER BY mylite_id LIMIT 5;
VALUES ROW(1, 'alpha'), ROW(2, 'beta');

-- transactions, locks, and prepared statements
START TRANSACTION READ WRITE, WITH CONSISTENT SNAPSHOT;
SAVEPOINT mylite_before_import;
ROLLBACK TO SAVEPOINT mylite_before_import;
RELEASE SAVEPOINT mylite_before_import;
COMMIT AND NO CHAIN NO RELEASE;
ROLLBACK AND CHAIN;
LOCK TABLES mylite_posts WRITE, mylite_postmeta READ LOCAL;
UNLOCK TABLES;
SET TRANSACTION ISOLATION LEVEL READ COMMITTED;
PREPARE mylite_stmt FROM 'SELECT mylite_id FROM mylite_posts WHERE mylite_id = ?';
EXECUTE mylite_stmt USING @mylite_id;
DEALLOCATE PREPARE mylite_stmt;

-- set, show, describe, and explain
SET NAMES utf8mb4 COLLATE utf8mb4_0900_ai_ci;
SET CHARACTER SET utf8mb4;
SET SESSION sql_mode = CONCAT(@@session.sql_mode, ',ANSI_QUOTES');
SET @mylite_counter := 0, @'quoted-counter' := 1;
SHOW CHARACTER SET LIKE 'utf8%';
SHOW COLLATION WHERE Charset = 'utf8mb4';
SHOW COLUMNS FROM mylite_posts LIKE 'mylite_%';
SHOW CREATE TABLE mylite_posts;
SHOW INDEXES FROM mylite_posts;
SHOW TABLE STATUS FROM mylite_app LIKE 'mylite_posts';
SHOW VARIABLES WHERE Variable_name IN ('sql_mode', 'time_zone');
SHOW WARNINGS LIMIT 5;
SHOW COUNT(*) ERRORS;
DESCRIBE mylite_posts mylite_title;
EXPLAIN FORMAT=TREE SELECT mylite_id FROM mylite_posts WHERE mylite_status = 'publish';
EXPLAIN ANALYZE SELECT COUNT(*) FROM mylite_posts;
USE mylite_app;
HELP 'contents';

-- account and administration syntax
CREATE USER IF NOT EXISTS 'mylite_user'@'localhost' IDENTIFIED BY 'secret' PASSWORD EXPIRE INTERVAL 90 DAY;
ALTER USER 'mylite_user'@'localhost' ACCOUNT LOCK;
CREATE ROLE IF NOT EXISTS mylite_editor;
GRANT SELECT, INSERT, UPDATE ON mylite_app.* TO mylite_editor WITH GRANT OPTION;
GRANT mylite_editor TO 'mylite_user'@'localhost';
SET DEFAULT ROLE mylite_editor TO 'mylite_user'@'localhost';
REVOKE UPDATE ON mylite_app.* FROM mylite_editor;
DROP USER IF EXISTS 'mylite_user'@'localhost';
DROP ROLE IF EXISTS mylite_editor;
ANALYZE TABLE mylite_posts UPDATE HISTOGRAM ON mylite_status WITH 16 BUCKETS;
CHECK TABLE mylite_posts FOR UPGRADE;
OPTIMIZE TABLE mylite_posts;
REPAIR TABLE mylite_posts QUICK;
FLUSH TABLES mylite_posts WITH READ LOCK;
RESET PERSIST IF EXISTS mylite_custom_variable;
KILL QUERY 12345;
