#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_schema_index_statistics_$$"

fail() {
    printf '%s\n' "mysql_baseline_sys_schema_index_statistics_views_expectations: $1" >&2
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

expect_contains() {
    label=$1
    needle=$2
    sql=$3
    shift 3

    output=$(run_mysql "$sql" "$@")
    case "$output" in
        *"$needle"*) ;;
        *) fail "$label: expected output to contain [$needle], got [$output]" ;;
    esac
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

formatted_show_columns_expected=$(
    printf '%b' \
'table_schema\tvarchar(64)\tYES\t\tNULL\t
table_name\tvarchar(64)\tYES\t\tNULL\t
index_name\tvarchar(64)\tYES\t\tNULL\t
rows_selected\tbigint unsigned\tNO\t\tNULL\t
select_latency\tvarchar(11)\tYES\t\tNULL\t
rows_inserted\tbigint unsigned\tNO\t\tNULL\t
insert_latency\tvarchar(11)\tYES\t\tNULL\t
rows_updated\tbigint unsigned\tNO\t\tNULL\t
update_latency\tvarchar(11)\tYES\t\tNULL\t
rows_deleted\tbigint unsigned\tNO\t\tNULL\t
delete_latency\tvarchar(11)\tYES\t\tNULL\t'
)

raw_show_columns_expected=$(
    printf '%b' \
'table_schema\tvarchar(64)\tYES\t\tNULL\t
table_name\tvarchar(64)\tYES\t\tNULL\t
index_name\tvarchar(64)\tYES\t\tNULL\t
rows_selected\tbigint unsigned\tNO\t\tNULL\t
select_latency\tbigint unsigned\tNO\t\tNULL\t
rows_inserted\tbigint unsigned\tNO\t\tNULL\t
insert_latency\tbigint unsigned\tNO\t\tNULL\t
rows_updated\tbigint unsigned\tNO\t\tNULL\t
update_latency\tbigint unsigned\tNO\t\tNULL\t
rows_deleted\tbigint unsigned\tNO\t\tNULL\t
delete_latency\tbigint unsigned\tNO\t\tNULL\t'
)

expect_output \
    "sys.schema_index_statistics SHOW COLUMNS" \
    "$formatted_show_columns_expected" \
    "SHOW COLUMNS FROM sys.schema_index_statistics;"

expect_output \
    "sys.x schema_index_statistics SHOW COLUMNS" \
    "$raw_show_columns_expected" \
    "SHOW COLUMNS FROM sys.\`x\$schema_index_statistics\`;"

formatted_columns_expected=$(
    printf '%b' \
'table_schema\t1\tNULL\tYES\tvarchar\t64\t256\tNULL\tNULL\tutf8mb4\tutf8mb4_0900_ai_ci\tvarchar(64)\t\t\tselect,insert,update,references\t
table_name\t2\tNULL\tYES\tvarchar\t64\t256\tNULL\tNULL\tutf8mb4\tutf8mb4_0900_ai_ci\tvarchar(64)\t\t\tselect,insert,update,references\t
index_name\t3\tNULL\tYES\tvarchar\t64\t256\tNULL\tNULL\tutf8mb4\tutf8mb4_0900_ai_ci\tvarchar(64)\t\t\tselect,insert,update,references\t
rows_selected\t4\tNULL\tNO\tbigint\tNULL\tNULL\t20\t0\tNULL\tNULL\tbigint unsigned\t\t\tselect,insert,update,references\t
select_latency\t5\tNULL\tYES\tvarchar\t11\t33\tNULL\tNULL\tutf8mb3\tutf8mb3_general_ci\tvarchar(11)\t\t\tselect,insert,update,references\t
rows_inserted\t6\tNULL\tNO\tbigint\tNULL\tNULL\t20\t0\tNULL\tNULL\tbigint unsigned\t\t\tselect,insert,update,references\t
insert_latency\t7\tNULL\tYES\tvarchar\t11\t33\tNULL\tNULL\tutf8mb3\tutf8mb3_general_ci\tvarchar(11)\t\t\tselect,insert,update,references\t
rows_updated\t8\tNULL\tNO\tbigint\tNULL\tNULL\t20\t0\tNULL\tNULL\tbigint unsigned\t\t\tselect,insert,update,references\t
update_latency\t9\tNULL\tYES\tvarchar\t11\t33\tNULL\tNULL\tutf8mb3\tutf8mb3_general_ci\tvarchar(11)\t\t\tselect,insert,update,references\t
rows_deleted\t10\tNULL\tNO\tbigint\tNULL\tNULL\t20\t0\tNULL\tNULL\tbigint unsigned\t\t\tselect,insert,update,references\t
delete_latency\t11\tNULL\tYES\tvarchar\t11\t33\tNULL\tNULL\tutf8mb3\tutf8mb3_general_ci\tvarchar(11)\t\t\tselect,insert,update,references\t'
)

