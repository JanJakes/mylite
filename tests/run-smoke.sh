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
	*"select[1:2,0:8]/query,commit[4:4,10:16]"*) ;;
	*)
		echo "unexpected statement span output: $span_output" >&2
		exit 1
		;;
esac

grouped_query_output=$("$parser" '(SELECT 1) UNION SELECT 2; ((VALUES ROW(1),ROW(2))) ORDER BY 1; (TABLE t)')
case "$grouped_query_output" in
	*"kinds=select[1:7,0:25]/query,values[9:25,27:62]/query,table[27:30,64:73]/table:t"*) ;;
	*)
		echo "unexpected grouped query output: $grouped_query_output" >&2
		exit 1
		;;
esac

values_query_output=$("$parser" 'VALUES ROW(1), ROW(2); VALUES ROW(1), ROW(2) ORDER BY column_0 DESC LIMIT 1; ((VALUES ROW(3))) ORDER BY 1; EXPLAIN VALUES ROW(1)')
case "$values_query_output" in
	*"values"*/query*"values"*/query*"values"*/query*"explain"*) ;;
	*)
		echo "unexpected values query output: $values_query_output" >&2
		exit 1
		;;
esac

if "$parser" --quiet 'VALUES'; then
	echo "expected missing VALUES row constructor to fail" >&2
	exit 1
fi

if "$parser" --quiet 'VALUES (1)'; then
	echo "expected VALUES without ROW constructor to fail" >&2
	exit 1
fi

if "$parser" --quiet 'VALUES ROW()'; then
	echo "expected empty VALUES ROW constructor to fail" >&2
	exit 1
fi

if "$parser" --quiet 'VALUES ROW(1),'; then
	echo "expected trailing VALUES row comma to fail" >&2
	exit 1
fi

if "$parser" --quiet 'VALUES ROW(1) ROW(2)'; then
	echo "expected missing VALUES row comma to fail" >&2
	exit 1
fi

if "$parser" --quiet 'VALUES ROW(1) ORDER'; then
	echo "expected incomplete VALUES ORDER BY to fail" >&2
	exit 1
fi

if "$parser" --quiet 'VALUES ROW(1) LIMIT'; then
	echo "expected missing VALUES LIMIT count to fail" >&2
	exit 1
fi

do_query_output=$("$parser" 'DO 1 + 1; DO SLEEP(1); DO 1, SLEEP(0), @a := 2; DO (SELECT @x:=b FROM t1 WHERE a=5); DO ST_AsText(@p) AS p')
case "$do_query_output" in
	*"kinds=do[1:4,0:8]/query,do[6:10,10:21]/query,do"*/query*",do"*/query*",do"*/query*) ;;
	*)
		echo "unexpected DO query output: $do_query_output" >&2
		exit 1
		;;
esac

if "$parser" --quiet 'DO , 1'; then
	echo "expected leading DO expression-list comma to fail" >&2
	exit 1
fi

if "$parser" --quiet 'DO 1,'; then
	echo "expected trailing DO expression-list comma to fail" >&2
	exit 1
fi

if "$parser" --quiet 'DO 1,,2'; then
	echo "expected empty DO expression-list item to fail" >&2
	exit 1
fi

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

cursor_output=$("$parser" 'DECLARE c CURSOR FOR SELECT 1; DECLARE x INT; DECLARE y INT; OPEN c; FETCH c INTO x; FETCH c INTO x, y; FETCH FROM c INTO x; FETCH NEXT FROM c INTO x; CLOSE c; CREATE TABLE cursor (id int)')
case "$cursor_output" in
	*"declare"*/cursor:c*"declare"*/local_variable:x*"declare"*/local_variable:y*"open"*/cursor:c*"fetch"*/cursor:c*"fetch"*/cursor:c*"fetch"*/cursor:c*"fetch"*/cursor:c*"close"*/cursor:c*"create"*/table:cursor*) ;;
	*)
		echo "unexpected cursor output: $cursor_output" >&2
		exit 1
	;;
esac

if "$parser" --quiet 'OPEN'; then
	echo "expected missing OPEN cursor name to fail" >&2
	exit 1
fi

if "$parser" --quiet 'OPEN c extra'; then
	echo "expected trailing OPEN cursor tokens to fail" >&2
	exit 1
fi

if "$parser" --quiet 'FETCH c'; then
	echo "expected FETCH without INTO list to fail" >&2
	exit 1
fi

if "$parser" --quiet 'FETCH c INTO'; then
	echo "expected FETCH without INTO target to fail" >&2
	exit 1
fi

if "$parser" --quiet 'FETCH NEXT c INTO x'; then
	echo "expected FETCH NEXT without FROM to fail" >&2
	exit 1
fi

if "$parser" --quiet 'FETCH FROM INTO x'; then
	echo "expected FETCH FROM without cursor name to fail" >&2
	exit 1
fi

if "$parser" --quiet 'FETCH c INTO x,'; then
	echo "expected trailing FETCH variable comma to fail" >&2
	exit 1
fi

if "$parser" --quiet 'FETCH c INTO 1'; then
	echo "expected numeric FETCH target to fail" >&2
	exit 1
fi

if "$parser" --quiet 'CLOSE c extra'; then
	echo "expected trailing CLOSE cursor tokens to fail" >&2
	exit 1
fi

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

view_output=$("$parser" "CREATE OR REPLACE ALGORITHM=MERGE DEFINER = user@localhost SQL SECURITY INVOKER VIEW db.v (a,b) AS SELECT 1,2 WITH CASCADED CHECK OPTION; ALTER DEFINER = CURRENT_USER() SQL SECURITY DEFINER ALGORITHM=TEMPTABLE VIEW v AS TABLE t WITH LOCAL CHECK OPTION; CREATE VIEW v2 AS (SELECT 1 ORDER BY 1) UNION (SELECT 2) WITH CHECK OPTION")
case "$view_output" in
	*"create"*/view:db.v*"alter"*/view:v*"create"*/view:v2*) ;;
	*)
		echo "unexpected view DDL output: $view_output" >&2
		exit 1
		;;
esac

if "$parser" --quiet 'CREATE VIEW'; then
	echo "expected CREATE VIEW without a name to fail" >&2
	exit 1
fi

if "$parser" --quiet 'CREATE VIEW v SELECT 1'; then
	echo "expected CREATE VIEW without AS to fail" >&2
	exit 1
fi

if "$parser" --quiet 'CREATE VIEW v AS'; then
	echo "expected CREATE VIEW without query to fail" >&2
	exit 1
fi

if "$parser" --quiet 'CREATE VIEW v () AS SELECT 1'; then
	echo "expected CREATE VIEW empty column list to fail" >&2
	exit 1
fi

if "$parser" --quiet 'CREATE VIEW v AS SELECT 1 WITH BAD CHECK OPTION'; then
	echo "expected CREATE VIEW malformed CHECK OPTION to fail" >&2
	exit 1
fi

if "$parser" --quiet 'CREATE ALGORITHM=FAST VIEW v AS SELECT 1'; then
	echo "expected CREATE VIEW invalid ALGORITHM to fail" >&2
	exit 1
fi

if "$parser" --quiet 'CREATE DEFINER user@localhost VIEW v AS SELECT 1'; then
	echo "expected CREATE VIEW DEFINER without equals to fail" >&2
	exit 1
fi

if "$parser" --quiet 'ALTER VIEW v SELECT 1'; then
	echo "expected ALTER VIEW without AS to fail" >&2
	exit 1
fi

if "$parser" --quiet 'ALTER SQL SECURITY CURRENT_USER VIEW v AS SELECT 1'; then
	echo "expected ALTER VIEW invalid SQL SECURITY value to fail" >&2
	exit 1
fi

if "$parser" --quiet 'CREATE OR VIEW v AS SELECT 1'; then
	echo "expected CREATE OR VIEW to fail" >&2
	exit 1
fi

event_output=$("$parser" "CREATE DEFINER=CURRENT_USER EVENT e1 ON SCHEDULE EVERY 1 DAY DO SELECT 1; CREATE DEFINER=mysqltest_u1@localhost EVENT db.e2 ON SCHEDULE AT CURRENT_TIMESTAMP + INTERVAL 1 HOUR ON COMPLETION NOT PRESERVE DISABLE ON REPLICA COMMENT 'c' DO SELECT 2; CREATE EVENT e3 ON SCHEDULE EVERY 1 WEEK DO BEGIN SELECT 1; END; ALTER DEFINER=mysqltest_u1@localhost EVENT e1; ALTER DEFINER=mysqltest_u1@localhost EVENT e1 ON SCHEDULE EVERY '2:3' DAY_HOUR STARTS CURRENT_TIMESTAMP + INTERVAL 1 HOUR ENDS CURRENT_TIMESTAMP + INTERVAL 2 HOUR; ALTER EVENT e1 ON COMPLETION PRESERVE RENAME TO db.e2 ENABLE COMMENT 'x'; ALTER EVENT db.e2 DISABLE ON SLAVE DO SELECT 2")
case "$event_output" in
	*"create"*/event:e1*"create"*/event:db.e2*"create"*/event:e3*"alter"*/event:e1*"alter"*/event:e1*"alter"*/event:e1*"alter"*/event:db.e2*) ;;
	*)
		echo "unexpected event DDL output: $event_output" >&2
		exit 1
		;;
esac

if "$parser" --quiet 'CREATE EVENT e DO SELECT 1'; then
	echo "expected CREATE EVENT without schedule to fail" >&2
	exit 1
fi

if "$parser" --quiet 'CREATE EVENT e ON SCHEDULE EVERY 1 DAY'; then
	echo "expected CREATE EVENT without body to fail" >&2
	exit 1
fi

if "$parser" --quiet 'CREATE EVENT e ON SCHEDULE DO SELECT 1'; then
	echo "expected CREATE EVENT without schedule form to fail" >&2
	exit 1
fi

if "$parser" --quiet 'CREATE EVENT e ON SCHEDULE EVERY 1 DO SELECT 1'; then
	echo "expected CREATE EVENT interval without unit to fail" >&2
	exit 1
fi

if "$parser" --quiet 'CREATE EVENT e ON SCHEDULE EVERY 1 DAY COMMENT x DO SELECT 1'; then
	echo "expected CREATE EVENT nonstring comment to fail" >&2
	exit 1
fi

if "$parser" --quiet 'CREATE DEFINER user@localhost EVENT e ON SCHEDULE EVERY 1 DAY DO SELECT 1'; then
	echo "expected CREATE EVENT DEFINER without equals to fail" >&2
	exit 1
fi

if "$parser" --quiet 'ALTER EVENT e'; then
	echo "expected ALTER EVENT without changes to fail" >&2
	exit 1
fi

if "$parser" --quiet 'ALTER EVENT e ON SCHEDULE EVERY 1'; then
	echo "expected ALTER EVENT interval without unit to fail" >&2
	exit 1
fi

if "$parser" --quiet 'ALTER EVENT e RENAME e2'; then
	echo "expected ALTER EVENT RENAME without TO to fail" >&2
	exit 1
fi

if "$parser" --quiet 'ALTER EVENT e DISABLE ON SOURCE'; then
	echo "expected ALTER EVENT invalid status tail to fail" >&2
	exit 1
fi

trigger_output=$("$parser" "CREATE DEFINER=CURRENT_USER TRIGGER tr BEFORE INSERT ON t FOR EACH ROW SET NEW.a = 1; CREATE TRIGGER IF NOT EXISTS db.tr2 AFTER UPDATE ON db.t FOR EACH ROW FOLLOWS tr SET @x = OLD.a; CREATE DEFINER=trigger@localhost TRIGGER tr3 BEFORE DELETE ON t FOR EACH ROW BEGIN SET @x = OLD.a; END")
case "$trigger_output" in
	*"create"*/trigger:tr*"create"*/trigger:db.tr2*"create"*/trigger:tr3*) ;;
	*)
		echo "unexpected trigger DDL output: $trigger_output" >&2
		exit 1
		;;
esac

if "$parser" --quiet 'CREATE TRIGGER tr INSERT ON t FOR EACH ROW SET @x = 1'; then
	echo "expected CREATE TRIGGER without trigger time to fail" >&2
	exit 1
fi

if "$parser" --quiet 'CREATE TRIGGER tr BEFORE SELECT ON t FOR EACH ROW SET @x = 1'; then
	echo "expected CREATE TRIGGER invalid event to fail" >&2
	exit 1
fi

if "$parser" --quiet 'CREATE TRIGGER tr BEFORE INSERT t FOR EACH ROW SET @x = 1'; then
	echo "expected CREATE TRIGGER without ON to fail" >&2
	exit 1
fi

if "$parser" --quiet 'CREATE TRIGGER tr BEFORE INSERT ON t FOR ROW SET @x = 1'; then
	echo "expected CREATE TRIGGER without EACH ROW to fail" >&2
	exit 1
fi

if "$parser" --quiet 'CREATE TRIGGER tr BEFORE INSERT ON t FOR EACH ROW FOLLOWS SET @x = 1'; then
	echo "expected CREATE TRIGGER order without target to fail" >&2
	exit 1
fi

if "$parser" --quiet 'CREATE DEFINER user@localhost TRIGGER tr BEFORE INSERT ON t FOR EACH ROW SET @x = 1'; then
	echo "expected CREATE TRIGGER DEFINER without equals to fail" >&2
	exit 1
fi

if "$parser" --quiet 'CREATE TRIGGER tr BEFORE INSERT ON t FOR EACH ROW'; then
	echo "expected CREATE TRIGGER without body to fail" >&2
	exit 1
fi

create_routine_output=$("$parser" "CREATE DEFINER=CURRENT_USER PROCEDURE p(IN a INT, OUT b VARCHAR(32)) COMMENT 'p' LANGUAGE SQL MODIFIES SQL DATA SQL SECURITY INVOKER BEGIN SET b = a; END; CREATE PROCEDURE p_keywords(IN value INT, IN start BIGINT) SELECT value; CREATE FUNCTION IF NOT EXISTS db.f(x INT, y DECIMAL(10,2)) RETURNS DECIMAL(10,2) DETERMINISTIC READS SQL DATA RETURN x + y; CREATE DEFINER=routine@localhost FUNCTION f2() RETURNS SET('a','b') NOT DETERMINISTIC CONTAINS SQL BEGIN RETURN 'a'; END")
case "$create_routine_output" in
	*"create"*/procedure:p*"create"*/procedure:p_keywords*"create"*/function:db.f*"create"*/function:f2*) ;;
	*)
		echo "unexpected CREATE routine output: $create_routine_output" >&2
		exit 1
		;;
esac

if "$parser" --quiet 'CREATE PROCEDURE p SELECT 1'; then
	echo "expected CREATE PROCEDURE without parameter list to fail" >&2
	exit 1
fi

if "$parser" --quiet 'CREATE PROCEDURE p(IN a) SELECT 1'; then
	echo "expected CREATE PROCEDURE parameter without type to fail" >&2
	exit 1
fi

if "$parser" --quiet 'CREATE PROCEDURE p(IN a INT,) SELECT 1'; then
	echo "expected CREATE PROCEDURE trailing parameter comma to fail" >&2
	exit 1
fi

if "$parser" --quiet 'CREATE PROCEDURE p() COMMENT x SELECT 1'; then
	echo "expected CREATE PROCEDURE nonstring comment to fail" >&2
	exit 1
fi

if "$parser" --quiet 'CREATE PROCEDURE p() LANGUAGE JAVASCRIPT SELECT 1'; then
	echo "expected CREATE PROCEDURE invalid language to fail" >&2
	exit 1
fi

if "$parser" --quiet 'CREATE FUNCTION f(x INT) RETURN x'; then
	echo "expected CREATE FUNCTION without RETURNS to fail" >&2
	exit 1
fi

if "$parser" --quiet 'CREATE FUNCTION f(IN x INT) RETURNS INT RETURN x'; then
	echo "expected CREATE FUNCTION parameter mode to fail" >&2
	exit 1
fi

if "$parser" --quiet 'CREATE FUNCTION f() RETURNS DETERMINISTIC RETURN 1'; then
	echo "expected CREATE FUNCTION without return type to fail" >&2
	exit 1
fi

if "$parser" --quiet 'CREATE FUNCTION f() RETURNS INT SQL SECURITY CURRENT_USER RETURN 1'; then
	echo "expected CREATE FUNCTION invalid SQL SECURITY value to fail" >&2
	exit 1
fi

if "$parser" --quiet 'CREATE DEFINER user@localhost PROCEDURE p() SELECT 1'; then
	echo "expected CREATE PROCEDURE DEFINER without equals to fail" >&2
	exit 1
fi

create_table_compact_output=$("$parser" "CREATE TEMPORARY TABLE IF NOT EXISTS new_tbl LIKE orig_tbl; CREATE TABLE db.new2 (LIKE db.orig2); CREATE TABLE ctas AS SELECT * FROM src; CREATE TABLE ctas2 (id INT PRIMARY KEY) ENGINE=InnoDB IGNORE SELECT id FROM src; CREATE TABLE ctas3 REPLACE AS WITH c AS (SELECT 1 AS id) SELECT id FROM c; CREATE TABLE ctas4 AS (SELECT 1); CREATE TABLE ctas5 AS ((VALUES ROW (1, 1), ROW (2, 2) ORDER BY column_0 LIMIT 2) ORDER BY column_1 LIMIT 1)")
case "$create_table_compact_output" in
	*"create"*/table:new_tbl*"create"*/table:db.new2*"create"*/table:ctas*"create"*/table:ctas2*"create"*/table:ctas3*"create"*/table:ctas4*"create"*/table:ctas5*) ;;
	*)
		echo "unexpected compact CREATE TABLE output: $create_table_compact_output" >&2
		exit 1
		;;
esac

if "$parser" --quiet 'CREATE TABLE t LIKE'; then
	echo "expected CREATE TABLE LIKE without source to fail" >&2
	exit 1
fi

if "$parser" --quiet 'CREATE TABLE t (LIKE)'; then
	echo "expected parenthesized CREATE TABLE LIKE without source to fail" >&2
	exit 1
fi

if "$parser" --quiet 'CREATE TABLE t LIKE old extra'; then
	echo "expected CREATE TABLE LIKE with trailing tokens to fail" >&2
	exit 1
fi

if "$parser" --quiet 'CREATE TABLE t AS'; then
	echo "expected CREATE TABLE AS without query to fail" >&2
	exit 1
fi

if "$parser" --quiet 'CREATE TABLE t IGNORE'; then
	echo "expected CREATE TABLE IGNORE without query to fail" >&2
	exit 1
fi

if "$parser" --quiet 'CREATE TABLE t () SELECT 1'; then
	echo "expected CREATE TABLE SELECT with empty definition list to fail" >&2
	exit 1
