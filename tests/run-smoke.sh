#!/bin/sh
set -eu

parser="${PARSER:-bin/mylite-parse}"

python3 tests/check_keywords.py

"$parser" --quiet "SELECT 1"
"$parser" --quiet "SELECT IF(a > 1, 'yes', 'no') FROM t WHERE b IN (SELECT b FROM u)"
"$parser" --quiet "SELECT max(CASE col WHEN 1 THEN val ELSE NULL END) FROM t1 GROUP BY row_id"
"$parser" --quiet "SELECT 0b1010, 0x1f, .5, 1e-3"
"$parser" --quiet "SELECT _utf8mb4'abc', N'n', X'0a', b'1010'"
"$parser" --quiet "CREATE TABLE t1 (id bigint unsigned not null auto_increment, title varchar(255), primary key (id)) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4"
"$parser" --quiet "INSERT INTO t1 (id, title) VALUES (1, 'a'), (2, 'b') ON DUPLICATE KEY UPDATE title = VALUES(title)"
"$parser" --quiet "CREATE PROCEDURE p1() BEGIN SELECT 1; IF 1 THEN SELECT 2; END IF; END"
"$parser" --quiet "WITH cte AS (SELECT 0 /*! ) */ SELECT * FROM cte a, cte b"
"$parser" --quiet "WITH c AS (SELECT 1) UPDATE t SET a=1"
"$parser" --quiet "COMMIT"

span_output=$("$parser" "SELECT 1; COMMIT")
case "$span_output" in
	*"select[1:2,0:8],commit[4:4,10:16]"*) ;;
	*)
		echo "unexpected statement span output: $span_output" >&2
		exit 1
		;;
esac

grouped_query_output=$("$parser" '(SELECT 1) UNION SELECT 2; ((VALUES ROW(1),ROW(2))) ORDER BY 1; (TABLE t)')
case "$grouped_query_output" in
	*"kinds=select[1:7,0:25],values[9:25,27:62],table[27:30,64:73]"*) ;;
	*)
		echo "unexpected grouped query output: $grouped_query_output" >&2
		exit 1
		;;
esac

stored_head_output=$("$parser" 'DECLARE x INT; OPEN c; FETCH c INTO x; CLOSE c; IF x THEN RETURN x END IF; LOOP LEAVE done END LOOP; REPEAT ITERATE done UNTIL x END REPEAT; WHILE x DO SET x=x+1 END WHILE; CASE x WHEN 1 THEN RETURN 1 END CASE; LEAVE done; ITERATE done; RETURN 1')
case "$stored_head_output" in
	*"kinds=declare"*"open"*"fetch"*"close"*"if"*"loop"*"repeat"*"while"*"case"*"leave"*"iterate"*"return"*) ;;
	*)
		echo "unexpected stored-program head output: $stored_head_output" >&2
		exit 1
		;;
esac

object_output=$("$parser" 'CREATE TABLE IF NOT EXISTS `db`.`t` (id int); ALTER VIEW v AS SELECT 1; DROP FUNCTION f')
case "$object_output" in
	*"create"*/table:'`db`.`t`'*"alter"*/view:v*"drop"*/function:f*) ;;
	*)
		echo "unexpected object output: $object_output" >&2
		exit 1
		;;
esac

dml_sql='INSERT INTO `db`.`t` VALUES (1);
REPLACE LOW_PRIORITY INTO r VALUES (1);
UPDATE IGNORE u SET a=1;
DELETE LOW_PRIORITY QUICK IGNORE FROM `db`.`d` WHERE a=1;
WITH c AS (SELECT 1) UPDATE wt SET a=1'
dml_object_output=$("$parser" "$dml_sql")
case "$dml_object_output" in
	*"insert"*/table:'`db`.`t`'*"replace"*/table:r*"update"*/table:u*"delete"*/table:'`db`.`d`'*"update"*/table:wt*) ;;
	*)
		echo "unexpected DML object output: $dml_object_output" >&2
		exit 1
		;;
esac

utility_sql='TRUNCATE t;
TRUNCATE TABLE `db`.`t`;
USE `db`;
TABLE `db`.`t`;
HANDLER `db`.`h` OPEN;
LOAD DATA INFILE "x" INTO TABLE `db`.`ld`;
LOCK TABLES `db`.`lt` READ'
utility_object_output=$("$parser" "$utility_sql")
case "$utility_object_output" in
	*"truncate"*/table:t*"truncate"*/table:'`db`.`t`'*"use"*/database:'`db`'*"table"*/table:'`db`.`t`'*"handler"*/table:'`db`.`h`'*"load"*/table:'`db`.`ld`'*"lock"*/table:'`db`.`lt`'*) ;;
	*)
		echo "unexpected utility object output: $utility_object_output" >&2
		exit 1
		;;
esac

