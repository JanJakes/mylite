#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_show_table_status_metadata_$$"

fail() {
    printf '%s\n' "mysql_baseline_show_table_status_metadata_expectations: $1" >&2
    exit 1
}

run_mysql() {
    sql=$1
    shift
    printf '%s\n' "$sql" \
        | docker exec -i "$MYSQL_CONTAINER" mysql --protocol=TCP -h127.0.0.1 -uroot --batch --raw --skip-column-names "$@"
}

expect_value() {
    label=$1
    expected=$2
    actual=$3

    if [ "$actual" != "$expected" ]; then
        fail "$label: expected [$expected], got [$actual]"
    fi
}

field_from_rows() {
    rows=$1
    table_name=$2
    field_index=$3

    printf '%s\n' "$rows" | awk -F '\t' -v table="$table_name" -v field="$field_index" '$1 == table { print $field; exit }'
}

expect_datetime_or_null() {
    label=$1
    value=$2

    case "$value" in
        NULL) ;;
        [0-9][0-9][0-9][0-9]-[0-9][0-9]-[0-9][0-9]\ [0-9][0-9]:[0-9][0-9]:[0-9][0-9]) ;;
        *) fail "$label: expected MySQL DATETIME text or NULL, got [$value]" ;;
    esac
}

cleanup() {
    run_mysql "DROP DATABASE IF EXISTS ${DATABASE};" >/dev/null 2>&1 || true
}

trap cleanup EXIT

version=$(run_mysql 'SELECT VERSION();')
case "$version" in
    8.4.9*) ;;
    *) fail "expected MySQL 8.4.9 runtime, got [$version]" ;;
esac

cleanup

run_mysql \
    "CREATE DATABASE ${DATABASE};
     USE ${DATABASE};
     SET time_zone = '+00:00';
     SET timestamp = 1700000000;
     CREATE TABLE no_index(id INT, v INT) ENGINE=InnoDB;
     CREATE TABLE primary_only(id INT PRIMARY KEY, v INT) ENGINE=InnoDB;
     CREATE TABLE secondary_index(id INT PRIMARY KEY, v INT, KEY v_key(v)) ENGINE=InnoDB;
     CREATE TABLE unique_index(id INT PRIMARY KEY, v INT, UNIQUE KEY v_unique(v)) ENGINE=InnoDB;
     INSERT INTO no_index VALUES (1, 10), (2, 20);
     INSERT INTO secondary_index VALUES (1, 10), (2, 20);
     INSERT INTO unique_index VALUES (1, 10), (2, 20);" >/dev/null

status_rows=$(run_mysql "USE ${DATABASE}; SET time_zone = '+00:00'; SHOW TABLE STATUS;")
info_rows=$(
    run_mysql \
        "SELECT TABLE_NAME, ENGINE, VERSION, ROW_FORMAT, TABLE_ROWS, AVG_ROW_LENGTH, DATA_LENGTH,
                MAX_DATA_LENGTH, INDEX_LENGTH, DATA_FREE, AUTO_INCREMENT, CREATE_TIME, UPDATE_TIME,
                CHECK_TIME, TABLE_COLLATION, CHECKSUM, CREATE_OPTIONS, TABLE_COMMENT
           FROM INFORMATION_SCHEMA.TABLES
          WHERE TABLE_SCHEMA = '${DATABASE}'
          ORDER BY TABLE_NAME;"
)

expect_value "no-index create time" "2023-11-14 22:13:20" "$(field_from_rows "$status_rows" "no_index" 12)"
expect_value "primary create time" "2023-11-14 22:13:20" "$(field_from_rows "$status_rows" "primary_only" 12)"
expect_value "secondary create time" "2023-11-14 22:13:20" "$(field_from_rows "$status_rows" "secondary_index" 12)"

expect_value "no-index index length" "0" "$(field_from_rows "$status_rows" "no_index" 9)"
expect_value "primary-only index length" "0" "$(field_from_rows "$status_rows" "primary_only" 9)"
expect_value "secondary index length" "16384" "$(field_from_rows "$status_rows" "secondary_index" 9)"
expect_value "unique index length" "16384" "$(field_from_rows "$status_rows" "unique_index" 9)"

expect_value "information schema no-index create time" \
    "$(field_from_rows "$status_rows" "no_index" 12)" \
    "$(field_from_rows "$info_rows" "no_index" 12)"
expect_value "information schema secondary index length" \
    "$(field_from_rows "$status_rows" "secondary_index" 9)" \
    "$(field_from_rows "$info_rows" "secondary_index" 9)"

expect_datetime_or_null "no-index update time" "$(field_from_rows "$status_rows" "no_index" 13)"
expect_datetime_or_null "secondary update time" "$(field_from_rows "$status_rows" "secondary_index" 13)"

tz_rows=$(run_mysql "USE ${DATABASE}; SET time_zone = '+02:00'; SHOW TABLE STATUS LIKE 'no\\_index';")
expect_value "create time session time zone" "2023-11-15 00:13:20" "$(field_from_rows "$tz_rows" "no_index" 12)"

printf '%s\n' "mysql_baseline_show_table_status_metadata_expectations: ok"