fi

create_table_definition_output=$("$parser" "CREATE TABLE base (id INT NOT NULL, PRIMARY KEY (id)); CREATE TEMPORARY TABLE db.tmp (a VARCHAR(10), KEY k (a)) ENGINE=InnoDB; CREATE TABLE \"quoted name\" (i INT)")
case "$create_table_definition_output" in
	*"create"*/table:base*"create"*/table:db.tmp*"create"*/table:'"quoted name"'*) ;;
	*)
		echo "unexpected CREATE TABLE definition output: $create_table_definition_output" >&2
		exit 1
		;;
esac

if "$parser" --quiet 'CREATE TABLE t'; then
	echo "expected CREATE TABLE without a definition, LIKE, or query to fail" >&2
	exit 1
fi

if "$parser" --quiet 'CREATE TABLE t ()'; then
	echo "expected CREATE TABLE empty definition list to fail" >&2
	exit 1
fi

if "$parser" --quiet 'CREATE TABLE t (id INT,)'; then
	echo "expected CREATE TABLE trailing definition comma to fail" >&2
	exit 1
fi

alter_table_output=$("$parser" "ALTER TABLE t; ALTER TABLE db.t ADD COLUMN c INT NOT NULL, DROP COLUMN old_c, RENAME COLUMN c TO c2; ALTER TABLE t ADD COLUMN (a INT), ADD UNIQUE KEY u (a), DROP PRIMARY KEY; ALTER TABLE t ALTER COLUMN c SET DEFAULT 1, ALTER INDEX u INVISIBLE; ALTER TABLE t ORDER BY db.t.c DESC, c2 ASC; ALTER TABLE t ALGORITHM=INPLACE, LOCK=NONE, WITH VALIDATION; ALTER TABLE t REORGANIZE PARTITION p0, p1 INTO (PARTITION p2 VALUES LESS THAN (10)); ALTER TABLE t RENAME TO t2 REMOVE PARTITIONING")
case "$alter_table_output" in
	*"statements=8"*"alter"*/table:t*"alter"*/table:db.t*) ;;
	*)
		echo "unexpected ALTER TABLE output: $alter_table_output" >&2
		exit 1
		;;
esac

if "$parser" --quiet 'ALTER TABLE'; then
	echo "expected ALTER TABLE without a target to fail" >&2
	exit 1
fi

if "$parser" --quiet 'ALTER TABLE t ADD'; then
	echo "expected ALTER TABLE ADD without an operand to fail" >&2
	exit 1
fi

if "$parser" --quiet 'ALTER TABLE t ADD COLUMN c'; then
	echo "expected ALTER TABLE ADD COLUMN without a type to fail" >&2
	exit 1
fi

if "$parser" --quiet 'ALTER TABLE t ADD INDEX i'; then
	echo "expected ALTER TABLE ADD INDEX without key parts to fail" >&2
	exit 1
fi

if "$parser" --quiet 'ALTER TABLE t DROP'; then
	echo "expected ALTER TABLE DROP without a target to fail" >&2
	exit 1
fi

if "$parser" --quiet 'ALTER TABLE t DROP PRIMARY'; then
	echo "expected ALTER TABLE DROP PRIMARY without KEY to fail" >&2
	exit 1
fi

if "$parser" --quiet 'ALTER TABLE t CHANGE c d'; then
	echo "expected ALTER TABLE CHANGE without a type to fail" >&2
	exit 1
fi

if "$parser" --quiet 'ALTER TABLE t MODIFY c'; then
	echo "expected ALTER TABLE MODIFY without a type to fail" >&2
	exit 1
fi

if "$parser" --quiet 'ALTER TABLE t RENAME COLUMN c d'; then
	echo "expected ALTER TABLE RENAME COLUMN without TO to fail" >&2
	exit 1
fi

if "$parser" --quiet 'ALTER TABLE t ORDER BY'; then
	echo "expected ALTER TABLE ORDER BY without a list to fail" >&2
	exit 1
fi

if "$parser" --quiet 'ALTER TABLE t ALGORITHM='; then
	echo "expected ALTER TABLE ALGORITHM without a value to fail" >&2
	exit 1
fi

if "$parser" --quiet 'ALTER TABLE t ADD COLUMN c INT,'; then
	echo "expected ALTER TABLE trailing action comma to fail" >&2
	exit 1
fi

create_index_output=$("$parser" "CREATE INDEX i ON t (c); CREATE UNIQUE INDEX i2 USING BTREE ON db.t (c(10) DESC, (lower(name)) ASC) KEY_BLOCK_SIZE=8 WITH PARSER parser_name COMMENT 'c' VISIBLE ENGINE_ATTRIBUTE='{}' SECONDARY_ENGINE_ATTRIBUTE='{}' ALGORITHM=INPLACE LOCK=NONE; CREATE FULLTEXT INDEX ft ON t (body) WITH PARSER ngram; CREATE SPATIAL INDEX sp ON t (g) USING HASH; CREATE INDEX legacy TYPE BTREE ON t (c) TYPE RTREE")
case "$create_index_output" in
	*"create"*/index:i*"create"*/index:i2*"create"*/index:ft*"create"*/index:sp*"create"*/index:legacy*) ;;
	*)
		echo "unexpected CREATE INDEX output: $create_index_output" >&2
		exit 1
		;;
esac

if "$parser" --quiet 'CREATE INDEX'; then
	echo "expected CREATE INDEX without a name to fail" >&2
	exit 1
fi

if "$parser" --quiet 'CREATE INDEX i t (c)'; then
	echo "expected CREATE INDEX without ON to fail" >&2
	exit 1
fi

if "$parser" --quiet 'CREATE INDEX i ON t'; then
	echo "expected CREATE INDEX without key part list to fail" >&2
	exit 1
fi

if "$parser" --quiet 'CREATE INDEX i ON t ()'; then
	echo "expected CREATE INDEX empty key part list to fail" >&2
	exit 1
fi

if "$parser" --quiet 'CREATE INDEX i ON t (c,)'; then
	echo "expected CREATE INDEX trailing key part comma to fail" >&2
	exit 1
fi

if "$parser" --quiet 'CREATE INDEX i ON t (c) KEY_BLOCK_SIZE'; then
	echo "expected CREATE INDEX KEY_BLOCK_SIZE without value to fail" >&2
	exit 1
fi

if "$parser" --quiet 'CREATE INDEX i ON t (c) WITH PARSER'; then
	echo "expected CREATE INDEX WITH PARSER without name to fail" >&2
	exit 1
fi

if "$parser" --quiet 'CREATE INDEX i ON t (c) COMMENT c'; then
	echo "expected CREATE INDEX COMMENT without string to fail" >&2
	exit 1
fi

if "$parser" --quiet 'CREATE INDEX i ON t (c) ALGORITHM=BAD'; then
	echo "expected CREATE INDEX invalid ALGORITHM value to fail" >&2
	exit 1
fi

if "$parser" --quiet 'CREATE INDEX i ON t (c) LOCK=BAD'; then
	echo "expected CREATE INDEX invalid LOCK value to fail" >&2
	exit 1
fi

if "$parser" --quiet 'CREATE INDEX i ON t (c) ENGINE_ATTRIBUTE=1'; then
	echo "expected CREATE INDEX numeric ENGINE_ATTRIBUTE to fail" >&2
	exit 1
fi

drop_index_output=$("$parser" 'DROP INDEX i ON t; DROP INDEX `PRIMARY` ON `db`.`t` ALGORITHM=INPLACE LOCK=NONE; DROP INDEX i2 ON t LOCK SHARED ALGORITHM COPY')
case "$drop_index_output" in
	*"drop"*/index:i*"drop"*/index:'`PRIMARY`'*"drop"*/index:i2*) ;;
	*)
		echo "unexpected DROP INDEX output: $drop_index_output" >&2
		exit 1
		;;
esac

if "$parser" --quiet 'DROP'; then
	echo "expected empty DROP statement to fail" >&2
	exit 1
fi

if "$parser" --quiet 'DROP INDEX'; then
	echo "expected missing DROP INDEX name to fail" >&2
	exit 1
fi

if "$parser" --quiet 'DROP INDEX i'; then
	echo "expected missing DROP INDEX table to fail" >&2
	exit 1
fi

if "$parser" --quiet 'DROP INDEX i t'; then
	echo "expected DROP INDEX without ON to fail" >&2
	exit 1
fi

if "$parser" --quiet 'DROP INDEX i ON'; then
	echo "expected missing DROP INDEX ON table to fail" >&2
	exit 1
fi

if "$parser" --quiet 'DROP INDEX PRIMARY ON t'; then
	echo "expected unquoted DROP INDEX PRIMARY to fail" >&2
	exit 1
fi

if "$parser" --quiet 'DROP INDEX i ON t ALGORITHM'; then
	echo "expected missing DROP INDEX ALGORITHM value to fail" >&2
	exit 1
fi

if "$parser" --quiet 'DROP INDEX i ON t ALGORITHM=BAD'; then
	echo "expected invalid DROP INDEX ALGORITHM value to fail" >&2
	exit 1
fi

if "$parser" --quiet 'DROP INDEX i ON t LOCK=BAD'; then
	echo "expected invalid DROP INDEX LOCK value to fail" >&2
	exit 1
fi

drop_table_view_output=$("$parser" 'DROP TABLE IF EXISTS t1, db.t2 RESTRICT; DROP TEMPORARY TABLES IF EXISTS tmp1, tmp2; DROP VIEW IF EXISTS v1, db.v2 CASCADE')
case "$drop_table_view_output" in
	*"drop"*/table:t1*"drop"*/table:tmp1*"drop"*/view:v1*) ;;
	*)
		echo "unexpected DROP TABLE/VIEW output: $drop_table_view_output" >&2
		exit 1
		;;
esac

if "$parser" --quiet 'DROP TABLE'; then
	echo "expected missing DROP TABLE list to fail" >&2
	exit 1
fi

if "$parser" --quiet 'DROP TABLE IF EXISTS'; then
	echo "expected missing DROP TABLE IF EXISTS list to fail" >&2
	exit 1
fi

if "$parser" --quiet 'DROP TABLE t,'; then
	echo "expected trailing DROP TABLE comma to fail" >&2
	exit 1
fi

if "$parser" --quiet 'DROP TABLE t RESTRICT CASCADE'; then
	echo "expected multiple DROP TABLE tail options to fail" >&2
	exit 1
fi

if "$parser" --quiet 'DROP TEMPORARY VIEW v'; then
	echo "expected DROP TEMPORARY VIEW to fail" >&2
	exit 1
fi

if "$parser" --quiet 'DROP VIEW'; then
	echo "expected missing DROP VIEW list to fail" >&2
	exit 1
fi

if "$parser" --quiet 'DROP VIEW IF EXISTS'; then
	echo "expected missing DROP VIEW IF EXISTS list to fail" >&2
	exit 1
fi

if "$parser" --quiet 'DROP VIEW v, CASCADE'; then
	echo "expected missing DROP VIEW name before tail option to fail" >&2
	exit 1
fi

drop_database_output=$("$parser" 'DROP DATABASE IF EXISTS db; DROP SCHEMA `s`')
case "$drop_database_output" in
	*"drop"*/database:db*"drop"*/schema:'`s`'*) ;;
	*)
		echo "unexpected DROP DATABASE/SCHEMA output: $drop_database_output" >&2
		exit 1
		;;
esac

if "$parser" --quiet 'DROP DATABASE'; then
	echo "expected missing DROP DATABASE name to fail" >&2
	exit 1
fi

if "$parser" --quiet 'DROP DATABASE IF EXISTS'; then
	echo "expected missing DROP DATABASE IF EXISTS name to fail" >&2
	exit 1
fi

if "$parser" --quiet 'DROP DATABASE db extra'; then
	echo "expected trailing DROP DATABASE tokens to fail" >&2
	exit 1
fi

if "$parser" --quiet 'DROP SCHEMA db.name'; then
	echo "expected qualified DROP SCHEMA name to fail" >&2
	exit 1
fi

drop_stored_object_output=$("$parser" 'DROP EVENT IF EXISTS db.e; DROP PROCEDURE p; DROP FUNCTION IF EXISTS test.metaphon; DROP TRIGGER IF EXISTS test.tr')
case "$drop_stored_object_output" in
	*"drop"*/event:db.e*"drop"*/procedure:p*"drop"*/function:test.metaphon*"drop"*/trigger:test.tr*) ;;
	*)
		echo "unexpected stored-object DROP output: $drop_stored_object_output" >&2
		exit 1
		;;
esac

if "$parser" --quiet 'DROP EVENT'; then
	echo "expected missing DROP EVENT name to fail" >&2
	exit 1
fi

if "$parser" --quiet 'DROP PROCEDURE IF EXISTS'; then
	echo "expected missing DROP PROCEDURE IF EXISTS name to fail" >&2
	exit 1
fi

if "$parser" --quiet 'DROP FUNCTION f extra'; then
	echo "expected trailing DROP FUNCTION tokens to fail" >&2
	exit 1
fi

if "$parser" --quiet 'DROP TRIGGER IF EXISTS'; then
	echo "expected missing DROP TRIGGER IF EXISTS name to fail" >&2
	exit 1
fi

alter_routine_output=$("$parser" "ALTER FUNCTION db.f COMMENT 'c' LANGUAGE SQL READS SQL DATA SQL SECURITY INVOKER; ALTER PROCEDURE p CONTAINS SQL NO SQL MODIFIES SQL DATA SQL SECURITY DEFINER COMMENT 'p'")
case "$alter_routine_output" in
	*"alter"*/function:db.f*"alter"*/procedure:p*) ;;
	*)
		echo "unexpected ALTER routine output: $alter_routine_output" >&2
		exit 1
		;;
esac

if "$parser" --quiet 'ALTER FUNCTION'; then
	echo "expected ALTER FUNCTION without a name to fail" >&2
	exit 1
fi

if "$parser" --quiet 'ALTER FUNCTION f COMMENT'; then
	echo "expected ALTER FUNCTION COMMENT without string to fail" >&2
	exit 1
fi

if "$parser" --quiet 'ALTER FUNCTION f LANGUAGE C'; then
	echo "expected ALTER FUNCTION LANGUAGE other than SQL to fail" >&2
	exit 1
fi

if "$parser" --quiet 'ALTER PROCEDURE p READS DATA'; then
	echo "expected ALTER PROCEDURE READS without SQL DATA to fail" >&2
	exit 1
fi

if "$parser" --quiet 'ALTER PROCEDURE p SQL SECURITY CURRENT_USER'; then
	echo "expected ALTER PROCEDURE SQL SECURITY unsupported value to fail" >&2
	exit 1
fi

if "$parser" --quiet 'ALTER FUNCTION f DETERMINISTIC'; then
	echo "expected ALTER FUNCTION DETERMINISTIC to fail" >&2
	exit 1
fi

loadable_function_output=$("$parser" "CREATE FUNCTION IF NOT EXISTS udf RETURNS STRING SONAME 'udf.so'; CREATE AGGREGATE FUNCTION agg RETURNS INTEGER SONAME 'agg.so'")
case "$loadable_function_output" in
	*"create"*/function:udf*"create"*/function:agg*) ;;
	*)
		echo "unexpected CREATE loadable FUNCTION output: $loadable_function_output" >&2
		exit 1
		;;
esac

if "$parser" --quiet "CREATE AGGREGATE FUNCTION agg RETURNS INTEGER"; then
	echo "expected CREATE AGGREGATE FUNCTION without SONAME to fail" >&2
	exit 1
fi

if "$parser" --quiet "CREATE FUNCTION udf RETURNS STRING SONAME"; then
	echo "expected CREATE FUNCTION SONAME without library to fail" >&2
	exit 1
fi

if "$parser" --quiet "CREATE FUNCTION udf RETURNS BOOL SONAME 'udf.so'"; then
	echo "expected CREATE FUNCTION with invalid loadable return type to fail" >&2
	exit 1
fi

if "$parser" --quiet "CREATE FUNCTION udf RETURNS REAL SONAME 1"; then
	echo "expected CREATE FUNCTION with numeric SONAME to fail" >&2
	exit 1
fi

if "$parser" --quiet "CREATE FUNCTION IF EXISTS udf RETURNS DECIMAL SONAME 'udf.so'"; then
	echo "expected CREATE FUNCTION IF EXISTS loadable form to fail" >&2
	exit 1
fi

rename_table_output=$("$parser" 'RENAME TABLE old TO new; RENAME TABLE db.old TO other.new, a TO b; RENAME TABLES t2 TO t0, t4 TO t2')
case "$rename_table_output" in
	*"rename"*/table:old*"rename"*/table:db.old*"rename"*/table:t2*) ;;
	*)
		echo "unexpected RENAME TABLE output: $rename_table_output" >&2
		exit 1
		;;
esac

if "$parser" --quiet 'RENAME'; then
	echo "expected empty RENAME statement to fail" >&2
	exit 1
fi

if "$parser" --quiet 'RENAME TABLE'; then
	echo "expected missing RENAME TABLE pair to fail" >&2
	exit 1
fi

if "$parser" --quiet 'RENAME TABLE old'; then
	echo "expected incomplete RENAME TABLE pair to fail" >&2
	exit 1
fi

if "$parser" --quiet 'RENAME TABLE old new'; then
	echo "expected RENAME TABLE without TO to fail" >&2
	exit 1
fi

if "$parser" --quiet 'RENAME TABLE old TO'; then
	echo "expected missing RENAME TABLE destination to fail" >&2
	exit 1
fi

if "$parser" --quiet 'RENAME TABLE old TO new,'; then
	echo "expected trailing RENAME TABLE comma to fail" >&2
	exit 1
fi

if "$parser" --quiet 'RENAME TABLE old TO new extra'; then
	echo "expected trailing RENAME TABLE tokens to fail" >&2
	exit 1
fi

plural_tables_output=$("$parser" 'DROP TABLES t1, t4; RENAME TABLES old TO new; ANALYZE TABLES c, cc; CHECK TABLES t1; OPTIMIZE TABLES columns_priv, db, user')
case "$plural_tables_output" in
	*"drop"*/table:t1*"rename"*/table:old*"analyze"*/table:c*"check"*/table:t1*"optimize"*/table:columns_priv*) ;;
	*)
		echo "unexpected plural TABLES output: $plural_tables_output" >&2
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

if ! "$parser" --quiet 'CREATE DATABASE db'; then
	echo "expected CREATE DATABASE minimal form to parse" >&2
	exit 1
fi

if ! "$parser" --quiet "CREATE SCHEMA IF NOT EXISTS s DEFAULT CHARACTER SET = utf8mb4 COLLATE utf8mb4_bin ENCRYPTION = 'N'"; then
	echo "expected CREATE SCHEMA options to parse" >&2
	exit 1
