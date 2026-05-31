#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_schema_object_overview_$$"

fail() {
    printf '%s\n' "mysql_baseline_sys_schema_object_overview_view_expectations: $1" >&2
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
'db\tvarchar(64)\tNO\t\t\t
object_type\tvarchar(19)\tYES\t\tNULL\t
count\tbigint\tNO\t\t0\t'
)

expect_output \
    "sys.schema_object_overview SHOW COLUMNS" \
    "$show_columns_expected" \
    "SHOW COLUMNS FROM sys.schema_object_overview;"

expect_output \
    "sys.schema_object_overview DESCRIBE" \
    "$show_columns_expected" \
    "DESCRIBE sys.schema_object_overview;"

columns_expected=$(
    printf '%b' \
'db\t1\t\tNO\tvarchar\t64\t192\tNULL\tNULL\tNULL\tutf8mb3\tutf8mb3_bin\tvarchar(64)\t\t\tselect,insert,update,references\t\t
object_type\t2\tNULL\tYES\tvarchar\t19\t57\tNULL\tNULL\tNULL\tutf8mb3\tutf8mb3_bin\tvarchar(19)\t\t\tselect,insert,update,references\t\t
count\t3\t0\tNO\tbigint\tNULL\tNULL\t19\t0\tNULL\tNULL\tNULL\tbigint\t\t\tselect,insert,update,references\t\t'
)

expect_output \
    "sys.schema_object_overview INFORMATION_SCHEMA.COLUMNS" \
    "$columns_expected" \
    "SELECT COLUMN_NAME, ORDINAL_POSITION, COLUMN_DEFAULT, IS_NULLABLE, DATA_TYPE,
            CHARACTER_MAXIMUM_LENGTH, CHARACTER_OCTET_LENGTH, NUMERIC_PRECISION,
            NUMERIC_SCALE, DATETIME_PRECISION, CHARACTER_SET_NAME, COLLATION_NAME,
            COLUMN_TYPE, COLUMN_KEY, EXTRA, PRIVILEGES, COLUMN_COMMENT,
            GENERATION_EXPRESSION
       FROM INFORMATION_SCHEMA.COLUMNS
      WHERE TABLE_SCHEMA = 'sys'
        AND TABLE_NAME = 'schema_object_overview'
      ORDER BY ORDINAL_POSITION;"

expect_output \
    "sys.schema_object_overview INFORMATION_SCHEMA.TABLES row" \
    "$(printf '%b' 'sys\tschema_object_overview\tVIEW\tNULL\tNULL\tNULL\tNULL\tNULL\tNULL\tNULL\tVIEW')" \
    "SELECT TABLE_SCHEMA, TABLE_NAME, TABLE_TYPE, ENGINE, TABLE_ROWS, DATA_LENGTH,
            INDEX_LENGTH, AUTO_INCREMENT, TABLE_COLLATION, CREATE_OPTIONS,
            TABLE_COMMENT
       FROM INFORMATION_SCHEMA.TABLES
      WHERE TABLE_SCHEMA = 'sys'
        AND TABLE_NAME = 'schema_object_overview';"

expect_output \
    "sys.schema_object_overview INFORMATION_SCHEMA.VIEWS row" \
    "$(printf '%b' 'def\tsys\tschema_object_overview\tNONE\tNO\tmysql.sys@localhost\tINVOKER\tutf8mb4\tutf8mb4_0900_ai_ci')" \
    "SELECT TABLE_CATALOG, TABLE_SCHEMA, TABLE_NAME, CHECK_OPTION, IS_UPDATABLE,
            DEFINER, SECURITY_TYPE, CHARACTER_SET_CLIENT, COLLATION_CONNECTION
       FROM INFORMATION_SCHEMA.VIEWS
      WHERE TABLE_SCHEMA = 'sys'
        AND TABLE_NAME = 'schema_object_overview';"

expect_output \
    "sys.schema_object_overview empty index and constraints" \
    "$(printf '%b' '0\t0\t0\t0')" \
    "SELECT
        (SELECT COUNT(*) FROM INFORMATION_SCHEMA.STATISTICS
          WHERE TABLE_SCHEMA = 'sys' AND TABLE_NAME = 'schema_object_overview'),
        (SELECT COUNT(*) FROM INFORMATION_SCHEMA.TABLE_CONSTRAINTS
          WHERE TABLE_SCHEMA = 'sys' AND TABLE_NAME = 'schema_object_overview'),
        (SELECT COUNT(*) FROM INFORMATION_SCHEMA.KEY_COLUMN_USAGE
          WHERE TABLE_SCHEMA = 'sys' AND TABLE_NAME = 'schema_object_overview'),
        (SELECT COUNT(*) FROM INFORMATION_SCHEMA.TABLE_CONSTRAINTS_EXTENSIONS
          WHERE CONSTRAINT_SCHEMA = 'sys' AND TABLE_NAME = 'schema_object_overview');"

