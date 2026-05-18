#!/usr/bin/env sh

set -eu

MYSQL_BIN="${MYLITE_MYSQL_BIN:-mysql}"
MYSQL_SOCKET="${MYLITE_MYSQL_SOCKET:-}"
MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_show_columns_where_$$"
TAB=$(printf '\t')

fail() {
    printf '%s\n' "mysql_baseline_show_columns_where_expectations: $1" >&2
    exit 1
}

run_mysql() {
    sql=$1
    shift
    if [ -n "$MYSQL_SOCKET" ]; then
        printf '%s\n' "$sql" \
            | "$MYSQL_BIN" \
                --protocol=SOCKET \
                --socket="$MYSQL_SOCKET" \
                -uroot \
                --batch \
                --raw \
                --skip-column-names \
                --default-character-set=utf8mb4 \
                "$@"
    else
        printf '%s\n' "$sql" \
            | docker exec -i "$MYSQL_CONTAINER" mysql \
                --protocol=TCP \
                -h127.0.0.1 \
                -uroot \
                --batch \
                --raw \
                --skip-column-names \
                --default-character-set=utf8mb4 \
                "$@"
    fi
}

run_mysql_with_headers() {
    sql=$1
    shift
    if [ -n "$MYSQL_SOCKET" ]; then
        printf '%s\n' "$sql" \
            | "$MYSQL_BIN" \
                --protocol=SOCKET \
                --socket="$MYSQL_SOCKET" \
                -uroot \
                --batch \
                --raw \
                --default-character-set=utf8mb4 \
                "$@"
    else
        printf '%s\n' "$sql" \
            | docker exec -i "$MYSQL_CONTAINER" mysql \
                --protocol=TCP \
                -h127.0.0.1 \
                -uroot \
                --batch \
                --raw \
                --default-character-set=utf8mb4 \
                "$@"
    fi
}

expect_error() {
    label=$1
    code=$2
    state=$3
    message=$4
    sql=$5
    shift 5

    set +e
    output=$(run_mysql "$sql" "$@" 2>&1)
    status_code=$?
    set -e

    if [ "$status_code" -eq 0 ]; then
        fail "$label: expected error $code/$state, command succeeded with [$output]"
    fi

    case "$output" in
        *"ERROR $code ($state)"*"$message"*) ;;
        *)
            fail "$label: expected error $code/$state containing [$message], got [$output]"
            ;;
    esac
}

expect_value() {
    label=$1
    expected=$2
    actual=$3

    if [ "$actual" != "$expected" ]; then
        fail "$label: expected [$expected], got [$actual]"
    fi
}

expect_positive_integer() {
    label=$1
    value=$2

    case "$value" in
        ''|*[!0-9]*) fail "$label: expected positive integer, got [$value]" ;;
    esac
    if [ "$value" -eq 0 ]; then
        fail "$label: expected positive integer, got [$value]"
    fi
}

