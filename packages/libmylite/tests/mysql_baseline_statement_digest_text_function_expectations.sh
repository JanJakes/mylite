#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_statement_digest_text_expectations_$$"

fail() {
    printf '%s\n' "mysql_baseline_statement_digest_text_function_expectations: $1" >&2
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

scalar_expected=$(cat <<\EXPECTED
SELECT ?	SELECT ? ;	SELECT ? FROM DUAL	SELECT * FROM `t` WHERE `id` IN (...)	SELECT `a` + ? , `b` >= ? , `c` != ? , `d` <=> ? FROM `t` WHERE `e` LIKE ? OR `f` IS NOT NULL	NULL	utf8mb4	utf8mb4_0900_ai_ci	4
EXPECTED
)
expect_output \
    "statement_digest_text scalar normalization" \
    "$scalar_expected" \
    "SELECT STATEMENT_DIGEST_TEXT('select 1'), "\
"STATEMENT_DIGEST_TEXT('select 1;'), "\
"STATEMENT_DIGEST_TEXT('select /*x*/ 1 from dual'), "\
"STATEMENT_DIGEST_TEXT('select * from t where id in (1,2,3)'), "\
"STATEMENT_DIGEST_TEXT('select a+1,b>=2,c<>3,d<=>null "\
"from t where e like ''x%'' or f is not null'), "\
"STATEMENT_DIGEST_TEXT(NULL), "\
"CHARSET(STATEMENT_DIGEST_TEXT('select 1')), "\
"COLLATION(STATEMENT_DIGEST_TEXT('select 1')), "\
"COERCIBILITY(STATEMENT_DIGEST_TEXT('select 1'));" \
    "$DATABASE"

row_expected=$(cat <<\EXPECTED
1	SELECT ?
2	SELECT * FROM `t` WHERE `id` IN (...)
3	NULL
EXPECTED
)
expect_output \
    "statement_digest_text row projection" \
    "$row_expected" \
    "CREATE TABLE digest_probe(id INT, sql_text TEXT); "\
"INSERT INTO digest_probe VALUES "\
"(1, 'select 1'), "\
"(2, 'select * from t where id in (1,2,3)'), "\
"(3, NULL); "\
"SELECT id, STATEMENT_DIGEST_TEXT(sql_text) FROM digest_probe ORDER BY id;" \
    "$DATABASE"

expect_output \
    "statement_digest_text do status" \
    "0	0" \
    "DO STATEMENT_DIGEST_TEXT('select 1'); SELECT ROW_COUNT(), @@warning_count;" \
    "$DATABASE"

metadata_output=$(run_mysql_type_info \
    "USE ${DATABASE}; SET NAMES utf8mb4; "\
"SELECT STATEMENT_DIGEST_TEXT('select 1') AS digest_text;" \
    "$DATABASE")

expect_contains "statement_digest_text metadata field" 'Field   1:  `digest_text`' \
    "$metadata_output"
expect_contains "statement_digest_text metadata type" 'Type:       LONG_BLOB' "$metadata_output"
expect_contains "statement_digest_text metadata collation" \
    'Collation:  utf8mb4_0900_ai_ci (255)' "$metadata_output"
expect_contains "statement_digest_text metadata length" 'Length:     268435456' "$metadata_output"
expect_contains "statement_digest_text metadata decimals" 'Decimals:   31' "$metadata_output"
expect_contains "statement_digest_text metadata flags" 'Flags:      ' "$metadata_output"

expect_error \
    "statement_digest_text rejects zero arguments" \
    1582 \
    42000 \
    "Incorrect parameter count in the call to native function 'STATEMENT_DIGEST_TEXT'" \
    "SELECT STATEMENT_DIGEST_TEXT();" \
    "$DATABASE"

expect_error \
    "statement_digest_text rejects two arguments" \
    1582 \
    42000 \
    "Incorrect parameter count in the call to native function 'STATEMENT_DIGEST_TEXT'" \
    "SELECT STATEMENT_DIGEST_TEXT('a','b');" \
    "$DATABASE"

expect_error \
    "statement_digest_text charset wrapper preserves arity diagnostics" \
    1582 \
    42000 \
    "Incorrect parameter count in the call to native function 'STATEMENT_DIGEST_TEXT'" \
    "SELECT CHARSET(STATEMENT_DIGEST_TEXT());" \
    "$DATABASE"

expect_error \
    "statement_digest_text rejects invalid argument SQL" \
    3676 \
    HY000 \
    "Could not parse argument to digest function" \
    "SELECT STATEMENT_DIGEST_TEXT('select from');" \
    "$DATABASE"

expect_error \
    "statement_digest_text rejects parameter marker argument SQL" \
    3676 \
    HY000 \
    "Could not parse argument to digest function" \
    "SELECT STATEMENT_DIGEST_TEXT('select ?');" \
    "$DATABASE"

expect_error \
    "statement_digest_text rejects multi statement argument SQL" \
    3676 \
    HY000 \
    "Could not parse argument to digest function" \
    "SELECT STATEMENT_DIGEST_TEXT('select 1; select 2');" \
    "$DATABASE"

printf '%s\n' "mysql_baseline_statement_digest_text_function_expectations: ok"