fi

if ! "$parser" --quiet 'CREATE DATABASE db DEFAULT CHARSET utf8mb4'; then
	echo "expected CREATE DATABASE CHARSET alias to parse" >&2
	exit 1
fi

if ! "$parser" --quiet "ALTER DATABASE testdb CHARACTER SET 'latin1'"; then
	echo "expected ALTER DATABASE string charset name to parse" >&2
	exit 1
fi

if ! "$parser" --quiet "ALTER DATABASE db DEFAULT CHARACTER SET=utf8mb4 DEFAULT COLLATE=utf8mb4_bin DEFAULT ENCRYPTION='Y' READ ONLY=0"; then
	echo "expected ALTER DATABASE full options to parse" >&2
	exit 1
fi

if ! "$parser" --quiet 'ALTER SCHEMA db CHARSET utf8mb4 READ ONLY 1'; then
	echo "expected ALTER SCHEMA CHARSET and READ ONLY to parse" >&2
	exit 1
fi

if ! "$parser" --quiet 'ALTER DATABASE CHARSET utf8mb4'; then
	echo "expected nameless ALTER DATABASE CHARSET to parse" >&2
	exit 1
fi

if "$parser" --quiet 'CREATE DATABASE'; then
	echo "expected CREATE DATABASE without name to fail" >&2
	exit 1
fi

if "$parser" --quiet 'CREATE DATABASE IF EXISTS db'; then
	echo "expected CREATE DATABASE IF EXISTS to fail" >&2
	exit 1
fi

if "$parser" --quiet 'CREATE DATABASE db READ ONLY=1'; then
	echo "expected CREATE DATABASE READ ONLY to fail" >&2
	exit 1
fi

if "$parser" --quiet 'CREATE DATABASE db CHARACTER SET'; then
	echo "expected CREATE DATABASE incomplete CHARACTER SET to fail" >&2
	exit 1
fi

if "$parser" --quiet 'ALTER DATABASE'; then
	echo "expected ALTER DATABASE without name or options to fail" >&2
	exit 1
fi

if "$parser" --quiet 'ALTER DATABASE db'; then
	echo "expected ALTER DATABASE without options to fail" >&2
	exit 1
fi

if "$parser" --quiet 'ALTER DATABASE db CHARACTER'; then
	echo "expected ALTER DATABASE incomplete CHARACTER SET to fail" >&2
	exit 1
fi

if "$parser" --quiet 'ALTER DATABASE db READ ONLY'; then
	echo "expected ALTER DATABASE READ ONLY without value to fail" >&2
	exit 1
fi

if "$parser" --quiet 'ALTER DATABASE db READ ONLY=2'; then
	echo "expected ALTER DATABASE invalid READ ONLY value to fail" >&2
	exit 1
fi

if "$parser" --quiet 'ALTER DATABASE db DEFAULT'; then
	echo "expected ALTER DATABASE dangling DEFAULT to fail" >&2
	exit 1
fi

if "$parser" --quiet 'ALTER DATABASE db ENCRYPTION = utf8mb4'; then
	echo "expected ALTER DATABASE nonstring ENCRYPTION to fail" >&2
	exit 1
fi

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

variable_assignment_output=$("$parser" "SELECT a INTO @x FROM t; SELECT a INTO local_var FROM t; SELECT a FROM t INTO @x; SELECT a INTO OUTFILE '/tmp/x' FROM t; SELECT a INTO DUMPFILE '/tmp/y' FROM t; SET @x = 1; SET @'my-var' = 1; SET @\"my-var\" := 2; SET @\`my-var\` = 3; SET @iv=-20010101; SET @plus=+.5; SET @@session.sql_mode = 'ANSI'; SET SESSION sql_mode = 'ANSI'; SET GLOBAL keycache1.key_buffer_size = 128 * 1024; SET keycache1.key_buffer_size = 1; SET PERSIST_ONLY plugin.var = DEFAULT; SET PERSIST default.key_buffer_size = 1024*1024; SET GLOBAL flush = 1; SET sql_log_bin = 0; SET autocommit = 1; SET x = 1; GET DIAGNOSTICS @n = NUMBER; GET CURRENT DIAGNOSTICS CONDITION 1 @state = RETURNED_SQLSTATE")
case "$variable_assignment_output" in
	*"select"*/user_variable:@x*"select"*/local_variable:local_var*"select"*/user_variable:@x*"select"*/outfile:"'/tmp/x'"*"select"*/dumpfile:"'/tmp/y'"*"set"*/user_variable:@x*"set"*/user_variable:@*my-var*"set"*/user_variable:@\"my-var\"*"set"*/user_variable:@\`my-var\`*"set"*/user_variable:@iv*"set"*/user_variable:@plus*"set"*/system_variable:@@session.sql_mode*"set"*/system_variable:sql_mode*"set"*/system_variable:keycache1.key_buffer_size*"set"*/system_variable:keycache1.key_buffer_size*"set"*/system_variable:plugin.var*"set"*/system_variable:default.key_buffer_size*"set"*/system_variable:flush*"set"*/system_variable:sql_log_bin*"set"*/system_variable:autocommit*"set"*/system_variable:x*"get"*/user_variable:@n*"get"*/diagnostics_condition:1*) ;;
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

handler_output=$("$parser" 'HANDLER t OPEN; HANDLER `db`.`t` OPEN AS h; HANDLER t OPEN alias; HANDLER t READ FIRST; HANDLER t READ NEXT LIMIT 1; HANDLER h READ PRIMARY >= (1, @v) WHERE b > 1 LIMIT 5; HANDLER h READ `PRIMARY` PREV; HANDLER h READ a=(49); HANDLER t CLOSE')
case "$handler_output" in
	*"handler"*/table:t*"handler"*/table:'`db`.`t`'*"handler"*/table:t*"handler"*/table:t*"handler"*/table:t*"handler"*/table:h*"handler"*/table:h*"handler"*/table:h*"handler"*/table:t*) ;;
	*)
		echo "unexpected HANDLER output: $handler_output" >&2
		exit 1
		;;
esac

if "$parser" --quiet 'HANDLER'; then
	echo "expected empty HANDLER statement to fail" >&2
	exit 1
fi

if "$parser" --quiet 'HANDLER t'; then
	echo "expected missing HANDLER operation to fail" >&2
	exit 1
fi

if "$parser" --quiet 'HANDLER t OPEN AS'; then
	echo "expected missing HANDLER alias to fail" >&2
	exit 1
fi

if "$parser" --quiet 'HANDLER t OPEN AS a extra'; then
	echo "expected trailing HANDLER OPEN tokens to fail" >&2
	exit 1
fi

if "$parser" --quiet 'HANDLER t READ'; then
	echo "expected missing HANDLER READ form to fail" >&2
	exit 1
fi

if "$parser" --quiet 'HANDLER t READ a'; then
	echo "expected incomplete HANDLER READ index form to fail" >&2
	exit 1
fi

if "$parser" --quiet 'HANDLER t READ PREV'; then
	echo "expected HANDLER READ PREV without an index to fail" >&2
	exit 1
fi

if "$parser" --quiet 'HANDLER t READ a ='; then
	echo "expected missing HANDLER READ key values to fail" >&2
	exit 1
fi

if "$parser" --quiet 'HANDLER t READ a = ()'; then
	echo "expected empty HANDLER READ key values to fail" >&2
	exit 1
fi

if "$parser" --quiet 'HANDLER t READ a NEXT WHERE LIMIT 1'; then
	echo "expected empty HANDLER WHERE clause to fail" >&2
	exit 1
fi

if "$parser" --quiet 'HANDLER t READ a NEXT LIMIT'; then
	echo "expected missing HANDLER LIMIT expression to fail" >&2
	exit 1
fi

if "$parser" --quiet 'HANDLER t CLOSE extra'; then
	echo "expected trailing HANDLER CLOSE tokens to fail" >&2
	exit 1
fi

key_cache_output=$("$parser" 'CACHE INDEX c KEY (PRIMARY, i) IN DEFAULT; CACHE INDEX `db`.`pt` PARTITION (p0, p1) KEY (i) IN hot_cache; LOAD INDEX INTO CACHE `db`.`li` PARTITION (ALL) KEY (PRIMARY) IGNORE LEAVES; LOAD INDEX INTO CACHE t, u IGNORE LEAVES')
case "$key_cache_output" in
	*"cache"*/table:c*"cache"*/table:'`db`.`pt`'*"load"*/table:'`db`.`li`'*"load"*/table:t*) ;;
	*)
		echo "unexpected key cache output: $key_cache_output" >&2
		exit 1
		;;
esac

if "$parser" --quiet 'CACHE INDEX'; then
	echo "expected missing CACHE INDEX list to fail" >&2
	exit 1
fi

if "$parser" --quiet 'CACHE INDEX t'; then
	echo "expected missing CACHE INDEX key cache to fail" >&2
	exit 1
fi

if "$parser" --quiet 'CACHE INDEX t IN'; then
	echo "expected missing CACHE INDEX key cache name to fail" >&2
	exit 1
fi

if "$parser" --quiet "CACHE INDEX t IN 'keycache'"; then
	echo "expected string CACHE INDEX key cache name to fail" >&2
	exit 1
fi

if "$parser" --quiet 'CACHE INDEX t KEY () IN DEFAULT'; then
	echo "expected empty CACHE INDEX key list to fail" >&2
	exit 1
fi

if "$parser" --quiet 'CACHE INDEX t KEY (i,) IN DEFAULT'; then
	echo "expected trailing CACHE INDEX key list comma to fail" >&2
	exit 1
fi

if "$parser" --quiet 'CACHE INDEX t, IN DEFAULT'; then
	echo "expected incomplete CACHE INDEX table list to fail" >&2
	exit 1
fi

if "$parser" --quiet 'LOAD INDEX INTO CACHE'; then
	echo "expected missing LOAD INDEX table list to fail" >&2
	exit 1
fi

if "$parser" --quiet 'LOAD INDEX CACHE t'; then
	echo "expected malformed LOAD INDEX INTO CACHE head to fail" >&2
	exit 1
fi

if "$parser" --quiet 'LOAD INDEX INTO CACHE t IGNORE'; then
	echo "expected missing LOAD INDEX IGNORE LEAVES clause to fail" >&2
	exit 1
fi

if "$parser" --quiet 'LOAD INDEX INTO CACHE t IGNORE LEAF'; then
	echo "expected invalid LOAD INDEX IGNORE LEAVES clause to fail" >&2
	exit 1
fi

if "$parser" --quiet 'LOAD INDEX INTO CACHE t PARTITION ()'; then
	echo "expected empty LOAD INDEX partition list to fail" >&2
	exit 1
fi

if "$parser" --quiet 'LOAD INDEX INTO CACHE t,'; then
	echo "expected trailing LOAD INDEX table-list comma to fail" >&2
	exit 1
fi

if "$parser" --quiet 'LOAD INDEX INTO CACHE t IN DEFAULT'; then
	echo "expected LOAD INDEX IN key cache clause to fail" >&2
	exit 1
fi

load_import_output=$("$parser" "LOAD DATA INFILE 'x' INTO TABLE db.t PARTITION (p0, p1) CHARACTER SET utf8mb4 FIELDS TERMINATED BY ',' OPTIONALLY ENCLOSED BY '\"' LINES TERMINATED BY '\n' IGNORE 1 LINES (a, @b) SET b = concat(@b, '!'); LOAD XML LOCAL INFILE 'x' INTO TABLE t ROWS IDENTIFIED BY '<row>' IGNORE 2 ROWS (a, @b) SET b = DEFAULT")
case "$load_import_output" in
	*"load"*/table:db.t*"load"*/table:t*) ;;
	*)
		echo "unexpected LOAD DATA/XML output: $load_import_output" >&2
		exit 1
		;;
esac

if "$parser" --quiet 'LOAD DATA'; then
	echo "expected LOAD DATA without INFILE to fail" >&2
	exit 1
fi

if "$parser" --quiet "LOAD DATA INFILE 'x'"; then
	echo "expected LOAD DATA without INTO TABLE to fail" >&2
	exit 1
fi

if "$parser" --quiet "LOAD DATA INFILE 1 INTO TABLE t"; then
	echo "expected LOAD DATA with nonstring INFILE to fail" >&2
	exit 1
fi

if "$parser" --quiet "LOAD DATA INFILE 'x' INTO t"; then
	echo "expected LOAD DATA INTO without TABLE to fail" >&2
	exit 1
fi

if "$parser" --quiet "LOAD DATA INFILE 'x' INTO TABLE t FIELDS TERMINATED"; then
	echo "expected LOAD DATA FIELDS TERMINATED without BY string to fail" >&2
	exit 1
fi

if "$parser" --quiet "LOAD DATA INFILE 'x' INTO TABLE t IGNORE LINES"; then
	echo "expected LOAD DATA IGNORE without row count to fail" >&2
	exit 1
fi

if "$parser" --quiet "LOAD DATA INFILE 'x' INTO TABLE t (a,)"; then
	echo "expected LOAD DATA trailing column-list comma to fail" >&2
	exit 1
fi

if "$parser" --quiet "LOAD DATA INFILE 'x' INTO TABLE t SET c="; then
	echo "expected LOAD DATA SET without expression to fail" >&2
	exit 1
fi

if "$parser" --quiet "LOAD XML INFILE 'x' INTO TABLE t ROWS IDENTIFIED"; then
	echo "expected LOAD XML ROWS IDENTIFIED without BY tag to fail" >&2
	exit 1
fi

if "$parser" --quiet "LOAD XML INFILE 'x' INTO TABLE t FIELDS TERMINATED BY ','"; then
	echo "expected LOAD XML to reject LOAD DATA field clauses" >&2
	exit 1
fi

truncate_output=$("$parser" 'TRUNCATE t; TRUNCATE TABLE `db`.`t`; TRUNCATE TABLE `select`')
case "$truncate_output" in
	*"truncate"*/table:t*"truncate"*/table:'`db`.`t`'*"truncate"*/table:'`select`'*) ;;
	*)
		echo "unexpected TRUNCATE output: $truncate_output" >&2
		exit 1
		;;
esac

if "$parser" --quiet 'TRUNCATE'; then
	echo "expected missing TRUNCATE table to fail" >&2
	exit 1
fi

if "$parser" --quiet 'TRUNCATE TABLE'; then
	echo "expected missing TRUNCATE TABLE name to fail" >&2
	exit 1
fi

if "$parser" --quiet 'TRUNCATE t extra'; then
	echo "expected trailing TRUNCATE tokens to fail" >&2
	exit 1
fi

if "$parser" --quiet 'TRUNCATE t, u'; then
	echo "expected multi-table TRUNCATE to fail" >&2
	exit 1
fi

if "$parser" --quiet 'TRUNCATE TABLE 1'; then
	echo "expected numeric TRUNCATE table to fail" >&2
	exit 1
fi

if "$parser" --quiet 'TRUNCATE TABLE @t'; then
	echo "expected variable TRUNCATE table to fail" >&2
	exit 1
fi

use_output=$("$parser" 'USE db; USE `select`; USE transaction')
case "$use_output" in
	*"use"*/database:db*"use"*/database:'`select`'*"use"*/database:transaction*) ;;
	*)
		echo "unexpected USE output: $use_output" >&2
		exit 1
		;;
esac

if "$parser" --quiet 'USE'; then
	echo "expected missing USE schema to fail" >&2
	exit 1
fi

if "$parser" --quiet 'USE db extra'; then
	echo "expected trailing USE tokens to fail" >&2
	exit 1
fi

if "$parser" --quiet 'USE db.test'; then
	echo "expected qualified USE schema to fail" >&2
	exit 1
fi

if "$parser" --quiet 'USE "db"'; then
	echo "expected quoted-string USE schema to fail" >&2
	exit 1
fi

if "$parser" --quiet 'USE @db'; then
	echo "expected variable USE schema to fail" >&2
	exit 1
fi

table_lock_output=$("$parser" 'LOCK TABLE t READ; LOCK TABLES `db`.`lt` AS l WRITE, u u_alias READ LOCAL; LOCK TABLE v local READ; LOCK TABLES legacy LOW_PRIORITY WRITE; LOCK INSTANCE FOR BACKUP; UNLOCK TABLES; UNLOCK TABLE; UNLOCK INSTANCE')
case "$table_lock_output" in
	*"lock"*/table:t*"lock"*/table:'`db`.`lt`'*"lock"*/table:v*"lock"*/table:legacy*"lock"*/instance*"unlock"*/table*"unlock"*/table*"unlock"*/instance*) ;;
	*)
		echo "unexpected table lock output: $table_lock_output" >&2
		exit 1
		;;
esac

if "$parser" --quiet 'LOCK TABLES'; then
	echo "expected missing LOCK TABLES list to fail" >&2
	exit 1
fi

if "$parser" --quiet 'LOCK TABLES t'; then
	echo "expected missing LOCK TABLES mode to fail" >&2
	exit 1
fi

if "$parser" --quiet 'LOCK TABLES t READ,'; then
	echo "expected trailing LOCK TABLES comma to fail" >&2
	exit 1
fi

if "$parser" --quiet 'LOCK TABLES t READ, u'; then
	echo "expected incomplete LOCK TABLES entry to fail" >&2
	exit 1
fi

if "$parser" --quiet 'LOCK TABLES t SHARE'; then
	echo "expected unsupported LOCK TABLES mode to fail" >&2
	exit 1
fi

if "$parser" --quiet 'LOCK TABLES t LOW_PRIORITY'; then
	echo "expected incomplete LOCK TABLES LOW_PRIORITY mode to fail" >&2
	exit 1
fi

if "$parser" --quiet 'LOCK TABLES t LOW_PRIORITY READ'; then
	echo "expected invalid LOCK TABLES LOW_PRIORITY READ mode to fail" >&2
	exit 1
fi

if "$parser" --quiet 'LOCK TABLES @t READ'; then
	echo "expected variable LOCK TABLES target to fail" >&2
	exit 1
fi

if "$parser" --quiet 'LOCK TABLES t AS READ READ'; then
	echo "expected LOCK TABLES mode keyword alias to fail" >&2
	exit 1
fi

if "$parser" --quiet 'LOCK INSTANCE'; then
	echo "expected incomplete LOCK INSTANCE to fail" >&2
	exit 1
fi

if "$parser" --quiet 'LOCK INSTANCE FOR'; then
	echo "expected incomplete LOCK INSTANCE FOR clause to fail" >&2
	exit 1
fi

if "$parser" --quiet 'LOCK INSTANCE FOR BACKUP extra'; then
	echo "expected trailing LOCK INSTANCE tokens to fail" >&2
	exit 1
fi

if "$parser" --quiet 'UNLOCK'; then
	echo "expected missing UNLOCK target to fail" >&2
	exit 1
