#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_sys_auto_increment_columns_$$"

fail() {
    printf '%s\n' "mysql_baseline_sys_schema_auto_increment_columns_view_expectations: $1" >&2
    exit 1
}

run_mysql() {
    sql=$1
    shift
    printf '%s\n' "$sql" \
        | docker exec -i "$MYSQL_CONTAINER" mysql --protocol=TCP -h127.0.0.1 -uroot \
            --batch --raw --skip-column-names "$@"
}

expect_output() {
    label=$1
    expected=$2
    sql=$3
    shift 3

    output=$(run_mysql "$sql" "$@")
    if [ "$output" != "$expected" ]; then
        fail "$label: expected [$expected], got [$output]"
    fi
}

cleanup() {
    run_mysql "DROP DATABASE IF EXISTS ${DATABASE};" >/dev/null 2>&1 || true
}

trap cleanup EXIT

version=$(run_mysql "SELECT VERSION();")
case "$version" in
    8.4.9*) ;;
    *) fail "expected MySQL 8.4.9 runtime, got [$version]" ;;
esac

cleanup
run_mysql "CREATE DATABASE ${DATABASE};" >/dev/null

show_columns_expected=$(
    printf '%b' \
'table_schema\tvarchar(64)\tNO\t\tNULL\t
table_name\tvarchar(64)\tNO\t\tNULL\t
column_name\tvarchar(64)\tYES\t\tNULL\t
data_type\tlongtext\tYES\t\tNULL\t
column_type\tmediumtext\tNO\t\tNULL\t
is_signed\tint\tNO\t\t0\t
is_unsigned\tint\tNO\t\t0\t
max_value\tbigint unsigned\tYES\t\tNULL\t
auto_increment\tbigint unsigned\tYES\t\tNULL\t
auto_increment_ratio\tdecimal(25,4) unsigned\tYES\t\tNULL\t'
)

expect_output \
    "sys.schema_auto_increment_columns SHOW COLUMNS" \
    "$show_columns_expected" \
    "SHOW COLUMNS FROM sys.schema_auto_increment_columns;"

expect_output \
    "sys.schema_auto_increment_columns DESCRIBE" \
    "$show_columns_expected" \
    "DESCRIBE sys.schema_auto_increment_columns;"

columns_expected=$(
    printf '%b' \
'table_schema\t1\tNULL\tNO\tvarchar\t64\t192\tNULL\tNULL\tNULL\tutf8mb3\tutf8mb3_bin\tvarchar(64)\t\t\tselect,insert,update,references\t\t
table_name\t2\tNULL\tNO\tvarchar\t64\t192\tNULL\tNULL\tNULL\tutf8mb3\tutf8mb3_bin\tvarchar(64)\t\t\tselect,insert,update,references\t\t
column_name\t3\tNULL\tYES\tvarchar\t64\t192\tNULL\tNULL\tNULL\tutf8mb3\tutf8mb3_tolower_ci\tvarchar(64)\t\t\tselect,insert,update,references\t\t
data_type\t4\tNULL\tYES\tlongtext\t4294967295\t4294967295\tNULL\tNULL\tNULL\tutf8mb3\tutf8mb3_bin\tlongtext\t\t\tselect,insert,update,references\t\t
column_type\t5\tNULL\tNO\tmediumtext\t16777215\t16777215\tNULL\tNULL\tNULL\tutf8mb3\tutf8mb3_bin\tmediumtext\t\t\tselect,insert,update,references\t\t
is_signed\t6\t0\tNO\tint\tNULL\tNULL\t10\t0\tNULL\tNULL\tNULL\tint\t\t\tselect,insert,update,references\t\t
is_unsigned\t7\t0\tNO\tint\tNULL\tNULL\t10\t0\tNULL\tNULL\tNULL\tint\t\t\tselect,insert,update,references\t\t
max_value\t8\tNULL\tYES\tbigint\tNULL\tNULL\t20\t0\tNULL\tNULL\tNULL\tbigint unsigned\t\t\tselect,insert,update,references\t\t
auto_increment\t9\tNULL\tYES\tbigint\tNULL\tNULL\t20\t0\tNULL\tNULL\tNULL\tbigint unsigned\t\t\tselect,insert,update,references\t\t
auto_increment_ratio\t10\tNULL\tYES\tdecimal\tNULL\tNULL\t25\t4\tNULL\tNULL\tNULL\tdecimal(25,4) unsigned\t\t\tselect,insert,update,references\t\t'
)

