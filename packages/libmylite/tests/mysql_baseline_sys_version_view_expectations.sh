#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
PRIVILEGES="select,insert,update,references"

fail() {
    printf '%s\n' "mysql_baseline_sys_version_view_expectations: $1" >&2
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

expect_show_table_status_row() {
    output=$(run_mysql "SHOW TABLE STATUS FROM sys LIKE 'version';")
    field_count=$(printf '%s\n' "$output" | awk -F '\t' '{print NF}')
    prefix=$(printf '%s\n' "$output" | cut -f 1-11)
    create_time=$(printf '%s\n' "$output" | cut -f 12)
    suffix=$(printf '%s\n' "$output" | cut -f 13-18)

    if [ "$field_count" != "18" ]; then
        fail "SHOW TABLE STATUS sys.version: expected 18 fields, got [$field_count]"
    fi
    expected_prefix=$(
        printf '%b' 'version\tNULL\tNULL\tNULL\tNULL\tNULL\tNULL\tNULL\tNULL\tNULL\tNULL'
    )
    if [ "$prefix" != "$expected_prefix" ]; then
        fail "SHOW TABLE STATUS sys.version: expected prefix [$expected_prefix], got [$prefix]"
    fi
    case "$create_time" in
        ????-??-??\ ??:??:??) ;;
        *) fail "SHOW TABLE STATUS sys.version: expected Create_time datetime, got [$create_time]" ;;
    esac
    expected_suffix=$(printf '%b' 'NULL\tNULL\tNULL\tNULL\tNULL\tVIEW')
    if [ "$suffix" != "$expected_suffix" ]; then
        fail "SHOW TABLE STATUS sys.version: expected suffix [$expected_suffix], got [$suffix]"
    fi
}

version=$(run_mysql "SELECT VERSION();")
case "$version" in
    8.4.9*) ;;
    *) fail "expected MySQL 8.4.9 runtime, got [$version]" ;;
esac

expect_output \
    "sys.version direct row" \
    "$(printf '%b' '2.1.3\t8.4.9')" \
    "SELECT sys_version, mysql_version FROM sys.version;"

expect_output \
    "sys.version selected-schema row" \
    "$(printf '%b' '2.1.3\t8.4.9')" \
    "USE sys; SELECT * FROM version;"

status=$(run_mysql "SELECT * FROM sys.version; SELECT @@warning_count, ROW_COUNT();" | tail -n 1)
if [ "$status" != "0	-1" ]; then
    fail "sys.version status: expected [0	-1], got [$status]"
fi

show_columns_expected=$(
    {
        printf '%b\n' 'sys_version\tvarchar(5)\tNO\t\t\t'
        printf '%b\n' 'mysql_version\tvarchar(5)\tNO\t\t\t'
    }
)
expect_output \
    "sys.version SHOW COLUMNS rows" \
    "$show_columns_expected" \
    "SHOW COLUMNS FROM sys.version;"

full_columns_expected=$(
    {
        printf '%b\n' "sys_version\tvarchar(5)\tutf8mb4_0900_ai_ci\tNO\t\t\t\t$PRIVILEGES\t"
        printf '%b\n' "mysql_version\tvarchar(5)\tutf8mb3_general_ci\tNO\t\t\t\t$PRIVILEGES\t"
    }
)
expect_output \
    "sys.version SHOW FULL COLUMNS rows" \
    "$full_columns_expected" \
    "SHOW FULL COLUMNS FROM sys.version;"

information_schema_columns_expected=$(
    {
        printf '%b\n' "sys_version\t1\t\tNO\tvarchar\t5\t20\tNULL\tNULL\tNULL\tutf8mb4\tutf8mb4_0900_ai_ci\tvarchar(5)\t\t\t$PRIVILEGES\t\t"
        printf '%b\n' "mysql_version\t2\t\tNO\tvarchar\t5\t15\tNULL\tNULL\tNULL\tutf8mb3\tutf8mb3_general_ci\tvarchar(5)\t\t\t$PRIVILEGES\t\t"
    }
)
expect_output \
    "sys.version INFORMATION_SCHEMA.COLUMNS rows" \
    "$information_schema_columns_expected" \
    "SELECT COLUMN_NAME, ORDINAL_POSITION, COLUMN_DEFAULT, IS_NULLABLE, DATA_TYPE,
            CHARACTER_MAXIMUM_LENGTH, CHARACTER_OCTET_LENGTH, NUMERIC_PRECISION,
            NUMERIC_SCALE, DATETIME_PRECISION, CHARACTER_SET_NAME, COLLATION_NAME,
            COLUMN_TYPE, COLUMN_KEY, EXTRA, PRIVILEGES, COLUMN_COMMENT,
            GENERATION_EXPRESSION
       FROM INFORMATION_SCHEMA.COLUMNS
      WHERE TABLE_SCHEMA = 'sys' AND TABLE_NAME = 'version'
      ORDER BY ORDINAL_POSITION;"

expect_output \
    "sys.version SHOW INDEX is empty" \
    "" \
    "SHOW INDEX FROM sys.version;"

expect_output \
    "sys.version empty secondary metadata counts" \
    "$(printf '%b' '0\t0\t0\t0')" \
    "SELECT
        (SELECT COUNT(*) FROM INFORMATION_SCHEMA.STATISTICS
          WHERE TABLE_SCHEMA = 'sys' AND TABLE_NAME = 'version'),
        (SELECT COUNT(*) FROM INFORMATION_SCHEMA.TABLE_CONSTRAINTS
          WHERE TABLE_SCHEMA = 'sys' AND TABLE_NAME = 'version'),
        (SELECT COUNT(*) FROM INFORMATION_SCHEMA.KEY_COLUMN_USAGE
          WHERE TABLE_SCHEMA = 'sys' AND TABLE_NAME = 'version'),
        (SELECT COUNT(*) FROM INFORMATION_SCHEMA.TABLE_CONSTRAINTS_EXTENSIONS
          WHERE CONSTRAINT_SCHEMA = 'sys' AND TABLE_NAME = 'version');"

expect_output \
    "sys.version INFORMATION_SCHEMA.TABLES row" \
    "$(printf '%b' 'version\tVIEW\tNULL\tNULL\tNULL\tNULL\tNULL\tNULL\tNULL\tNULL\tNULL\tNULL\t1\t1\t1\tNULL\t1\tNULL\tVIEW')" \
    "SELECT TABLE_NAME, TABLE_TYPE, ENGINE, VERSION, ROW_FORMAT, TABLE_ROWS,
            AVG_ROW_LENGTH, DATA_LENGTH, MAX_DATA_LENGTH, INDEX_LENGTH, DATA_FREE,
            AUTO_INCREMENT, CREATE_TIME IS NOT NULL, UPDATE_TIME IS NULL,
            CHECK_TIME IS NULL, TABLE_COLLATION, CHECKSUM IS NULL, CREATE_OPTIONS,
            TABLE_COMMENT
       FROM INFORMATION_SCHEMA.TABLES
      WHERE TABLE_SCHEMA = 'sys' AND TABLE_NAME = 'version';"

expect_show_table_status_row

printf '%s\n' "mysql_baseline_sys_version_view_expectations: ok"