fi

if "$parser" --quiet 'UNLOCK TABLES t'; then
	echo "expected named UNLOCK TABLES target to fail" >&2
	exit 1
fi

if "$parser" --quiet 'UNLOCK INSTANCE extra'; then
	echo "expected trailing UNLOCK INSTANCE tokens to fail" >&2
	exit 1
fi

import_output=$("$parser" "IMPORT TABLE FROM '/tmp/a.sdi', '/tmp/b.sdi'")
case "$import_output" in
	*"import"*/sdi_file:"'/tmp/a.sdi'"*) ;;
	*)
		echo "unexpected IMPORT output: $import_output" >&2
		exit 1
		;;
esac

if "$parser" --quiet "IMPORT TABLE"; then
	echo "expected missing IMPORT FROM clause to fail" >&2
	exit 1
fi

if "$parser" --quiet "IMPORT TABLE FROM"; then
	echo "expected missing IMPORT SDI file to fail" >&2
	exit 1
fi

if "$parser" --quiet "IMPORT FROM '/tmp/a.sdi'"; then
	echo "expected IMPORT without TABLE to fail" >&2
	exit 1
fi

if "$parser" --quiet "IMPORT TABLE FROM @file"; then
	echo "expected variable IMPORT SDI file to fail" >&2
	exit 1
fi

if "$parser" --quiet "IMPORT TABLE FROM '/tmp/a.sdi' extra"; then
	echo "expected trailing IMPORT tokens to fail" >&2
	exit 1
fi

if "$parser" --quiet "IMPORT TABLE FROM '/tmp/a.sdi',"; then
	echo "expected trailing IMPORT comma to fail" >&2
	exit 1
fi

call_output=$("$parser" 'CALL p; CALL p(); CALL `db`.`p`(@a, 1 + 2); CALL 15298_1(); CALL `select`()')
case "$call_output" in
	*"call"*/procedure:p*"call"*/procedure:p*"call"*/procedure:'`db`.`p`'*"call"*/procedure:15298_1*"call"*/procedure:'`select`'*) ;;
	*)
		echo "unexpected CALL output: $call_output" >&2
		exit 1
		;;
esac

if "$parser" --quiet 'CALL'; then
	echo "expected missing CALL procedure to fail" >&2
	exit 1
fi

if "$parser" --quiet 'CALL 1'; then
	echo "expected numeric CALL procedure to fail" >&2
	exit 1
fi

if "$parser" --quiet 'CALL @p'; then
	echo "expected variable CALL procedure to fail" >&2
	exit 1
fi

if "$parser" --quiet 'CALL p extra'; then
	echo "expected trailing CALL tokens to fail" >&2
	exit 1
fi

if "$parser" --quiet 'CALL p() extra'; then
	echo "expected trailing CALL group tokens to fail" >&2
	exit 1
fi

if "$parser" --quiet 'CALL p(@a, )'; then
	echo "expected trailing CALL argument comma to fail" >&2
	exit 1
fi

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

show_engine_output=$("$parser" 'SHOW ENGINE InnoDB STATUS; SHOW ENGINE performance_schema MUTEX; SHOW ENGINE NDB STATUS; SHOW ENGINE csv LOGS; SHOW ENGINES')
case "$show_engine_output" in
	*"show"*/engine:InnoDB*"show"*/engine:performance_schema*"show"*/engine:NDB*"show"*/engine:csv*"show"*/engine*) ;;
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

if "$parser" --quiet 'PURGE'; then
	echo "expected missing PURGE body to fail" >&2
	exit 1
fi

if "$parser" --quiet 'PURGE BINARY LOGS'; then
	echo "expected missing PURGE BINARY LOGS action to fail" >&2
	exit 1
fi

if "$parser" --quiet 'PURGE BINARY LOGS TO'; then
	echo "expected missing PURGE BINARY LOGS TO target to fail" >&2
	exit 1
fi

if "$parser" --quiet 'PURGE BINARY LOGS TO @log_file'; then
	echo "expected variable PURGE BINARY LOGS TO target to fail" >&2
	exit 1
fi

if "$parser" --quiet "PURGE BINARY LOGS TO 'bin.000001' extra"; then
	echo "expected trailing PURGE BINARY LOGS TO tokens to fail" >&2
	exit 1
fi

if "$parser" --quiet 'PURGE BINARY LOGS BEFORE'; then
	echo "expected missing PURGE BINARY LOGS BEFORE expression to fail" >&2
	exit 1
fi

if "$parser" --quiet 'PURGE BINARY LOGS BEFORE ,'; then
	echo "expected invalid PURGE BINARY LOGS BEFORE expression to fail" >&2
	exit 1
fi

if "$parser" --quiet "PURGE RELAY LOGS TO 'relay.000001'"; then
	echo "expected unsupported PURGE RELAY LOGS to fail" >&2
	exit 1
fi

if "$parser" --quiet "PURGE BINARY TO 'bin.000001'"; then
	echo "expected missing PURGE LOGS keyword to fail" >&2
	exit 1
fi

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

binlog_event_output=$("$parser" "BINLOG 'abc'")
case "$binlog_event_output" in
	*"binlog"*/binary_log_event:"'abc'"*) ;;
	*)
		echo "unexpected BINLOG event output: $binlog_event_output" >&2
		exit 1
		;;
esac

if "$parser" --quiet "BINLOG"; then
	echo "expected missing BINLOG payload to fail" >&2
	exit 1
fi

if "$parser" --quiet "BINLOG @payload"; then
	echo "expected variable BINLOG payload to fail" >&2
	exit 1
fi

if "$parser" --quiet "BINLOG 1"; then
	echo "expected numeric BINLOG payload to fail" >&2
	exit 1
fi

if "$parser" --quiet "BINLOG 'abc' extra"; then
	echo "expected trailing BINLOG tokens to fail" >&2
	exit 1
fi

if "$parser" --quiet "BINLOG 'abc', 'def'"; then
	echo "expected multi-payload BINLOG to fail" >&2
	exit 1
fi

kill_output=$("$parser" 'KILL 123; KILL QUERY 456; KILL CONNECTION 789; KILL @id; KILL QUERY @thread_id; KILL CONNECTION_ID(); KILL "1"; KILL QUERY @id + 1')
case "$kill_output" in
	*"/connection:USER"*)
		echo "unexpected KILL non-numeric connection output: $kill_output" >&2
		exit 1
		;;
	*"kill"*/connection:123*"kill"*/query:456*"kill"*/connection:789*"kill"*/connection:@id*"kill"*/query:@thread_id*"kill"*/connection:CONNECTION_ID"()"*"kill"*/connection:'"1"'*"kill"*/query:"@id + 1"*) ;;
	*)
		echo "unexpected KILL output: $kill_output" >&2
		exit 1
		;;
esac

if "$parser" --quiet 'KILL QUERY'; then
	echo "expected missing KILL target to fail" >&2
	exit 1
fi

if "$parser" --quiet 'KILL QUERY @id, @id'; then
	echo "expected multi-target KILL to fail" >&2
	exit 1
fi

if "$parser" --quiet 'KILL USER "u"'; then
	echo "expected invalid KILL modifier to fail" >&2
	exit 1
fi

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

if "$parser" --quiet 'FLUSH'; then
	echo "expected missing FLUSH body to fail" >&2
	exit 1
fi

if "$parser" --quiet 'FLUSH LOCAL'; then
	echo "expected missing FLUSH LOCAL body to fail" >&2
	exit 1
fi

if "$parser" --quiet 'FLUSH LOCAL NO_WRITE_TO_BINLOG STATUS'; then
	echo "expected duplicate FLUSH modifier to fail" >&2
	exit 1
fi

if "$parser" --quiet 'FLUSH TABLES t,'; then
	echo "expected trailing FLUSH TABLES comma to fail" >&2
	exit 1
fi

if "$parser" --quiet 'FLUSH TABLES t, WITH READ LOCK'; then
	echo "expected incomplete FLUSH TABLES table list to fail" >&2
	exit 1
fi

if "$parser" --quiet 'FLUSH TABLES WITH READ'; then
	echo "expected incomplete FLUSH TABLES WITH READ LOCK clause to fail" >&2
	exit 1
fi

if "$parser" --quiet 'FLUSH TABLES FOR EXPORT'; then
	echo "expected nameless FLUSH TABLES FOR EXPORT to fail" >&2
	exit 1
fi

if "$parser" --quiet 'FLUSH TABLES t FOR'; then
	echo "expected incomplete FLUSH TABLES FOR EXPORT clause to fail" >&2
	exit 1
fi

if "$parser" --quiet 'FLUSH STATUS,'; then
	echo "expected trailing FLUSH option comma to fail" >&2
	exit 1
fi

if "$parser" --quiet 'FLUSH STATUS USER_RESOURCES'; then
	echo "expected missing FLUSH option comma to fail" >&2
	exit 1
fi

if "$parser" --quiet 'FLUSH RELAY LOGS FOR CHANNEL ch'; then
	echo "expected unquoted FLUSH RELAY channel to fail" >&2
	exit 1
fi

if "$parser" --quiet 'FLUSH RELAY LOGS FOR CHANNEL'; then
	echo "expected missing FLUSH RELAY channel to fail" >&2
	exit 1
fi

if "$parser" --quiet 'FLUSH BINARY'; then
	echo "expected incomplete FLUSH BINARY LOGS option to fail" >&2
	exit 1
fi

if "$parser" --quiet "FLUSH BINARY LOGS FOR CHANNEL 'ch'"; then
	echo "expected FLUSH BINARY LOGS channel clause to fail" >&2
	exit 1
fi

maintenance_output=$("$parser" 'ANALYZE TABLE t; CHECK TABLE `db`.`t`; CHECKSUM TABLE t QUICK; OPTIMIZE TABLE t; REPAIR TABLE t USE_FRM; ANALYZE FORMAT=JSON TABLE t; ANALYZE TABLE t UPDATE HISTOGRAM ON c WITH 10 BUCKETS; ANALYZE TABLE t DROP HISTOGRAM ON c, d; CHECK TABLE t FAST QUICK FOR UPGRADE; OPTIMIZE LOCAL TABLE t, u; REPAIR NO_WRITE_TO_BINLOG TABLE t QUICK EXTENDED USE_FRM')
case "$maintenance_output" in
	*"analyze"*/table:t*"check"*/table:'`db`.`t`'*"checksum"*/table:t*"optimize"*/table:t*"repair"*/table:t*"analyze"*/table:t*"analyze"*/table:t*"analyze"*/table:t*"check"*/table:t*"optimize"*/table:t*"repair"*/table:t*) ;;
	*)
		echo "unexpected maintenance output: $maintenance_output" >&2
		exit 1
		;;
esac

if "$parser" --quiet 'ANALYZE'; then
	echo "expected missing ANALYZE body to fail" >&2
	exit 1
fi

if "$parser" --quiet 'ANALYZE TABLE'; then
	echo "expected missing ANALYZE TABLE target to fail" >&2
	exit 1
fi

if "$parser" --quiet 'ANALYZE TABLE t,'; then
	echo "expected trailing ANALYZE TABLE comma to fail" >&2
	exit 1
fi

if "$parser" --quiet 'ANALYZE FORMAT TABLE t'; then
	echo "expected malformed ANALYZE FORMAT clause to fail" >&2
	exit 1
fi

if "$parser" --quiet 'ANALYZE TABLE t UPDATE HISTOGRAM'; then
	echo "expected incomplete ANALYZE UPDATE HISTOGRAM clause to fail" >&2
	exit 1
fi

if "$parser" --quiet 'ANALYZE TABLE t UPDATE HISTOGRAM ON'; then
	echo "expected missing ANALYZE histogram column to fail" >&2
	exit 1
fi

if "$parser" --quiet 'ANALYZE TABLE t UPDATE HISTOGRAM ON c WITH BUCKETS'; then
	echo "expected malformed ANALYZE histogram bucket clause to fail" >&2
	exit 1
fi

if "$parser" --quiet 'CHECK TABLE'; then
	echo "expected missing CHECK TABLE target to fail" >&2
	exit 1
fi

if "$parser" --quiet 'CHECK TABLE t UNKNOWN'; then
	echo "expected unknown CHECK TABLE option to fail" >&2
	exit 1
fi

if "$parser" --quiet 'CHECK TABLE t FOR'; then
	echo "expected incomplete CHECK TABLE FOR UPGRADE clause to fail" >&2
	exit 1
fi

if "$parser" --quiet 'CHECKSUM TABLE'; then
	echo "expected missing CHECKSUM TABLE target to fail" >&2
	exit 1
fi

if "$parser" --quiet 'CHECKSUM TABLE t QUICK EXTENDED'; then
	echo "expected duplicate CHECKSUM TABLE option to fail" >&2
	exit 1
fi

if "$parser" --quiet 'OPTIMIZE TABLE t QUICK'; then
	echo "expected OPTIMIZE TABLE option to fail" >&2
	exit 1
fi

if "$parser" --quiet 'REPAIR TABLE'; then
	echo "expected missing REPAIR TABLE target to fail" >&2
	exit 1
fi

if "$parser" --quiet 'REPAIR TABLE t UNKNOWN'; then
	echo "expected unknown REPAIR TABLE option to fail" >&2
	exit 1
fi

reset_output=$("$parser" 'RESET PERSIST max_connections; RESET PERSIST IF EXISTS autocommit; RESET PERSIST; RESET BINARY LOGS AND GTIDS; RESET BINARY LOGS AND GTIDS TO 100; RESET MASTER; RESET MASTER TO 100; RESET REPLICA, BINARY LOGS AND GTIDS TO 101')
case "$reset_output" in
	*"/system_variable:IF"*)
		echo "unexpected RESET PERSIST IF target output: $reset_output" >&2
		exit 1
		;;
esac
case "$reset_output" in
	*"reset"*/system_variable:max_connections*"reset"*/system_variable:autocommit*"reset[11:12"*/system_variable*"reset"*/binary_log*"reset"*/binary_log*"reset"*/binary_log*"reset"*/binary_log*"reset"*/replication_channel*) ;;
	*)
		echo "unexpected RESET output: $reset_output" >&2
		exit 1
		;;
esac

if "$parser" --quiet 'RESET'; then
	echo "expected missing RESET body to fail" >&2
	exit 1
fi

if "$parser" --quiet 'RESET PERSIST IF EXISTS'; then
	echo "expected missing RESET PERSIST IF EXISTS variable to fail" >&2
	exit 1
fi

if "$parser" --quiet 'RESET PERSIST @@global.autocommit'; then
	echo "expected scoped RESET PERSIST variable to fail" >&2
	exit 1
fi

if "$parser" --quiet 'RESET PERSIST max_connections extra'; then
	echo "expected trailing RESET PERSIST tokens to fail" >&2
	exit 1
fi

if "$parser" --quiet 'RESET BINARY LOGS'; then
	echo "expected incomplete RESET BINARY LOGS to fail" >&2
	exit 1
fi

if "$parser" --quiet 'RESET BINARY LOGS AND GTIDS TO'; then
	echo "expected missing RESET BINARY LOGS index to fail" >&2
	exit 1
fi

if "$parser" --quiet "RESET BINARY LOGS AND GTIDS TO '100'"; then
	echo "expected string RESET BINARY LOGS index to fail" >&2
	exit 1
fi

if "$parser" --quiet 'RESET BINARY LOGS AND GTIDS TO 100 extra'; then
	echo "expected trailing RESET BINARY LOGS tokens to fail" >&2
	exit 1
fi

if "$parser" --quiet 'RESET REPLICA FOR CHANNEL ch'; then
	echo "expected unquoted RESET REPLICA channel to fail" >&2
	exit 1
fi

if "$parser" --quiet 'RESET REPLICA FOR CHANNEL'; then
	echo "expected missing RESET REPLICA channel to fail" >&2
	exit 1
fi

if "$parser" --quiet 'RESET REPLICA ALL ALL'; then
	echo "expected duplicate RESET REPLICA ALL to fail" >&2
	exit 1
fi

if "$parser" --quiet 'RESET REPLICA,'; then
	echo "expected trailing RESET option comma to fail" >&2
	exit 1
fi

if "$parser" --quiet 'RESET MASTER TO'; then
	echo "expected missing RESET MASTER index to fail" >&2
	exit 1
fi

if "$parser" --quiet "RESET MASTER TO '100'"; then
	echo "expected string RESET MASTER index to fail" >&2
	exit 1
fi

replication_channel_output=$("$parser" "START REPLICA FOR CHANNEL 'ch'; STOP REPLICA SQL_THREAD FOR CHANNEL 'ch'; RESET REPLICA ALL FOR CHANNEL 'ch'; CHANGE REPLICATION SOURCE TO SOURCE_HOST='h' FOR CHANNEL 'ch'; CHANGE REPLICATION FILTER REPLICATE_DO_DB=(db) FOR CHANNEL 'ch'; START REPLICA; STOP REPLICA; RESET REPLICA; CHANGE REPLICATION SOURCE TO SOURCE_HOST='h'; CHANGE MASTER TO MASTER_HOST='h'; START GROUP_REPLICATION; STOP GROUP_REPLICATION")
case "$replication_channel_output" in
	*"start"*/replication_channel:"'ch'"*"stop"*/replication_channel:"'ch'"*"reset"*/replication_channel:"'ch'"*"change"*/replication_channel:"'ch'"*"change"*/replication_channel:"'ch'"*"start"*/replication_channel*"stop"*/replication_channel*"reset"*/replication_channel*"change"*/replication_channel*"change"*/replication_channel*"start"*/group_replication*"stop"*/group_replication*) ;;
	*)
		echo "unexpected replication channel output: $replication_channel_output" >&2
		exit 1
		;;
esac

start_replica_output=$("$parser" "START REPLICA; START SLAVE SQL_THREAD UNTIL SQL_AFTER_MTS_GAPS; START REPLICA IO_THREAD, SQL_THREAD UNTIL SOURCE_LOG_FILE='bin.000001', SOURCE_LOG_POS=4 USER='u' PASSWORD='p' DEFAULT_AUTH='mysql_native_password' PLUGIN_DIR='/tmp' FOR CHANNEL 'ch'; START REPLICA UNTIL SQL_AFTER_GTIDS = 3E11FA47-71CA-11E1-9E33-C80AA9429562:11-56")
case "$start_replica_output" in
	*"start"*/replication_channel*"start"*/replication_channel*"start"*/replication_channel:"'ch'"*"start"*/replication_channel*) ;;
	*)
		echo "unexpected START REPLICA output: $start_replica_output" >&2
		exit 1
		;;
esac

if "$parser" --quiet 'START'; then
	echo "expected missing START operation to fail" >&2
	exit 1
fi

