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

version_comment_output=$("$parser" "SELECT /*! STRAIGHT_JOIN */ 1; /*!80409 SET @ok=1 */; /*!080409 SET @six=1 */; /*!80410 SET @future=1 */; /*!99999 SET @far=1 */; /*!123 SET @short=1 */; SELECT 2")
case "$version_comment_output" in
	*"@future"*|*"@far"*|*"@short"*)
		echo "unexpected gated executable comment output: $version_comment_output" >&2
		exit 1
		;;
esac
case "$version_comment_output" in
	*"select[1:3"*"/user_variable:@ok"*"/user_variable:@six"*"select[18:19"*) ;;
	*)
		echo "unexpected executable comment output: $version_comment_output" >&2
		exit 1
		;;
esac

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
	*"kinds=select[1:7,0:25],values[9:25,27:62]/query,table[27:30,64:73]/table:t"*) ;;
	*)
		echo "unexpected grouped query output: $grouped_query_output" >&2
		exit 1
		;;
esac

values_query_output=$("$parser" 'VALUES ROW(1), ROW(2); ((VALUES ROW(3))) ORDER BY 1; EXPLAIN VALUES ROW(1)')
case "$values_query_output" in
	*"kinds=values[1:10,0:21]/query,values[12:23,23:51]/query,explain[25:30,53:74]"*) ;;
	*)
		echo "unexpected values query output: $values_query_output" >&2
		exit 1
		;;
esac

table_query_output=$("$parser" 'TABLE t; (TABLE `db`.`t`); ((TABLE t)) ORDER BY c LIMIT 1')
case "$table_query_output" in
	*"kinds=table[1:2,0:7]/table:t,table[4:9,9:25]/table:\`db\`.\`t\`,table[11:21,27:57]/table:t"*) ;;
	*)
		echo "unexpected table query output: $table_query_output" >&2
		exit 1
		;;
esac

table_into_output=$("$parser" "TABLE t INTO OUTFILE '/tmp/t.tsv'; TABLE t INTO DUMPFILE '/tmp/t.bin'; TABLE t INTO @row; (TABLE t) INTO local_var; (TABLE t INTO @inner)")
case "$table_into_output" in
	*"table"*/outfile:"'/tmp/t.tsv'"*"table"*/dumpfile:"'/tmp/t.bin'"*"table"*/user_variable:@row*"table"*/local_variable:local_var*"table"*/user_variable:@inner*) ;;
	*)
		echo "unexpected table into output: $table_into_output" >&2
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

compound_span_output=$("$parser" 'IF x THEN SELECT 1; END IF; CASE x WHEN 1 THEN SELECT 1; END CASE; LOOP LEAVE done; END LOOP done; REPEAT SELECT 1; UNTIL x END REPEAT rpt; WHILE x DO SELECT 1; END WHILE wh')
case "$compound_span_output" in
	*"kinds=if[1:7"*"case[9:17"*"loop[19:24"*"repeat[26:33"*"while[35:42"*) ;;
	*)
		echo "unexpected compound statement span output: $compound_span_output" >&2
		exit 1
		;;
esac

cursor_output=$("$parser" 'DECLARE c CURSOR FOR SELECT 1; DECLARE x INT; OPEN c; FETCH c INTO x; FETCH FROM c INTO x; FETCH NEXT FROM c INTO x; CLOSE c; CREATE TABLE cursor (id int)')
case "$cursor_output" in
	*"declare"*/cursor:c*"declare"*/local_variable:x*"open"*/cursor:c*"fetch"*/cursor:c*"fetch"*/cursor:c*"fetch"*/cursor:c*"close"*/cursor:c*"create"*/table:cursor*) ;;
	*)
		echo "unexpected cursor output: $cursor_output" >&2
		exit 1
	;;
esac

declare_condition_sql=$(cat <<'SQL'
DECLARE cond CONDITION FOR SQLSTATE '45000';
DECLARE not_found CONDITION FOR NOT FOUND;
DECLARE `cond` CONDITION FOR 1051;
DECLARE x INT
SQL
)
declare_condition_output=$("$parser" "$declare_condition_sql")
case "$declare_condition_output" in
	*"declare"*/condition:cond*"declare"*/condition:not_found*"declare"*/condition:'`cond`'*"declare"*/local_variable:x*) ;;
	*)
		echo "unexpected DECLARE CONDITION output: $declare_condition_output" >&2
		exit 1
		;;
esac

declare_variable_output=$("$parser" 'DECLARE x, y INT DEFAULT 1; DECLARE `return` VARCHAR(10)')
case "$declare_variable_output" in
	*"declare"*/local_variable:x*"declare"*/local_variable:'`return`'*) ;;
	*)
		echo "unexpected DECLARE variable output: $declare_variable_output" >&2
		exit 1
		;;
esac

declare_handler_sql=$(cat <<'SQL'
DECLARE EXIT HANDLER FOR SQLEXCEPTION SET @x = 1;
DECLARE CONTINUE HANDLER FOR SQLSTATE '45000' BEGIN RESIGNAL; END;
DECLARE CONTINUE HANDLER FOR SQLSTATE VALUE '01000' SET @x = 1;
DECLARE CONTINUE HANDLER FOR NOT FOUND SET done = TRUE;
DECLARE CONTINUE HANDLER FOR 1051 SET @x = 1;
DECLARE CONTINUE HANDLER FOR my_condition SET @x = 1;
DECLARE c CURSOR FOR SELECT 1
SQL
)
declare_handler_output=$("$parser" "$declare_handler_sql")
case "$declare_handler_output" in
	*"declare"*/condition:SQLEXCEPTION*"declare"*/sqlstate:"'45000'"*"declare"*/sqlstate:"'01000'"*"declare"*/condition:"NOT FOUND"*"declare"*/condition:1051*"declare"*/condition:my_condition*"declare"*/cursor:c*) ;;
	*)
		echo "unexpected DECLARE HANDLER output: $declare_handler_output" >&2
		exit 1
	;;
esac

diagnostics_output=$("$parser" 'GET DIAGNOSTICS @n = NUMBER; GET CURRENT DIAGNOSTICS CONDITION 1 @state = RETURNED_SQLSTATE; GET STACKED DIAGNOSTICS CONDITION @i v = MYSQL_ERRNO; GET DIAGNOSTICS CONDITION local_index v = MESSAGE_TEXT')
case "$diagnostics_output" in
	*"get[1:5"*"get"*/diagnostics_condition:1*"get"*/diagnostics_condition:@i*"get"*/diagnostics_condition:local_index*) ;;
	*)
		echo "unexpected GET DIAGNOSTICS output: $diagnostics_output" >&2
		exit 1
		;;