raw_columns_expected=$(
    printf '%b' \
'table_schema\t1\tNULL\tYES\tvarchar\t64\t256\tNULL\tNULL\tutf8mb4\tutf8mb4_0900_ai_ci\tvarchar(64)\t\t\tselect,insert,update,references\t
table_name\t2\tNULL\tYES\tvarchar\t64\t256\tNULL\tNULL\tutf8mb4\tutf8mb4_0900_ai_ci\tvarchar(64)\t\t\tselect,insert,update,references\t
index_name\t3\tNULL\tYES\tvarchar\t64\t256\tNULL\tNULL\tutf8mb4\tutf8mb4_0900_ai_ci\tvarchar(64)\t\t\tselect,insert,update,references\t
rows_selected\t4\tNULL\tNO\tbigint\tNULL\tNULL\t20\t0\tNULL\tNULL\tbigint unsigned\t\t\tselect,insert,update,references\t
select_latency\t5\tNULL\tNO\tbigint\tNULL\tNULL\t20\t0\tNULL\tNULL\tbigint unsigned\t\t\tselect,insert,update,references\t
rows_inserted\t6\tNULL\tNO\tbigint\tNULL\tNULL\t20\t0\tNULL\tNULL\tbigint unsigned\t\t\tselect,insert,update,references\t
insert_latency\t7\tNULL\tNO\tbigint\tNULL\tNULL\t20\t0\tNULL\tNULL\tbigint unsigned\t\t\tselect,insert,update,references\t
rows_updated\t8\tNULL\tNO\tbigint\tNULL\tNULL\t20\t0\tNULL\tNULL\tbigint unsigned\t\t\tselect,insert,update,references\t
update_latency\t9\tNULL\tNO\tbigint\tNULL\tNULL\t20\t0\tNULL\tNULL\tbigint unsigned\t\t\tselect,insert,update,references\t
rows_deleted\t10\tNULL\tNO\tbigint\tNULL\tNULL\t20\t0\tNULL\tNULL\tbigint unsigned\t\t\tselect,insert,update,references\t
delete_latency\t11\tNULL\tNO\tbigint\tNULL\tNULL\t20\t0\tNULL\tNULL\tbigint unsigned\t\t\tselect,insert,update,references\t'
)

expect_output \
    "sys.schema_index_statistics INFORMATION_SCHEMA.COLUMNS" \
    "$formatted_columns_expected" \
    "SELECT COLUMN_NAME, ORDINAL_POSITION, COLUMN_DEFAULT, IS_NULLABLE, DATA_TYPE,
            CHARACTER_MAXIMUM_LENGTH, CHARACTER_OCTET_LENGTH, NUMERIC_PRECISION,
            NUMERIC_SCALE, CHARACTER_SET_NAME, COLLATION_NAME, COLUMN_TYPE,
            COLUMN_KEY, EXTRA, PRIVILEGES, COLUMN_COMMENT
       FROM INFORMATION_SCHEMA.COLUMNS
      WHERE TABLE_SCHEMA = 'sys'
        AND TABLE_NAME = 'schema_index_statistics'
      ORDER BY ORDINAL_POSITION;"

expect_output \
    "sys.x schema_index_statistics INFORMATION_SCHEMA.COLUMNS" \
    "$raw_columns_expected" \
    "SELECT COLUMN_NAME, ORDINAL_POSITION, COLUMN_DEFAULT, IS_NULLABLE, DATA_TYPE,
            CHARACTER_MAXIMUM_LENGTH, CHARACTER_OCTET_LENGTH, NUMERIC_PRECISION,
            NUMERIC_SCALE, CHARACTER_SET_NAME, COLLATION_NAME, COLUMN_TYPE,
            COLUMN_KEY, EXTRA, PRIVILEGES, COLUMN_COMMENT
       FROM INFORMATION_SCHEMA.COLUMNS
      WHERE TABLE_SCHEMA = 'sys'
        AND TABLE_NAME = 'x\$schema_index_statistics'
      ORDER BY ORDINAL_POSITION;"

expect_output \
    "sys schema index statistics TABLES rows" \
    "$(printf '%b' 'sys\tschema_index_statistics\tVIEW\tNULL\tNULL\tNULL\tVIEW\nsys\tx$schema_index_statistics\tVIEW\tNULL\tNULL\tNULL\tVIEW')" \
    "SELECT TABLE_SCHEMA, TABLE_NAME, TABLE_TYPE, ENGINE, TABLE_ROWS, DATA_LENGTH,
            TABLE_COMMENT
       FROM INFORMATION_SCHEMA.TABLES
      WHERE TABLE_SCHEMA = 'sys'
        AND TABLE_NAME IN ('schema_index_statistics', 'x\$schema_index_statistics')
      ORDER BY TABLE_NAME;"

expect_output \
    "sys schema index statistics VIEWS rows" \
    "$(printf '%b' 'sys\tschema_index_statistics\tNONE\tYES\tmysql.sys@localhost\tINVOKER\nsys\tx$schema_index_statistics\tNONE\tYES\tmysql.sys@localhost\tINVOKER')" \
    "SELECT TABLE_SCHEMA, TABLE_NAME, CHECK_OPTION, IS_UPDATABLE, DEFINER, SECURITY_TYPE
       FROM INFORMATION_SCHEMA.VIEWS
      WHERE TABLE_SCHEMA = 'sys'
        AND TABLE_NAME IN ('schema_index_statistics', 'x\$schema_index_statistics')
      ORDER BY TABLE_NAME;"

