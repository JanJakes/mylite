#!/usr/bin/env sh

set -eu

MYSQL_BIN="${MYLITE_MYSQL_BIN:-mysql}"
MYSQL_SOCKET="${MYLITE_MYSQL_SOCKET:-}"
MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_show_metadata_regexp_filters_$$"
TAB=$(printf '\t')

fail() {
    printf '%s\n' "mysql_baseline_show_metadata_regexp_filters_expectations: $1" >&2
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

expect_value() {
    label=$1
    expected=$2
    actual=$3

    if [ "$actual" != "$expected" ]; then
        fail "$label: expected [$expected], got [$actual]"
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
    status=$?
    set -e

    if [ "$status" -eq 0 ]; then
        fail "$label: expected error $code/$state, command succeeded with [$output]"
    fi
    case "$output" in
        *"ERROR $code ($state)"*"$message"*) ;;
        *) fail "$label: expected error $code/$state containing [$message], got [$output]" ;;
    esac
}

normalize_tsv() {
    sed "s/${TAB}/|/g"
}

row_names() {
    awk -F '\t' 'NF > 0 { print $1 }'
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

case "$(run_mysql 'SELECT @@lower_case_table_names;')" in
    0) ;;
    *) fail "expected @@lower_case_table_names=0 for SHOW TABLE STATUS Name REGEXP probes" ;;
esac

cleanup
run_mysql "CREATE DATABASE ${DATABASE};" >/dev/null
run_mysql \
    "USE ${DATABASE};
     CREATE TABLE inspected (
         id INT NOT NULL,
         name VARCHAR(16) DEFAULT 'anon',
         nn INT NOT NULL DEFAULT 7,
         txt TEXT,
         KEY k_name (name(3)),
         KEY rss_idx (txt(4))
     ) ENGINE=InnoDB;" >/dev/null

columns_regexp=$(run_mysql "SHOW COLUMNS FROM ${DATABASE}.inspected WHERE Field REGEXP '^i';" | normalize_tsv)
expect_value "show columns regexp" "id|int|NO||NULL|" "$columns_regexp"

columns_rlike=$(run_mysql "SHOW COLUMNS FROM ${DATABASE}.inspected WHERE Field RLIKE 'N.*e';" | normalize_tsv)
expect_value "show columns rlike" "name|varchar(16)|YES|MUL|anon|" "$columns_rlike"

columns_not_regexp=$(run_mysql "SHOW COLUMNS FROM ${DATABASE}.inspected WHERE Field NOT REGEXP '^i';" | normalize_tsv)
expect_value \
    "show columns not regexp" \
    "name|varchar(16)|YES|MUL|anon|
nn|int|NO||7|
txt|text|YES|MUL|NULL|" \
    "$columns_not_regexp"

default_regexp=$(run_mysql "SHOW COLUMNS FROM ${DATABASE}.inspected WHERE \`Default\` REGEXP '^a';" | normalize_tsv)
expect_value "show columns regexp skips null default cells" "name|varchar(16)|YES|MUL|anon|" "$default_regexp"

full_collation_regexp=$(run_mysql "SHOW FULL COLUMNS FROM ${DATABASE}.inspected WHERE Collation REGEXP '^utf8mb4';" | normalize_tsv)
expect_value \
    "show full columns collation regexp" \
    "name|varchar(16)|utf8mb4_0900_ai_ci|YES|MUL|anon||select,insert,update,references|
txt|text|utf8mb4_0900_ai_ci|YES|MUL|NULL||select,insert,update,references|" \
    "$full_collation_regexp"

full_collation_not_regexp=$(run_mysql "SHOW FULL COLUMNS FROM ${DATABASE}.inspected WHERE Collation NOT REGEXP '^utf8mb4';" | normalize_tsv)
expect_value "show full columns not regexp skips null collation cells" "" "$full_collation_not_regexp"

index_key_regexp=$(run_mysql "SHOW INDEX FROM ${DATABASE}.inspected WHERE Key_name REGEXP '.*_.*';" | normalize_tsv)
expect_value \
    "show index key regexp" \
    "inspected|1|k_name|1|name|A|0|3|NULL|YES|BTREE|||YES|NULL
inspected|1|rss_idx|1|txt|A|0|4|NULL|YES|BTREE|||YES|NULL" \
    "$index_key_regexp"

index_numeric_regexp=$(run_mysql "SHOW INDEX FROM ${DATABASE}.inspected WHERE Sub_part REGEXP '^3$';" | normalize_tsv)
expect_value \
    "show index numeric metadata regexp" \
    "inspected|1|k_name|1|name|A|0|3|NULL|YES|BTREE|||YES|NULL" \
    "$index_numeric_regexp"

index_null_regexp=$(run_mysql "SHOW INDEX FROM ${DATABASE}.inspected WHERE Packed REGEXP '.*';" | normalize_tsv)
expect_value "show index regexp skips null cells" "" "$index_null_regexp"

table_status_regexp=$(run_mysql "SHOW TABLE STATUS FROM ${DATABASE} WHERE Name REGEXP '^ins';" | row_names)
expect_value "show table status name regexp" "inspected" "$table_status_regexp"

table_status_name_upper_regexp=$(run_mysql "SHOW TABLE STATUS FROM ${DATABASE} WHERE Name REGEXP '^INS';" | row_names)
expect_value "show table status name regexp case-sensitive" "" "$table_status_name_upper_regexp"

table_status_name_not_regexp=$(run_mysql "SHOW TABLE STATUS FROM ${DATABASE} WHERE Name NOT REGEXP '^INS';" | row_names)
expect_value "show table status name not regexp case-sensitive" "inspected" "$table_status_name_not_regexp"

table_status_name_upper_rlike=$(run_mysql "SHOW TABLE STATUS FROM ${DATABASE} WHERE Name RLIKE '^INS';" | row_names)
expect_value "show table status name rlike case-sensitive" "" "$table_status_name_upper_rlike"

table_status_name_not_rlike=$(run_mysql "SHOW TABLE STATUS FROM ${DATABASE} WHERE Name NOT RLIKE '^INS';" | row_names)
expect_value "show table status name not rlike case-sensitive" "inspected" "$table_status_name_not_rlike"

table_status_rlike=$(run_mysql "SHOW TABLE STATUS FROM ${DATABASE} WHERE Engine RLIKE '^innodb$';" | row_names)
expect_value "show table status rlike" "inspected" "$table_status_rlike"

table_status_null_regexp=$(run_mysql "SHOW TABLE STATUS FROM ${DATABASE} WHERE Auto_increment REGEXP '^1$';" | row_names)
expect_value "show table status regexp skips null cells" "" "$table_status_null_regexp"

status=$(
    run_mysql \
        "SHOW TABLE STATUS FROM ${DATABASE} WHERE Name REGEXP '^ins';
         SELECT @@warning_count, @@error_count, ROW_COUNT();" \
        | tail -n 1 \
        | normalize_tsv
)
expect_value "status after successful regexp filter" "0|0|-1" "$status"

expect_error \
    "unclosed bracket pattern" \
    3696 \
    HY000 \
    "unclosed bracket expression" \
    "SHOW COLUMNS FROM ${DATABASE}.inspected WHERE Field REGEXP '[';"

expect_error \
    "invalid character range pattern" \
    3697 \
    HY000 \
    "[x-y] character range where x comes after y" \
    "SHOW INDEX FROM ${DATABASE}.inspected WHERE Key_name REGEXP '[z-a]';"

printf '%s\n' "baseline-show-metadata-regexp-filters MySQL 8.4.9 expectations verified"