expect_output \
    "sys.schema_auto_increment_columns INFORMATION_SCHEMA.COLUMNS" \
    "$columns_expected" \
    "SELECT COLUMN_NAME, ORDINAL_POSITION, COLUMN_DEFAULT, IS_NULLABLE, DATA_TYPE,
            CHARACTER_MAXIMUM_LENGTH, CHARACTER_OCTET_LENGTH, NUMERIC_PRECISION,
            NUMERIC_SCALE, DATETIME_PRECISION, CHARACTER_SET_NAME, COLLATION_NAME,
            COLUMN_TYPE, COLUMN_KEY, EXTRA, PRIVILEGES, COLUMN_COMMENT,
            GENERATION_EXPRESSION
       FROM INFORMATION_SCHEMA.COLUMNS
      WHERE TABLE_SCHEMA = 'sys'
        AND TABLE_NAME = 'schema_auto_increment_columns'
      ORDER BY ORDINAL_POSITION;"

expect_output \
    "sys.schema_auto_increment_columns INFORMATION_SCHEMA.TABLES row" \
    "$(printf '%b' 'sys\tschema_auto_increment_columns\tVIEW\tNULL\tNULL\tNULL\tNULL\tNULL\tNULL\tNULL\tVIEW')" \
    "SELECT TABLE_SCHEMA, TABLE_NAME, TABLE_TYPE, ENGINE, TABLE_ROWS, DATA_LENGTH,
            INDEX_LENGTH, AUTO_INCREMENT, TABLE_COLLATION, CREATE_OPTIONS,
            TABLE_COMMENT
       FROM INFORMATION_SCHEMA.TABLES
      WHERE TABLE_SCHEMA = 'sys'
        AND TABLE_NAME = 'schema_auto_increment_columns';"

expect_output \
    "sys.schema_auto_increment_columns INFORMATION_SCHEMA.VIEWS row" \
    "$(printf '%b' 'def\tsys\tschema_auto_increment_columns\tNONE\tNO\tmysql.sys@localhost\tINVOKER\tutf8mb4\tutf8mb4_0900_ai_ci')" \
    "SELECT TABLE_CATALOG, TABLE_SCHEMA, TABLE_NAME, CHECK_OPTION, IS_UPDATABLE,
            DEFINER, SECURITY_TYPE, CHARACTER_SET_CLIENT, COLLATION_CONNECTION
       FROM INFORMATION_SCHEMA.VIEWS
      WHERE TABLE_SCHEMA = 'sys'
        AND TABLE_NAME = 'schema_auto_increment_columns';"

expect_output \
    "sys.schema_auto_increment_columns empty index and constraints" \
    "$(printf '%b' '0\t0\t0\t0')" \
    "SELECT
        (SELECT COUNT(*) FROM INFORMATION_SCHEMA.STATISTICS
          WHERE TABLE_SCHEMA = 'sys' AND TABLE_NAME = 'schema_auto_increment_columns'),
        (SELECT COUNT(*) FROM INFORMATION_SCHEMA.TABLE_CONSTRAINTS
          WHERE TABLE_SCHEMA = 'sys' AND TABLE_NAME = 'schema_auto_increment_columns'),
        (SELECT COUNT(*) FROM INFORMATION_SCHEMA.KEY_COLUMN_USAGE
          WHERE TABLE_SCHEMA = 'sys' AND TABLE_NAME = 'schema_auto_increment_columns'),
        (SELECT COUNT(*) FROM INFORMATION_SCHEMA.TABLE_CONSTRAINTS_EXTENSIONS
          WHERE CONSTRAINT_SCHEMA = 'sys' AND TABLE_NAME = 'schema_auto_increment_columns');"

