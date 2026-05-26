#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_checksum_table_expectations_$$"

fail() {
    printf '%s\n' "mysql_baseline_checksum_table_expectations: $1" >&2
    exit 1
}

run_mysql() {
    sql=$1
    shift
    printf '%s\n' "$sql" \
        | docker exec -i "$MYSQL_CONTAINER" mysql --protocol=TCP -h127.0.0.1 -uroot \
            --batch --raw --skip-column-names "$@"
}

run_mysql_type_info() {
    sql=$1
    shift
    printf '%s\n' "$sql" \
        | docker exec -i "$MYSQL_CONTAINER" mysql --protocol=TCP -h127.0.0.1 -uroot \
            --column-type-info -vvv "$@"
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

expect_error() {
    label=$1
    expected=$2
    sql=$3
    shift 3

    set +e
    output=$(run_mysql "$sql" "$@" 2>&1)
    status=$?
    set -e

    if [ "$status" -eq 0 ]; then
        fail "$label: expected error containing [$expected], got success [$output]"
    fi

    case "$output" in
        *"$expected"*) ;;
        *) fail "$label: expected error containing [$expected], got [$output]" ;;
    esac
}

expect_checksum_digits() {
    label=$1
    sql=$2

    output=$(run_mysql "$sql")
    table_name=$(printf '%s\n' "$output" | sed -n '1s/	.*//p')
    checksum=$(printf '%s\n' "$output" | sed -n '1s/.*	//p')
    status=$(printf '%s\n' "$output" | sed -n '2p')

    if [ "$table_name" != "${DATABASE}.a" ]; then
        fail "$label: expected table [${DATABASE}.a], got [$table_name]"
    fi
    case "$checksum" in
        ''|*[!0-9]*) fail "$label: expected numeric checksum, got [$checksum]" ;;
        *) ;;
    esac
    if [ "$status" != "-1	0	0" ]; then
        fail "$label: expected status [-1	0	0], got [$status]"
    fi
}

expect_metadata_contains() {
    label=$1
    text=$2
    expected=$3

    case "$text" in
        *"$expected"*) ;;
        *) fail "$label: expected metadata containing [$expected], got [$text]" ;;
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
    "CREATE DATABASE ${DATABASE}; "\
"CREATE TABLE ${DATABASE}.a (id INT PRIMARY KEY, v VARCHAR(20)) ENGINE=InnoDB; "\
"CREATE TABLE ${DATABASE}.empty_t (id INT PRIMARY KEY) ENGINE=InnoDB; "\
"INSERT INTO ${DATABASE}.a VALUES (1, 'one'), (2, 'two'); "\
"CREATE VIEW ${DATABASE}.v AS SELECT id FROM ${DATABASE}.a;" \
    >/dev/null

expect_checksum_digits \
    "basic checksum result row" \
    "CHECKSUM TABLE ${DATABASE}.a; SELECT ROW_COUNT(), @@warning_count, @@error_count;"

metadata=$(run_mysql_type_info "CHECKSUM TABLE ${DATABASE}.a;")
table_metadata=$(printf '%s\n' "$metadata" | sed -n '/Field   1:/,/Field   2:/p')
checksum_metadata=$(printf '%s\n' "$metadata" | sed -n '/Field   2:/,/+---/p')

expect_metadata_contains "table metadata type" "$table_metadata" 'Type:       VAR_STRING'
expect_metadata_contains "table metadata collation" "$table_metadata" \
    'Collation:  latin1_swedish_ci (8)'
expect_metadata_contains "table metadata length" "$table_metadata" 'Length:     384'
expect_metadata_contains "table metadata decimals" "$table_metadata" 'Decimals:   31'
case "$table_metadata" in
    *'Flags:      NOT_NULL'*|*'Flags:      BINARY'*|*'Flags:      NUM'*)
        fail "table metadata flags: expected no flags, got [$table_metadata]"
        ;;
esac