expect_output \
    "sys schema index statistics dependency metadata" \
    "$(printf '%b' 'sys\tschema_index_statistics\tperformance_schema\ttable_io_waits_summary_by_index_usage\nsys\tx$schema_index_statistics\tperformance_schema\ttable_io_waits_summary_by_index_usage\n0')" \
    "SELECT VIEW_SCHEMA, VIEW_NAME, TABLE_SCHEMA, TABLE_NAME
       FROM INFORMATION_SCHEMA.VIEW_TABLE_USAGE
      WHERE VIEW_SCHEMA = 'sys'
        AND VIEW_NAME IN ('schema_index_statistics', 'x\$schema_index_statistics')
      ORDER BY VIEW_NAME;
     SELECT COUNT(*)
       FROM INFORMATION_SCHEMA.VIEW_ROUTINE_USAGE
      WHERE TABLE_SCHEMA = 'sys'
        AND TABLE_NAME IN ('schema_index_statistics', 'x\$schema_index_statistics');"

expect_output \
    "sys schema index statistics empty index and constraints" \
    "$(printf '%b' '0\t0\t0\t0')" \
    "SELECT
        (SELECT COUNT(*) FROM INFORMATION_SCHEMA.STATISTICS
          WHERE TABLE_SCHEMA = 'sys' AND TABLE_NAME = 'schema_index_statistics'),
        (SELECT COUNT(*) FROM INFORMATION_SCHEMA.TABLE_CONSTRAINTS
          WHERE TABLE_SCHEMA = 'sys' AND TABLE_NAME = 'schema_index_statistics'),
        (SELECT COUNT(*) FROM INFORMATION_SCHEMA.KEY_COLUMN_USAGE
          WHERE TABLE_SCHEMA = 'sys' AND TABLE_NAME = 'schema_index_statistics'),
        (SELECT COUNT(*) FROM INFORMATION_SCHEMA.TABLE_CONSTRAINTS_EXTENSIONS
          WHERE CONSTRAINT_SCHEMA = 'sys' AND TABLE_NAME = 'schema_index_statistics');"

run_mysql "CREATE TABLE base_one (
               id INT PRIMARY KEY,
               a INT,
               b INT,
               KEY idx_a (a),
               UNIQUE KEY uq_b (b)
           );
           INSERT INTO base_one VALUES (1, 10, 100), (2, 20, 200);
           SELECT * FROM base_one WHERE a = 10;
           SELECT * FROM base_one WHERE b = 200;" \
    "$DATABASE" >/dev/null

rows_expected=$(cat <<EXPECTED
${DATABASE}	base_one	idx_a	1	0	0	0
${DATABASE}	base_one	PRIMARY	0	0	0	0
${DATABASE}	base_one	uq_b	1	0	0	0
EXPECTED
)

expect_output \
    "sys.schema_index_statistics row values" \
    "$rows_expected" \
    "SELECT table_schema, table_name, index_name, rows_selected, rows_inserted,
            rows_updated, rows_deleted
       FROM sys.schema_index_statistics
      WHERE table_schema = '${DATABASE}'
      ORDER BY index_name;"

expect_output \
    "selected sys x schema index statistics read" \
    "$(printf '%b' "${DATABASE}\tbase_one\tuq_b\t1")" \
    "USE sys;
     SELECT table_schema, table_name, index_name, rows_selected
       FROM \`x\$schema_index_statistics\`
      WHERE table_schema = '${DATABASE}' AND index_name = 'uq_b';"

expect_contains \
    "sys.schema_index_statistics SHOW CREATE VIEW" \
    "CREATE ALGORITHM=MERGE DEFINER=\`mysql.sys\`@\`localhost\` SQL SECURITY INVOKER VIEW \`sys\`.\`schema_index_statistics\`" \
    "SHOW CREATE VIEW sys.schema_index_statistics;"

expect_contains \
    "sys.x schema_index_statistics SHOW CREATE TABLE" \
    "CREATE ALGORITHM=MERGE DEFINER=\`mysql.sys\`@\`localhost\` SQL SECURITY INVOKER VIEW \`x\$schema_index_statistics\`" \
    "USE sys; SHOW CREATE TABLE \`x\$schema_index_statistics\`;"

status=$(run_mysql "SELECT COUNT(*) FROM sys.schema_index_statistics WHERE table_schema = '${DATABASE}'; SELECT @@warning_count, ROW_COUNT();")
status=$(printf '%s\n' "$status" | tail -n 1)
if [ "$status" != "0	-1" ]; then
    fail "sys.schema_index_statistics SELECT status: expected [0	-1], got [$status]"
fi

printf '%s\n' "mysql_baseline_sys_schema_index_statistics_views_expectations: ok"