esac

label_output=$("$parser" 'LEAVE done; ITERATE done; RETURN done')
case "$label_output" in
	*"leave"*/label:done*"iterate"*/label:done*"return[7:8"*) ;;
	*)
		echo "unexpected label output: $label_output" >&2
		exit 1
		;;
esac

signal_sql=$(cat <<'SQL'
SIGNAL SQLSTATE '45000';
SIGNAL SQLSTATE VALUE '02000';
SIGNAL my_condition;
SIGNAL `cond` SET MESSAGE_TEXT = 'x';
RESIGNAL;
RESIGNAL SET MESSAGE_TEXT = 'x';
RESIGNAL SQLSTATE '45000';
RESIGNAL SQLSTATE VALUE '01000' SET MYSQL_ERRNO = 1000;
RESIGNAL my_condition
SQL
)
signal_output=$("$parser" "$signal_sql")
case "$signal_output" in
	*"signal"*/sqlstate:"'45000'"*"signal"*/sqlstate:"'02000'"*"signal"*/condition:my_condition*"signal"*/condition:'`cond`'*"resignal[22:26"*"resignal"*/sqlstate:"'45000'"*"resignal"*/sqlstate:"'01000'"*"resignal"*/condition:my_condition*) ;;
	*)
		echo "unexpected SIGNAL/RESIGNAL output: $signal_output" >&2
		exit 1
		;;
esac

label_target_keyword_output=$("$parser" "LEAVE open; ITERATE engine; LEAVE no; ITERATE read; LEAVE \`read\`; LEAVE 'done'")
case "$label_target_keyword_output" in
	*"/label:no"*|*"/label:read"*|*"/label:'done'"*)
		echo "unexpected restricted label target output: $label_target_keyword_output" >&2
		exit 1
		;;
	*"leave"*/label:open*"iterate"*/label:engine*"leave[7:8"*"iterate[10:11"*"leave"*/label:'`read`'*) ;;
	*)
		echo "unexpected label target keyword output: $label_target_keyword_output" >&2
		exit 1
		;;
esac

labeled_statement_output=$("$parser" 'done: LOOP LEAVE done; END LOOP done; rpt: REPEAT ITERATE rpt; UNTIL done END REPEAT rpt; wh: WHILE done DO LEAVE wh; END WHILE wh; blk: BEGIN SELECT 1; END blk')
case "$labeled_statement_output" in
	*"loop"*/label:done*"repeat"*/label:rpt*"while"*/label:wh*"begin"*/label:blk*) ;;
	*)
		echo "unexpected labeled statement output: $labeled_statement_output" >&2
		exit 1
		;;
esac

label_keyword_output=$("$parser" 'open: LOOP LEAVE open; END LOOP open; engine: LOOP LEAVE engine; END LOOP engine; value: LOOP LEAVE value; END LOOP value; quick: LOOP LEAVE quick; END LOOP quick; no: LOOP LEAVE no; END LOOP no; read: LOOP LEAVE read; END LOOP read; `read`: LOOP LEAVE `read`; END LOOP `read`')
case "$label_keyword_output" in
	*"/label:no"*|*"/label:read"*)
		echo "unexpected restricted label keyword output: $label_keyword_output" >&2
		exit 1
		;;
	*"loop"*/label:open*"loop"*/label:engine*"loop"*/label:value*"loop"*/label:quick*"loop"*/label:'`read`'*) ;;
	*)
		echo "unexpected label keyword output: $label_keyword_output" >&2
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

qualified_keyword_output=$("$parser" 'CREATE TABLE db.select (id int); ALTER TABLE db.key ADD c INT; DROP TABLE db.group; SHOW CREATE PROCEDURE db.order')
case "$qualified_keyword_output" in
	*"create"*/table:db.select*"alter"*/table:db.key*"drop"*/table:db.group*"show"*/procedure:db.order*) ;;
	*)
		echo "unexpected qualified keyword output: $qualified_keyword_output" >&2
		exit 1
		;;
esac

database_option_output=$("$parser" 'ALTER DATABASE CHARACTER SET utf8mb4; ALTER SCHEMA DEFAULT COLLATE utf8mb4_bin; ALTER DATABASE READ ONLY = DEFAULT; ALTER DATABASE db READ ONLY = 1')
case "$database_option_output" in
	*"/database:CHARACTER"*|*"/schema:utf8mb4_bin"*|*"/database:READ"*)
		echo "unexpected nameless database option target: $database_option_output" >&2
		exit 1
		;;
esac
case "$database_option_output" in
	*"alter"*/database*"alter"*/schema*"alter"*/database*"alter"*/database:db*) ;;
	*)
		echo "unexpected database option output: $database_option_output" >&2
		exit 1
		;;
esac

definer_object_output=$("$parser" 'CREATE DEFINER = user@localhost PROCEDURE p() SELECT 1; CREATE DEFINER = event@localhost VIEW v AS SELECT 1; CREATE DEFINER = trigger@localhost EVENT e ON SCHEDULE EVERY 1 DAY DO SELECT 1; ALTER DEFINER = user@localhost VIEW v AS SELECT 1')
case "$definer_object_output" in
	*"/user:"*)
		echo "unexpected definer account object output: $definer_object_output" >&2
		exit 1
		;;
esac
case "$definer_object_output" in
	*"create"*/procedure:p*"create"*/view:v*"create"*/event:e*"alter"*/view:v*) ;;
	*)
		echo "unexpected definer object output: $definer_object_output" >&2
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

update_reference_output=$("$parser" 'UPDATE IGNORE (SELECT 1) x, t3 SET t3.a = 0; UPDATE (SELECT 1) AS x JOIN t4 SET t4.a = 1; UPDATE (t1 JOIN t2) SET t1.a = 1')
case "$update_reference_output" in
	*"update"*/table:t3*"update"*/table:t4*"update"*/table:t1*) ;;
	*)
		echo "unexpected UPDATE table reference output: $update_reference_output" >&2
		exit 1
		;;
esac