expect_output \
    "sys.schema_auto_increment_columns dependency metadata" \
    "$(printf '%b' 'sys\tschema_auto_increment_columns\tinformation_schema\tCOLUMNS\nsys\tschema_auto_increment_columns\tinformation_schema\tTABLES\n0')" \
    "SELECT VIEW_SCHEMA, VIEW_NAME, TABLE_SCHEMA, TABLE_NAME
       FROM INFORMATION_SCHEMA.VIEW_TABLE_USAGE
      WHERE VIEW_SCHEMA = 'sys' AND VIEW_NAME = 'schema_auto_increment_columns'
      ORDER BY TABLE_NAME;
     SELECT COUNT(*)
       FROM INFORMATION_SCHEMA.VIEW_ROUTINE_USAGE
      WHERE TABLE_SCHEMA = 'sys' AND TABLE_NAME = 'schema_auto_increment_columns';"

run_mysql "CREATE TABLE empty_default (id INT AUTO_INCREMENT PRIMARY KEY);
           CREATE TABLE explicit_next (id INT AUTO_INCREMENT PRIMARY KEY) AUTO_INCREMENT = 100;
           CREATE TABLE signed_int (id INT AUTO_INCREMENT PRIMARY KEY, v INT);
           CREATE TABLE signed_tiny (id TINYINT AUTO_INCREMENT PRIMARY KEY, v INT);
           CREATE TABLE unsigned_big (id BIGINT UNSIGNED AUTO_INCREMENT PRIMARY KEY, v INT);
           INSERT INTO signed_int (v) VALUES (10),(20);
           INSERT INTO signed_tiny (v) VALUES (1),(2),(3);
           INSERT INTO unsigned_big (id, v) VALUES (7, 70);" \
    "$DATABASE" >/dev/null

rows_expected=$(cat <<EXPECTED
${DATABASE}	empty_default	id	int	int	1	0	2147483647	NULL	NULL
${DATABASE}	explicit_next	id	int	int	1	0	2147483647	100	0.0000
${DATABASE}	signed_int	id	int	int	1	0	2147483647	3	0.0000
${DATABASE}	signed_tiny	id	tinyint	tinyint	1	0	127	4	0.0315
${DATABASE}	unsigned_big	id	bigint	bigint unsigned	0	1	18446744073709551615	8	0.0000
EXPECTED
)

expect_output \
    "sys.schema_auto_increment_columns row values" \
    "$rows_expected" \
    "SELECT table_schema, table_name, column_name, data_type, column_type,
            is_signed, is_unsigned, max_value, auto_increment,
            auto_increment_ratio
       FROM sys.schema_auto_increment_columns
      WHERE table_schema = '${DATABASE}'
      ORDER BY table_name;"

expect_output \
    "selected sys schema auto_increment view read" \
    "$(printf '%b' 'explicit_next\t100\nsigned_tiny\t4')" \
    "USE sys;
     SELECT table_name, auto_increment
       FROM schema_auto_increment_columns
      WHERE table_schema = '${DATABASE}'
        AND table_name IN ('explicit_next', 'signed_tiny')
      ORDER BY table_name;"

