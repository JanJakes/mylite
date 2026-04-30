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

cursor_output=$("$parser" 'DECLARE c CURSOR FOR SELECT 1; DECLARE x INT; OPEN c; FETCH c INTO x; CLOSE c; CREATE TABLE cursor (id int)')
case "$cursor_output" in
	*"declare"*/cursor:c*"declare"*/local_variable:x*"open"*/cursor:c*"fetch"*/cursor:c*"close"*/cursor:c*"create"*/table:cursor*) ;;
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

labeled_statement_output=$("$parser" 'done: LOOP LEAVE done END LOOP done; rpt: REPEAT ITERATE rpt UNTIL done END REPEAT rpt; wh: WHILE done DO LEAVE wh END WHILE wh; blk: BEGIN SELECT 1 END blk')
case "$labeled_statement_output" in
	*"loop"*/label:done*"repeat"*/label:rpt*"while"*/label:wh*"begin"*/label:blk*) ;;
	*)
		echo "unexpected labeled statement output: $labeled_statement_output" >&2
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

variable_assignment_output=$("$parser" "SELECT a INTO @x FROM t; SELECT a INTO local_var FROM t; SELECT a FROM t INTO @x; SELECT a INTO OUTFILE '/tmp/x' FROM t; SELECT a INTO DUMPFILE '/tmp/y' FROM t; SET @x = 1; SET @@session.sql_mode = 'ANSI'; SET SESSION sql_mode = 'ANSI'; SET x = 1; GET DIAGNOSTICS @n = NUMBER; GET CURRENT DIAGNOSTICS CONDITION 1 @state = RETURNED_SQLSTATE")
case "$variable_assignment_output" in
	*"/system_variable:x"*|*"/local_variable:x"*)
		echo "unexpected unadorned SET variable output: $variable_assignment_output" >&2
		exit 1
		;;
esac
case "$variable_assignment_output" in
	*"select"*/user_variable:@x*"select"*/local_variable:local_var*"select"*/user_variable:@x*"select"*/outfile:"'/tmp/x'"*"select"*/dumpfile:"'/tmp/y'"*"set"*/user_variable:@x*"set"*/system_variable:@@session.sql_mode*"set"*/system_variable:sql_mode*"get"*/user_variable:@n*"get"*/diagnostics_condition:1*) ;;
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

import_output=$("$parser" "IMPORT TABLE FROM '/tmp/a.sdi', '/tmp/b.sdi'; IMPORT TABLE FROM @file")
case "$import_output" in
	*"import"*/sdi_file:"'/tmp/a.sdi'"*"import[8:11"*) ;;
	*)
		echo "unexpected IMPORT output: $import_output" >&2
		exit 1
		;;
esac

call_output=$("$parser" 'CALL p; CALL p(); CALL `db`.`p`(@a); CALL 1')
case "$call_output" in
	*"call"*/procedure:p*"call"*/procedure:p*"call"*/procedure:'`db`.`p`'*"call[17:18"*) ;;
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
EXPLAIN SELECT 1 FOR CONNECTION 789'
explain_object_output=$("$parser" "$explain_sql")
case "$explain_object_output" in
	*"/table:FORMAT"*|*"/table:SELECT"*|*"explain[43:48"*/connection:*)
		echo "unexpected EXPLAIN/DESCRIBE object output: $explain_object_output" >&2
		exit 1
		;;
	*"describe"*/table:'`db`.`t`'*"describe"*/table:t*"explain"*/table:'`db`.`e`'*"explain[15:17"*"explain[19:24"*"describe[26:28"*"connection:123"*"connection:456"*"explain[43:48"*) ;;
	*)
		echo "unexpected EXPLAIN/DESCRIBE object output: $explain_object_output" >&2
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
	*"show"*/table:'`db`.`t`'*"show"*/view:v*"show"*/table:'`db`.`c`'*"show"*/table:f*"show"*/table:'`db`.`i`'*"show"*/table:k*"show"*/database:'`db`'*"show"*/user:"'u'@'h'"*"show"*/user:"'u'@'h'"*"show[57:58"*"show"*/system_variable*) ;;
	*)
		echo "unexpected SHOW object output: $show_object_output" >&2
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