variable_assignment_output=$("$parser" "SELECT a INTO @x FROM t; SELECT a INTO local_var FROM t; SELECT a FROM t INTO @x; SELECT a INTO OUTFILE '/tmp/x' FROM t; SELECT a INTO DUMPFILE '/tmp/y' FROM t; SET @x = 1; SET @'my-var' = 1; SET @\"my-var\" := 2; SET @\`my-var\` = 3; SET @iv=-20010101; SET @plus=+.5; SET @@session.sql_mode = 'ANSI'; SET SESSION sql_mode = 'ANSI'; SET sql_log_bin = 0; SET autocommit = 1; SET x = 1; GET DIAGNOSTICS @n = NUMBER; GET CURRENT DIAGNOSTICS CONDITION 1 @state = RETURNED_SQLSTATE")
case "$variable_assignment_output" in
	*"select"*/user_variable:@x*"select"*/local_variable:local_var*"select"*/user_variable:@x*"select"*/outfile:"'/tmp/x'"*"select"*/dumpfile:"'/tmp/y'"*"set"*/user_variable:@x*"set"*/user_variable:@*my-var*"set"*/user_variable:@\"my-var\"*"set"*/user_variable:@\`my-var\`*"set"*/user_variable:@iv*"set"*/user_variable:@plus*"set"*/system_variable:@@session.sql_mode*"set"*/system_variable:sql_mode*"set"*/system_variable:sql_log_bin*"set"*/system_variable:autocommit*"set"*/system_variable:x*"get"*/user_variable:@n*"get"*/diagnostics_condition:1*) ;;
	*)
		echo "unexpected variable assignment output: $variable_assignment_output" >&2
		exit 1
		;;
esac

utility_sql='TRUNCATE t;
TRUNCATE TABLE `db`.`t`;
USE `db`;
TABLE `db`.`t`;
HANDLER `db`.`h` OPEN;
LOAD DATA INFILE "x" INTO TABLE `db`.`ld`;
CACHE INDEX c IN keycache;
LOAD INDEX INTO CACHE `db`.`li`;
LOCK TABLES `db`.`lt` READ'
utility_object_output=$("$parser" "$utility_sql")
case "$utility_object_output" in
	*"truncate"*/table:t*"truncate"*/table:'`db`.`t`'*"use"*/database:'`db`'*"table"*/table:'`db`.`t`'*"handler"*/table:'`db`.`h`'*"load"*/table:'`db`.`ld`'*"cache"*/table:c*"load"*/table:'`db`.`li`'*"lock"*/table:'`db`.`lt`'*) ;;
	*)
		echo "unexpected utility object output: $utility_object_output" >&2
		exit 1
		;;
esac

table_lock_output=$("$parser" 'LOCK TABLE t READ; LOCK TABLES `db`.`lt` AS l WRITE; UNLOCK TABLES; UNLOCK TABLE; UNLOCK INSTANCE')
case "$table_lock_output" in
	*"lock"*/table:t*"lock"*/table:'`db`.`lt`'*"unlock"*/table*"unlock"*/table*"unlock"*/instance*) ;;
	*)
		echo "unexpected table lock output: $table_lock_output" >&2
		exit 1
		;;
esac

import_output=$("$parser" "IMPORT TABLE FROM '/tmp/a.sdi', '/tmp/b.sdi'; IMPORT TABLE FROM @file")
case "$import_output" in
	*"import"*/sdi_file:"'/tmp/a.sdi'"*"import[8:11"*) ;;
	*)
		echo "unexpected IMPORT output: $import_output" >&2
		exit 1
		;;
esac

call_output=$("$parser" 'CALL p; CALL p(); CALL `db`.`p`(@a); CALL 15298_1(); CALL 1')
case "$call_output" in
	*"call"*/procedure:p*"call"*/procedure:p*"call"*/procedure:'`db`.`p`'*"call"*/procedure:15298_1*"call[22:23"*) ;;
	*)
		echo "unexpected CALL output: $call_output" >&2
		exit 1
		;;
esac

explain_sql='DESC `db`.`t`;
DESCRIBE t c;
EXPLAIN `db`.`e`;
EXPLAIN SELECT 1;
EXPLAIN FORMAT = JSON SELECT 1;
DESCRIBE SELECT 1;
EXPLAIN FOR CONNECTION 123;
EXPLAIN FORMAT = JSON FOR CONNECTION 456;
EXPLAIN SELECT 1 FOR CONNECTION 789;
EXPLAIN FORMAT = JSON INTO @plan SELECT 1'
explain_object_output=$("$parser" "$explain_sql")
case "$explain_object_output" in
	*"/table:FORMAT"*|*"/table:SELECT"*|*"explain[43:48"*/connection:*)
		echo "unexpected EXPLAIN/DESCRIBE object output: $explain_object_output" >&2
		exit 1
		;;
	*"describe"*/table:'`db`.`t`'*"describe"*/table:t*"explain"*/table:'`db`.`e`'*"explain[15:17"*/query*"explain[19:24"*/query*"describe[26:28"*/query*"connection:123"*"connection:456"*"explain[43:48"*/query*"user_variable:@plan"*) ;;
	*)
		echo "unexpected EXPLAIN/DESCRIBE object output: $explain_object_output" >&2
		exit 1
		;;
esac

explain_query_output=$("$parser" 'EXPLAIN ANALYZE SELECT 1; EXPLAIN FOR SCHEMA db SELECT 1; EXPLAIN INSERT INTO t VALUES (1); EXPLAIN UPDATE t SET c=1; EXPLAIN DELETE FROM t; EXPLAIN TABLE t')
case "$explain_query_output" in
	*"explain"*/query*"explain"*/query*"explain"*/query*"explain"*/query*"explain"*/query*"explain"*/query*) ;;
	*)
		echo "unexpected EXPLAIN query output: $explain_query_output" >&2
		exit 1
		;;
esac