expect_metadata_contains "checksum metadata type" "$checksum_metadata" 'Type:       LONGLONG'
expect_metadata_contains "checksum metadata collation" "$checksum_metadata" 'Collation:  binary (63)'
expect_metadata_contains "checksum metadata length" "$checksum_metadata" 'Length:     22'
expect_metadata_contains "checksum metadata decimals" "$checksum_metadata" 'Decimals:   0'
expect_metadata_contains "checksum metadata flags" "$checksum_metadata" 'Flags:      BINARY NUM'

expect_checksum_digits \
    "extended checksum result row" \
    "CHECKSUM TABLE ${DATABASE}.a EXTENDED; SELECT ROW_COUNT(), @@warning_count, @@error_count;"

expect_output \
    "quick checksum nullable result" \
    "${DATABASE}.a	NULL
-1	0	0" \
    "CHECKSUM TABLE ${DATABASE}.a QUICK; SELECT ROW_COUNT(), @@warning_count, @@error_count;"

expect_output \
    "empty table checksum" \
    "${DATABASE}.empty_t	0
-1	0	0" \
    "CHECKSUM TABLE ${DATABASE}.empty_t; SELECT ROW_COUNT(), @@warning_count, @@error_count;"

multi_output=$(run_mysql "CHECKSUM TABLE ${DATABASE}.empty_t, ${DATABASE}.a;")
first_line=$(printf '%s\n' "$multi_output" | sed -n '1p')
second_table=$(printf '%s\n' "$multi_output" | sed -n '2s/	.*//p')
second_checksum=$(printf '%s\n' "$multi_output" | sed -n '2s/.*	//p')
if [ "$first_line" != "${DATABASE}.empty_t	0" ]; then
    fail "multiple tables: expected first line [${DATABASE}.empty_t	0], got [$first_line]"
fi
if [ "$second_table" != "${DATABASE}.a" ]; then
    fail "multiple tables: expected second table [${DATABASE}.a], got [$second_table]"
fi
case "$second_checksum" in
    ''|*[!0-9]*) fail "multiple tables: expected numeric second checksum, got [$second_checksum]" ;;
    *) ;;
esac

expect_error \
    "missing default schema for unqualified table" \
    "ERROR 1046 (3D000)" \
    "CHECKSUM TABLE a;"

expect_error \
    "duplicate table aliases are rejected" \
    "ERROR 1066 (42000)" \
    "USE ${DATABASE}; CHECKSUM TABLE a, a;"

expect_output \
    "unknown table returns row and error warning" \
    "${DATABASE}.missing	NULL
Error	1146	Table '${DATABASE}.missing' doesn't exist
-1	1	1" \
    "CHECKSUM TABLE ${DATABASE}.missing; SHOW WARNINGS; "\
"SELECT ROW_COUNT(), @@warning_count, @@error_count;"

expect_output \
    "unknown schema returns row and error warning" \
    "missing_schema.t	NULL
Error	1049	Unknown database 'missing_schema'
-1	1	1" \
    "CHECKSUM TABLE missing_schema.t; SHOW WARNINGS; "\
"SELECT ROW_COUNT(), @@warning_count, @@error_count;"

expect_output \
    "view target returns row and not base table warning" \
    "${DATABASE}.v	NULL
Error	1347	'${DATABASE}.v' is not BASE TABLE
-1	1	1" \
    "CHECKSUM TABLE ${DATABASE}.v; SHOW WARNINGS; "\
"SELECT ROW_COUNT(), @@warning_count, @@error_count;"

expect_error \
    "local modifier is rejected" \
    "ERROR 1064 (42000)" \
    "CHECKSUM LOCAL TABLE ${DATABASE}.a;"

expect_error \
    "unsupported checksum option is rejected" \
    "ERROR 1064 (42000)" \
    "CHECKSUM TABLE ${DATABASE}.a FAST;"

expect_error \
    "multiple checksum options are rejected" \
    "ERROR 1064 (42000)" \
    "CHECKSUM TABLE ${DATABASE}.a QUICK EXTENDED;"

expect_error \
    "option before later target is rejected" \
    "ERROR 1064 (42000)" \
    "CHECKSUM TABLE ${DATABASE}.a QUICK, ${DATABASE}.empty_t;"

cleanup

printf '%s\n' "mysql_baseline_checksum_table_expectations: ok"