show_schema_output=$("$parser" 'SHOW TABLE STATUS FROM `db`; SHOW OPEN TABLES FROM `db`; SHOW TRIGGERS IN `db`; SHOW EVENTS FROM `db`; SHOW TABLE STATUS LIKE "wp_%"')
case "$show_schema_output" in
	*"show"*/database:'`db`'*"show"*/database:'`db`'*"show"*/database:'`db`'*"show"*/database:'`db`'*"show[23:27"*) ;;
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

show_engine_output=$("$parser" 'SHOW ENGINE InnoDB STATUS; SHOW ENGINE performance_schema MUTEX; SHOW ENGINES')
case "$show_engine_output" in
	*"show"*/engine:InnoDB*"show"*/engine:performance_schema*"show"*/engine*) ;;
	*)
		echo "unexpected SHOW engine output: $show_engine_output" >&2
		exit 1
		;;
esac

show_collection_output=$("$parser" 'SHOW ENGINES; SHOW PLUGINS; SHOW PRIVILEGES; SHOW PROCESSLIST; SHOW FULL PROCESSLIST; SHOW ENGINE InnoDB STATUS; SHOW CREATE USER current_user()')
case "$show_collection_output" in
	*"show"*/engine*"show"*/plugin*"show"*/privilege*"show"*/connection*"show"*/connection*"show"*/engine:InnoDB*"show"*/user:current_user*) ;;
	*)
		echo "unexpected SHOW collection output: $show_collection_output" >&2
		exit 1
		;;
esac

show_profile_output=$("$parser" 'SHOW PROFILE; SHOW PROFILES; SHOW PROFILE FOR QUERY 1; SHOW PROFILE CPU FOR QUERY 2; SHOW PROFILE FOR QUERY @q')
case "$show_profile_output" in
	*"show[1:2"*"show[4:5"*"show"*/query:1*"show"*/query:2*"show[20:24"*) ;;
	*)
		echo "unexpected SHOW PROFILE output: $show_profile_output" >&2
		exit 1
		;;
esac

binary_log_output=$("$parser" "SHOW BINARY LOGS; SHOW BINARY LOG STATUS; SHOW MASTER STATUS; SHOW BINLOG EVENTS IN 'bin.000001' FROM 4; SHOW BINLOG EVENTS; PURGE BINARY LOGS TO 'bin.000001'; PURGE BINARY LOGS BEFORE NOW()")
case "$binary_log_output" in
	*"show"*/binary_log*"show"*/binary_log*"show"*/binary_log*"show"*/binary_log:"'bin.000001'"*"show"*/binary_log*"purge"*/binary_log:"'bin.000001'"*"purge[32:38"*) ;;
	*)
		echo "unexpected binary log output: $binary_log_output" >&2
		exit 1
		;;
esac

relay_log_output=$("$parser" "SHOW RELAYLOG EVENTS IN 'relay.000001' FROM 4; SHOW RELAYLOG EVENTS FOR CHANNEL 'ch'; SHOW RELAYLOG EVENTS; SHOW RELAYLOG EVENTS IN 'relay.000001' FOR CHANNEL 'ch'")
case "$relay_log_output" in
	*"show"*/relay_log:"'relay.000001'"*"show"*/replication_channel:"'ch'"*"show[16:18"*"show"*/relay_log:"'relay.000001'"*) ;;
	*)
		echo "unexpected relay log output: $relay_log_output" >&2
		exit 1
		;;
esac

replica_status_output=$("$parser" "SHOW REPLICAS; SHOW REPLICA STATUS FOR CHANNEL 'ch'; SHOW REPLICA STATUS; SHOW SLAVE STATUS FOR CHANNEL 'old'")
case "$replica_status_output" in
	*"show"*/replication_channel*"show"*/replication_channel:"'ch'"*"show"*/replication_channel*"show[15:20"*) ;;
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

kill_output=$("$parser" 'KILL 123; KILL QUERY 456; KILL CONNECTION 789; KILL QUERY')
case "$kill_output" in
	*"kill"*/connection:123*"kill"*/connection:456*"kill"*/connection:789*"kill[12:13"*) ;;
	*)
		echo "unexpected KILL output: $kill_output" >&2
		exit 1
		;;
esac

flush_output=$("$parser" 'FLUSH TABLES t; FLUSH TABLE `db`.`t`, u WITH READ LOCK; FLUSH TABLES WITH READ LOCK; FLUSH PRIVILEGES')
case "$flush_output" in
	*"flush"*/table:t*"flush"*/table:'`db`.`t`'*"flush[16:20"*"flush[22:23"*) ;;
	*)
		echo "unexpected FLUSH output: $flush_output" >&2
		exit 1
		;;