show_sql=$(cat <<'SQL'
SHOW CREATE TABLE `db`.`t`;
SHOW CREATE VIEW v;
SHOW COLUMNS FROM `db`.`c`;
SHOW FULL FIELDS FROM f;
SHOW INDEXES FROM `db`.`i`;
SHOW KEYS FROM k;
SHOW TABLES FROM `db`;
SHOW CREATE USER 'u'@'h';
SHOW GRANTS FOR 'u'@'h';
SHOW GRANTS FOR CURRENT_USER();
SHOW GRANTS;
SHOW VARIABLES
SQL
)
show_object_output=$("$parser" "$show_sql")
case "$show_object_output" in
	*"/table:VARIABLES"*)
		echo "unexpected SHOW object output: $show_object_output" >&2
		exit 1
		;;
	*"show"*/table:'`db`.`t`'*"show"*/view:v*"show"*/table:'`db`.`c`'*"show"*/table:f*"show"*/table:'`db`.`i`'*"show"*/table:k*"show"*/database:'`db`'*"show"*/user:"'u'@'h'"*"show"*/user:"'u'@'h'"*"show"*/user:CURRENT_USER"()"*"show"*/user*"show"*/system_variable*) ;;
	*)
		echo "unexpected SHOW object output: $show_object_output" >&2
		exit 1
		;;
esac

show_create_output=$("$parser" 'SHOW CREATE DATABASE IF NOT EXISTS db; SHOW CREATE EVENT e; SHOW CREATE FUNCTION f; SHOW CREATE PROCEDURE p; SHOW CREATE TRIGGER tr; SHOW CREATE TABLE t; SHOW CREATE VIEW v; SHOW CREATE USER CURRENT_USER()')
case "$show_create_output" in
	*"show"*/database:db*"show"*/event:e*"show"*/function:f*"show"*/procedure:p*"show"*/trigger:tr*"show"*/table:t*"show"*/view:v*"show"*/user:CURRENT_USER"()"*) ;;
	*)
		echo "unexpected SHOW CREATE output: $show_create_output" >&2
		exit 1
		;;
esac

show_table_detail_output=$("$parser" 'SHOW COLUMNS FROM t; SHOW FULL FIELDS FROM `db`.`v` LIKE "id%"; SHOW EXTENDED COLUMNS IN t IN `db` WHERE Field = "id"; SHOW INDEX FROM t; SHOW EXTENDED INDEXES IN `db`.`t`; SHOW KEYS FROM k FROM `db`')
case "$show_table_detail_output" in
	*"show"*/table:t*"show"*/table:'`db`.`v`'*"show"*/table:t*"show"*/table:t*"show"*/table:'`db`.`t`'*"show"*/table:k*) ;;
	*)
		echo "unexpected SHOW table detail output: $show_table_detail_output" >&2
		exit 1
		;;
esac

show_variable_output=$("$parser" "SHOW VARIABLES; SHOW VARIABLES LIKE 'autocommit'; SHOW SESSION VARIABLES LIKE 'sql_mode'; SHOW LOCAL VARIABLES WHERE Variable_name = 'time_zone'; SHOW STATUS; SHOW GLOBAL STATUS LIKE 'Com_select'; SHOW SESSION STATUS LIKE 'Bytes_sent'")
case "$show_variable_output" in
	*"show"*/system_variable*"show"*/system_variable:"'autocommit'"*"show"*/system_variable:"'sql_mode'"*"show"*/system_variable*"show"*/status_variable*"show"*/status_variable:"'Com_select'"*"show"*/status_variable:"'Bytes_sent'"*) ;;
	*)
		echo "unexpected SHOW variable/status output: $show_variable_output" >&2
		exit 1
		;;
esac

show_diagnostics_output=$("$parser" 'SHOW WARNINGS; SHOW WARNINGS LIMIT 5; SHOW ERRORS; SHOW ERRORS LIMIT 2, 10; SHOW COUNT(*) WARNINGS; SHOW COUNT(*) ERRORS; SHOW STATUS')
case "$show_diagnostics_output" in
	*"show"*/diagnostics_area*"show"*/diagnostics_area*"show"*/diagnostics_area*"show"*/diagnostics_area*"show"*/diagnostics_area*"show"*/diagnostics_area*"show"*/status_variable*) ;;
	*)
		echo "unexpected SHOW diagnostics output: $show_diagnostics_output" >&2
		exit 1
		;;
esac

show_database_output=$("$parser" "SHOW DATABASES; SHOW DATABASES LIKE 'wp%'; SHOW SCHEMAS; SHOW SCHEMAS WHERE Schema_name = 'test'; SHOW TABLES FROM db")
case "$show_database_output" in
	*"show"*/database*"show"*/database:"'wp%'"*"show"*/database*"show"*/database*"show"*/database:db*) ;;
	*)
		echo "unexpected SHOW database output: $show_database_output" >&2
		exit 1
		;;
esac

show_charset_output=$("$parser" "SHOW CHARACTER SET; SHOW CHARACTER SET LIKE 'utf8%'; SHOW CHARSET LIKE 'latin1'; SHOW COLLATION; SHOW COLLATION LIKE 'utf8mb4_0900_ai_ci'; SHOW COLLATION WHERE Charset = 'latin1'; SHOW CHARACTERISTICS AS TRANSACTION READ WRITE")
case "$show_charset_output" in
	*"show"*/character_set*"show"*/character_set:"'utf8%'"*"show"*/character_set:"'latin1'"*"show"*/collation*"show"*/collation:"'utf8mb4_0900_ai_ci'"*"show"*/collation*"show[31:36"*) ;;
	*)
		echo "unexpected SHOW character set/collation output: $show_charset_output" >&2
		exit 1
		;;
esac

show_schema_output=$("$parser" 'SHOW TABLE STATUS FROM `db`; SHOW OPEN TABLES FROM `db`; SHOW TRIGGERS IN `db`; SHOW EVENTS FROM `db`; SHOW TABLES; SHOW TABLES LIKE "wp_%"; SHOW TABLE STATUS LIKE "wp_%"; SHOW OPEN TABLES; SHOW OPEN TABLES LIKE "wp_%"; SHOW EVENTS; SHOW EVENTS LIKE "e_%"; SHOW TRIGGERS; SHOW TRIGGERS LIKE "wp_%"')
case "$show_schema_output" in
	*"show"*/database:'`db`'*"show"*/database:'`db`'*"show"*/database:'`db`'*"show"*/database:'`db`'*"show"*/table*"show"*/table:'"wp_%"'*"show"*/table:'"wp_%"'*"show"*/table*"show"*/table:'"wp_%"'*"show"*/event*"show"*/event:'"e_%"'*"show"*/trigger*"show"*/table:'"wp_%"'*) ;;
	*)
		echo "unexpected SHOW schema output: $show_schema_output" >&2
		exit 1
		;;
esac

