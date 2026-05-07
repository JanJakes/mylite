DROP DATABASE IF EXISTS mylite_timestamp_column_crud;
CREATE DATABASE mylite_timestamp_column_crud CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci;
USE mylite_timestamp_column_crud;

CREATE TABLE wp_events_timestamp_like (
  event_id INT NOT NULL AUTO_INCREMENT,
  event_name VARCHAR(64) NOT NULL,
  created_at TIMESTAMP NOT NULL,
  updated_at TIMESTAMP(3) NOT NULL,
  seen_at TIMESTAMP(6) DEFAULT NULL,
  PRIMARY KEY (event_id),
  UNIQUE KEY event_name (event_name)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

SELECT column_name, data_type, column_type, datetime_precision,
       is_nullable, column_default
FROM information_schema.columns
WHERE table_schema = 'mylite_timestamp_column_crud'
  AND table_name = 'wp_events_timestamp_like'
  AND column_name IN ('created_at', 'updated_at', 'seen_at')
ORDER BY ordinal_position;

INSERT INTO wp_events_timestamp_like (event_name, created_at, updated_at, seen_at) VALUES
  ('epoch-start', '1970-01-01 00:00:01', '1970-01-01 00:00:01.1234',
   '1970-01-01 00:00:01.1234567'),
  ('max-edge', '2038-01-19 03:14:07', '2038-01-19 03:14:07.9994',
   '2038-01-19 03:14:07.999999'),
  ('ordinary', 20240229123456, '2024-02-29 12:34:56.7896', NULL);

UPDATE wp_events_timestamp_like
SET updated_at = '2024-03-01 01:02:03.9999',
    seen_at = '2024-03-01 01:02:03.000001'
WHERE event_name = 'ordinary';

DELETE FROM wp_events_timestamp_like WHERE event_name = 'max-edge';

SELECT 'after-delete', event_id, event_name, created_at, updated_at,
       IFNULL(seen_at, 'SQLNULL')
FROM wp_events_timestamp_like
ORDER BY event_id;

SELECT 'summary', COUNT(*), MIN(created_at), MAX(updated_at),
       GROUP_CONCAT(event_name ORDER BY event_id SEPARATOR ',')
FROM wp_events_timestamp_like;

INSERT INTO wp_events_timestamp_like (event_name, created_at, updated_at, seen_at) VALUES
  ('too-low', '1970-01-01 00:00:00', '1970-01-01 00:00:01', NULL);
GET DIAGNOSTICS CONDITION 1 @errno = MYSQL_ERRNO, @state = RETURNED_SQLSTATE;
SELECT 'after-too-low', @errno, @state;

INSERT INTO wp_events_timestamp_like (event_name, created_at, updated_at, seen_at) VALUES
  ('too-high', '2038-01-19 03:14:08', '2038-01-19 03:14:07', NULL);
GET DIAGNOSTICS CONDITION 1 @errno = MYSQL_ERRNO, @state = RETURNED_SQLSTATE;
SELECT 'after-too-high', @errno, @state;

INSERT INTO wp_events_timestamp_like (event_name, created_at, updated_at, seen_at) VALUES
  ('round-high', '2038-01-19 03:14:07.5', '2038-01-19 03:14:07', NULL);
GET DIAGNOSTICS CONDITION 1 @errno = MYSQL_ERRNO, @state = RETURNED_SQLSTATE;
SELECT 'after-round-high', @errno, @state;

TRUNCATE TABLE wp_events_timestamp_like;
SELECT 'after-truncate', COUNT(*), COALESCE(MAX(event_id), 0) FROM wp_events_timestamp_like;

INSERT INTO wp_events_timestamp_like (event_name, created_at, updated_at, seen_at) VALUES
  ('restored', '2038-01-19 03:14:07', '2038-01-19 03:14:07.9994',
   '2038-01-19 03:14:07.999999');

SELECT 'after-reinsert', event_id, event_name, created_at, updated_at, seen_at
FROM wp_events_timestamp_like;

DROP TABLE wp_events_timestamp_like;

SELECT 'remaining-tables', COUNT(*)
FROM information_schema.tables
WHERE table_schema = 'mylite_timestamp_column_crud'
  AND table_name = 'wp_events_timestamp_like';
