#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_stored_procedure_select_call_$$"
PROCEDURE_NAME="test_mysqli_flush_sync_procedure_$$"

fail() {
    printf '%s\n' "mysql_baseline_stored_procedure_select_call_expectations: $1" >&2
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

expect_contains() {
    label=$1
    actual=$2
    expected=$3

    case "$actual" in
        *"$expected"*) ;;
        *) fail "$label: expected output to contain [$expected], got [$actual]" ;;
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
     CREATE TABLE posts (ID INT PRIMARY KEY);
     INSERT INTO posts VALUES (42);" \
    >/dev/null

run_mysql \
    "USE ${DATABASE};
     DELIMITER //
     CREATE PROCEDURE ${PROCEDURE_NAME}() BEGIN SELECT ID FROM posts LIMIT 1; END//
     DELIMITER ;" \
    >/dev/null

show_create=$(run_mysql "USE ${DATABASE}; SHOW CREATE PROCEDURE ${PROCEDURE_NAME};")
expect_contains "show create procedure name" "$show_create" "$PROCEDURE_NAME"
expect_contains \
    "show create procedure ddl" \
    "$show_create" \
    "CREATE DEFINER=\`root\`@\`%\` PROCEDURE \`${PROCEDURE_NAME}\`()"
expect_contains "show create procedure body" "$show_create" "BEGIN SELECT ID FROM posts LIMIT 1; END"
expect_contains "show create character set" "$show_create" "utf8mb4"
expect_contains "show create collation" "$show_create" "utf8mb4_0900_ai_ci"

call_rows=$(run_mysql "USE ${DATABASE}; CALL ${PROCEDURE_NAME}; SELECT ROW_COUNT();")
expect_value "call and row_count" "42
0" "$call_rows"

call_parens=$(run_mysql "USE ${DATABASE}; CALL ${PROCEDURE_NAME}();")
expect_value "call with parentheses" "42" "$call_parens"

expect_error \
    "duplicate create procedure" \
    1304 \
    42000 \
    "PROCEDURE ${PROCEDURE_NAME} already exists" \
    "USE ${DATABASE};
     DELIMITER //
     CREATE PROCEDURE ${PROCEDURE_NAME}() BEGIN SELECT ID FROM posts LIMIT 1; END//
     DELIMITER ;"

run_mysql "USE ${DATABASE}; DROP PROCEDURE IF EXISTS ${PROCEDURE_NAME};" >/dev/null

expect_error \
    "show create missing procedure" \
    1305 \
    42000 \
    "PROCEDURE ${PROCEDURE_NAME} does not exist" \
    "USE ${DATABASE}; SHOW CREATE PROCEDURE ${PROCEDURE_NAME};"

expect_error \
    "call missing procedure" \
    1305 \
    42000 \
    "PROCEDURE ${DATABASE}.${PROCEDURE_NAME} does not exist" \
    "USE ${DATABASE}; CALL ${PROCEDURE_NAME};"

missing_drop=$(run_mysql \
    "USE ${DATABASE};
     DROP PROCEDURE IF EXISTS ${PROCEDURE_NAME};
     SELECT @@warning_count;")
expect_value "drop missing procedure warning count" "1" "$missing_drop"

missing_drop_warnings=$(run_mysql \
    "USE ${DATABASE};
     DROP PROCEDURE IF EXISTS ${PROCEDURE_NAME};
     SHOW WARNINGS;")
expect_contains \
    "drop missing procedure note" \
    "$missing_drop_warnings" \
    "Note	1305	PROCEDURE ${DATABASE}.${PROCEDURE_NAME} does not exist"

printf '%s\n' "mysql_baseline_stored_procedure_select_call_expectations: ok"