show_routine_code_output=$("$parser" 'SHOW FUNCTION CODE f; SHOW PROCEDURE CODE p; SHOW FUNCTION STATUS; SHOW FUNCTION STATUS LIKE "f%"; SHOW PROCEDURE STATUS; SHOW PROCEDURE STATUS LIKE "p%"')
case "$show_routine_code_output" in
	*"show"*/function:f*"show"*/procedure:p*"show"*/function*"show"*/function:'"f%"'*"show"*/procedure*"show"*/procedure:'"p%"'*) ;;
	*)
		echo "unexpected SHOW routine code output: $show_routine_code_output" >&2
		exit 1
		;;
esac

show_engine_output=$("$parser" 'SHOW ENGINE InnoDB STATUS; SHOW ENGINE performance_schema MUTEX; SHOW ENGINE NDB STATUS; SHOW ENGINE InnoDB LOGS; SHOW ENGINES')
case "$show_engine_output" in
	*"show"*/engine:InnoDB*"show"*/engine:performance_schema*"show"*/engine:NDB*"show[16:19"*"show"*/engine*) ;;
	*)
		echo "unexpected SHOW engine output: $show_engine_output" >&2
		exit 1
		;;
esac

show_collection_output=$("$parser" 'SHOW ENGINES; SHOW STORAGE ENGINES; SHOW PLUGINS; SHOW PRIVILEGES; SHOW PROCESSLIST; SHOW FULL PROCESSLIST; SHOW ENGINE InnoDB STATUS; SHOW CREATE USER current_user()')
case "$show_collection_output" in
	*"show"*/engine*"show"*/engine*"show"*/plugin*"show"*/privilege*"show"*/connection*"show"*/connection*"show"*/engine:InnoDB*"show"*/user:current_user"()"*) ;;
	*)
		echo "unexpected SHOW collection output: $show_collection_output" >&2
		exit 1
		;;
esac

show_profile_output=$("$parser" 'SHOW PROFILE; SHOW PROFILES; SHOW PROFILE FOR QUERY 1; SHOW PROFILE CPU FOR QUERY 2; SHOW PROFILE FOR QUERY @q')
case "$show_profile_output" in
	*"show"*/query*"show"*/query*"show"*/query:1*"show"*/query:2*"show[20:24"*) ;;
	*)
		echo "unexpected SHOW PROFILE output: $show_profile_output" >&2
		exit 1
		;;
esac

show_parse_tree_output=$("$parser" 'SHOW PARSE_TREE SELECT 1; SHOW PARSE_TREE SELECT * FROM t; SHOW PARSE_TREE UPDATE t SET a = 1')
case "$show_parse_tree_output" in
	"ok statements=3 kinds=show[1:4,0:24]/query,show[6:11,26:57]/query,show[13:20,59:93]") ;;
	*)
		echo "unexpected SHOW PARSE_TREE output: $show_parse_tree_output" >&2
		exit 1
		;;
esac

binary_log_output=$("$parser" "SHOW BINARY LOGS; SHOW MASTER LOGS; SHOW BINARY LOG STATUS; SHOW MASTER STATUS; SHOW BINLOG EVENTS IN 'bin.000001' FROM 4; SHOW BINLOG EVENTS; PURGE BINARY LOGS TO 'bin.000001'; PURGE BINARY LOGS BEFORE NOW(); PURGE MASTER LOGS BEFORE '2024-01-01'")
case "$binary_log_output" in
	*"show"*/binary_log*"show"*/binary_log*"show"*/binary_log*"show"*/binary_log*"show"*/binary_log:"'bin.000001'"*"show"*/binary_log*"purge"*/binary_log:"'bin.000001'"*"purge"*/binary_log*"purge"*/binary_log*) ;;
	*)
		echo "unexpected binary log output: $binary_log_output" >&2
		exit 1
		;;
esac

relay_log_output=$("$parser" "SHOW RELAYLOG EVENTS IN 'relay.000001' FROM 4; SHOW RELAYLOG EVENTS FOR CHANNEL 'ch'; SHOW RELAYLOG EVENTS; SHOW RELAYLOG EVENTS IN 'relay.000001' FOR CHANNEL 'ch'")
case "$relay_log_output" in
	*"show"*/relay_log:"'relay.000001'"*"show"*/replication_channel:"'ch'"*"show"*/relay_log*"show"*/relay_log:"'relay.000001'"*) ;;
	*)
		echo "unexpected relay log output: $relay_log_output" >&2
		exit 1
		;;
esac

replica_status_output=$("$parser" "SHOW REPLICAS; SHOW SLAVE HOSTS; SHOW REPLICA STATUS FOR CHANNEL 'ch'; SHOW REPLICA STATUS; SHOW SLAVE STATUS FOR CHANNEL 'old'; SHOW SLAVE STATUS")
case "$replica_status_output" in
	*"show"*/replication_channel*"show"*/replication_channel*"show"*/replication_channel:"'ch'"*"show"*/replication_channel*"show"*/replication_channel:"'old'"*"show"*/replication_channel*) ;;
	*)
		echo "unexpected SHOW REPLICA STATUS output: $replica_status_output" >&2
		exit 1
		;;
esac

binlog_event_output=$("$parser" "BINLOG 'abc'; BINLOG @payload")
case "$binlog_event_output" in
	*"binlog"*/binary_log_event:"'abc'"*"binlog[4:5"*) ;;
	*)
		echo "unexpected BINLOG event output: $binlog_event_output" >&2
		exit 1
		;;
esac

kill_output=$("$parser" 'KILL 123; KILL QUERY 456; KILL CONNECTION 789; KILL QUERY; KILL QUERY @id; KILL USER "u"')
case "$kill_output" in
	*"/connection:@id"*|*"/connection:USER"*|*"/query:@id"*)
		echo "unexpected KILL non-numeric connection output: $kill_output" >&2
		exit 1
		;;
	*"kill"*/connection:123*"kill"*/query:456*"kill"*/connection:789*"kill[12:13"*"kill[15:17"*"kill[19:21"*) ;;
	*)
		echo "unexpected KILL output: $kill_output" >&2
		exit 1
		;;
esac

