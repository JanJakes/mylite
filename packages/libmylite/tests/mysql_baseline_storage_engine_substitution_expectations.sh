#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_engine_substitution_$$"

fail() {
    printf '%s\n' "mysql_baseline_storage_engine_substitution_expectations: $1" >&2
    exit 1
}

run_mysql() {
    sql=$1
    shift
    printf '%s\n' "$sql" \
        | docker exec -i "$MYSQL_CONTAINER" mysql --protocol=TCP -h127.0.0.1 -uroot --batch --raw --skip-column-names "$@"
}

run_mysql_with_headers() {
    sql=$1
    shift
    printf '%s\n' "$sql" \
        | docker exec -i "$MYSQL_CONTAINER" mysql --protocol=TCP -h127.0.0.1 -uroot --batch --raw "$@"
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

expect_contains() {
    label=$1
    haystack=$2
    needle=$3

    case "$haystack" in
        *"$needle"*) ;;
        *) fail "$label: expected output containing [$needle], got [$haystack]" ;;
    esac
}

expect_show_create() {
    show_label=$1
    table=$2
    expected_create=$3

    output=$(run_mysql_with_headers "USE ${DATABASE}; SHOW CREATE TABLE ${table};")
    headers=$(printf '%s\n' "$output" | sed -n '1p')
    table_name=$(printf '%s\n' "$output" | sed -n '2p' | cut -f 1)
    create_text=$(printf '%s\n' "$output" | sed -n '2,$p' | cut -f 2-)

    expect_value "$show_label headers" "Table	Create Table" "$headers"
    expect_value "$show_label table" "$table" "$table_name"
    expect_value "$show_label create" "$expected_create" "$create_text"
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
run_mysql "CREATE DATABASE ${DATABASE}; USE ${DATABASE};" >/dev/null

default_mode=$(run_mysql "SELECT @@sql_mode;")
case "$default_mode" in
    *NO_ENGINE_SUBSTITUTION*) ;;
    *) fail "expected default SQL mode to include NO_ENGINE_SUBSTITUTION, got [$default_mode]" ;;
esac

expect_error \
    "strict unknown engine" \
    1286 \
    42000 \
    "Unknown storage engine 'NoSuchEngine'" \
    "USE ${DATABASE}; SET SESSION sql_mode = DEFAULT; CREATE TABLE strict_unknown(id INT) ENGINE=NoSuchEngine;"

loose_warnings=$(run_mysql \
    "USE ${DATABASE};
     SET SESSION sql_mode = '';
     CREATE TABLE unknown_loose(id INT) ENGINE=NoSuchEngine;
     SHOW WARNINGS;
     SHOW COUNT(*) WARNINGS;
     SELECT @@warning_count, ROW_COUNT();")
expect_value \
    "loose unknown engine warnings" \
    "Warning	1286	Unknown storage engine 'NoSuchEngine'
Warning	1266	Using storage engine InnoDB for table 'unknown_loose'
2
2	-1" \
    "$loose_warnings"

expected_unknown_create="CREATE TABLE \`unknown_loose\` (
  \`id\` int DEFAULT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci"
expect_show_create "loose unknown engine show create" "unknown_loose" "$expected_unknown_create"

rows=$(run_mysql "USE ${DATABASE}; INSERT INTO unknown_loose VALUES (7); SELECT id FROM unknown_loose;")
expect_value "loose substituted table stores rows" "7" "$rows"

empty_warnings=$(run_mysql \
    "USE ${DATABASE};
     SET SESSION sql_mode = '';
     CREATE TABLE empty_loose(id INT) ENGINE='';
     SHOW WARNINGS;
     SHOW COUNT(*) WARNINGS;")
expect_value \
    "loose empty engine warnings" \
    "Warning	1286	Unknown storage engine ''
Warning	1266	Using storage engine InnoDB for table 'empty_loose'
2" \
    "$empty_warnings"

if_exists_warnings=$(run_mysql \
    "USE ${DATABASE};
     SET SESSION sql_mode = '';
     CREATE TABLE IF NOT EXISTS unknown_loose(id INT) ENGINE=NoSuchEngine;
     SHOW WARNINGS;
     SHOW COUNT(*) WARNINGS;")
expect_value \
    "if not exists warning order" \
    "Warning	1286	Unknown storage engine 'NoSuchEngine'
Warning	1266	Using storage engine InnoDB for table 'unknown_loose'
Note	1050	Table 'unknown_loose' already exists
3" \
    "$if_exists_warnings"

temp_output=$(run_mysql \
    "USE ${DATABASE};
     SET SESSION sql_mode = '';
     CREATE TEMPORARY TABLE temp_unknown(id INT) ENGINE=NoSuchEngine;
     SHOW WARNINGS;
     SHOW CREATE TABLE temp_unknown;")
expect_contains \
    "temporary unknown engine first warning" \
    "$temp_output" \
    "Warning	1286	Unknown storage engine 'NoSuchEngine'"
expect_contains \
    "temporary unknown engine substitution warning" \
    "$temp_output" \
    "Warning	1266	Using storage engine InnoDB for table 'temp_unknown'"
expect_contains \
    "temporary unknown engine show create" \
    "$temp_output" \
    "CREATE TEMPORARY TABLE \`temp_unknown\` ("
expect_contains "temporary unknown engine suffix" "$temp_output" "ENGINE=InnoDB"

expect_error \
    "strict temporary unknown engine" \
    1286 \
    42000 \
    "Unknown storage engine 'NoSuchEngine'" \
    "USE ${DATABASE}; SET SESSION sql_mode = 'NO_ENGINE_SUBSTITUTION'; CREATE TEMPORARY TABLE temp_strict(id INT) ENGINE=NoSuchEngine;"

myisam_reference=$(run_mysql \
    "USE ${DATABASE};
     SET SESSION sql_mode = DEFAULT;
     CREATE TABLE myisam_reference(id INT) ENGINE=MyISAM;
     SHOW WARNINGS;
     SHOW CREATE TABLE myisam_reference;")
expect_contains \
    "reference runtime supports real MyISAM" \
    "$myisam_reference" \
    "ENGINE=MyISAM"

memory_reference=$(run_mysql \
    "USE ${DATABASE};
     SET SESSION sql_mode = DEFAULT;
     CREATE TABLE memory_reference(id INT) ENGINE=MEMORY;
     SHOW WARNINGS;
     SHOW CREATE TABLE memory_reference;")
expect_contains \
    "reference runtime supports real MEMORY" \
    "$memory_reference" \
    "ENGINE=MEMORY"