explain_sql='DESC `db`.`t`;
DESCRIBE t c;
EXPLAIN `db`.`e`;
EXPLAIN SELECT 1;
EXPLAIN FORMAT = JSON SELECT 1;
DESCRIBE SELECT 1'
explain_object_output=$("$parser" "$explain_sql")
case "$explain_object_output" in
	*"/table:FORMAT"*|*"/table:SELECT"*)
		echo "unexpected EXPLAIN/DESCRIBE object output: $explain_object_output" >&2
		exit 1
		;;
	*"describe"*/table:'`db`.`t`'*"describe"*/table:t*"explain"*/table:'`db`.`e`'*"explain[15:17"*"explain[19:24"*"describe[26:28"*) ;;
	*)
		echo "unexpected EXPLAIN/DESCRIBE object output: $explain_object_output" >&2
		exit 1
		;;
esac

show_sql='SHOW CREATE TABLE `db`.`t`;
SHOW CREATE VIEW v;
SHOW COLUMNS FROM `db`.`c`;
SHOW FULL FIELDS FROM f;
SHOW INDEXES FROM `db`.`i`;
SHOW KEYS FROM k;
SHOW TABLES FROM `db`;
SHOW VARIABLES'
show_object_output=$("$parser" "$show_sql")
case "$show_object_output" in
	*"/table:VARIABLES"*)
		echo "unexpected SHOW object output: $show_object_output" >&2
		exit 1
		;;
	*"show"*/table:'`db`.`t`'*"show"*/view:v*"show"*/table:'`db`.`c`'*"show"*/table:f*"show"*/table:'`db`.`i`'*"show"*/table:k*"show"*/database:'`db`'*"show[43:44"*) ;;
	*)
		echo "unexpected SHOW object output: $show_object_output" >&2
		exit 1
		;;
esac

prepared_output=$("$parser" 'PREPARE stmt FROM @sql; EXECUTE stmt USING @a; DEALLOCATE PREPARE stmt; DROP PREPARE stmt')
case "$prepared_output" in
	*"prepare"*/prepared_statement:stmt*"execute"*/prepared_statement:stmt*"deallocate"*/prepared_statement:stmt*"drop"*/prepared_statement:stmt*) ;;
	*)
		echo "unexpected prepared statement output: $prepared_output" >&2
		exit 1
		;;
esac

principal_output=$("$parser" "GRANT SELECT ON db.t TO 'u'@'h'; GRANT r TO u; REVOKE SELECT ON db.t FROM 'u'@'%'; REVOKE r FROM u")
case "$principal_output" in
	*"grant"*/user:"'u'@'h'"*"grant"*/user:u*"revoke"*/user:"'u'@'%'"*"revoke"*/user:u*) ;;
	*)
		echo "unexpected principal output: $principal_output" >&2
		exit 1
		;;
esac

with_output=$("$parser" "WITH c AS (SELECT 1) UPDATE t SET a=1; WITH c AS (SELECT 1) DELETE FROM t; WITH c AS (SELECT 1) INSERT INTO t SELECT * FROM c")
case "$with_output" in
	*"kinds=update"*"delete"*"insert"*) ;;
	*)
		echo "unexpected WITH output: $with_output" >&2
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

keyword_output=$("$parser" --tokens 'SHOW FULL COLUMNS FROM t; EXPLAIN FORMAT = JSON SELECT 1; LOAD DATA LOCAL INFILE "x" REPLACE INTO TABLE data; START TRANSACTION READ WRITE; COMMIT AND CHAIN NO RELEASE; ROLLBACK TO SAVEPOINT s; CREATE TABLE json (id int); INSERT INTO local VALUES (1)')
case "$keyword_output" in
	*"show"*/table:t*"load"*/table:data*"create"*/table:json*"insert"*/table:local*"token 2 keyword"*"token 3 keyword"*"token 8 keyword"*"token 10 keyword"*"token 15 keyword"*"token 16 keyword"*"token 17 keyword"*"token 25 keyword"*"token 26 keyword"*"token 27 keyword"*"token 31 keyword"*"token 32 keyword"*"token 36 keyword"*) ;;
	*)
		echo "unexpected keyword output: $keyword_output" >&2
		exit 1
		;;
esac

match_output=$("$parser" --tokens "SELECT (1), CASE WHEN a THEN b END")
case "$match_output" in
	*"match 2 4"*"match 4 2"*"match 6 11"*"match 11 6"*) ;;
	*)
		echo "unexpected match output: $match_output" >&2
		exit 1
		;;
esac

stored_match_output=$("$parser" --tokens 'CREATE PROCEDURE p() BEGIN IF x THEN CREATE TABLE IF NOT EXISTS t (id int); END IF; LOOP LEAVE done; END LOOP; REPEAT ITERATE done; UNTIL x END REPEAT; WHILE x DO SET x=x+1; END WHILE; END')
case "$stored_match_output" in
	*"match 7 21"*"match 21 7"*"match 23 27"*"match 27 23"*"match 29 35"*"match 35 29"*"match 37 47"*"match 47 37"*) ;;
	*)
		echo "unexpected stored-program match output: $stored_match_output" >&2
		exit 1
		;;
esac

literal_output=$("$parser" --tokens "SELECT _utf8mb4'abc', N'n', X'0a', b'1010'")
case "$literal_output" in
	*"token 2 string"*"token 4 string"*"token 6 number"*"token 8 number"*) ;;
	*)
		echo "unexpected prefixed literal output: $literal_output" >&2
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