flush_output=$("$parser" 'FLUSH TABLES t; FLUSH LOCAL TABLES t; FLUSH NO_WRITE_TO_BINLOG TABLES `db`.`t`; FLUSH TABLES WITH READ LOCK; FLUSH BINARY LOGS; FLUSH LOCAL BINARY LOGS; FLUSH PRIVILEGES; FLUSH STATUS')
case "$flush_output" in
	*"flush"*/table:t*"flush"*/table:t*"flush"*/table:'`db`.`t`'*"flush[17:21"*/table*"flush"*/binary_log*"flush"*/binary_log*"flush"*/privilege*"flush"*/status_variable*) ;;
	*)
		echo "unexpected FLUSH output: $flush_output" >&2
		exit 1
		;;
esac

flush_table_collection_output=$("$parser" 'FLUSH TABLES; FLUSH TABLES WITH READ LOCK; FLUSH TABLE WITH READ LOCK; FLUSH TABLES t FOR EXPORT; FLUSH TABLES t, u WITH READ LOCK')
case "$flush_table_collection_output" in
	*"flush[1:2"*/table*"flush[4:8"*/table*"flush[10:14"*/table*"flush"*/table:t*"flush"*/table:t*) ;;
	*)
		echo "unexpected FLUSH table collection output: $flush_table_collection_output" >&2
		exit 1
		;;
esac

flush_global_output=$("$parser" 'FLUSH ENGINE LOGS; FLUSH ERROR LOGS; FLUSH GENERAL LOGS; FLUSH LOGS; FLUSH SLOW LOGS; FLUSH HOSTS; FLUSH OPTIMIZER_COSTS; FLUSH USER_RESOURCES; FLUSH NO_WRITE_TO_BINLOG ERROR LOGS')
case "$flush_global_output" in
	*"flush"*/engine_log*"flush"*/error_log*"flush"*/general_log*"flush"*/log*"flush"*/slow_log*"flush"*/host_cache*"flush"*/optimizer_cost*"flush"*/user_resource*"flush"*/error_log*) ;;
	*)
		echo "unexpected global FLUSH output: $flush_global_output" >&2
		exit 1
		;;
esac

flush_relay_output=$("$parser" "FLUSH RELAY LOGS FOR CHANNEL 'ch'; FLUSH NO_WRITE_TO_BINLOG RELAY LOGS FOR CHANNEL 'ch2'; FLUSH RELAY LOGS; FLUSH TABLES t")
case "$flush_relay_output" in
	*"flush"*/replication_channel:"'ch'"*"flush"*/replication_channel:"'ch2'"*"flush[16:18"*/replication_channel*"flush"*/table:t*) ;;
	*)
		echo "unexpected FLUSH RELAY output: $flush_relay_output" >&2
		exit 1
		;;
esac

maintenance_output=$("$parser" 'ANALYZE TABLE t; CHECK TABLE `db`.`t`; CHECKSUM TABLE t QUICK; OPTIMIZE TABLE t; REPAIR TABLE t USE_FRM; ANALYZE FORMAT=JSON TABLE t; CHECKSUM TABLE QUICK')
case "$maintenance_output" in
	*"analyze"*/table:t*"check"*/table:'`db`.`t`'*"checksum"*/table:t*"optimize"*/table:t*"repair"*/table:t*"analyze"*/table:t*"checksum[32:34"*) ;;
	*)
		echo "unexpected maintenance output: $maintenance_output" >&2
		exit 1
		;;
esac

reset_output=$("$parser" 'RESET PERSIST max_connections; RESET PERSIST IF EXISTS autocommit; RESET PERSIST; RESET PERSIST IF EXISTS; RESET BINARY LOGS AND GTIDS; RESET BINARY LOGS AND GTIDS TO 100; RESET MASTER; RESET MASTER TO 100')
case "$reset_output" in
	*"/system_variable:IF"*)
		echo "unexpected RESET PERSIST IF target output: $reset_output" >&2
		exit 1
		;;
esac
case "$reset_output" in
	*"reset"*/system_variable:max_connections*"reset"*/system_variable:autocommit*"reset[11:12"*/system_variable*"reset[14:17"*",reset[19:23"*/binary_log*"reset"*/binary_log*"reset"*/binary_log*"reset"*/binary_log*) ;;
	*)
		echo "unexpected RESET output: $reset_output" >&2
		exit 1
		;;
esac

replication_channel_output=$("$parser" "START REPLICA FOR CHANNEL 'ch'; STOP REPLICA SQL_THREAD FOR CHANNEL 'ch'; RESET REPLICA ALL FOR CHANNEL 'ch'; CHANGE REPLICATION SOURCE TO SOURCE_HOST='h' FOR CHANNEL 'ch'; CHANGE REPLICATION FILTER REPLICATE_DO_DB=(db) FOR CHANNEL 'ch'; START REPLICA; STOP REPLICA; RESET REPLICA; CHANGE REPLICATION SOURCE TO SOURCE_HOST='h'; CHANGE MASTER TO MASTER_HOST='h'; START GROUP_REPLICATION; STOP GROUP_REPLICATION")
case "$replication_channel_output" in
	*"start"*/replication_channel:"'ch'"*"stop"*/replication_channel:"'ch'"*"reset"*/replication_channel:"'ch'"*"change"*/replication_channel:"'ch'"*"change"*/replication_channel:"'ch'"*"start"*/replication_channel*"stop"*/replication_channel*"reset"*/replication_channel*"change"*/replication_channel*"change"*/replication_channel*"start"*/group_replication*"stop"*/group_replication*) ;;
	*)
		echo "unexpected replication channel output: $replication_channel_output" >&2
		exit 1
		;;
esac

xa_output=$("$parser" "XA START 'x'; XA END 'x'; XA PREPARE 'x'; XA COMMIT 'x' ONE PHASE; XA ROLLBACK 'x'; XA RECOVER; XA RECOVER CONVERT XID")
case "$xa_output" in
	*"xa"*/xa_transaction:"'x'"*"xa"*/xa_transaction:"'x'"*"xa"*/xa_transaction:"'x'"*"xa"*/xa_transaction:"'x'"*"xa"*/xa_transaction:"'x'"*"xa[23:24"*"xa[26:29"*) ;;
	*)
		echo "unexpected XA output: $xa_output" >&2
		exit 1
		;;
esac

help_output=$("$parser" "HELP 'contents'; HELP SELECT; HELP 'CREATE TABLE'")
case "$help_output" in
	*"help"*/help_topic:"'contents'"*"help[4:5"*"help"*/help_topic:"'CREATE TABLE'"*) ;;
	*)
		echo "unexpected HELP output: $help_output" >&2
		exit 1
		;;
