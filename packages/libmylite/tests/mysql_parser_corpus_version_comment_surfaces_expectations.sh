#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
MYSQL_BIN="${MYLITE_MYSQL_BIN:-mysql}"
MYSQL_SOCKET="${MYLITE_MYSQL_SOCKET:-}"
DATABASE="mylite_parser_version_comments_$$"

fail() {
    printf '%s\n' "mysql_parser_corpus_version_comment_surfaces_expectations: $1" >&2
    exit 1
}

run_mysql() {
    sql=$1
    shift
    if [ -n "$MYSQL_SOCKET" ]; then
        printf '%s\n' "$sql" \
            | "$MYSQL_BIN" --protocol=SOCKET --socket="$MYSQL_SOCKET" -uroot \
                --batch --raw --skip-column-names "$@"
    else
        printf '%s\n' "$sql" \
            | docker exec -i "$MYSQL_CONTAINER" mysql --protocol=TCP -h127.0.0.1 -uroot \
                --batch --raw --skip-column-names "$@"
    fi
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

expect_output \
    "lower version gate executes" \
    "2" \
    "SELECT 1 /*!80000 + 1 */;"

expect_output \
    "equal version gate executes" \
    "2" \
    "SELECT 1 /*!80409 + 1 */;"

expect_output \
    "higher version gate skips" \
    "1" \
    "SELECT 1 /*!80410 + 1 */;"

expect_output \
    "no version gate executes" \
    "9" \
    "SELECT /*! 9 */;"

expect_output \
    "higher version comment spans inner block comment" \
    "1" \
    "SELECT 1 /*!99999 /* */ */;"

expect_output \
    "lower version comment spans inner block comment" \
    "2" \
    "SELECT 2 /*!12345 /* */ */;"

expect_error \
    "skipped payload can leave invalid select list" \
    1064 \
    42000 \
    "near 'AS skipped_payload_value'" \
    "SELECT /*!99999 9 */ AS skipped_payload_value;"

expect_error \
    "ordinary block comments remain non-nesting" \
    1064 \
    42000 \
    "near '/ AS ordinary_nested'" \
    "SELECT 1 /* outer /* inner */ */ AS ordinary_nested;"

expect_output \
    "version comment closes CTE body" \
    "$(printf '%b' '0\t0')" \
    "WITH cte AS (SELECT 0 /*! ) */ SELECT * FROM cte a, cte b;"

expect_output \
    "version comment opens CTE body" \
    "$(printf '%b' '0\t0')" \
    "WITH cte AS /*! ( */ SELECT 0) SELECT * FROM cte a, cte b;"

expect_output \
    "version comment contributes foreign key reference clauses" \
    "SET NULL" \
    "CREATE DATABASE ${DATABASE};
USE ${DATABASE};
CREATE TABLE parent(id INT PRIMARY KEY);
CREATE TABLE child(fk INT);
ALTER TABLE child ADD CONSTRAINT c2 FOREIGN KEY (fk) REFERENCES parent /*! (id) */ \
/*!40008 ON DELETE SET NULL */;
SELECT DELETE_RULE
FROM information_schema.REFERENTIAL_CONSTRAINTS
WHERE CONSTRAINT_SCHEMA = DATABASE()
  AND TABLE_NAME = 'child'
  AND CONSTRAINT_NAME = 'c2';
DROP DATABASE ${DATABASE};"

printf '%s\n' "mysql_parser_corpus_version_comment_surfaces_expectations: ok"
