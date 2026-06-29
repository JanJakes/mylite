#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_statement_digest_expectations_$$"

fail() {
    printf '%s\n' "mysql_baseline_statement_digest_function_expectations: $1" >&2
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

null_expected=$(cat <<\EXPECTED
NULL	utf8mb4	utf8mb4_0900_ai_ci	4	utf8mb4	utf8mb4_0900_ai_ci	4
EXPECTED
)
expect_output \
    "statement_digest NULL and metadata wrappers" \
    "$null_expected" \
    "SELECT STATEMENT_DIGEST(NULL), "\
"CHARSET(STATEMENT_DIGEST(NULL)), "\
"COLLATION(STATEMENT_DIGEST(NULL)), "\
"COERCIBILITY(STATEMENT_DIGEST(NULL)), "\
"CHARSET(STATEMENT_DIGEST('select 1')), "\
"COLLATION(STATEMENT_DIGEST('select 1')), "\
"COERCIBILITY(STATEMENT_DIGEST('select 1'));" \
    "$DATABASE"

hash_boundary_expected=$(cat <<\EXPECTED
d1b44b0c19af710b5a679907e284acd2ddc285201794bc69a2389d77baedddae	SELECT ?	66cbb3a40d4bbd150b75825ad291a6545399f3098fc1079e4d8b5bb061a6a481
EXPECTED
)
expect_output \
    "statement_digest hash is not digest text sha2" \
    "$hash_boundary_expected" \
    "SELECT STATEMENT_DIGEST('select 1'), "\
"STATEMENT_DIGEST_TEXT('select 1'), "\
"SHA2(STATEMENT_DIGEST_TEXT('select 1'), 256);" \
    "$DATABASE"

expect_output \
    "statement_digest do NULL status" \
    "0	0" \
    "DO STATEMENT_DIGEST(NULL); SELECT ROW_COUNT(), @@warning_count;" \
    "$DATABASE"

metadata_output=$(run_mysql_type_info \
    "USE ${DATABASE}; SET NAMES utf8mb4; SELECT STATEMENT_DIGEST(NULL) AS digest;" \
    "$DATABASE")

expect_contains "statement_digest metadata field" 'Field   1:  `digest`' "$metadata_output"
expect_contains "statement_digest metadata type" 'Type:       VAR_STRING' "$metadata_output"
expect_contains "statement_digest metadata collation" \
    'Collation:  utf8mb4_0900_ai_ci (255)' "$metadata_output"
expect_contains "statement_digest metadata length" 'Length:     256' "$metadata_output"
expect_contains "statement_digest metadata decimals" 'Decimals:   31' "$metadata_output"
expect_contains "statement_digest metadata flags" 'Flags:      ' "$metadata_output"

expect_error \
    "statement_digest rejects zero arguments" \
    1582 \
    42000 \
    "Incorrect parameter count in the call to native function 'STATEMENT_DIGEST'" \
    "SELECT STATEMENT_DIGEST();" \
    "$DATABASE"

expect_error \
    "statement_digest rejects two arguments" \
    1582 \
    42000 \
    "Incorrect parameter count in the call to native function 'STATEMENT_DIGEST'" \
    "SELECT STATEMENT_DIGEST('a','b');" \
    "$DATABASE"

expect_error \
    "statement_digest charset wrapper preserves arity diagnostics" \
    1582 \
    42000 \
    "Incorrect parameter count in the call to native function 'STATEMENT_DIGEST'" \
    "SELECT CHARSET(STATEMENT_DIGEST());" \
    "$DATABASE"

printf '%s\n' "mysql_baseline_statement_digest_function_expectations: ok"