esac

clone_output=$("$parser" "CLONE LOCAL DATA DIRECTORY = '/tmp/clone'; CLONE LOCAL DATA DIRECTORY '/tmp/clone2'; CLONE INSTANCE FROM user@host:3306 IDENTIFIED BY 'p'; CLONE INSTANCE FROM 'u'@'h':3306 IDENTIFIED BY 'p' DATA DIRECTORY = '/tmp/clone' REQUIRE NO SSL")
case "$clone_output" in
	*"clone"*/directory:"'/tmp/clone'"*"clone"*/directory:"'/tmp/clone2'"*"clone"*/server:user@host:3306*"clone"*/server:"'u'@'h':3306"*) ;;
	*)
		echo "unexpected CLONE output: $clone_output" >&2
		exit 1
		;;
esac

stop_output=$("$parser" 'STOP REPLICA; STOP GROUP_REPLICATION; STOP SLAVE SQL_THREAD; CREATE TABLE stop (id int)')
case "$stop_output" in
	*"stop"*/replication_channel*"stop"*/group_replication*"stop"*/replication_channel*"create"*/table:stop*) ;;
	*)
		echo "unexpected STOP output: $stop_output" >&2
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

account_ddl_output=$("$parser" "CREATE USER 'u'@'h'; ALTER USER 'u'@'%'; DROP USER IF EXISTS 'u'@'%'; RENAME USER 'u'@'h' TO 'v'@'h'; CREATE ROLE IF NOT EXISTS 'r'@'%'; DROP ROLE r")
case "$account_ddl_output" in
	*"create"*/user:"'u'@'h'"*"alter"*/user:"'u'@'%'"*"drop"*/user:"'u'@'%'"*"rename"*/user:"'u'@'h'"*"create"*/role:"'r'@'%'"*"drop"*/role:r*) ;;
	*)
		echo "unexpected account DDL output: $account_ddl_output" >&2
		exit 1
		;;
esac

set_account_output=$("$parser" "SET ROLE r; SET ROLE ALL; SET ROLE NONE; SET ROLE ALL EXCEPT 'r'@'h'; SET ROLE DEFAULT; SET DEFAULT ROLE r TO 'u'@'h'; SET DEFAULT ROLE ALL TO 'u'@'h'; SET PASSWORD FOR 'u'@'h' = 'x'; SET PASSWORD = 'x'; SET PASSWORD TO RANDOM; SET autocommit=1")
case "$set_account_output" in
	*"set"*/role:r*"set[5:7"*/role*"set[9:11"*/role*"set"*/role:"'r'@'h'"*"set[20:22"*/role*"set"*/role:r*"set"*/user:"'u'@'h'"*"set"*/user:"'u'@'h'"*"set[48:51"*/user*"set[53:56"*/user*"set[58:61"*/system_variable:autocommit*) ;;
	*)
		echo "unexpected SET account output: $set_account_output" >&2
		exit 1
		;;
esac

set_charset_output=$("$parser" "SET NAMES utf8mb4 COLLATE utf8mb4_0900_ai_ci; SET NAMES DEFAULT; SET CHARACTER SET 'latin1'; SET CHARSET DEFAULT; SET CHARACTERISTICS AS TRANSACTION READ WRITE")
case "$set_charset_output" in
	*"set"*/character_set:utf8mb4*"set"*/character_set:DEFAULT*"set"*/character_set:"'latin1'"*"set"*/character_set:DEFAULT*"set[20:25"*) ;;
	*)
		echo "unexpected SET character set output: $set_charset_output" >&2
		exit 1
		;;
esac

resource_group_output=$("$parser" 'CREATE RESOURCE GROUP rg TYPE = USER; ALTER RESOURCE GROUP rg ENABLE; DROP RESOURCE GROUP rg; SET RESOURCE GROUP rg')
case "$resource_group_output" in
	*"create"*/resource_group:rg*"alter"*/resource_group:rg*"drop"*/resource_group:rg*"set"*/resource_group:rg*) ;;
	*)
		echo "unexpected resource group output: $resource_group_output" >&2
		exit 1
		;;
esac

server_logfile_output=$("$parser" 'CREATE SERVER s FOREIGN DATA WRAPPER mysql OPTIONS (HOST "h"); ALTER SERVER s OPTIONS (USER "u"); DROP SERVER s; CREATE LOGFILE GROUP lg ADD UNDOFILE "u.dat"; ALTER LOGFILE GROUP lg ADD UNDOFILE "v.dat"; DROP LOGFILE GROUP lg ENGINE NDB; CREATE TABLESPACE ts ADD DATAFILE "ts.ibd"; ALTER UNDO TABLESPACE uts SET INACTIVE; DROP UNDO TABLESPACE uts')
case "$server_logfile_output" in
	*"create"*/server:s*"alter"*/server:s*"drop"*/server:s*"create"*/logfile_group:lg*"alter"*/logfile_group:lg*"drop"*/logfile_group:lg*"create"*/tablespace:ts*"alter"*/undo_tablespace:uts*"drop"*/undo_tablespace:uts*) ;;
	*)
		echo "unexpected server/logfile output: $server_logfile_output" >&2
		exit 1
		;;
esac

instance_output=$("$parser" 'RESTART; SHUTDOWN; ALTER INSTANCE ROTATE INNODB MASTER KEY; ALTER INSTANCE RELOAD TLS; LOCK INSTANCE FOR BACKUP; UNLOCK INSTANCE; LOCK TABLES t READ; ALTER TABLE t ADD COLUMN c int')
case "$instance_output" in
	*"restart"*/instance*"shutdown"*/instance*"alter"*/instance*"alter"*/instance*"lock"*/instance*"unlock"*/instance*"lock"*/table:t*"alter"*/table:t*) ;;
	*)
		echo "unexpected instance output: $instance_output" >&2
		exit 1
		;;
esac

install_output=$("$parser" "INSTALL PLUGIN p SONAME 'x.so'; UNINSTALL PLUGIN p; INSTALL COMPONENT 'file://component'; UNINSTALL COMPONENT 'file://component'")
case "$install_output" in
	*"install"*/plugin:p*"uninstall"*/plugin:p*"install"*/component:"'file://component'"*"uninstall"*/component:"'file://component'"*) ;;
	*)
		echo "unexpected install output: $install_output" >&2
		exit 1
		;;
