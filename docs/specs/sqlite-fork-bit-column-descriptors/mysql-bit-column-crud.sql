DROP DATABASE IF EXISTS mylite_bit_column_crud;
CREATE DATABASE mylite_bit_column_crud DEFAULT CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci;
USE mylite_bit_column_crud;

CREATE TABLE wp_feature_flags_like (
  flag_id INT NOT NULL AUTO_INCREMENT,
  blog_id BIGINT UNSIGNED NOT NULL,
  flag_key VARCHAR(64) NOT NULL,
  enabled BIT NOT NULL,
  mask BIT(8) NOT NULL,
  rollout BIT(9) DEFAULT NULL,
  PRIMARY KEY (flag_id),
  KEY blog_flag (blog_id, flag_key)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

SELECT column_name, data_type, column_type, numeric_precision, numeric_scale,
       character_maximum_length, character_octet_length, character_set_name, collation_name
FROM information_schema.columns
WHERE table_schema = 'mylite_bit_column_crud'
  AND table_name = 'wp_feature_flags_like'
  AND column_name IN ('enabled', 'mask', 'rollout')
ORDER BY ordinal_position;

INSERT INTO wp_feature_flags_like (blog_id, flag_key, enabled, mask, rollout) VALUES
  (1, 'search', b'1', b'00000101', b'101010101'),
  (1, 'editor', 0, 15, 257),
  (2, 'cache', b'', '', NULL);

UPDATE wp_feature_flags_like
SET enabled = b'1', mask = b'00111100', rollout = b'000000001'
WHERE flag_key = 'cache';

DELETE FROM wp_feature_flags_like WHERE flag_key = 'editor';

SELECT flag_id, blog_id, flag_key,
       enabled + 0 AS enabled_num, LENGTH(enabled) AS enabled_len, BIT_LENGTH(enabled) AS enabled_bits,
       mask + 0 AS mask_num, LENGTH(mask) AS mask_len, BIT_LENGTH(mask) AS mask_bits,
       rollout + 0 AS rollout_num, LENGTH(rollout) AS rollout_len, BIT_LENGTH(rollout) AS rollout_bits
FROM wp_feature_flags_like
ORDER BY mask, flag_id;

SELECT COUNT(*) AS row_count, MIN(mask + 0) AS min_mask, MAX(mask + 0) AS max_mask
FROM wp_feature_flags_like;

TRUNCATE TABLE wp_feature_flags_like;

SELECT COUNT(*) AS truncated_count, COALESCE(MAX(flag_id), 0) AS max_flag_id
FROM wp_feature_flags_like;

INSERT INTO wp_feature_flags_like (blog_id, flag_key, enabled, mask, rollout)
VALUES (3, 'restored', b'1', X'7f', b'000000010');

SELECT flag_id, blog_id, flag_key, enabled + 0, mask + 0, rollout + 0,
       LENGTH(enabled), LENGTH(mask), LENGTH(rollout)
FROM wp_feature_flags_like;

DROP TABLE wp_feature_flags_like;

SELECT COUNT(*) AS remaining_tables
FROM information_schema.tables
WHERE table_schema = 'mylite_bit_column_crud'
  AND table_name = 'wp_feature_flags_like';