if "$parser" --quiet 'START REPLICA IO_THREAD,'; then
	echo "expected trailing START REPLICA thread comma to fail" >&2
	exit 1
fi

if "$parser" --quiet 'START REPLICA IO_THREAD IO_THREAD'; then
	echo "expected missing START REPLICA thread comma to fail" >&2
	exit 1
fi

if "$parser" --quiet 'START REPLICA UNTIL'; then
	echo "expected missing START REPLICA UNTIL option to fail" >&2
	exit 1
fi

if "$parser" --quiet "START REPLICA UNTIL SOURCE_LOG_FILE='bin.000001'"; then
	echo "expected incomplete START REPLICA log-position UNTIL to fail" >&2
	exit 1
fi

if "$parser" --quiet "START REPLICA UNTIL SOURCE_LOG_FILE='bin.000001', SOURCE_LOG_POS='4'"; then
	echo "expected string START REPLICA log position to fail" >&2
	exit 1
fi

if "$parser" --quiet "START REPLICA SQL_THREAD USER='u'"; then
	echo "expected START REPLICA SQL_THREAD connection options to fail" >&2
	exit 1
fi

if "$parser" --quiet "START REPLICA PASSWORD='p'"; then
	echo "expected START REPLICA password without user to fail" >&2
	exit 1
fi

if "$parser" --quiet 'START REPLICA FOR CHANNEL'; then
	echo "expected missing START REPLICA channel name to fail" >&2
	exit 1
fi

if "$parser" --quiet "START REPLICA FOR CHANNEL 'ch' USER='u'"; then
	echo "expected trailing START REPLICA tokens after channel to fail" >&2
	exit 1
fi

start_group_replication_output=$("$parser" "START GROUP_REPLICATION; START GROUP_REPLICATION USER='u'; START GROUP_REPLICATION USER='u', PASSWORD='p'; START GROUP_REPLICATION USER='u', DEFAULT_AUTH='mysql_native_password'; START GROUP_REPLICATION USER='u', PASSWORD='p', DEFAULT_AUTH='mysql_native_password'")
case "$start_group_replication_output" in
	*"start"*/group_replication*"start"*/group_replication*"start"*/group_replication*"start"*/group_replication*"start"*/group_replication*) ;;
	*)
		echo "unexpected START GROUP_REPLICATION output: $start_group_replication_output" >&2
		exit 1
		;;
esac

if "$parser" --quiet "START GROUP_REPLICATION USER=''"; then
	echo "expected empty START GROUP_REPLICATION user to fail" >&2
	exit 1
fi

if "$parser" --quiet "START GROUP_REPLICATION PASSWORD='p'"; then
	echo "expected START GROUP_REPLICATION password without user to fail" >&2
	exit 1
fi

if "$parser" --quiet "START GROUP_REPLICATION USER='u' PASSWORD='p'"; then
	echo "expected missing START GROUP_REPLICATION option comma to fail" >&2
	exit 1
fi

if "$parser" --quiet "START GROUP_REPLICATION USER='u',"; then
	echo "expected trailing START GROUP_REPLICATION option comma to fail" >&2
	exit 1
fi

if "$parser" --quiet "START GROUP_REPLICATION USER='u', PLUGIN_DIR='/tmp'"; then
	echo "expected unsupported START GROUP_REPLICATION option to fail" >&2
	exit 1
fi

xa_output=$("$parser" "XA START 'x'; XA BEGIN X'6162', 0x62, 7 JOIN; XA START b'1010' RESUME; XA END 'x' SUSPEND FOR MIGRATE; XA PREPARE 'x'; XA COMMIT 'x' ONE PHASE; XA ROLLBACK 'x'; XA RECOVER; XA RECOVER CONVERT XID")
case "$xa_output" in
	*"xa"*/xa_transaction:"'x'"*"xa"*/xa_transaction:"X'6162'"*"xa"*/xa_transaction:"b'1010'"*"xa"*/xa_transaction:"'x'"*"xa"*/xa_transaction:"'x'"*"xa"*/xa_transaction:"'x'"*"xa"*/xa_transaction:"'x'"*"/xa_transaction,xa"*/xa_transaction*) ;;
	*)
		echo "unexpected XA output: $xa_output" >&2
		exit 1
		;;
esac

if "$parser" --quiet 'XA'; then
	echo "expected missing XA action to fail" >&2
	exit 1
fi

if "$parser" --quiet 'XA START'; then
	echo "expected missing XA START xid to fail" >&2
	exit 1
fi

if "$parser" --quiet 'XA START 7'; then
	echo "expected plain numeric XA gtrid to fail" >&2
	exit 1
fi

if "$parser" --quiet "XA START 'x',"; then
	echo "expected missing XA bqual to fail" >&2
	exit 1
fi

if "$parser" --quiet "XA START 'x', 'b', X'07'"; then
	echo "expected non-integer XA formatID to fail" >&2
	exit 1
fi

if "$parser" --quiet "XA START 'x' JOIN RESUME"; then
	echo "expected duplicate XA START option to fail" >&2
	exit 1
fi

if "$parser" --quiet "XA END 'x' SUSPEND FOR"; then
	echo "expected incomplete XA END SUSPEND FOR MIGRATE to fail" >&2
	exit 1
fi

if "$parser" --quiet "XA COMMIT 'x' PHASE"; then
	echo "expected malformed XA COMMIT ONE PHASE to fail" >&2
	exit 1
fi

if "$parser" --quiet 'XA RECOVER CONVERT'; then
	echo "expected incomplete XA RECOVER CONVERT XID to fail" >&2
	exit 1
fi

help_output=$("$parser" "HELP 'contents'; HELP SELECT; HELP no_such_topic; HELP CREATE TABLE; HELP 'CREATE TABLE'")
case "$help_output" in
	*"help"*/help_topic:"'contents'"*"help"*/help_topic:SELECT*"help"*/help_topic:no_such_topic*"help"*/help_topic:"CREATE TABLE"*"help"*/help_topic:"'CREATE TABLE'"*) ;;
	*)
		echo "unexpected HELP output: $help_output" >&2
		exit 1
		;;
esac

if "$parser" --quiet 'HELP @topic'; then
	echo "expected HELP variable topic to fail" >&2
	exit 1
fi

if "$parser" --quiet "HELP 'CREATE' 'TABLE'"; then
	echo "expected multiple HELP topic tokens to fail" >&2
	exit 1
fi

if "$parser" --quiet 'HELP ,'; then
	echo "expected punctuation HELP topic to fail" >&2
	exit 1
fi

clone_output=$("$parser" "CLONE LOCAL DATA DIRECTORY = '/tmp/clone'; CLONE LOCAL DATA DIRECTORY '/tmp/clone2'; CLONE INSTANCE FROM user@host:3306 IDENTIFIED BY 'p'; CLONE INSTANCE FROM 'u'@'h':3306 IDENTIFIED BY 'p' DATA DIRECTORY = '/tmp/clone' REQUIRE NO SSL; CLONE INSTANCE FROM 'u' @ 'h' : 3306 IDENTIFIED BY 'p' REQUIRE SSL")
case "$clone_output" in
	*"clone"*/directory:"'/tmp/clone'"*"clone"*/directory:"'/tmp/clone2'"*"clone"*/server:user@host:3306*"clone"*/server:"'u'@'h':3306"*"clone"*/server:"'u' @ 'h' : 3306"*) ;;
	*)
		echo "unexpected CLONE output: $clone_output" >&2
		exit 1
		;;
esac

if "$parser" --quiet 'CLONE'; then
	echo "expected missing CLONE action to fail" >&2
	exit 1
fi

if "$parser" --quiet 'CLONE LOCAL'; then
	echo "expected incomplete CLONE LOCAL to fail" >&2
	exit 1
fi

if "$parser" --quiet 'CLONE LOCAL DATA DIRECTORY'; then
	echo "expected missing CLONE LOCAL directory to fail" >&2
	exit 1
fi

if "$parser" --quiet "CLONE LOCAL DATA DIRECTORY '/tmp/clone' EXTRA"; then
	echo "expected extra CLONE LOCAL tokens to fail" >&2
	exit 1
fi

if "$parser" --quiet "CLONE INSTANCE FROM user@host IDENTIFIED BY 'p'"; then
	echo "expected CLONE remote endpoint without port to fail" >&2
	exit 1
fi

if "$parser" --quiet 'CLONE INSTANCE FROM user@:3306 IDENTIFIED BY '\''p'\'''; then
	echo "expected CLONE remote endpoint without host to fail" >&2
	exit 1
fi

if "$parser" --quiet 'CLONE INSTANCE FROM user@host:3306'; then
	echo "expected CLONE remote without IDENTIFIED clause to fail" >&2
	exit 1
fi

if "$parser" --quiet 'CLONE INSTANCE FROM user@host:3306 IDENTIFIED '\''p'\'''; then
	echo "expected malformed CLONE IDENTIFIED clause to fail" >&2
	exit 1
fi

if "$parser" --quiet 'CLONE INSTANCE FROM user@host:3306 IDENTIFIED BY password'; then
	echo "expected unquoted CLONE password to fail" >&2
	exit 1
fi

if "$parser" --quiet 'CLONE INSTANCE FROM user@host:3306 REQUIRE SSL IDENTIFIED BY '\''p'\'''; then
	echo "expected out-of-order CLONE REQUIRE clause to fail" >&2
	exit 1
fi

if "$parser" --quiet 'CLONE INSTANCE FROM user@host:3306 IDENTIFIED BY '\''p'\'' DATA DIRECTORY ='; then
	echo "expected missing CLONE DATA DIRECTORY value to fail" >&2
	exit 1
fi

if "$parser" --quiet 'CLONE INSTANCE FROM user@host:3306 IDENTIFIED BY '\''p'\'' REQUIRE NO'; then
	echo "expected incomplete CLONE REQUIRE NO SSL clause to fail" >&2
	exit 1
fi

stop_output=$("$parser" "STOP REPLICA; STOP GROUP_REPLICATION; STOP SLAVE SQL_THREAD; STOP REPLICA IO_THREAD, SQL_THREAD FOR CHANNEL 'ch'; CREATE TABLE stop (id int)")
case "$stop_output" in
	*"stop"*/replication_channel*"stop"*/group_replication*"stop"*/replication_channel*"stop"*/replication_channel:"'ch'"*"create"*/table:stop*) ;;
	*)
		echo "unexpected STOP output: $stop_output" >&2
		exit 1
		;;
esac

if "$parser" --quiet 'STOP'; then
	echo "expected missing STOP operation to fail" >&2
	exit 1
fi

if "$parser" --quiet 'STOP GROUP_REPLICATION NOW'; then
	echo "expected trailing STOP GROUP_REPLICATION tokens to fail" >&2
	exit 1
fi

if "$parser" --quiet 'STOP REPLICA IO_THREAD,'; then
	echo "expected trailing STOP REPLICA thread comma to fail" >&2
	exit 1
fi

if "$parser" --quiet 'STOP REPLICA IO_THREAD IO_THREAD'; then
	echo "expected missing STOP REPLICA thread comma to fail" >&2
	exit 1
fi

if "$parser" --quiet 'STOP REPLICA FOR CHANNEL'; then
	echo "expected missing STOP REPLICA channel name to fail" >&2
	exit 1
fi

prepared_output=$("$parser" "PREPARE stmt FROM @sql; PREPARE \`select\` FROM 'SELECT 1'; EXECUTE stmt USING @a, @b; EXECUTE \`select\`; DEALLOCATE PREPARE stmt; DEALLOCATE PREPARE \`select\`; DROP PREPARE stmt")
case "$prepared_output" in
	*"prepare"*/prepared_statement:stmt*"prepare"*/prepared_statement:'`select`'*"execute"*/prepared_statement:stmt*"execute"*/prepared_statement:'`select`'*"deallocate"*/prepared_statement:stmt*"deallocate"*/prepared_statement:'`select`'*"drop"*/prepared_statement:stmt*) ;;
	*)
		echo "unexpected prepared statement output: $prepared_output" >&2
		exit 1
		;;
esac

if "$parser" --quiet "PREPARE stmt FROM 1"; then
	echo "expected invalid PREPARE source to fail" >&2
	exit 1
fi

if "$parser" --quiet "PREPARE stmt FROM 'SELECT 1' extra"; then
	echo "expected trailing PREPARE tokens to fail" >&2
	exit 1
fi

if "$parser" --quiet "EXECUTE stmt USING @a, 1"; then
	echo "expected invalid EXECUTE binding to fail" >&2
	exit 1
fi

if "$parser" --quiet "EXECUTE stmt extra"; then
	echo "expected trailing EXECUTE tokens to fail" >&2
	exit 1
fi

if "$parser" --quiet "DEALLOCATE stmt"; then
	echo "expected DEALLOCATE without PREPARE to fail" >&2
	exit 1
fi

if "$parser" --quiet "DEALLOCATE PREPARE stmt extra"; then
	echo "expected trailing DEALLOCATE PREPARE tokens to fail" >&2
	exit 1
fi

if "$parser" --quiet "DROP PREPARE stmt extra"; then
	echo "expected trailing DROP PREPARE tokens to fail" >&2
	exit 1
fi

principal_output=$("$parser" "GRANT SELECT ON db.t TO 'u'@'h'; GRANT r TO u WITH ADMIN OPTION; GRANT SELECT ON *.* TO u AS admin WITH ROLE DEFAULT; REVOKE IF EXISTS SELECT ON *.* FROM CURRENT_USER() IGNORE UNKNOWN USER; REVOKE r FROM u")
case "$principal_output" in
	*"grant"*/user:"'u'@'h'"*"grant"*/user:u*"grant"*/user:u*"revoke"*/user:CURRENT_USER*"revoke"*/user:u*) ;;
	*)
		echo "unexpected principal output: $principal_output" >&2
		exit 1
		;;
esac

if "$parser" --quiet "GRANT SELECT ON db.t"; then
	echo "expected GRANT without TO target to fail" >&2
	exit 1
fi

if "$parser" --quiet "GRANT TO u"; then
	echo "expected GRANT without privilege or role source to fail" >&2
	exit 1
fi

if "$parser" --quiet "GRANT SELECT ON db.t TO"; then
	echo "expected GRANT without target principal to fail" >&2
	exit 1
fi

if "$parser" --quiet "GRANT SELECT ON db.t TO u,"; then
	echo "expected GRANT trailing principal comma to fail" >&2
	exit 1
fi

if "$parser" --quiet "GRANT SELECT ON db.t TO u WITH GRANT"; then
	echo "expected incomplete GRANT WITH GRANT OPTION tail to fail" >&2
	exit 1
fi

if "$parser" --quiet "REVOKE SELECT ON db.t"; then
	echo "expected REVOKE without FROM target to fail" >&2
	exit 1
fi

if "$parser" --quiet "REVOKE FROM u"; then
	echo "expected REVOKE without privilege or role source to fail" >&2
	exit 1
fi

if "$parser" --quiet "REVOKE SELECT ON db.t FROM"; then
	echo "expected REVOKE without target principal to fail" >&2
	exit 1
fi

if "$parser" --quiet "REVOKE SELECT ON db.t FROM u IGNORE UNKNOWN"; then
	echo "expected incomplete REVOKE IGNORE UNKNOWN USER tail to fail" >&2
	exit 1
fi

account_ddl_output=$("$parser" "CREATE USER 'u'@'h'; ALTER USER 'u'@'%'; DROP USER IF EXISTS 'u'@'%', user1@; RENAME USER 'u'@'h' TO 'v'@'h', u2@localhost TO u3@localhost; CREATE ROLE IF NOT EXISTS 'r'@'%'; DROP ROLE IF EXISTS 'r'@'%', role2")
case "$account_ddl_output" in
	*"create"*/user:"'u'@'h'"*"alter"*/user:"'u'@'%'"*"drop"*/user:"'u'@'%'"*"rename"*/user:"'u'@'h'"*"create"*/role:"'r'@'%'"*"drop"*/role:"'r'@'%"*) ;;
	*)
		echo "unexpected account DDL output: $account_ddl_output" >&2
		exit 1
		;;
esac

if ! "$parser" --quiet "CREATE ROLE r1, r2@localhost, 'r3'@'%'"; then
	echo "expected CREATE ROLE role list to parse" >&2
	exit 1
fi

if ! "$parser" --quiet "CREATE ROLE IF NOT EXISTS 'r'@'%'"; then
	echo "expected CREATE ROLE IF NOT EXISTS to parse" >&2
	exit 1
fi

if "$parser" --quiet 'CREATE ROLE'; then
	echo "expected missing CREATE ROLE role list to fail" >&2
	exit 1
fi

if "$parser" --quiet 'CREATE ROLE IF EXISTS r'; then
	echo "expected CREATE ROLE IF EXISTS to fail" >&2
	exit 1
fi

if "$parser" --quiet 'CREATE ROLE IF NOT EXISTS'; then
	echo "expected CREATE ROLE IF NOT EXISTS without role list to fail" >&2
	exit 1
fi

if "$parser" --quiet 'CREATE ROLE CURRENT_USER()'; then
	echo "expected CREATE ROLE CURRENT_USER() to fail" >&2
	exit 1
fi

if "$parser" --quiet 'CREATE ROLE r,'; then
	echo "expected CREATE ROLE with trailing comma to fail" >&2
	exit 1
fi

if "$parser" --quiet 'CREATE ROLE r extra'; then
	echo "expected CREATE ROLE with trailing tokens to fail" >&2
	exit 1
fi

if ! "$parser" --quiet "CREATE USER u1 IDENTIFIED BY 'p', u2 IDENTIFIED WITH caching_sha2_password BY RANDOM PASSWORD DEFAULT ROLE r REQUIRE SSL WITH MAX_QUERIES_PER_HOUR 2 PASSWORD EXPIRE INTERVAL 4 DAY ACCOUNT LOCK COMMENT 'x'"; then
	echo "expected CREATE USER with auth and global options to parse" >&2
	exit 1
fi

if ! "$parser" --quiet "CREATE USER IF NOT EXISTS 'u'@'h' IDENTIFIED WITH 'mysql_native_password' AS '*hash'"; then
	echo "expected CREATE USER IF NOT EXISTS with auth hash to parse" >&2
	exit 1
fi

if ! "$parser" --quiet "CREATE USER u REQUIRE CIPHER 'c' AND ISSUER 'i' SUBJECT 's'"; then
	echo "expected CREATE USER TLS options to parse" >&2
	exit 1
fi

if ! "$parser" --quiet "CREATE USER u PASSWORD HISTORY 1 PASSWORD REUSE INTERVAL 2 DAY PASSWORD REQUIRE CURRENT OPTIONAL FAILED_LOGIN_ATTEMPTS 3 PASSWORD_LOCK_TIME UNBOUNDED ATTRIBUTE '{}'"; then
	echo "expected CREATE USER password-management options to parse" >&2
	exit 1