qualified_show_create=$(cat <<'EXPECTED'
CREATE ALGORITHM=MERGE DEFINER=`mysql.sys`@`localhost` SQL SECURITY INVOKER VIEW `sys`.`schema_auto_increment_columns` (`table_schema`,`table_name`,`column_name`,`data_type`,`column_type`,`is_signed`,`is_unsigned`,`max_value`,`auto_increment`,`auto_increment_ratio`) AS select `information_schema`.`COLUMNS`.`TABLE_SCHEMA` AS `TABLE_SCHEMA`,`information_schema`.`COLUMNS`.`TABLE_NAME` AS `TABLE_NAME`,`information_schema`.`COLUMNS`.`COLUMN_NAME` AS `COLUMN_NAME`,`information_schema`.`COLUMNS`.`DATA_TYPE` AS `DATA_TYPE`,`information_schema`.`COLUMNS`.`COLUMN_TYPE` AS `COLUMN_TYPE`,(locate('unsigned',`information_schema`.`COLUMNS`.`COLUMN_TYPE`) = 0) AS `is_signed`,(locate('unsigned',`information_schema`.`COLUMNS`.`COLUMN_TYPE`) > 0) AS `is_unsigned`,((case `information_schema`.`COLUMNS`.`DATA_TYPE` when 'tinyint' then 255 when 'smallint' then 65535 when 'mediumint' then 16777215 when 'int' then 4294967295 when 'bigint' then 18446744073709551615 end) >> if((locate('unsigned',`information_schema`.`COLUMNS`.`COLUMN_TYPE`) > 0),0,1)) AS `max_value`,`information_schema`.`TABLES`.`AUTO_INCREMENT` AS `AUTO_INCREMENT`,(`information_schema`.`TABLES`.`AUTO_INCREMENT` / ((case `information_schema`.`COLUMNS`.`DATA_TYPE` when 'tinyint' then 255 when 'smallint' then 65535 when 'mediumint' then 16777215 when 'int' then 4294967295 when 'bigint' then 18446744073709551615 end) >> if((locate('unsigned',`information_schema`.`COLUMNS`.`COLUMN_TYPE`) > 0),0,1))) AS `auto_increment_ratio` from (`information_schema`.`COLUMNS` join `information_schema`.`TABLES` on(((`information_schema`.`COLUMNS`.`TABLE_SCHEMA` = `information_schema`.`TABLES`.`TABLE_SCHEMA`) and (`information_schema`.`COLUMNS`.`TABLE_NAME` = `information_schema`.`TABLES`.`TABLE_NAME`)))) where ((`information_schema`.`COLUMNS`.`TABLE_SCHEMA` not in ('mysql','sys','INFORMATION_SCHEMA','performance_schema')) and (`information_schema`.`TABLES`.`TABLE_TYPE` = 'BASE TABLE') and (`information_schema`.`COLUMNS`.`EXTRA` = 'auto_increment')) order by (`information_schema`.`TABLES`.`AUTO_INCREMENT` / ((case `information_schema`.`COLUMNS`.`DATA_TYPE` when 'tinyint' then 255 when 'smallint' then 65535 when 'mediumint' then 16777215 when 'int' then 4294967295 when 'bigint' then 18446744073709551615 end) >> if((locate('unsigned',`information_schema`.`COLUMNS`.`COLUMN_TYPE`) > 0),0,1))) desc,((case `information_schema`.`COLUMNS`.`DATA_TYPE` when 'tinyint' then 255 when 'smallint' then 65535 when 'mediumint' then 16777215 when 'int' then 4294967295 when 'bigint' then 18446744073709551615 end) >> if((locate('unsigned',`information_schema`.`COLUMNS`.`COLUMN_TYPE`) > 0),0,1))
EXPECTED
)

