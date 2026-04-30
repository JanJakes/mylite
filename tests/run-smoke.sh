#!/bin/sh
set -eu

parser="${PARSER:-bin/mylite-parse}"

"$parser" --quiet "SELECT 1"
"$parser" --quiet "SELECT IF(a > 1, 'yes', 'no') FROM t WHERE b IN (SELECT b FROM u)"
"$parser" --quiet "SELECT max(CASE col WHEN 1 THEN val ELSE NULL END) FROM t1 GROUP BY row_id"
"$parser" --quiet "CREATE TABLE t1 (id bigint unsigned not null auto_increment, title varchar(255), primary key (id)) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4"
"$parser" --quiet "INSERT INTO t1 (id, title) VALUES (1, 'a'), (2, 'b') ON DUPLICATE KEY UPDATE title = VALUES(title)"
"$parser" --quiet "CREATE PROCEDURE p1() BEGIN SELECT 1; IF 1 THEN SELECT 2; END IF; END"
"$parser" --quiet "WITH cte AS (SELECT 0 /*! ) */ SELECT * FROM cte a, cte b"

if "$parser" --quiet "SELECT (1"; then
	echo "expected unmatched parenthesis to fail" >&2
	exit 1
fi

if "$parser" --quiet "SELECT 'unterminated"; then
	echo "expected unterminated string to fail" >&2
	exit 1
fi