fi

if ! "$parser" --quiet "CREATE USER u IDENTIFIED WITH plugin INITIAL AUTHENTICATION IDENTIFIED BY RANDOM PASSWORD"; then
	echo "expected CREATE USER initial authentication option to parse" >&2
	exit 1
fi

if ! "$parser" --quiet "CREATE USER user1@ IDENTIFIED BY 'p'"; then
	echo "expected CREATE USER trailing-at account with auth to parse" >&2
	exit 1
fi

if ! "$parser" --quiet "CREATE USER u IDENTIFIED BY 'p' AND IDENTIFIED WITH plugin AS 'h' AND IDENTIFIED BY RANDOM PASSWORD"; then
	echo "expected CREATE USER multifactor authentication to parse" >&2
	exit 1
fi

if "$parser" --quiet 'CREATE USER'; then
	echo "expected missing CREATE USER account list to fail" >&2
	exit 1
fi

if "$parser" --quiet 'CREATE USER IF EXISTS u'; then
	echo "expected CREATE USER IF EXISTS to fail" >&2
	exit 1
fi

if "$parser" --quiet 'CREATE USER IF NOT EXISTS'; then
	echo "expected CREATE USER IF NOT EXISTS without account list to fail" >&2
	exit 1
fi

if "$parser" --quiet 'CREATE USER u IDENTIFIED'; then
	echo "expected CREATE USER incomplete IDENTIFIED clause to fail" >&2
	exit 1
fi

if "$parser" --quiet 'CREATE USER u IDENTIFIED BY'; then
	echo "expected CREATE USER IDENTIFIED BY without password to fail" >&2
	exit 1
fi

if "$parser" --quiet 'CREATE USER u IDENTIFIED BY 123'; then
	echo "expected CREATE USER IDENTIFIED BY numeric password to fail" >&2
	exit 1
fi

if "$parser" --quiet 'CREATE USER u IDENTIFIED WITH'; then
	echo "expected CREATE USER IDENTIFIED WITH without plugin to fail" >&2
	exit 1
fi

if "$parser" --quiet "CREATE USER u IDENTIFIED BY 'p' AND IDENTIFIED BY 'q' AND IDENTIFIED BY 'r' AND IDENTIFIED BY 's'"; then
	echo "expected CREATE USER with too many authentication factors to fail" >&2
	exit 1
fi

if "$parser" --quiet 'CREATE USER u DEFAULT ROLE'; then
	echo "expected CREATE USER DEFAULT ROLE without role list to fail" >&2
	exit 1
fi

if "$parser" --quiet 'CREATE USER u REQUIRE SSL AND'; then
	echo "expected CREATE USER REQUIRE with trailing AND to fail" >&2
	exit 1
fi

if "$parser" --quiet 'CREATE USER u WITH MAX_QUERIES_PER_HOUR'; then
	echo "expected CREATE USER resource option without count to fail" >&2
	exit 1
fi

if "$parser" --quiet 'CREATE USER u PASSWORD EXPIRE INTERVAL 4'; then
	echo "expected CREATE USER incomplete PASSWORD EXPIRE INTERVAL to fail" >&2
	exit 1
fi

if "$parser" --quiet 'CREATE USER u ACCOUNT'; then
	echo "expected CREATE USER ACCOUNT without lock state to fail" >&2
	exit 1
fi

if "$parser" --quiet "CREATE USER u COMMENT 'x' extra"; then
	echo "expected CREATE USER COMMENT with trailing tokens to fail" >&2
	exit 1
fi

if "$parser" --quiet 'CREATE USER u,'; then
	echo "expected CREATE USER with trailing comma to fail" >&2
	exit 1
fi

if ! "$parser" --quiet "ALTER USER IF EXISTS u1 IDENTIFIED BY 'new' REPLACE 'old' RETAIN CURRENT PASSWORD, u2 IDENTIFIED WITH plugin BY RANDOM PASSWORD REQUIRE SSL WITH MAX_USER_CONNECTIONS 2 PASSWORD REUSE INTERVAL 1 DAY ACCOUNT UNLOCK COMMENT 'x'"; then
	echo "expected ALTER USER with auth and global options to parse" >&2
	exit 1
fi

if ! "$parser" --quiet "ALTER USER USER() IDENTIFIED BY 'new' RETAIN CURRENT PASSWORD"; then
	echo "expected ALTER USER USER() password change to parse" >&2
	exit 1
fi

if ! "$parser" --quiet "ALTER USER user1@ IDENTIFIED BY 'p'"; then
	echo "expected ALTER USER trailing-at account with auth to parse" >&2
	exit 1
fi

if ! "$parser" --quiet "ALTER USER CURRENT_USER() DEFAULT ROLE NONE"; then
	echo "expected ALTER USER CURRENT_USER() DEFAULT ROLE NONE to parse" >&2
	exit 1
fi

if ! "$parser" --quiet "ALTER USER u DEFAULT ROLE r1, r2"; then
	echo "expected ALTER USER DEFAULT ROLE role list to parse" >&2
	exit 1
fi

if ! "$parser" --quiet "ALTER USER u DISCARD OLD PASSWORD ATTRIBUTE '{}'"; then
	echo "expected ALTER USER DISCARD OLD PASSWORD to parse" >&2
	exit 1
fi

if ! "$parser" --quiet "ALTER USER u ADD 2 FACTOR IDENTIFIED BY 'x' ADD 3 FACTOR IDENTIFIED WITH plugin AS 'hash'"; then
	echo "expected ALTER USER factor ADD operations to parse" >&2
	exit 1
fi

if ! "$parser" --quiet "ALTER USER u 2 FACTOR INITIATE REGISTRATION"; then
	echo "expected ALTER USER factor registration initiation to parse" >&2
	exit 1
fi

if ! "$parser" --quiet "ALTER USER u 3 FACTOR FINISH REGISTRATION SET CHALLENGE_RESPONSE AS 'x'"; then
	echo "expected ALTER USER factor registration finish to parse" >&2
	exit 1
fi

if "$parser" --quiet 'ALTER USER'; then
	echo "expected missing ALTER USER account list to fail" >&2
	exit 1
fi

if "$parser" --quiet 'ALTER USER IF NOT EXISTS u'; then
	echo "expected ALTER USER IF NOT EXISTS to fail" >&2
	exit 1
fi

if "$parser" --quiet 'ALTER USER IF EXISTS'; then
	echo "expected ALTER USER IF EXISTS without account list to fail" >&2
	exit 1
fi

if "$parser" --quiet 'ALTER USER USER() IDENTIFIED WITH plugin'; then
	echo "expected ALTER USER USER() IDENTIFIED WITH to fail" >&2
	exit 1
fi

if "$parser" --quiet 'ALTER USER u IDENTIFIED BY 123'; then
	echo "expected ALTER USER IDENTIFIED BY numeric password to fail" >&2
	exit 1
fi

if "$parser" --quiet "ALTER USER u IDENTIFIED BY 'x' RETAIN CURRENT PASSWORD REPLACE 'old'"; then
	echo "expected ALTER USER REPLACE after RETAIN CURRENT PASSWORD to fail" >&2
	exit 1
fi

if "$parser" --quiet 'ALTER USER u DEFAULT ROLE'; then
	echo "expected ALTER USER DEFAULT ROLE without role target to fail" >&2
	exit 1
fi

if "$parser" --quiet 'ALTER USER u DEFAULT ROLE r1,'; then
	echo "expected ALTER USER DEFAULT ROLE with trailing comma to fail" >&2
	exit 1
fi

if "$parser" --quiet 'ALTER USER u REQUIRE SSL AND'; then
	echo "expected ALTER USER REQUIRE with trailing AND to fail" >&2
	exit 1
fi

if "$parser" --quiet 'ALTER USER u WITH MAX_USER_CONNECTIONS'; then
	echo "expected ALTER USER resource option without count to fail" >&2
	exit 1
fi

if "$parser" --quiet 'ALTER USER u PASSWORD REUSE INTERVAL 1'; then
	echo "expected ALTER USER incomplete PASSWORD REUSE INTERVAL to fail" >&2
	exit 1
fi

if "$parser" --quiet 'ALTER USER u ACCOUNT'; then
	echo "expected ALTER USER ACCOUNT without lock state to fail" >&2
	exit 1
fi

if "$parser" --quiet "ALTER USER u COMMENT 'x' extra"; then
	echo "expected ALTER USER COMMENT with trailing tokens to fail" >&2
	exit 1
fi

if "$parser" --quiet 'ALTER USER u ADD 2 FACTOR'; then
	echo "expected ALTER USER ADD factor without auth option to fail" >&2
	exit 1
fi

if "$parser" --quiet "ALTER USER u 2 FACTOR FINISH REGISTRATION AS 'x'"; then
	echo "expected ALTER USER incomplete registration finish to fail" >&2
	exit 1
fi

if ! "$parser" --quiet 'DROP USER CURRENT_USER()'; then
	echo "expected DROP USER CURRENT_USER() to parse" >&2
	exit 1
fi

if "$parser" --quiet 'DROP USER'; then
	echo "expected missing DROP USER account list to fail" >&2
	exit 1
fi

if "$parser" --quiet 'DROP USER IF EXISTS'; then
	echo "expected missing DROP USER IF EXISTS account list to fail" >&2
	exit 1
fi

if "$parser" --quiet 'DROP USER u extra'; then
	echo "expected DROP USER with trailing tokens to fail" >&2
	exit 1
fi

if "$parser" --quiet 'DROP USER u,'; then
	echo "expected DROP USER with a trailing comma to fail" >&2
	exit 1
fi

if "$parser" --quiet 'DROP ROLE'; then
	echo "expected missing DROP ROLE role list to fail" >&2
	exit 1
fi

if "$parser" --quiet 'DROP ROLE CURRENT_USER()'; then
	echo "expected DROP ROLE CURRENT_USER() to fail" >&2
	exit 1
fi

if "$parser" --quiet 'DROP ROLE r extra'; then
	echo "expected DROP ROLE with trailing tokens to fail" >&2
	exit 1
fi

if "$parser" --quiet 'RENAME USER'; then
	echo "expected missing RENAME USER pair list to fail" >&2
	exit 1
fi

if "$parser" --quiet 'RENAME USER u'; then
	echo "expected RENAME USER without TO target to fail" >&2
	exit 1
fi

if "$parser" --quiet 'RENAME USER u TO'; then
	echo "expected RENAME USER with missing new account to fail" >&2
	exit 1
fi

if "$parser" --quiet 'RENAME USER u TO v extra'; then
	echo "expected RENAME USER with trailing tokens to fail" >&2
	exit 1
fi

if "$parser" --quiet 'RENAME USER u TO v,'; then
	echo "expected RENAME USER with a trailing comma to fail" >&2
	exit 1
fi

set_account_output=$("$parser" "SET ROLE r; SET ROLE ALL; SET ROLE NONE; SET ROLE ALL EXCEPT 'r'@'h'; SET ROLE DEFAULT; SET DEFAULT ROLE r TO 'u'@'h'; SET DEFAULT ROLE ALL TO 'u'@'h'; SET PASSWORD FOR 'u'@'h' = 'x'; SET PASSWORD = 'x'; SET PASSWORD TO RANDOM; SET autocommit=1")
case "$set_account_output" in
	*"set"*/role:r*"set[5:7"*/role*"set[9:11"*/role*"set"*/role:"'r'@'h'"*"set[20:22"*/role*"set"*/role:r*"set"*/user:"'u'@'h'"*"set"*/user:"'u'@'h'"*"set[48:51"*/user*"set[53:56"*/user*"set[58:61"*/system_variable:autocommit*) ;;
	*)
		echo "unexpected SET account output: $set_account_output" >&2
		exit 1
		;;
esac

if ! "$parser" --quiet "SET PASSWORD = 'new' REPLACE 'old'"; then
	echo "expected SET PASSWORD REPLACE to parse" >&2
	exit 1
fi

if ! "$parser" --quiet "SET PASSWORD = 'new' RETAIN CURRENT PASSWORD"; then
	echo "expected SET PASSWORD RETAIN CURRENT PASSWORD to parse" >&2
	exit 1
fi

if ! "$parser" --quiet "SET PASSWORD = 'new' REPLACE 'old' RETAIN CURRENT PASSWORD"; then
	echo "expected SET PASSWORD REPLACE plus RETAIN CURRENT PASSWORD to parse" >&2
	exit 1
fi

if ! "$parser" --quiet "SET PASSWORD FOR 'usr1'@'localhost' TO RANDOM"; then
	echo "expected SET PASSWORD FOR account TO RANDOM to parse" >&2
	exit 1
fi

if "$parser" --quiet 'SET PASSWORD'; then
	echo "expected missing SET PASSWORD auth option to fail" >&2
	exit 1
fi

if "$parser" --quiet 'SET PASSWORD FOR'; then
	echo "expected missing SET PASSWORD FOR account to fail" >&2
	exit 1
fi

if "$parser" --quiet "SET PASSWORD ="; then
	echo "expected SET PASSWORD assignment without string to fail" >&2
	exit 1
fi

if "$parser" --quiet 'SET PASSWORD TO'; then
	echo "expected SET PASSWORD TO without RANDOM to fail" >&2
	exit 1
fi

if "$parser" --quiet "SET PASSWORD = 'new' REPLACE"; then
	echo "expected SET PASSWORD REPLACE without current password to fail" >&2
	exit 1
fi

if "$parser" --quiet "SET PASSWORD = 'new' RETAIN CURRENT"; then
	echo "expected incomplete SET PASSWORD RETAIN CURRENT PASSWORD to fail" >&2
	exit 1
fi

if "$parser" --quiet "SET PASSWORD = 'new' RETAIN CURRENT PASSWORD REPLACE 'old'"; then
	echo "expected SET PASSWORD REPLACE after RETAIN CURRENT PASSWORD to fail" >&2
	exit 1
fi

if "$parser" --quiet "SET PASSWORD = 'new' REPLACE 'old' extra"; then
	echo "expected SET PASSWORD with trailing tokens to fail" >&2
	exit 1
fi

if ! "$parser" --quiet "SET DEFAULT ROLE NONE TO 'u'@'localhost'"; then
	echo "expected SET DEFAULT ROLE NONE to parse" >&2
	exit 1
fi

if "$parser" --quiet 'SET ROLE'; then
	echo "expected missing SET ROLE target to fail" >&2
	exit 1
fi

if "$parser" --quiet 'SET ROLE ALL EXCEPT'; then
	echo "expected SET ROLE ALL EXCEPT without roles to fail" >&2
	exit 1
fi

if "$parser" --quiet 'SET ROLE r,'; then
	echo "expected SET ROLE with a trailing comma to fail" >&2
	exit 1
fi

if "$parser" --quiet 'SET ROLE ALL role1'; then
	echo "expected SET ROLE ALL with trailing role to fail" >&2
	exit 1
fi

if "$parser" --quiet 'SET DEFAULT ROLE'; then
	echo "expected missing SET DEFAULT ROLE target to fail" >&2
	exit 1
fi

if "$parser" --quiet 'SET DEFAULT ROLE ALL'; then
	echo "expected SET DEFAULT ROLE ALL without TO users to fail" >&2
	exit 1
fi

if "$parser" --quiet 'SET DEFAULT ROLE r TO'; then
	echo "expected SET DEFAULT ROLE without users to fail" >&2
	exit 1
fi

if "$parser" --quiet 'SET DEFAULT ROLE r TO CURRENT_USER()'; then
	echo "expected SET DEFAULT ROLE CURRENT_USER() target to fail" >&2
	exit 1
fi

set_charset_output=$("$parser" "SET NAMES utf8mb4 COLLATE utf8mb4_0900_ai_ci; SET NAMES DEFAULT; SET CHARACTER SET 'latin1'; SET CHARSET DEFAULT; SET NAMES 'latin1', @dummy = 'B'; SET CHARSET DEFAULT, @dummy = 'A'; SET CHARACTERISTICS AS TRANSACTION READ WRITE")
case "$set_charset_output" in
	*"set"*/character_set:utf8mb4*"set"*/character_set:DEFAULT*"set"*/character_set:"'latin1'"*"set"*/character_set:DEFAULT*"set"*/character_set:"'latin1'"*"set"*/character_set:DEFAULT*"set[36:41"*) ;;
	*)
		echo "unexpected SET character set output: $set_charset_output" >&2
		exit 1
		;;
esac

if "$parser" --quiet 'SET NAMES'; then
	echo "expected missing SET NAMES character set to fail" >&2
	exit 1
fi

if "$parser" --quiet 'SET NAMES utf8mb4 COLLATE'; then
	echo "expected missing SET NAMES collation to fail" >&2
	exit 1
fi

if "$parser" --quiet 'SET NAMES DEFAULT COLLATE utf8mb4_0900_ai_ci'; then
	echo "expected SET NAMES DEFAULT COLLATE to fail" >&2
	exit 1
fi

if "$parser" --quiet 'SET NAMES utf8mb4 COLLATE utf8mb4_0900_ai_ci EXTRA'; then
	echo "expected trailing SET NAMES tokens to fail" >&2
	exit 1
fi

if "$parser" --quiet 'SET NAMES utf8mb4,'; then
	echo "expected trailing SET NAMES assignment comma to fail" >&2
	exit 1
fi

if "$parser" --quiet 'SET CHARACTER SET'; then
	echo "expected missing SET CHARACTER SET value to fail" >&2
	exit 1
fi

if "$parser" --quiet 'SET CHARACTER SET utf8mb4 COLLATE utf8mb4_0900_ai_ci'; then
	echo "expected SET CHARACTER SET collation clause to fail" >&2
	exit 1
fi

if "$parser" --quiet 'SET CHARSET utf8mb4 @x = 1'; then
	echo "expected SET CHARSET trailing tokens without comma to fail" >&2
	exit 1
fi

resource_group_output=$("$parser" 'CREATE RESOURCE GROUP rg TYPE = USER; ALTER RESOURCE GROUP rg ENABLE; DROP RESOURCE GROUP rg; DROP RESOURCE GROUP rg1 FORCE; SET RESOURCE GROUP rg')
case "$resource_group_output" in
	*"create"*/resource_group:rg*"alter"*/resource_group:rg*"drop"*/resource_group:rg*"drop"*/resource_group:rg1*"set"*/resource_group:rg*) ;;
	*)
		echo "unexpected resource group output: $resource_group_output" >&2
		exit 1
		;;
esac

if ! "$parser" --quiet 'CREATE RESOURCE GROUP rg TYPE=USER VCPU=0-3,9 THREAD_PRIORITY=5 DISABLE'; then
	echo "expected CREATE RESOURCE GROUP options to parse" >&2
	exit 1