unqualified_show_create=$(cat <<'EXPECTED'
CREATE ALGORITHM=MERGE DEFINER=`mysql.sys`@`localhost` SQL SECURITY INVOKER VIEW `schema_auto_increment_columns` (`table_schema`,`table_name`,`column_name`,`data_type`,`column_type`,`is_signed`,`is_unsigned`,`max_value`,`auto_increment`,`auto_increment_ratio`) AS select `information_schema`.`COLUMNS`.`TABLE_SCHEMA` AS `TABLE_SCHEMA`,`information_schema`.`COLUMNS`.`TABLE_NAME` AS `TABLE_NAME`,`information_schema`.`COLUMNS`.`COLUMN_NAME` AS `COLUMN_NAME`,`information_schema`.`COLUMNS`.`DATA_TYPE` AS `DATA_TYPE`,`information_schema`.`COLUMNS`.`COLUMN_TYPE` AS `COLUMN_TYPE`,(locate('unsigned',`information_schema`.`COLUMNS`.`COLUMN_TYPE`) = 0) AS `is_signed`,(locate('unsigned',`information_schema`.`COLUMNS`.`COLUMN_TYPE`) > 0) AS `is_unsigned`,((case `information_schema`.`COLUMNS`.`DATA_TYPE` when 'tinyint' then 255 when 'smallint' then 65535 when 'mediumint' then 16777215 when 'int' then 4294967295 when 'bigint' then 18446744073709551615 end) >> if((locate('unsigned',`information_schema`.`COLUMNS`.`COLUMN_TYPE`) > 0),0,1)) AS `max_value`,`information_schema`.`TABLES`.`AUTO_INCREMENT` AS `AUTO_INCREMENT`,(`information_schema`.`TABLES`.`AUTO_INCREMENT` / ((case `information_schema`.`COLUMNS`.`DATA_TYPE` when 'tinyint' then 255 when 'smallint' then 65535 when 'mediumint' then 16777215 when 'int' then 4294967295 when 'bigint' then 18446744073709551615 end) >> if((locate('unsigned',`information_schema`.`COLUMNS`.`COLUMN_TYPE`) > 0),0,1))) AS `auto_increment_ratio` from (`information_schema`.`COLUMNS` join `information_schema`.`TABLES` on(((`information_schema`.`COLUMNS`.`TABLE_SCHEMA` = `information_schema`.`TABLES`.`TABLE_SCHEMA`) and (`information_schema`.`COLUMNS`.`TABLE_NAME` = `information_schema`.`TABLES`.`TABLE_NAME`)))) where ((`information_schema`.`COLUMNS`.`TABLE_SCHEMA` not in ('mysql','sys','INFORMATION_SCHEMA','performance_schema')) and (`information_schema`.`TABLES`.`TABLE_TYPE` = 'BASE TABLE') and (`information_schema`.`COLUMNS`.`EXTRA` = 'auto_increment')) order by (`information_schema`.`TABLES`.`AUTO_INCREMENT` / ((case `information_schema`.`COLUMNS`.`DATA_TYPE` when 'tinyint' then 255 when 'smallint' then 65535 when 'mediumint' then 16777215 when 'int' then 4294967295 when 'bigint' then 18446744073709551615 end) >> if((locate('unsigned',`information_schema`.`COLUMNS`.`COLUMN_TYPE`) > 0),0,1))) desc,((case `information_schema`.`COLUMNS`.`DATA_TYPE` when 'tinyint' then 255 when 'smallint' then 65535 when 'mediumint' then 16777215 when 'int' then 4294967295 when 'bigint' then 18446744073709551615 end) >> if((locate('unsigned',`information_schema`.`COLUMNS`.`COLUMN_TYPE`) > 0),0,1))
EXPECTED
)

expect_output \
    "sys.schema_auto_increment_columns SHOW CREATE VIEW qualified" \
    "$(printf '%b' "schema_auto_increment_columns\t${qualified_show_create}\tutf8mb4\tutf8mb4_0900_ai_ci")" \
    "SHOW CREATE VIEW sys.schema_auto_increment_columns;"

expect_output \
    "sys.schema_auto_increment_columns SHOW CREATE TABLE selected-schema" \
    "$(printf '%b' "schema_auto_increment_columns\t${unqualified_show_create}\tutf8mb4\tutf8mb4_0900_ai_ci")" \
    "USE sys; SHOW CREATE TABLE schema_auto_increment_columns;"

status=$(run_mysql "SELECT COUNT(*) FROM sys.schema_auto_increment_columns WHERE table_schema = '${DATABASE}'; SELECT @@warning_count, ROW_COUNT();")
status=$(printf '%s\n' "$status" | tail -n 1)
if [ "$status" != "0	-1" ]; then
    fail "sys.schema_auto_increment_columns SELECT status: expected [0	-1], got [$status]"
fi

printf '%s\n' "mysql_baseline_sys_schema_auto_increment_columns_view_expectations: ok"
