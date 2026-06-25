#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_validate_password_strength_expectations_$$"

fail() {
    printf '%s\n' "mysql_baseline_validate_password_strength_function_expectations: $1" >&2
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
0	0	NULL	0	0	0	0	1	1
EXPECTED
)
expect_output \
    "scalar validate_password_strength values" \
    "$scalar_expected" \
    "SELECT VALIDATE_PASSWORD_STRENGTH('abc'), VALIDATE_PASSWORD_STRENGTH(''), "\
"VALIDATE_PASSWORD_STRENGTH(NULL), VALIDATE_PASSWORD_STRENGTH(123), "\
"VALIDATE_PASSWORD_STRENGTH(TRUE), VALIDATE_PASSWORD_STRENGTH(FALSE), "\
"VALIDATE_PASSWORD_STRENGTH(_binary'abc'), VALIDATE_PASSWORD_STRENGTH('abc') = 0, "\
"VALIDATE_PASSWORD_STRENGTH('abc') + 1;" \
    "$DATABASE"

expect_output \
    "dual validate_password_strength values" \
    "0	NULL" \
    "SELECT VALIDATE_PASSWORD_STRENGTH('abc'), VALIDATE_PASSWORD_STRENGTH(NULL) FROM DUAL;" \
    "$DATABASE"

expect_output \
    "do validate_password_strength status" \
    "0	0" \
    "DO VALIDATE_PASSWORD_STRENGTH('abc'), VALIDATE_PASSWORD_STRENGTH(NULL); "\
"SELECT ROW_COUNT(), @@warning_count;" \
    "$DATABASE"

dml_expected=$(cat <<\EXPECTED
0	0
1	0
1	NULL
EXPECTED
)
expect_output \
    "assignment and dml validate_password_strength values" \
    "$dml_expected" \
    "SET @strength = VALIDATE_PASSWORD_STRENGTH('abc'); "\
"SELECT @strength, ROW_COUNT(); "\
"CREATE TABLE values_t(id INT, strength INT); "\
"INSERT INTO values_t VALUES (1, VALIDATE_PASSWORD_STRENGTH('abc')); "\
"SELECT id, strength FROM values_t; "\
"UPDATE values_t SET strength = VALIDATE_PASSWORD_STRENGTH(NULL) WHERE id = 1; "\
"SELECT id, strength FROM values_t;" \
    "$DATABASE"

run_mysql \
    "CREATE TABLE vp(id INT, p VARCHAR(20)); "\
"INSERT INTO vp VALUES (1,'abc'),(2,NULL),(3,'N0Tweak');" \
    "$DATABASE" >/dev/null

projection_expected=$(cat <<\EXPECTED
1	0	NULL
2	NULL	NULL
3	0	NULL
EXPECTED
)
expect_output \
    "row validate_password_strength values" \
    "$projection_expected" \
    "SELECT id, VALIDATE_PASSWORD_STRENGTH(p), VALIDATE_PASSWORD_STRENGTH(NULL) "\
"FROM vp ORDER BY id;" \
    "$DATABASE"

predicate_expected=$(cat <<\EXPECTED
1
3
EXPECTED
)
expect_output \
    "row validate_password_strength predicate" \
    "$predicate_expected" \
    "SELECT id FROM vp WHERE VALIDATE_PASSWORD_STRENGTH(p) = 0 ORDER BY id;" \
    "$DATABASE"

metadata_output=$(run_mysql_type_info \
    "USE ${DATABASE}; SET NAMES utf8mb4; "\
"SELECT VALIDATE_PASSWORD_STRENGTH('abc') AS strength, "\
"VALIDATE_PASSWORD_STRENGTH(NULL) AS n;" \
    "$DATABASE")

expect_contains "strength metadata field" 'Field   1:  `strength`' "$metadata_output"
expect_contains "strength metadata type" 'Type:       LONGLONG' "$metadata_output"
expect_contains "strength metadata collation" 'Collation:  binary (63)' "$metadata_output"
expect_contains "strength metadata length" 'Length:     10' "$metadata_output"
expect_contains "strength metadata decimals" 'Decimals:   0' "$metadata_output"
expect_contains "strength metadata flags" 'Flags:      BINARY NUM' "$metadata_output"
expect_contains "null metadata field" 'Field   2:  `n`' "$metadata_output"
expect_contains "null metadata type" 'Type:       LONGLONG' "$metadata_output"
expect_contains "null metadata max length" 'Max_length: 0' "$metadata_output"

expect_error \
    "validate_password_strength rejects zero arguments" \
    1582 \
    42000 \
    "Incorrect parameter count in the call to native function 'VALIDATE_PASSWORD_STRENGTH'" \
    "SELECT VALIDATE_PASSWORD_STRENGTH();" \
    "$DATABASE"

expect_error \
    "validate_password_strength rejects extra arguments" \
    1582 \
    42000 \
    "Incorrect parameter count in the call to native function 'VALIDATE_PASSWORD_STRENGTH'" \
    "SELECT VALIDATE_PASSWORD_STRENGTH('a','b');" \
    "$DATABASE"

printf '%s\n' "mysql_baseline_validate_password_strength_function_expectations: ok"