fi

if ! "$parser" --quiet 'ALTER RESOURCE GROUP rg VCPU = 0-63 THREAD_PRIORITY = -20 DISABLE FORCE'; then
	echo "expected ALTER RESOURCE GROUP options to parse" >&2
	exit 1
fi

if ! "$parser" --quiet 'SET RESOURCE GROUP rg FOR 14, 78, 4'; then
	echo "expected SET RESOURCE GROUP thread list to parse" >&2
	exit 1
fi

if "$parser" --quiet 'CREATE RESOURCE GROUP rg'; then
	echo "expected CREATE RESOURCE GROUP without TYPE to fail" >&2
	exit 1
fi

if "$parser" --quiet 'CREATE RESOURCE GROUP rg TYPE'; then
	echo "expected CREATE RESOURCE GROUP incomplete TYPE to fail" >&2
	exit 1
fi

if "$parser" --quiet 'CREATE RESOURCE GROUP rg TYPE = OTHER'; then
	echo "expected CREATE RESOURCE GROUP invalid TYPE to fail" >&2
	exit 1
fi

if "$parser" --quiet 'CREATE RESOURCE GROUP rg TYPE = USER VCPU = 0,'; then
	echo "expected CREATE RESOURCE GROUP trailing VCPU comma to fail" >&2
	exit 1
fi

if "$parser" --quiet 'CREATE RESOURCE GROUP rg TYPE = USER DISABLE FORCE'; then
	echo "expected CREATE RESOURCE GROUP DISABLE FORCE to fail" >&2
	exit 1
fi

if "$parser" --quiet 'ALTER RESOURCE GROUP rg'; then
	echo "expected ALTER RESOURCE GROUP without options to fail" >&2
	exit 1
fi

if "$parser" --quiet 'ALTER RESOURCE GROUP rg ENABLE FORCE'; then
	echo "expected ALTER RESOURCE GROUP ENABLE FORCE to fail" >&2
	exit 1
fi

if "$parser" --quiet 'ALTER RESOURCE GROUP rg VCPU ='; then
	echo "expected ALTER RESOURCE GROUP incomplete VCPU to fail" >&2
	exit 1
fi

if "$parser" --quiet 'SET RESOURCE GROUP'; then
	echo "expected SET RESOURCE GROUP without name to fail" >&2
	exit 1
fi

if "$parser" --quiet 'SET RESOURCE GROUP rg FOR'; then
	echo "expected SET RESOURCE GROUP FOR without thread list to fail" >&2
	exit 1
fi

if "$parser" --quiet 'SET RESOURCE GROUP rg FOR 1,'; then
	echo "expected SET RESOURCE GROUP trailing thread comma to fail" >&2
	exit 1
fi

if "$parser" --quiet 'DROP RESOURCE GROUP'; then
	echo "expected missing DROP RESOURCE GROUP name to fail" >&2
	exit 1
fi

if "$parser" --quiet 'DROP RESOURCE GROUP rg IF EXISTS'; then
	echo "expected unsupported DROP RESOURCE GROUP IF EXISTS to fail" >&2
	exit 1
fi

if "$parser" --quiet 'DROP RESOURCE GROUP rg FORCE extra'; then
	echo "expected DROP RESOURCE GROUP with trailing FORCE tokens to fail" >&2
	exit 1
fi

server_logfile_output=$("$parser" 'CREATE SERVER s FOREIGN DATA WRAPPER mysql OPTIONS (HOST "h"); ALTER SERVER s OPTIONS (USER "u"); DROP SERVER IF EXISTS "server_one"; CREATE LOGFILE GROUP lg ADD UNDOFILE "u.dat" ENGINE=NDB; ALTER LOGFILE GROUP lg ADD UNDOFILE "v.dat" ENGINE=NDB; DROP LOGFILE GROUP lg ENGINE=NDB; CREATE TABLESPACE ts ADD DATAFILE "ts.ibd"; DROP TABLESPACE ts ENGINE InnoDB; ALTER UNDO TABLESPACE uts SET INACTIVE; DROP UNDO TABLESPACE undo_003 ENGINE InnoDB; DROP SPATIAL REFERENCE SYSTEM IF EXISTS 4120')
case "$server_logfile_output" in
	*"create"*/server:s*"alter"*/server:s*"drop"*/server:*server_one*"create"*/logfile_group:lg*"alter"*/logfile_group:lg*"drop"*/logfile_group:lg*"create"*/tablespace:ts*"drop"*/tablespace:ts*"alter"*/undo_tablespace:uts*"drop"*/undo_tablespace:undo_003*"drop"*/spatial_reference_system:4120*) ;;
	*)
		echo "unexpected server/logfile output: $server_logfile_output" >&2
		exit 1
		;;
esac

if ! "$parser" --quiet 'CREATE SERVER s FOREIGN DATA WRAPPER mysql OPTIONS (HOST "h", DATABASE "d", USER "u", PASSWORD "p", SOCKET "sock", OWNER "o", PORT 3306)'; then
	echo "expected CREATE SERVER full options to parse" >&2
	exit 1
fi

if ! "$parser" --quiet "CREATE SERVER 'server_one' FOREIGN DATA WRAPPER 'mysql' OPTIONS (HOST 'h')"; then
	echo "expected CREATE SERVER with quoted names to parse" >&2
	exit 1
fi

if ! "$parser" --quiet "ALTER SERVER s OPTIONS (USER 'sally', PORT 3307)"; then
	echo "expected ALTER SERVER options to parse" >&2
	exit 1
fi

if "$parser" --quiet 'CREATE SERVER'; then
	echo "expected CREATE SERVER without a name to fail" >&2
	exit 1
fi

if "$parser" --quiet 'CREATE SERVER s'; then
	echo "expected CREATE SERVER without wrapper clause to fail" >&2
	exit 1
fi

if "$parser" --quiet 'CREATE SERVER s FOREIGN DATA WRAPPER'; then
	echo "expected CREATE SERVER without wrapper name to fail" >&2
	exit 1
fi

if "$parser" --quiet 'CREATE SERVER s FOREIGN DATA WRAPPER mysql'; then
	echo "expected CREATE SERVER without OPTIONS to fail" >&2
	exit 1
fi

if "$parser" --quiet 'CREATE SERVER s FOREIGN DATA WRAPPER mysql OPTIONS ()'; then
	echo "expected CREATE SERVER empty OPTIONS to fail" >&2
	exit 1
fi

if "$parser" --quiet 'CREATE SERVER s FOREIGN DATA WRAPPER mysql OPTIONS (HOST)'; then
	echo "expected CREATE SERVER option without value to fail" >&2
	exit 1
fi

if "$parser" --quiet "CREATE SERVER s FOREIGN DATA WRAPPER mysql OPTIONS (PORT 'x')"; then
	echo "expected CREATE SERVER PORT string value to fail" >&2
	exit 1
fi

if "$parser" --quiet "CREATE SERVER s FOREIGN DATA WRAPPER mysql OPTIONS (HOST 'h',)"; then
	echo "expected CREATE SERVER trailing option comma to fail" >&2
	exit 1
fi

if "$parser" --quiet "CREATE SERVER s FOREIGN DATA WRAPPER mysql OPTIONS (BOGUS 'x')"; then
	echo "expected CREATE SERVER unknown option to fail" >&2
	exit 1
fi

if "$parser" --quiet 'ALTER SERVER'; then
	echo "expected ALTER SERVER without a name to fail" >&2
	exit 1
fi

if "$parser" --quiet 'ALTER SERVER s'; then
	echo "expected ALTER SERVER without OPTIONS to fail" >&2
	exit 1
fi

if "$parser" --quiet 'ALTER SERVER s OPTIONS ()'; then
	echo "expected ALTER SERVER empty OPTIONS to fail" >&2
	exit 1
fi

if "$parser" --quiet "ALTER SERVER s OPTIONS (USER 'u') extra"; then
	echo "expected ALTER SERVER with trailing tokens to fail" >&2
	exit 1
fi

if ! "$parser" --quiet "CREATE LOGFILE GROUP lg ADD UNDOFILE 'u.dat' ENGINE=NDB"; then
	echo "expected CREATE LOGFILE GROUP minimal form to parse" >&2
	exit 1
fi

if ! "$parser" --quiet "CREATE LOGFILE GROUP lg ADD UNDOFILE 'u.dat' INITIAL_SIZE=32M UNDO_BUFFER_SIZE 8M REDO_BUFFER_SIZE=16M NODEGROUP=1 WAIT COMMENT='c' ENGINE NDBCLUSTER"; then
	echo "expected CREATE LOGFILE GROUP full option sequence to parse" >&2
	exit 1
fi

if ! "$parser" --quiet "ALTER LOGFILE GROUP lg ADD UNDOFILE 'v.dat' INITIAL_SIZE=32M WAIT ENGINE=NDBCLUSTER"; then
	echo "expected ALTER LOGFILE GROUP option sequence to parse" >&2
	exit 1
fi

if "$parser" --quiet 'CREATE LOGFILE GROUP'; then
	echo "expected CREATE LOGFILE GROUP without a name to fail" >&2
	exit 1
fi

if "$parser" --quiet 'CREATE LOGFILE GROUP lg'; then
	echo "expected CREATE LOGFILE GROUP without ADD UNDOFILE to fail" >&2
	exit 1
fi

if "$parser" --quiet 'CREATE LOGFILE GROUP lg ADD UNDOFILE'; then
	echo "expected CREATE LOGFILE GROUP without undo file name to fail" >&2
	exit 1
fi

if "$parser" --quiet 'CREATE LOGFILE GROUP lg ADD UNDOFILE 7 ENGINE=NDB'; then
	echo "expected CREATE LOGFILE GROUP numeric undo file name to fail" >&2
	exit 1
fi

if "$parser" --quiet "CREATE LOGFILE GROUP lg ADD UNDOFILE 'u.dat'"; then
	echo "expected CREATE LOGFILE GROUP without ENGINE to fail" >&2
	exit 1
fi

if "$parser" --quiet "CREATE LOGFILE GROUP lg ADD UNDOFILE 'u.dat' ENGINE"; then
	echo "expected CREATE LOGFILE GROUP with incomplete ENGINE to fail" >&2
	exit 1
fi

if "$parser" --quiet "CREATE LOGFILE GROUP lg ADD UNDOFILE 'u.dat' COMMENT 1 ENGINE=NDB"; then
	echo "expected CREATE LOGFILE GROUP nonstring COMMENT to fail" >&2
	exit 1
fi

if "$parser" --quiet "CREATE LOGFILE GROUP lg DROP UNDOFILE 'u.dat' ENGINE=NDB"; then
	echo "expected CREATE LOGFILE GROUP DROP UNDOFILE to fail" >&2
	exit 1
fi

if "$parser" --quiet "CREATE LOGFILE GROUP lg ADD UNDOFILE 'u.dat' ENGINE=NDB extra"; then
	echo "expected CREATE LOGFILE GROUP with trailing ENGINE tokens to fail" >&2
	exit 1
fi

if "$parser" --quiet 'ALTER LOGFILE GROUP lg'; then
	echo "expected ALTER LOGFILE GROUP without ADD UNDOFILE to fail" >&2
	exit 1
fi

if "$parser" --quiet "ALTER LOGFILE GROUP lg ADD UNDOFILE 'v.dat' WAIT"; then
	echo "expected ALTER LOGFILE GROUP without ENGINE to fail" >&2
	exit 1
fi

if "$parser" --quiet "ALTER LOGFILE GROUP lg ADD UNDOFILE 'v.dat' UNDO_BUFFER_SIZE=8M ENGINE=NDB"; then
	echo "expected ALTER LOGFILE GROUP unsupported UNDO_BUFFER_SIZE to fail" >&2
	exit 1
fi

if "$parser" --quiet "ALTER LOGFILE GROUP lg DROP UNDOFILE 'v.dat' ENGINE=NDB"; then
	echo "expected ALTER LOGFILE GROUP DROP UNDOFILE to fail" >&2
	exit 1
fi

if ! "$parser" --quiet 'CREATE TABLESPACE ts'; then
	echo "expected CREATE TABLESPACE minimal form to parse" >&2
	exit 1
fi

if ! "$parser" --quiet "CREATE TABLESPACE ts ADD DATAFILE 'ts.ibd' AUTOEXTEND_SIZE=4M FILE_BLOCK_SIZE=16K ENCRYPTION='Y' ENGINE=InnoDB ENGINE_ATTRIBUTE='{}'"; then
	echo "expected CREATE TABLESPACE InnoDB options to parse" >&2
	exit 1
fi

if ! "$parser" --quiet "CREATE TABLESPACE ts ADD DATAFILE 'ts.dat' USE LOGFILE GROUP lg EXTENT_SIZE=1M INITIAL_SIZE=2M MAX_SIZE=16M NODEGROUP=1 WAIT COMMENT='c' ENGINE=NDB"; then
	echo "expected CREATE TABLESPACE NDB options to parse" >&2
	exit 1
fi

if ! "$parser" --quiet "CREATE UNDO TABLESPACE uts ADD DATAFILE 'uts.ibu' AUTOEXTEND_SIZE=8M ENGINE=InnoDB"; then
	echo "expected CREATE UNDO TABLESPACE options to parse" >&2
	exit 1
fi

if ! "$parser" --quiet "ALTER TABLESPACE ts ADD DATAFILE 'ts2.ibd' INITIAL_SIZE=32M WAIT ENGINE=InnoDB"; then
	echo "expected ALTER TABLESPACE ADD DATAFILE to parse" >&2
	exit 1
fi

if ! "$parser" --quiet "ALTER TABLESPACE ts DROP DATAFILE 'ts2.ibd' WAIT ENGINE=NDB"; then
	echo "expected ALTER TABLESPACE DROP DATAFILE to parse" >&2
	exit 1
fi

if ! "$parser" --quiet 'ALTER TABLESPACE ts RENAME TO ts2'; then
	echo "expected ALTER TABLESPACE RENAME to parse" >&2
	exit 1
fi

if ! "$parser" --quiet 'ALTER TABLESPACE ts AUTOEXTEND_SIZE=16M'; then
	echo "expected ALTER TABLESPACE AUTOEXTEND_SIZE to parse" >&2
	exit 1
fi

if ! "$parser" --quiet "ALTER TABLESPACE ts AUTOEXTEND_SIZE='16M'"; then
	echo "expected ALTER TABLESPACE quoted AUTOEXTEND_SIZE to parse" >&2
	exit 1
fi

if ! "$parser" --quiet "ALTER TABLESPACE ts ENCRYPTION='N'"; then
	echo "expected ALTER TABLESPACE ENCRYPTION to parse" >&2
	exit 1
fi

if ! "$parser" --quiet "ALTER TABLESPACE ts ENGINE_ATTRIBUTE='{}'"; then
	echo "expected ALTER TABLESPACE ENGINE_ATTRIBUTE to parse" >&2
	exit 1
fi

if ! "$parser" --quiet 'ALTER UNDO TABLESPACE uts SET ACTIVE ENGINE=InnoDB'; then
	echo "expected ALTER UNDO TABLESPACE SET ACTIVE to parse" >&2
	exit 1
fi

if "$parser" --quiet 'CREATE UNDO TABLESPACE uts'; then
	echo "expected CREATE UNDO TABLESPACE without ADD DATAFILE to fail" >&2
	exit 1
fi

if "$parser" --quiet 'CREATE TABLESPACE ts ADD DATAFILE 1'; then
	echo "expected CREATE TABLESPACE numeric DATAFILE to fail" >&2
	exit 1
fi

if "$parser" --quiet "CREATE TABLESPACE ts ADD DATAFILE 'x' AUTOEXTEND_SIZE='x'"; then
	echo "expected CREATE TABLESPACE string AUTOEXTEND_SIZE to fail" >&2
	exit 1
fi

if "$parser" --quiet 'CREATE TABLESPACE ts USE LOGFILE GROUP'; then
	echo "expected CREATE TABLESPACE incomplete logfile group clause to fail" >&2
	exit 1
fi

if "$parser" --quiet 'CREATE TABLESPACE ts USE LOGFILE GROUP lg ENGINE=NDB'; then
	echo "expected CREATE TABLESPACE logfile group without datafile to fail" >&2
	exit 1
fi

if "$parser" --quiet "CREATE TABLESPACE ts ADD DATAFILE 'x' FILE_BLOCK_SIZE 16K"; then
	echo "expected CREATE TABLESPACE FILE_BLOCK_SIZE without equals to fail" >&2
	exit 1
fi

if "$parser" --quiet "CREATE TABLESPACE ts ADD DATAFILE 'x' ENGINE"; then
	echo "expected CREATE TABLESPACE incomplete ENGINE to fail" >&2
	exit 1
fi

if "$parser" --quiet "CREATE TABLESPACE ts ADD DATAFILE 'x' ENGINE=InnoDB extra"; then
	echo "expected CREATE TABLESPACE trailing ENGINE tokens to fail" >&2
	exit 1
fi

if "$parser" --quiet 'ALTER TABLESPACE ts'; then
	echo "expected ALTER TABLESPACE without an action to fail" >&2
	exit 1
fi

if "$parser" --quiet 'ALTER TABLESPACE ts SET INACTIVE'; then
	echo "expected ALTER TABLESPACE SET state without UNDO to fail" >&2
	exit 1
fi

if "$parser" --quiet 'ALTER TABLESPACE ts ADD DATAFILE'; then
	echo "expected ALTER TABLESPACE ADD DATAFILE without file name to fail" >&2
	exit 1
fi

if "$parser" --quiet "ALTER TABLESPACE ts DROP DATAFILE 'x' INITIAL_SIZE=1M"; then
	echo "expected ALTER TABLESPACE DROP DATAFILE INITIAL_SIZE to fail" >&2
	exit 1
fi

if "$parser" --quiet 'ALTER TABLESPACE ts RENAME ts2'; then
	echo "expected ALTER TABLESPACE RENAME without TO to fail" >&2
	exit 1
fi

if "$parser" --quiet 'ALTER UNDO TABLESPACE uts'; then
	echo "expected ALTER UNDO TABLESPACE without SET state to fail" >&2
	exit 1
fi

if "$parser" --quiet 'ALTER UNDO TABLESPACE uts SET'; then
	echo "expected ALTER UNDO TABLESPACE incomplete SET to fail" >&2
	exit 1
fi

if "$parser" --quiet "ALTER UNDO TABLESPACE uts ADD DATAFILE 'x'"; then
	echo "expected ALTER UNDO TABLESPACE ADD DATAFILE to fail" >&2
	exit 1
fi