esac

flush_relay_output=$("$parser" "FLUSH RELAY LOGS FOR CHANNEL 'ch'; FLUSH RELAY LOGS; FLUSH TABLES t")
case "$flush_relay_output" in
	*"flush"*/replication_channel:"'ch'"*"flush[8:10"*"flush"*/table:t*) ;;
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

reset_output=$("$parser" 'RESET PERSIST max_connections; RESET PERSIST IF EXISTS autocommit; RESET PERSIST; RESET BINARY LOGS AND GTIDS')
case "$reset_output" in
	*"reset"*/system_variable:max_connections*"reset"*/system_variable:autocommit*"reset[11:12"*"reset[14:18"*) ;;
	*)
		echo "unexpected RESET output: $reset_output" >&2
		exit 1
		;;
esac

replication_channel_output=$("$parser" "START REPLICA FOR CHANNEL 'ch'; STOP REPLICA SQL_THREAD FOR CHANNEL 'ch'; RESET REPLICA ALL FOR CHANNEL 'ch'; CHANGE REPLICATION SOURCE TO SOURCE_HOST='h' FOR CHANNEL 'ch'; CHANGE REPLICATION FILTER REPLICATE_DO_DB=(db) FOR CHANNEL 'ch'; START REPLICA; STOP REPLICA; RESET REPLICA")
case "$replication_channel_output" in
	*"start"*/replication_channel:"'ch'"*"stop"*/replication_channel:"'ch'"*"reset"*/replication_channel:"'ch'"*"change"*/replication_channel:"'ch'"*"change"*/replication_channel:"'ch'"*"start[44:45"*"stop[47:48"*"reset[50:51"*) ;;
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

clone_output=$("$parser" "CLONE LOCAL DATA DIRECTORY = '/tmp/clone'; CLONE INSTANCE FROM user@host:3306 IDENTIFIED BY 'p'")
case "$clone_output" in
	*"kinds=clone"*"clone[8:17"*) ;;
	*)
		echo "unexpected CLONE output: $clone_output" >&2
		exit 1
		;;
esac

stop_output=$("$parser" 'STOP REPLICA; STOP GROUP_REPLICATION; STOP SLAVE SQL_THREAD; CREATE TABLE stop (id int)')
case "$stop_output" in
	*"kinds=stop"*"stop[4:5"*"stop[7:9"*"create"*/table:stop*) ;;
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

set_account_output=$("$parser" "SET ROLE r; SET ROLE ALL EXCEPT 'r'@'h'; SET ROLE DEFAULT; SET DEFAULT ROLE r TO 'u'@'h'; SET DEFAULT ROLE ALL TO 'u'@'h'; SET PASSWORD FOR 'u'@'h' = 'x'; SET PASSWORD = 'x'; SET autocommit=1")
case "$set_account_output" in
	*"set"*/role:r*"set"*/role:"'r'@'h'"*"set[13:15"*"set"*/role:r*"set"*/user:"'u'@'h'"*"set"*/user:"'u'@'h'"*"set[44:47"*"set[49:52"*) ;;
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

server_logfile_output=$("$parser" 'CREATE SERVER s FOREIGN DATA WRAPPER mysql OPTIONS (HOST "h"); ALTER SERVER s OPTIONS (USER "u"); DROP SERVER s; CREATE LOGFILE GROUP lg ADD UNDOFILE "u.dat"; ALTER LOGFILE GROUP lg ADD UNDOFILE "v.dat"; DROP LOGFILE GROUP lg ENGINE NDB; CREATE TABLESPACE ts ADD DATAFILE "ts.ibd"')
case "$server_logfile_output" in
	*"create"*/server:s*"alter"*/server:s*"drop"*/server:s*"create"*/logfile_group:lg*"alter"*/logfile_group:lg*"drop"*/logfile_group:lg*"create"*/tablespace:ts*) ;;
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

savepoint_output=$("$parser" 'SAVEPOINT s; RELEASE SAVEPOINT s; ROLLBACK TO SAVEPOINT `s`; ROLLBACK')
case "$savepoint_output" in
	*"savepoint"*/savepoint:s*"release"*/savepoint:s*"rollback"*/savepoint:'`s`'*"rollback"*/transaction*) ;;
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