expect_output \
    "sys.schema_object_overview dependency metadata" \
    "$(printf '%b' 'sys\tschema_object_overview\tinformation_schema\tEVENTS\nsys\tschema_object_overview\tinformation_schema\tROUTINES\nsys\tschema_object_overview\tinformation_schema\tSTATISTICS\nsys\tschema_object_overview\tinformation_schema\tTABLES\nsys\tschema_object_overview\tinformation_schema\tTRIGGERS\n0')" \
    "SELECT VIEW_SCHEMA, VIEW_NAME, TABLE_SCHEMA, TABLE_NAME
       FROM INFORMATION_SCHEMA.VIEW_TABLE_USAGE
      WHERE VIEW_SCHEMA = 'sys' AND VIEW_NAME = 'schema_object_overview'
      ORDER BY TABLE_NAME;
     SELECT COUNT(*)
       FROM INFORMATION_SCHEMA.VIEW_ROUTINE_USAGE
      WHERE TABLE_SCHEMA = 'sys' AND TABLE_NAME = 'schema_object_overview';"

run_mysql "CREATE TABLE base_one (
               id INT AUTO_INCREMENT PRIMARY KEY,
               a INT,
               b INT,
               KEY idx_a (a),
               UNIQUE KEY uq_ab (a,b)
           );
           CREATE TABLE base_two (id INT PRIMARY KEY, c VARCHAR(20));
           CREATE VIEW view_one AS SELECT id, a FROM base_one;" \
    "$DATABASE" >/dev/null

rows_expected=$(cat <<EXPECTED
${DATABASE}	BASE TABLE	2
${DATABASE}	INDEX (BTREE)	5
${DATABASE}	VIEW	1
EXPECTED
)

expect_output \
    "sys.schema_object_overview row values" \
    "$rows_expected" \
    "SELECT db, object_type, count
       FROM sys.schema_object_overview
      WHERE db = '${DATABASE}'
      ORDER BY db, object_type;"

expect_output \
    "selected sys schema object overview read" \
    "$(printf '%b' "${DATABASE}\tINDEX (BTREE)\t5")" \
    "USE sys;
     SELECT db, object_type, count
       FROM schema_object_overview
      WHERE db = '${DATABASE}' AND object_type = 'INDEX (BTREE)';"

schema_object_overview_definition=' (`db`,`object_type`,`count`) AS select `information_schema`.`routines`.`ROUTINE_SCHEMA` AS `db`,`information_schema`.`routines`.`ROUTINE_TYPE` AS `object_type`,count(0) AS `count` from `information_schema`.`ROUTINES` `routines` group by `information_schema`.`routines`.`ROUTINE_SCHEMA`,`information_schema`.`routines`.`ROUTINE_TYPE` union select `information_schema`.`tables`.`TABLE_SCHEMA` AS `TABLE_SCHEMA`,`information_schema`.`tables`.`TABLE_TYPE` AS `TABLE_TYPE`,count(0) AS `COUNT(*)` from `information_schema`.`TABLES` `tables` group by `information_schema`.`tables`.`TABLE_SCHEMA`,`information_schema`.`tables`.`TABLE_TYPE` union select `information_schema`.`statistics`.`TABLE_SCHEMA` AS `TABLE_SCHEMA`,concat('\''INDEX ('\'',`information_schema`.`statistics`.`INDEX_TYPE`,'\'')'\'') AS `CONCAT('\''INDEX ('\'', INDEX_TYPE, '\'')'\'')`,count(0) AS `COUNT(*)` from `information_schema`.`STATISTICS` `statistics` group by `information_schema`.`statistics`.`TABLE_SCHEMA`,`information_schema`.`statistics`.`INDEX_TYPE` union select `information_schema`.`triggers`.`TRIGGER_SCHEMA` AS `TRIGGER_SCHEMA`,'\''TRIGGER'\'' AS `TRIGGER`,count(0) AS `COUNT(*)` from `information_schema`.`TRIGGERS` `triggers` group by `information_schema`.`triggers`.`TRIGGER_SCHEMA` union select `information_schema`.`events`.`EVENT_SCHEMA` AS `EVENT_SCHEMA`,'\''EVENT'\'' AS `EVENT`,count(0) AS `COUNT(*)` from `information_schema`.`EVENTS` `events` group by `information_schema`.`events`.`EVENT_SCHEMA` order by `db`,`object_type`'

expect_output \
    "sys.schema_object_overview SHOW CREATE VIEW qualified" \
    "$(printf '%s\t%s\t%s\t%s' "schema_object_overview" "CREATE ALGORITHM=TEMPTABLE DEFINER=\`mysql.sys\`@\`localhost\` SQL SECURITY INVOKER VIEW \`sys\`.\`schema_object_overview\`${schema_object_overview_definition}" "utf8mb4" "utf8mb4_0900_ai_ci")" \
    "SHOW CREATE VIEW sys.schema_object_overview;"

expect_output \
    "sys.schema_object_overview SHOW CREATE TABLE selected-schema" \
    "$(printf '%s\t%s\t%s\t%s' "schema_object_overview" "CREATE ALGORITHM=TEMPTABLE DEFINER=\`mysql.sys\`@\`localhost\` SQL SECURITY INVOKER VIEW \`schema_object_overview\`${schema_object_overview_definition}" "utf8mb4" "utf8mb4_0900_ai_ci")" \
    "USE sys; SHOW CREATE TABLE schema_object_overview;"

status=$(run_mysql "SELECT COUNT(*) FROM sys.schema_object_overview WHERE db = '${DATABASE}'; SELECT @@warning_count, ROW_COUNT();")
status=$(printf '%s\n' "$status" | tail -n 1)
if [ "$status" != "0	-1" ]; then
    fail "sys.schema_object_overview SELECT status: expected [0	-1], got [$status]"
fi

printf '%s\n' "mysql_baseline_sys_schema_object_overview_view_expectations: ok"
