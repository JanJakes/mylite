#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_roles_graphml_expectations_$$"

fail() {
    printf '%s\n' "mysql_baseline_roles_graphml_function_expectations: $1" >&2
    exit 1
}

run_mysql() {
    sql=$1
    shift
    printf '%s\n' "$sql" \
        | docker exec -i "$MYSQL_CONTAINER" mysql --protocol=TCP -h127.0.0.1 -uroot \
            --batch --raw --binary-as-hex=1 --skip-column-names \
            --default-character-set=utf8mb4 "$@"
}

run_mysql_type_info() {
    sql=$1
    shift
    printf '%s\n' "$sql" \
        | docker exec -i "$MYSQL_CONTAINER" mysql --protocol=TCP -h127.0.0.1 -uroot \
            --default-character-set=utf8mb4 --column-type-info -vvv "$@"
}

expect_value() {
    label=$1
    expected=$2
    actual=$3

    if [ "$actual" != "$expected" ]; then
        fail "$label: expected [$expected], got [$actual]"
    fi
}

expect_output() {
    label=$1
    expected=$2
    sql=$3
    shift 3

    output=$(run_mysql "$sql" "$@")
    expect_value "$label" "$expected" "$output"
}

expect_contains() {
    label=$1
    needle=$2
    haystack=$3

    case "$haystack" in
        *"$needle"*) ;;
        *) fail "$label: expected output to contain [$needle]" ;;
    esac
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
        *) fail "$label: expected error $code/$state containing [$message], got [$output]" ;;
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
run_mysql \
    "CREATE DATABASE ${DATABASE} CHARACTER SET utf8mb4 COLLATE utf8mb4_0900_ai_ci; "\
"USE ${DATABASE}; SET NAMES utf8mb4; SET SESSION sql_mode = 'NO_ENGINE_SUBSTITUTION';" \
    >/dev/null

shape_expected=$(cat <<\EXPECTED
0	1	1	1	utf8mb3	utf8mb3_general_ci	3	1
EXPECTED
)
expect_output \
    "roles_graphml scalar shape" \
    "$shape_expected" \
    "SELECT ROLES_GRAPHML() IS NULL, "\
"ROLES_GRAPHML() LIKE '<?xml version=\"1.0\" encoding=\"UTF-8\"?>%', "\
"ROLES_GRAPHML() LIKE '%<graphml xmlns=\"http://graphml.graphdrawing.org/xmlns\"%', "\
"ROLES_GRAPHML() LIKE '%<graph id=\"G\" edgedefault=\"directed\"%', "\
"CHARSET(ROLES_GRAPHML()), COLLATION(ROLES_GRAPHML()), COERCIBILITY(ROLES_GRAPHML()), "\
"LENGTH(ROLES_GRAPHML()) > 0;" \
    "$DATABASE"

expect_output \
    "roles_graphml do status" \
    "0	0" \
    "DO ROLES_GRAPHML(); SELECT ROW_COUNT(), @@warning_count;" \
    "$DATABASE"

expect_output \
    "roles_graphml row projection" \
    "2	1" \
    "CREATE TABLE roles_probe(id INT); INSERT INTO roles_probe VALUES (1),(2); "\
"SELECT COUNT(*), MIN(ROLES_GRAPHML() LIKE '%<graphml%') FROM roles_probe;" \
    "$DATABASE"

metadata_output=$(run_mysql_type_info \
    "USE ${DATABASE}; SET NAMES utf8mb4; SELECT ROLES_GRAPHML() AS roles_graphml;" \
    "$DATABASE")

expect_contains "roles_graphml metadata field" 'Field   1:  `roles_graphml`' "$metadata_output"
expect_contains "roles_graphml metadata type" 'Type:       LONG_BLOB' "$metadata_output"
expect_contains "roles_graphml metadata collation" 'Collation:  utf8mb4_0900_ai_ci (255)' \
    "$metadata_output"
expect_contains "roles_graphml metadata length" 'Length:     201326592' "$metadata_output"
expect_contains "roles_graphml metadata decimals" 'Decimals:   31' "$metadata_output"
expect_contains "roles_graphml metadata flags" 'Flags:      ' "$metadata_output"

expect_error \
    "roles_graphml rejects one argument" \
    1582 \
    42000 \
    "Incorrect parameter count in the call to native function 'ROLES_GRAPHML'" \
    "SELECT ROLES_GRAPHML(NULL);" \
    "$DATABASE"

expect_error \
    "roles_graphml rejects two arguments" \
    1582 \
    42000 \
    "Incorrect parameter count in the call to native function 'ROLES_GRAPHML'" \
    "SELECT ROLES_GRAPHML('a','b');" \
    "$DATABASE"

expect_error \
    "roles_graphml charset wrapper preserves arity diagnostics" \
    1582 \
    42000 \
    "Incorrect parameter count in the call to native function 'ROLES_GRAPHML'" \
    "SELECT CHARSET(ROLES_GRAPHML(NULL));" \
    "$DATABASE"

expect_error \
    "roles_graphml coercibility wrapper preserves arity diagnostics" \
    1582 \
    42000 \
    "Incorrect parameter count in the call to native function 'ROLES_GRAPHML'" \
    "SELECT COERCIBILITY(ROLES_GRAPHML(1));" \
    "$DATABASE"

printf '%s\n' "mysql_baseline_roles_graphml_function_expectations: ok"
