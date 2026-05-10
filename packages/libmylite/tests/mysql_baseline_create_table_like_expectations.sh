#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_create_table_like_$$"
OTHER_DATABASE="${DATABASE}_other"

fail() {
    printf '%s\n' "mysql_baseline_create_table_like_expectations: $1" >&2
    exit 1
}

run_mysql() {
    sql=$1
    shift
    printf '%s\n' "$sql" \
        | docker exec -i "$MYSQL_CONTAINER" mysql --protocol=TCP -h127.0.0.1 -uroot --batch --raw --skip-column-names "$@"
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

cleanup() {
    run_mysql "DROP DATABASE IF EXISTS ${DATABASE}; DROP DATABASE IF EXISTS ${OTHER_DATABASE};" \
        >/dev/null 2>&1 || true
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
     CREATE DATABASE ${OTHER_DATABASE};
     USE ${DATABASE};
     CREATE TABLE src(
         id INT NOT NULL DEFAULT 7,
         n INTEGER NULL DEFAULT NULL,
         b BIGINT NOT NULL,
         inv INT NULL INVISIBLE
     ) ENGINE=InnoDB;
     INSERT INTO src(id, n, b, inv) VALUES (1, 2, 3, 4);
     CREATE TABLE dst LIKE src;
     SELECT ROW_COUNT(), @@warning_count, @@error_count;
     SELECT COUNT(*) FROM dst;
     SHOW COLUMNS FROM dst;" \
    >"/tmp/${DATABASE}_basic.out"

expect_value \
    "basic status" \
    "0	0	0" \
    "$(sed -n '1p' "/tmp/${DATABASE}_basic.out")"
expect_value "basic target empty" "0" "$(sed -n '2p' "/tmp/${DATABASE}_basic.out")"
expected_basic_columns=$(
    printf '%s\t%s\t%s\t%s\t%s\t%s\n' \
        "id" "int" "NO" "" "7" "" \
        "n" "int" "YES" "" "NULL" "" \
        "b" "bigint" "NO" "" "NULL" "" \
        "inv" "int" "YES" "" "NULL" "INVISIBLE"
)
expect_value \
    "basic columns" \
    "$expected_basic_columns" \
    "$(sed -n '3,$p' "/tmp/${DATABASE}_basic.out")"

paren_status=$(
    run_mysql \
        "USE ${DATABASE};
         CREATE TABLE paren_like (LIKE src);
         SELECT ROW_COUNT(), @@warning_count, @@error_count;
         SELECT COUNT(*) FROM paren_like;"
)
expect_value \
    "parenthesized status and empty target" \
    "0	0	0
0" \
    "$(printf '%s\n' "$paren_status" | tail -n 2)"

qualified_status=$(
    run_mysql \
        "CREATE TABLE ${OTHER_DATABASE}.qualified_dst LIKE ${DATABASE}.src;
         SELECT ROW_COUNT(), @@warning_count, @@error_count;
         SELECT COUNT(*) FROM ${OTHER_DATABASE}.qualified_dst;"
)
expect_value \
    "schema-qualified status and empty target without default database" \
    "0	0	0
0" \
    "$(printf '%s\n' "$qualified_status" | tail -n 2)"

if_not_exists_status=$(
    run_mysql \
        "USE ${DATABASE};
         CREATE TABLE existing(id INT);
         CREATE TABLE IF NOT EXISTS existing LIKE src;
         SELECT ROW_COUNT(), @@warning_count, @@error_count;"
)
expect_value \
    "existing target if not exists status" \
    "0	1	0" \
    "$(printf '%s\n' "$if_not_exists_status" | tail -n 1)"

expect_error \
    "missing default database for target" \
    1046 \
    3D000 \
    "No database selected" \
    "CREATE TABLE no_default_target LIKE ${DATABASE}.src;"

expect_error \
    "missing default database for source" \
    1046 \
    3D000 \
    "No database selected" \
    "CREATE TABLE ${DATABASE}.target_source_unqualified LIKE src;"

expect_error \
    "unknown target schema" \
    1049 \
    42000 \
    "Unknown database 'nosuch_schema_${DATABASE}'" \
    "CREATE TABLE nosuch_schema_${DATABASE}.dst LIKE ${DATABASE}.src;"

expect_error \
    "unknown source schema" \
    1049 \
    42000 \
    "Unknown database 'nosuch_schema_${DATABASE}'" \
    "CREATE TABLE ${DATABASE}.dst_unknown_source_schema LIKE nosuch_schema_${DATABASE}.src;"

expect_error \
    "source schema error before target schema error" \
    1049 \
    42000 \
    "Unknown database 'nosuch_source_schema_${DATABASE}'" \
    "CREATE TABLE nosuch_target_schema_${DATABASE}.dst LIKE nosuch_source_schema_${DATABASE}.src;"

expect_error \
    "source table error before target schema error" \
    1146 \
    42S02 \
    "Table '${DATABASE}.missing_source_precedence' doesn't exist" \
    "CREATE TABLE nosuch_target_schema_${DATABASE}.dst LIKE ${DATABASE}.missing_source_precedence;"

expect_error \
    "unknown source table" \
    1146 \
    42S02 \
    "Table '${DATABASE}.missing' doesn't exist" \
    "CREATE TABLE ${DATABASE}.dst_missing_source LIKE ${DATABASE}.missing;"

expect_error \
    "existing target without if not exists" \
    1050 \
    42S01 \
    "Table 'existing' already exists" \
    "USE ${DATABASE}; CREATE TABLE existing LIKE src;"

expect_error \
    "missing source before if not exists target noop" \
    1146 \
    42S02 \
    "Table '${DATABASE}.missing_source' doesn't exist" \
    "USE ${DATABASE}; CREATE TABLE IF NOT EXISTS existing LIKE missing_source;"

expect_error \
    "source view is not base table" \
    1347 \
    HY000 \
    "'${DATABASE}.src_view' is not BASE TABLE" \
    "USE ${DATABASE}; CREATE VIEW src_view AS SELECT id FROM src; CREATE TABLE dst_view LIKE src_view;"

rm -f "/tmp/${DATABASE}_basic.out"