esac

savepoint_output=$("$parser" 'SAVEPOINT s; RELEASE SAVEPOINT s; ROLLBACK TO SAVEPOINT `s`; ROLLBACK TO `s`; ROLLBACK WORK TO s; ROLLBACK WORK TO SAVEPOINT s; ROLLBACK; ROLLBACK WORK AND CHAIN; RELEASE s')
case "$savepoint_output" in
	*"savepoint"*/savepoint:s*"release"*/savepoint:s*"rollback"*/savepoint:'`s`'*"rollback"*/savepoint:'`s`'*"rollback"*/savepoint:s*"rollback"*/savepoint:s*"rollback"*/transaction*"rollback"*/transaction*"release[35:36"*) ;;
	*)
		echo "unexpected savepoint output: $savepoint_output" >&2
		exit 1
		;;
esac

transaction_output=$("$parser" "BEGIN; BEGIN WORK; BEGIN NOT ATOMIC SELECT 1 END; START TRANSACTION; START TRANSACTION READ WRITE; START REPLICA FOR CHANNEL 'ch'; COMMIT; COMMIT AND CHAIN; ROLLBACK; ROLLBACK AND NO CHAIN; ROLLBACK TO SAVEPOINT s; SET TRANSACTION ISOLATION LEVEL READ COMMITTED; SET SESSION TRANSACTION READ ONLY; SET GLOBAL TRANSACTION READ WRITE; SET SESSION sql_mode = 'ANSI'")
case "$transaction_output" in
	*"begin"*/transaction*"begin"*/transaction*"begin[6:11"*"start"*/transaction*"start"*/transaction*"start"*/replication_channel:"'ch'"*"commit"*/transaction*"commit"*/transaction*"rollback"*/transaction*"rollback"*/transaction*"rollback"*/savepoint:s*"set"*/transaction*"set"*/transaction*"set"*/transaction*"set"*/system_variable:sql_mode*) ;;
	*)
		echo "unexpected transaction output: $transaction_output" >&2
		exit 1
		;;
esac

begin_block_output=$("$parser" 'BEGIN SELECT 1; END blk; BEGIN END; BEGIN; BEGIN WORK')
case "$begin_block_output" in
	*"kinds=begin[1:6"*"begin[8:9"*"begin"*/transaction*"begin"*/transaction*) ;;
	*)
		echo "unexpected BEGIN block output: $begin_block_output" >&2
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

operator_sign_output=$("$parser" --tokens 'SET @iv=-20010101; SET @plus=+.5; SELECT a<=-1, b>=+.5, c<=>-4')
case "$operator_sign_output" in
	*"=-"*|*"=+"*|*"<=-"*|*">=+"*|*"<=>-"*)
		echo "unexpected signed-number operator token output: $operator_sign_output" >&2
		exit 1
		;;
esac
case "$operator_sign_output" in
	*"set"*/user_variable:@iv*"set"*/user_variable:@plus*"token 3 operator"*"token 4 punctuation"*"token 9 operator"*"token 10 punctuation"*"token 15 operator"*"token 16 punctuation"*"token 20 operator"*"token 21 punctuation"*"token 25 operator"*"token 26 punctuation"*) ;;
	*)
		echo "unexpected signed-number token output: $operator_sign_output" >&2
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

nonreserved_keyword_name_output=$("$parser" 'CREATE TABLE cache (id int); CREATE TABLE clone (id int); CREATE TABLE commit (id int); CREATE TABLE flush (id int); CREATE TABLE handler (id int); CREATE TABLE help (id int); CREATE TABLE prepare (id int); CREATE TABLE repair (id int); CREATE TABLE reset (id int); CREATE TABLE rollback (id int); CREATE TABLE savepoint (id int); CREATE TABLE xa (id int)')
case "$nonreserved_keyword_name_output" in
	*"create"*/table:cache*"create"*/table:clone*"create"*/table:commit*"create"*/table:flush*"create"*/table:handler*"create"*/table:help*"create"*/table:prepare*"create"*/table:repair*"create"*/table:reset*"create"*/table:rollback*"create"*/table:savepoint*"create"*/table:xa*) ;;
	*)
		echo "unexpected nonreserved keyword name output: $nonreserved_keyword_name_output" >&2
		exit 1
		;;
esac

nonreserved_modifier_name_output=$("$parser" 'CREATE TABLE temporary (id int); CREATE TABLE charset (id int); CREATE TABLE engine (id int); CREATE TABLE event (id int); CREATE TABLE offset (id int); CREATE TABLE quick (id int); CREATE TABLE role (id int); CREATE TABLE user (id int); CREATE TABLE until (id int); CREATE TABLE value (id int); CREATE TABLE view (id int)')
case "$nonreserved_modifier_name_output" in
	*"create"*/table:temporary*"create"*/table:charset*"create"*/table:engine*"create"*/table:event*"create"*/table:offset*"create"*/table:quick*"create"*/table:role*"create"*/table:user*"create"*/table:until*"create"*/table:value*"create"*/table:view*) ;;
	*)
		echo "unexpected nonreserved modifier name output: $nonreserved_modifier_name_output" >&2
		exit 1
		;;
esac

numeric_identifier_output=$("$parser" --tokens 'CREATE TABLE 1abc (id int); CALL 15298_1(); CREATE TABLE 123_abc (id int); SELECT 1e3, 0x1f, 0b1010, .5')
case "$numeric_identifier_output" in
	*"create"*/table:1abc*"call"*/procedure:15298_1*"create"*/table:123_abc*"token 3 identifier"*"token 10 identifier"*"token 16 identifier"*"token 23 number"*"token 25 number"*"token 27 number"*"token 29 number"*) ;;
	*)
		echo "unexpected numeric identifier output: $numeric_identifier_output" >&2
		exit 1
		;;
esac

begin_end_name_output=$("$parser" 'CREATE TABLE begin (id int); CREATE TABLE end (id int); CREATE TABLE t (begin int, end int); DROP TABLE begin; RENAME TABLE old TO begin; BEGIN')
case "$begin_end_name_output" in
	*"create"*/table:begin*"create"*/table:end*"create"*/table:t*"drop"*/table:begin*"rename"*/table:old*"begin"*/transaction*) ;;
	*)
		echo "unexpected BEGIN/END name output: $begin_end_name_output" >&2
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