normalize_tsv() {
    sed "s/${TAB}/|/g"
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
     SET sql_mode = '';
     CREATE TABLE inspected (
         id INT NOT NULL,
         name VARCHAR(12) DEFAULT NULL,
         title VARCHAR(20) NOT NULL DEFAULT 'hello',
         created DATETIME NOT NULL DEFAULT '0000-00-00 00:00:00',
         KEY name_idx (name)
     ) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;" >/dev/null

expected_columns="Field${TAB}Type${TAB}Null${TAB}Key${TAB}Default${TAB}Extra"
headers=$(run_mysql_with_headers "SHOW COLUMNS FROM ${DATABASE}.inspected WHERE Field = 'id';" | sed -n '1p')
expect_value "headers" "$expected_columns" "$headers"

field_filter=$(run_mysql "SHOW COLUMNS FROM ${DATABASE}.inspected WHERE Field = 'id';" | normalize_tsv)
expect_value "field filter" "id|int|NO||NULL|" "$field_filter"

fields_like=$(run_mysql "SHOW FIELDS FROM ${DATABASE}.inspected WHERE Type LIKE 'varchar%';" | normalize_tsv)
expect_value \
    "fields type like filter" \
    "name|varchar(12)|YES|MUL|NULL|
title|varchar(20)|NO||hello|" \
    "$fields_like"

default_null=$(
    run_mysql \
        "SHOW COLUMNS FROM ${DATABASE}.inspected
         WHERE \`Default\` <=> NULL AND Field IN ('id','name');" \
        | normalize_tsv
)
expect_value \
    "default null-safe and in filter" \
    "id|int|NO||NULL|
name|varchar(12)|YES|MUL|NULL|" \
    "$default_null"

not_in=$(
    run_mysql \
        "SHOW COLUMNS FROM ${DATABASE}.inspected
         WHERE Field NOT IN (NULL, 'id');" \
        | normalize_tsv
)
expect_value "not in with null returns no rows" "" "$not_in"

expected_full_columns="Field${TAB}Type${TAB}Collation${TAB}Null${TAB}Key${TAB}Default${TAB}Extra${TAB}Privileges${TAB}Comment"
full_headers=$(
    run_mysql_with_headers \
        "SHOW FULL COLUMNS FROM ${DATABASE}.inspected WHERE Collation IS NOT NULL;" \
        | sed -n '1p'
)
expect_value "full headers" "$expected_full_columns" "$full_headers"

collation_filter=$(
    run_mysql \
        "SHOW FULL COLUMNS FROM ${DATABASE}.inspected WHERE Collation IS NOT NULL;" \
        | normalize_tsv
)
expect_value \
    "full collation filter" \
    "name|varchar(12)|utf8mb4_0900_ai_ci|YES|MUL|NULL||select,insert,update,references|
title|varchar(20)|utf8mb4_0900_ai_ci|NO||hello||select,insert,update,references|" \
    "$collation_filter"

privileges_filter=$(
    run_mysql \
        "SHOW FULL FIELDS FROM ${DATABASE}.inspected
         WHERE Privileges LIKE '%update%' AND Comment = '';" \
        | normalize_tsv
)
expect_value \
    "full fields privileges and comment filter" \
    "id|int|NULL|NO||NULL||select,insert,update,references|
name|varchar(12)|utf8mb4_0900_ai_ci|YES|MUL|NULL||select,insert,update,references|
title|varchar(20)|utf8mb4_0900_ai_ci|NO||hello||select,insert,update,references|
created|datetime|NULL|NO||0000-00-00 00:00:00||select,insert,update,references|" \
    "$privileges_filter"

status=$(
    run_mysql \
        "SHOW FULL COLUMNS FROM ${DATABASE}.inspected WHERE \`Default\` IS NOT NULL;
         SELECT @@warning_count, @@error_count, ROW_COUNT();" \
        | tail -n 1 \
        | normalize_tsv
)
expect_value "status after successful where" "0|0|-1" "$status"

numeric_warning_count=$(
    run_mysql \
        "SHOW COLUMNS FROM ${DATABASE}.inspected WHERE Field = 1;
         SELECT @@warning_count;" \
        | tail -n 1
)
expect_positive_integer "mysql numeric comparison warning count" "$numeric_warning_count"

numeric_in_warning_count=$(
    run_mysql \
        "SHOW COLUMNS FROM ${DATABASE}.inspected WHERE Field IN ('id', 1);
         SELECT @@warning_count;" \
        | tail -n 1
)
expect_positive_integer "mysql numeric IN warning count" "$numeric_in_warning_count"

regexp_rows=$(run_mysql "SHOW COLUMNS FROM ${DATABASE}.inspected WHERE Field REGEXP '^i';" | normalize_tsv)
expect_value "mysql regexp accepted upstream" "id|int|NO||NULL|" "$regexp_rows"

expect_error \
    "like then where is syntax error" \
    1064 \
    42000 \
    "SQL syntax" \
    "SHOW COLUMNS FROM ${DATABASE}.inspected LIKE 'i%' WHERE Field = 'id';"

expect_error \
    "order by after where" \
    1064 \
    42000 \
    "SQL syntax" \
    "SHOW COLUMNS FROM ${DATABASE}.inspected WHERE Field = 'id' ORDER BY Field;"

expect_error \
    "limit after where" \
    1064 \
    42000 \
    "SQL syntax" \
    "SHOW COLUMNS FROM ${DATABASE}.inspected WHERE Field = 'id' LIMIT 1;"

expect_error \
    "full-only column in non-full output" \
    1054 \
    42S22 \
    "Unknown column 'Collation' in 'where clause'" \
    "SHOW COLUMNS FROM ${DATABASE}.inspected WHERE Collation IS NULL;"

expect_error \
    "unknown where column" \
    1054 \
    42S22 \
    "Unknown column 'missing' in 'where clause'" \
    "SHOW COLUMNS FROM ${DATABASE}.inspected WHERE missing = 'x';"

expect_error \
    "qualified where column" \
    1054 \
    42S22 \
    "Unknown column 't.Field' in 'where clause'" \
    "SHOW COLUMNS FROM ${DATABASE}.inspected WHERE t.Field = 'id';"

printf '%s\n' "baseline-show-columns-where MySQL 8.4.9 expectations verified"
