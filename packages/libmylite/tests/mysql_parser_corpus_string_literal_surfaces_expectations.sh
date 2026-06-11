#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
MYSQL_BIN="${MYLITE_MYSQL_BIN:-mysql}"
MYSQL_SOCKET="${MYLITE_MYSQL_SOCKET:-}"
DATABASE="mylite_parser_string_literals_$$"

fail() {
    printf '%s\n' "mysql_parser_corpus_string_literal_surfaces_expectations: $1" >&2
    exit 1
}

run_mysql() {
    sql=$1
    shift
    if [ -n "$MYSQL_SOCKET" ]; then
        printf '%s\n' "$sql" \
            | "$MYSQL_BIN" --protocol=SOCKET --socket="$MYSQL_SOCKET" -uroot \
                --batch --raw "$@"
    else
        printf '%s\n' "$sql" \
            | docker exec -i "$MYSQL_CONTAINER" mysql --protocol=TCP -h127.0.0.1 -uroot \
                --batch --raw "$@"
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
    run_mysql "DROP DATABASE IF EXISTS ${DATABASE};" --skip-column-names >/dev/null 2>&1 || true
}

trap cleanup EXIT

version=$(run_mysql "SELECT VERSION();" --skip-column-names)
case "$version" in
    8.4.9*) ;;
    *) fail "expected MySQL 8.4.9 runtime, got [$version]" ;;
esac

cleanup

expect_output \
    "adjacent projection label and value" \
    "$(printf '%b' 'a\nab')" \
    "SELECT 'a' 'b';"

expect_output \
    "adjacent and national insert values" \
    "$(printf '%b' '61626364\tabcd\n7879\txy')" \
    "CREATE DATABASE ${DATABASE};
USE ${DATABASE};
CREATE TABLE t (v VARCHAR(100));
INSERT INTO t VALUES ('ab' 'cd'), (N'xy');
SELECT HEX(v), v FROM t ORDER BY v;
DROP DATABASE ${DATABASE};" \
    --skip-column-names

expect_output \
    "national string escapes" \
    "$(printf '%b' '5C\tCote d'\''Ivoire')" \
    "SELECT HEX(N'\\\\'), N'Cote d\\'Ivoire';" \
    --skip-column-names

expect_error \
    "mixed adjacent ordinary and national string" \
    1064 \
    42000 \
    "near 'N'b''" \
    "SELECT 'a' N'b';"

printf '%s\n' "mysql_parser_corpus_string_literal_surfaces_expectations: ok"