if ! "$parser" --quiet "CREATE SPATIAL REFERENCE SYSTEM 4120 NAME 'srs' DEFINITION 'def' ORGANIZATION 'EPSG' IDENTIFIED BY 4120 DESCRIPTION 'desc'"; then
	echo "expected CREATE SPATIAL REFERENCE SYSTEM full attributes to parse" >&2
	exit 1
fi

if ! "$parser" --quiet "CREATE SPATIAL REFERENCE SYSTEM IF NOT EXISTS 4121 NAME 'srs'"; then
	echo "expected CREATE SPATIAL REFERENCE SYSTEM IF NOT EXISTS to parse" >&2
	exit 1
fi

if ! "$parser" --quiet "CREATE OR REPLACE SPATIAL REFERENCE SYSTEM 4122 NAME 'srs' ORGANIZATION 'EPSG' IDENTIFIED BY 4122"; then
	echo "expected CREATE OR REPLACE SPATIAL REFERENCE SYSTEM to parse" >&2
	exit 1
fi

if "$parser" --quiet 'CREATE SPATIAL REFERENCE SYSTEM'; then
	echo "expected CREATE SPATIAL REFERENCE SYSTEM without SRID to fail" >&2
	exit 1
fi

if "$parser" --quiet "CREATE SPATIAL REFERENCE SYSTEM IF EXISTS 4120 NAME 'srs'"; then
	echo "expected CREATE SPATIAL REFERENCE SYSTEM IF EXISTS to fail" >&2
	exit 1
fi

if "$parser" --quiet "CREATE OR SPATIAL REFERENCE SYSTEM 4120 NAME 'srs'"; then
	echo "expected malformed CREATE OR SPATIAL REFERENCE SYSTEM to fail" >&2
	exit 1
fi

if "$parser" --quiet "CREATE OR REPLACE SPATIAL REFERENCE SYSTEM IF NOT EXISTS 4120 NAME 'srs'"; then
	echo "expected CREATE OR REPLACE SPATIAL REFERENCE SYSTEM with IF NOT EXISTS to fail" >&2
	exit 1
fi

if "$parser" --quiet "CREATE SPATIAL REFERENCE SYSTEM srs NAME 'srs'"; then
	echo "expected CREATE SPATIAL REFERENCE SYSTEM nonnumeric SRID to fail" >&2
	exit 1
fi

if "$parser" --quiet 'CREATE SPATIAL REFERENCE SYSTEM 4120'; then
	echo "expected CREATE SPATIAL REFERENCE SYSTEM without attributes to fail" >&2
	exit 1
fi

if "$parser" --quiet 'CREATE SPATIAL REFERENCE SYSTEM 4120 NAME'; then
	echo "expected CREATE SPATIAL REFERENCE SYSTEM NAME without string to fail" >&2
	exit 1
fi

if "$parser" --quiet "CREATE SPATIAL REFERENCE SYSTEM 4120 ORGANIZATION 'EPSG' BY 4120"; then
	echo "expected CREATE SPATIAL REFERENCE SYSTEM ORGANIZATION without IDENTIFIED to fail" >&2
	exit 1
fi

if "$parser" --quiet "CREATE SPATIAL REFERENCE SYSTEM 4120 ORGANIZATION 'EPSG' IDENTIFIED BY 'x'"; then
	echo "expected CREATE SPATIAL REFERENCE SYSTEM nonnumeric organization id to fail" >&2
	exit 1
fi

if "$parser" --quiet "CREATE SPATIAL REFERENCE SYSTEM 4120 NAME 'srs' NAME 'duplicate'"; then
	echo "expected CREATE SPATIAL REFERENCE SYSTEM duplicate NAME to fail" >&2
	exit 1
fi

if "$parser" --quiet "CREATE SPATIAL REFERENCE SYSTEM 4120 NAME 'srs' EXTRA 'x'"; then
	echo "expected CREATE SPATIAL REFERENCE SYSTEM unknown attribute to fail" >&2
	exit 1
fi

if "$parser" --quiet 'DROP SERVER'; then
	echo "expected DROP SERVER without a name to fail" >&2
	exit 1
fi

if "$parser" --quiet 'DROP SERVER s extra'; then
	echo "expected DROP SERVER with trailing tokens to fail" >&2
	exit 1
fi

if "$parser" --quiet 'DROP SPATIAL REFERENCE SYSTEM IF EXISTS'; then
	echo "expected DROP SPATIAL REFERENCE SYSTEM without an SRID to fail" >&2
	exit 1
fi

if "$parser" --quiet 'DROP SPATIAL REFERENCE SYSTEM IF EXISTS srs'; then
	echo "expected DROP SPATIAL REFERENCE SYSTEM with a nonnumeric SRID to fail" >&2
	exit 1
fi

if "$parser" --quiet 'DROP SPATIAL REFERENCE SYSTEM 4120 extra'; then
	echo "expected DROP SPATIAL REFERENCE SYSTEM with trailing tokens to fail" >&2
	exit 1
fi

if "$parser" --quiet 'DROP TABLESPACE'; then
	echo "expected DROP TABLESPACE without a name to fail" >&2
	exit 1
fi

if "$parser" --quiet 'DROP TABLESPACE ts ENGINE'; then
	echo "expected DROP TABLESPACE with an incomplete ENGINE clause to fail" >&2
	exit 1
fi

if "$parser" --quiet 'DROP TABLESPACE ts ENGINE InnoDB extra'; then
	echo "expected DROP TABLESPACE with trailing ENGINE tokens to fail" >&2
	exit 1
fi

if "$parser" --quiet 'DROP UNDO TABLESPACE'; then
	echo "expected DROP UNDO TABLESPACE without a name to fail" >&2
	exit 1
fi

if "$parser" --quiet 'DROP UNDO TABLESPACE uts ENGINE InnoDB extra'; then
	echo "expected DROP UNDO TABLESPACE with trailing ENGINE tokens to fail" >&2
	exit 1
fi

if "$parser" --quiet 'DROP LOGFILE GROUP lg'; then
	echo "expected DROP LOGFILE GROUP without ENGINE to fail" >&2
	exit 1
fi

if "$parser" --quiet 'DROP LOGFILE GROUP lg ENGINE'; then
	echo "expected DROP LOGFILE GROUP with an incomplete ENGINE clause to fail" >&2
	exit 1
fi

if "$parser" --quiet 'DROP LOGFILE GROUP lg ENGINE NDB extra'; then
	echo "expected DROP LOGFILE GROUP with trailing ENGINE tokens to fail" >&2
	exit 1
fi

instance_output=$("$parser" 'RESTART; SHUTDOWN; ALTER INSTANCE ROTATE INNODB MASTER KEY; ALTER INSTANCE RELOAD TLS; LOCK INSTANCE FOR BACKUP; UNLOCK INSTANCE; LOCK TABLES t READ; ALTER TABLE t ADD COLUMN c int')
case "$instance_output" in
	*"restart"*/instance*"shutdown"*/instance*"alter"*/instance*"alter"*/instance*"lock"*/instance*"unlock"*/instance*"lock"*/table:t*"alter"*/table:t*) ;;
	*)
		echo "unexpected instance output: $instance_output" >&2
		exit 1
		;;
esac

if ! "$parser" --quiet 'ALTER INSTANCE ENABLE INNODB REDO_LOG'; then
	echo "expected ALTER INSTANCE ENABLE INNODB REDO_LOG to parse" >&2
	exit 1
fi

if ! "$parser" --quiet 'ALTER INSTANCE DISABLE INNODB REDO_LOG'; then
	echo "expected ALTER INSTANCE DISABLE INNODB REDO_LOG to parse" >&2
	exit 1
fi

if ! "$parser" --quiet 'ALTER INSTANCE ROTATE BINLOG MASTER KEY'; then
	echo "expected ALTER INSTANCE ROTATE BINLOG MASTER KEY to parse" >&2
	exit 1
fi

if ! "$parser" --quiet 'ALTER INSTANCE RELOAD TLS FOR CHANNEL mysql_main NO ROLLBACK ON ERROR'; then
	echo "expected ALTER INSTANCE RELOAD TLS FOR CHANNEL mysql_main NO ROLLBACK ON ERROR to parse" >&2
	exit 1
fi

if ! "$parser" --quiet 'ALTER INSTANCE RELOAD TLS FOR CHANNEL mysql_admin'; then
	echo "expected ALTER INSTANCE RELOAD TLS FOR CHANNEL mysql_admin to parse" >&2
	exit 1
fi

if ! "$parser" --quiet 'ALTER INSTANCE RELOAD KEYRING'; then
	echo "expected ALTER INSTANCE RELOAD KEYRING to parse" >&2
	exit 1
fi

if "$parser" --quiet 'ALTER INSTANCE'; then
	echo "expected ALTER INSTANCE without action to fail" >&2
	exit 1
fi

if "$parser" --quiet 'ALTER INSTANCE ENABLE REDO_LOG'; then
	echo "expected ALTER INSTANCE ENABLE without INNODB to fail" >&2
	exit 1
fi

if "$parser" --quiet 'ALTER INSTANCE ROTATE MASTER KEY'; then
	echo "expected ALTER INSTANCE ROTATE without key family to fail" >&2
	exit 1
fi

if "$parser" --quiet 'ALTER INSTANCE RELOAD TLS FOR CHANNEL mysql_replication'; then
	echo "expected ALTER INSTANCE RELOAD TLS unknown channel to fail" >&2
	exit 1
fi

if "$parser" --quiet 'ALTER INSTANCE RELOAD TLS NO ROLLBACK'; then
	echo "expected ALTER INSTANCE RELOAD TLS incomplete no-rollback clause to fail" >&2
	exit 1
fi

if "$parser" --quiet 'ALTER INSTANCE RELOAD TLS NO ROLLBACK ON ERROR FOR CHANNEL mysql_main'; then
	echo "expected ALTER INSTANCE RELOAD TLS wrong clause order to fail" >&2
	exit 1
fi

if "$parser" --quiet 'ALTER INSTANCE RELOAD KEYRING NO ROLLBACK ON ERROR'; then
	echo "expected ALTER INSTANCE RELOAD KEYRING trailing tokens to fail" >&2
	exit 1
fi

if "$parser" --quiet 'RESTART NOW'; then
	echo "expected RESTART body to fail" >&2
	exit 1
fi

if "$parser" --quiet 'SHUTDOWN NOW'; then
	echo "expected SHUTDOWN body to fail" >&2
	exit 1
fi

install_output=$("$parser" "INSTALL PLUGIN p SONAME 'x.so'; UNINSTALL PLUGIN p; INSTALL COMPONENT 'file://component', 'file://component2'; INSTALL COMPONENT 'file://component_validate_password' SET length = 8 + 8, PERSIST validate_password.number_count = 13; UNINSTALL COMPONENT 'file://component', 'file://component2'")
case "$install_output" in
	*"install"*/plugin:p*"uninstall"*/plugin:p*"install"*/component:"'file://component'"*"uninstall"*/component:"'file://component'"*) ;;
	*)
		echo "unexpected install output: $install_output" >&2
		exit 1
		;;
esac

if "$parser" --quiet "INSTALL PLUGIN p"; then
	echo "expected missing INSTALL PLUGIN SONAME to fail" >&2
	exit 1
fi

if "$parser" --quiet "INSTALL PLUGIN p SONAME"; then
	echo "expected missing INSTALL PLUGIN library to fail" >&2
	exit 1
fi

if "$parser" --quiet "INSTALL PLUGIN 1 SONAME 'x.so'"; then
	echo "expected numeric INSTALL PLUGIN target to fail" >&2
	exit 1
fi

if "$parser" --quiet "INSTALL COMPONENT @component"; then
	echo "expected variable INSTALL COMPONENT URI to fail" >&2
	exit 1
fi

if "$parser" --quiet "INSTALL COMPONENT 'file://component',"; then
	echo "expected trailing INSTALL COMPONENT comma to fail" >&2
	exit 1
fi

if "$parser" --quiet "INSTALL COMPONENT 'file://component' SET"; then
	echo "expected missing INSTALL COMPONENT SET assignment to fail" >&2
	exit 1
fi

if "$parser" --quiet "INSTALL COMPONENT 'file://component' SET length"; then
	echo "expected missing INSTALL COMPONENT SET assignment operator to fail" >&2
	exit 1
fi

if "$parser" --quiet "INSTALL COMPONENT 'file://component' SET length ="; then
	echo "expected missing INSTALL COMPONENT SET value to fail" >&2
	exit 1
fi

if "$parser" --quiet "INSTALL COMPONENT 'file://component' SET length = 12,"; then
	echo "expected trailing INSTALL COMPONENT SET comma to fail" >&2
	exit 1
fi

if "$parser" --quiet "UNINSTALL PLUGIN"; then
	echo "expected missing UNINSTALL PLUGIN target to fail" >&2
	exit 1
fi

if "$parser" --quiet "UNINSTALL PLUGIN p extra"; then
	echo "expected trailing UNINSTALL PLUGIN tokens to fail" >&2
	exit 1
fi

if "$parser" --quiet "UNINSTALL COMPONENT @component"; then
	echo "expected variable UNINSTALL COMPONENT URI to fail" >&2
	exit 1
fi

if "$parser" --quiet "UNINSTALL COMPONENT 'file://component' SET length = 12"; then
	echo "expected UNINSTALL COMPONENT SET clause to fail" >&2
	exit 1
fi

savepoint_output=$("$parser" 'SAVEPOINT s; SAVEPOINT `select`; RELEASE SAVEPOINT s; ROLLBACK TO SAVEPOINT `s`; ROLLBACK TO `s`; ROLLBACK WORK TO s; ROLLBACK WORK TO SAVEPOINT s; ROLLBACK; ROLLBACK WORK AND CHAIN')
case "$savepoint_output" in
	*"savepoint"*/savepoint:s*"savepoint"*/savepoint:'`select`'*"release"*/savepoint:s*"rollback"*/savepoint:'`s`'*"rollback"*/savepoint:'`s`'*"rollback"*/savepoint:s*"rollback"*/savepoint:s*"rollback"*/transaction*"rollback"*/transaction*) ;;
	*)
		echo "unexpected savepoint output: $savepoint_output" >&2
		exit 1
		;;
esac

if "$parser" --quiet 'SAVEPOINT 1'; then
	echo "expected numeric SAVEPOINT name to fail" >&2
	exit 1
fi

if "$parser" --quiet 'SAVEPOINT s extra'; then
	echo "expected trailing SAVEPOINT tokens to fail" >&2
	exit 1
fi

if "$parser" --quiet 'RELEASE s'; then
	echo "expected RELEASE without SAVEPOINT to fail" >&2
	exit 1
fi

if "$parser" --quiet 'RELEASE SAVEPOINT'; then
	echo "expected missing RELEASE SAVEPOINT name to fail" >&2
	exit 1
fi

if "$parser" --quiet 'ROLLBACK TO'; then
	echo "expected missing ROLLBACK TO name to fail" >&2
	exit 1
fi

if "$parser" --quiet 'ROLLBACK TO @s'; then
	echo "expected variable ROLLBACK TO name to fail" >&2
	exit 1
fi

if "$parser" --quiet 'ROLLBACK TO SAVEPOINT s extra'; then
	echo "expected trailing ROLLBACK TO tokens to fail" >&2
	exit 1
fi

transaction_output=$("$parser" "BEGIN; BEGIN WORK; BEGIN NOT ATOMIC SELECT 1 END; START TRANSACTION; START TRANSACTION READ WRITE; START TRANSACTION WITH CONSISTENT SNAPSHOT, READ ONLY; START REPLICA FOR CHANNEL 'ch'; COMMIT; COMMIT AND CHAIN; ROLLBACK; ROLLBACK AND NO CHAIN; ROLLBACK TO SAVEPOINT s; SET TRANSACTION ISOLATION LEVEL READ COMMITTED; SET SESSION TRANSACTION READ ONLY; SET GLOBAL TRANSACTION READ WRITE; SET LOCAL TRANSACTION READ ONLY; SET SESSION sql_mode = 'ANSI'")
case "$transaction_output" in
	*"begin"*/transaction*"begin"*/transaction*"begin[6:11"*"start"*/transaction*"start"*/transaction*"start"*/transaction*"start"*/replication_channel:"'ch'"*"commit"*/transaction*"commit"*/transaction*"rollback"*/transaction*"rollback"*/transaction*"rollback"*/savepoint:s*"set"*/transaction*"set"*/transaction*"set"*/transaction*"set"*/transaction*"set"*/system_variable:sql_mode*) ;;
	*)
		echo "unexpected transaction output: $transaction_output" >&2
		exit 1
		;;
esac

"$parser" --quiet 'COMMIT WORK; COMMIT RELEASE; COMMIT NO RELEASE; COMMIT AND CHAIN NO RELEASE; COMMIT AND NO CHAIN RELEASE; ROLLBACK WORK NO RELEASE; ROLLBACK AND CHAIN NO RELEASE; ROLLBACK AND NO CHAIN RELEASE'

if "$parser" --quiet 'COMMIT foo'; then
	echo "expected invalid COMMIT body to fail" >&2
	exit 1
fi

if "$parser" --quiet 'START TRANSACTION READ'; then
	echo "expected incomplete START TRANSACTION READ modifier to fail" >&2
	exit 1
fi

if "$parser" --quiet 'START TRANSACTION READ ONLY, READ WRITE'; then
	echo "expected conflicting START TRANSACTION read modes to fail" >&2
	exit 1
fi

if "$parser" --quiet 'START TRANSACTION WITH CONSISTENT SNAPSHOT,'; then
	echo "expected trailing START TRANSACTION modifier comma to fail" >&2
	exit 1
fi

if "$parser" --quiet 'START TRANSACTION READ ONLY WITH CONSISTENT SNAPSHOT'; then
	echo "expected missing START TRANSACTION modifier comma to fail" >&2
	exit 1
fi

if "$parser" --quiet 'COMMIT NO CHAIN'; then
	echo "expected COMMIT without AND before CHAIN to fail" >&2
	exit 1
fi

if "$parser" --quiet 'COMMIT AND CHAIN RELEASE'; then
	echo "expected incompatible COMMIT CHAIN RELEASE form to fail" >&2
	exit 1
fi

if "$parser" --quiet 'COMMIT RELEASE AND CHAIN'; then
	echo "expected reversed COMMIT completion clauses to fail" >&2
	exit 1
fi

if "$parser" --quiet 'ROLLBACK foo'; then
	echo "expected invalid ROLLBACK body to fail" >&2
	exit 1
fi

if "$parser" --quiet 'ROLLBACK WORK foo'; then
	echo "expected invalid ROLLBACK WORK body to fail" >&2
	exit 1
fi

if "$parser" --quiet 'ROLLBACK AND CHAIN RELEASE'; then
	echo "expected incompatible ROLLBACK CHAIN RELEASE form to fail" >&2
	exit 1
fi

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
