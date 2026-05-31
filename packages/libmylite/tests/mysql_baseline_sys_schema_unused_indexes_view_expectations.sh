#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_schema_unused_indexes_$$"

fail() {
    printf '%s\n' "mysql_baseline_sys_schema_unused_indexes_view_expectations: $1" >&2
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

expect_show_table_status_row() {
    output=$(run_mysql "SHOW TABLE STATUS FROM sys LIKE 'schema_unused_indexes';")
    field_count=$(printf '%s\n' "$output" | awk -F '\t' '{print NF}')
    prefix=$(printf '%s\n' "$output" | cut -f 1-11)
    create_time=$(printf '%s\n' "$output" | cut -f 12)
    suffix=$(printf '%s\n' "$output" | cut -f 13-18)

    if [ "$field_count" != "18" ]; then
        fail "SHOW TABLE STATUS sys.schema_unused_indexes: expected 18 fields, got [$field_count]"
    fi
    expected_prefix=$(
        printf '%b' 'schema_unused_indexes\tNULL\tNULL\tNULL\tNULL\tNULL\tNULL\tNULL\tNULL\tNULL\tNULL'
    )
    if [ "$prefix" != "$expected_prefix" ]; then
        fail "SHOW TABLE STATUS sys.schema_unused_indexes: expected prefix [$expected_prefix], got [$prefix]"
    fi
    case "$create_time" in
        ????-??-??\ ??:??:??) ;;
        *)
            fail "SHOW TABLE STATUS sys.schema_unused_indexes: expected Create_time datetime, got [$create_time]"
            ;;
    esac
    expected_suffix=$(printf '%b' 'NULL\tNULL\tNULL\tNULL\tNULL\tVIEW')
    if [ "$suffix" != "$expected_suffix" ]; then
        fail "SHOW TABLE STATUS sys.schema_unused_indexes: expected suffix [$expected_suffix], got [$suffix]"
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
'object_schema\tvarchar(64)\tYES\t\tNULL\t
object_name\tvarchar(64)\tYES\t\tNULL\t
index_name\tvarchar(64)\tYES\t\tNULL\t'
)

expect_output \
    "sys.schema_unused_indexes SHOW COLUMNS" \
    "$show_columns_expected" \
    "SHOW COLUMNS FROM sys.schema_unused_indexes;"

expect_output \
    "sys.schema_unused_indexes DESCRIBE" \
    "$show_columns_expected" \
    "DESCRIBE sys.schema_unused_indexes;"

expect_output \
    "sys.schema_unused_indexes SHOW INDEX" \
    "" \
    "SHOW INDEX FROM sys.schema_unused_indexes;"

expect_output \
    "sys.schema_unused_indexes INFORMATION_SCHEMA.COLUMNS" \
    "$(printf '%b' 'schema_unused_indexes\tobject_schema\t1\tYES\tvarchar(64)\tutf8mb4\tutf8mb4_0900_ai_ci\nschema_unused_indexes\tobject_name\t2\tYES\tvarchar(64)\tutf8mb4\tutf8mb4_0900_ai_ci\nschema_unused_indexes\tindex_name\t3\tYES\tvarchar(64)\tutf8mb4\tutf8mb4_0900_ai_ci')" \
    "SELECT TABLE_NAME, COLUMN_NAME, ORDINAL_POSITION, IS_NULLABLE, COLUMN_TYPE,
            COALESCE(CHARACTER_SET_NAME, 'NULL'), COALESCE(COLLATION_NAME, 'NULL')
       FROM INFORMATION_SCHEMA.COLUMNS
      WHERE TABLE_SCHEMA = 'sys'
        AND TABLE_NAME = 'schema_unused_indexes'
      ORDER BY ORDINAL_POSITION;"

expect_output \
    "sys.schema_unused_indexes TABLES row" \
    "$(printf '%b' 'sys\tschema_unused_indexes\tVIEW\tNULL\tNULL\tNULL\tVIEW')" \
    "SELECT TABLE_SCHEMA, TABLE_NAME, TABLE_TYPE, ENGINE, TABLE_ROWS, DATA_LENGTH,
            TABLE_COMMENT
       FROM INFORMATION_SCHEMA.TABLES
      WHERE TABLE_SCHEMA = 'sys'
        AND TABLE_NAME = 'schema_unused_indexes';"

expect_show_table_status_row

expect_output \
    "sys.schema_unused_indexes VIEWS row" \
    "$(printf '%b' 'sys\tschema_unused_indexes\tNONE\tYES\tmysql.sys@localhost\tINVOKER')" \
    "SELECT TABLE_SCHEMA, TABLE_NAME, CHECK_OPTION, IS_UPDATABLE, DEFINER, SECURITY_TYPE
       FROM INFORMATION_SCHEMA.VIEWS
      WHERE TABLE_SCHEMA = 'sys'
        AND TABLE_NAME = 'schema_unused_indexes';"

