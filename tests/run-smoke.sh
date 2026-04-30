#!/bin/sh
set -eu

parser="${PARSER:-bin/mylite-parse}"

python3 tests/check_keywords.py

"$parser" --quiet "SELECT 1"
"$parser" --quiet "SELECT IF(a > 1, 'yes', 'no') FROM t WHERE b IN (SELECT b FROM u)"
"$parser" --quiet "SELECT max(CASE col WHEN 1 THEN val ELSE NULL END) FROM t1 GROUP BY row_id"
"$parser" --quiet "SELECT 0b1010, 0x1f, .5, 1e-3"
"$parser" --quiet "CREATE TABLE t1 (id bigint unsigned not null auto_increment, title varchar(255), primary key (id)) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4"
"$parser" --quiet "INSERT INTO t1 (id, title) VALUES (1, 'a'), (2, 'b') ON DUPLICATE KEY UPDATE title = VALUES(title)"
"$parser" --quiet "CREATE PROCEDURE p1() BEGIN SELECT 1; IF 1 THEN SELECT 2; END IF; END"
"$parser" --quiet "WITH cte AS (SELECT 0 /*! ) */ SELECT * FROM cte a, cte b"
"$parser" --quiet "COMMIT"

span_output=$("$parser" "SELECT 1; COMMIT")
case "$span_output" in
	*"select[1:2,0:8],commit[4:4,10:16]"*) ;;
	*)
		echo "unexpected statement span output: $span_output" >&2
		exit 1
		;;
esac

object_output=$("$parser" "CREATE TABLE t (id int); ALTER VIEW v AS SELECT 1; DROP FUNCTION f")
case "$object_output" in
	*"create"*/table*"alter"*/view*"drop"*/function*) ;;
	*)
		echo "unexpected object output: $object_output" >&2
		exit 1
		;;
esac

token_output=$("$parser" --tokens "SELECT @a, ? FROM t WHERE a IS NULL")
case "$token_output" in
	*"token 1 keyword"*"token 2 user_variable"*"token 3 punctuation"*"token 4 parameter"*"token 5 keyword"*"token 7 keyword"*"token 10 keyword"*) ;;
	*)
		echo "unexpected token output: $token_output" >&2
		exit 1
		;;
esac

if "$parser" --quiet "SELECT (1"; then
	echo "expected unmatched parenthesis to fail" >&2
	exit 1
fi

if "$parser" --quiet "SELECT 'unterminated"; then
	echo "expected unterminated string to fail" >&2
	exit 1
fi

if "$parser" --quiet "SELECT"; then
	echo "expected bare SELECT to fail" >&2
	exit 1
fi

if "$parser" --quiet "CREATE"; then
	echo "expected bare CREATE to fail" >&2
	exit 1
fi

if "$parser" --quiet "SET"; then
	echo "expected bare SET to fail" >&2
	exit 1
fi