expect_output \
    "sys.schema_unused_indexes dependency metadata" \
    "$(printf '%b' 'sys\tschema_unused_indexes\tinformation_schema\tSTATISTICS\nsys\tschema_unused_indexes\tperformance_schema\ttable_io_waits_summary_by_index_usage\n0')" \
    "SELECT VIEW_SCHEMA, VIEW_NAME, TABLE_SCHEMA, TABLE_NAME
       FROM INFORMATION_SCHEMA.VIEW_TABLE_USAGE
      WHERE VIEW_SCHEMA = 'sys'
        AND VIEW_NAME = 'schema_unused_indexes'
      ORDER BY TABLE_SCHEMA, TABLE_NAME;
     SELECT COUNT(*)
       FROM INFORMATION_SCHEMA.VIEW_ROUTINE_USAGE
      WHERE TABLE_SCHEMA = 'sys'
        AND TABLE_NAME = 'schema_unused_indexes';"

expect_output \
    "sys.schema_unused_indexes empty index and constraints" \
    "$(printf '%b' '0\t0\t0\t0')" \
    "SELECT
        (SELECT COUNT(*) FROM INFORMATION_SCHEMA.STATISTICS
          WHERE TABLE_SCHEMA = 'sys' AND TABLE_NAME = 'schema_unused_indexes'),
        (SELECT COUNT(*) FROM INFORMATION_SCHEMA.TABLE_CONSTRAINTS
          WHERE TABLE_SCHEMA = 'sys' AND TABLE_NAME = 'schema_unused_indexes'),
        (SELECT COUNT(*) FROM INFORMATION_SCHEMA.KEY_COLUMN_USAGE
          WHERE TABLE_SCHEMA = 'sys' AND TABLE_NAME = 'schema_unused_indexes'),
        (SELECT COUNT(*) FROM INFORMATION_SCHEMA.TABLE_CONSTRAINTS_EXTENSIONS
          WHERE CONSTRAINT_SCHEMA = 'sys' AND TABLE_NAME = 'schema_unused_indexes');"

run_mysql "CREATE TABLE idx_target (
               id INT PRIMARY KEY,
               value_col INT,
               value_col_2 INT,
               filler VARCHAR(20),
               body TEXT,
               KEY value_idx(value_col),
               UNIQUE KEY filler_unique(filler),
               KEY composite_idx(value_col, value_col_2),
               FULLTEXT KEY body_ft(body)
           );" "$DATABASE" >/dev/null

expect_output \
    "sys.schema_unused_indexes user rows before index read" \
    "$(printf '%b' "${DATABASE}\tidx_target\tbody_ft\n${DATABASE}\tidx_target\tcomposite_idx\n${DATABASE}\tidx_target\tvalue_idx")" \
    "SELECT object_schema, object_name, index_name
       FROM sys.schema_unused_indexes
      WHERE object_schema = '${DATABASE}'
      ORDER BY object_name, index_name;"

run_mysql "SELECT * FROM idx_target WHERE value_col = 42;" "$DATABASE" >/dev/null

expect_output \
    "sys.schema_unused_indexes user rows after value index read" \
    "$(printf '%b' "${DATABASE}\tidx_target\tbody_ft\n${DATABASE}\tidx_target\tcomposite_idx")" \
    "SELECT object_schema, object_name, index_name
       FROM sys.schema_unused_indexes
      WHERE object_schema = '${DATABASE}'
      ORDER BY object_name, index_name;"

expect_contains \
    "sys.schema_unused_indexes SHOW CREATE VIEW" \
    "CREATE ALGORITHM=MERGE DEFINER=\`mysql.sys\`@\`localhost\` SQL SECURITY INVOKER VIEW \`sys\`.\`schema_unused_indexes\`" \
    "SHOW CREATE VIEW sys.schema_unused_indexes;"

expect_contains \
    "sys.schema_unused_indexes SHOW CREATE VIEW count filter" \
    "(\`t\`.\`COUNT_STAR\` = 0)" \
    "SHOW CREATE VIEW sys.schema_unused_indexes;"

expect_contains \
    "sys.schema_unused_indexes SHOW CREATE VIEW nonunique filter" \
    "(\`information_schema\`.\`s\`.\`NON_UNIQUE\` = 1)" \
    "SHOW CREATE VIEW sys.schema_unused_indexes;"

expect_contains \
    "sys.schema_unused_indexes SHOW CREATE TABLE" \
    "CREATE ALGORITHM=MERGE DEFINER=\`mysql.sys\`@\`localhost\` SQL SECURITY INVOKER VIEW \`schema_unused_indexes\`" \
    "USE sys; SHOW CREATE TABLE schema_unused_indexes;"

status=$(run_mysql "SELECT COUNT(*) FROM sys.schema_unused_indexes WHERE object_schema = '${DATABASE}'; SELECT @@warning_count, ROW_COUNT();")
status=$(printf '%s\n' "$status" | tail -n 1)
if [ "$status" != "0	-1" ]; then
    fail "sys.schema_unused_indexes SELECT status: expected [0	-1], got [$status]"
fi

printf '%s\n' "mysql_baseline_sys_schema_unused_indexes_view_expectations: ok"
