#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
MYSQL_BIN="${MYLITE_MYSQL_BIN:-mysql}"
MYSQL_SOCKET="${MYLITE_MYSQL_SOCKET:-}"
DATABASE="mylite_parser_charset_collation_$$"

fail() {
    printf '%s\n' "mysql_parser_corpus_charset_collation_surfaces_expectations: $1" >&2
    exit 1
}

run_mysql() {
    sql=$1
    shift
    if [ -n "$MYSQL_SOCKET" ]; then
        printf '%s\n' "$sql" \
            | "$MYSQL_BIN" --protocol=SOCKET --socket="$MYSQL_SOCKET" -uroot \
                --batch --raw --skip-column-names --default-character-set=utf8mb4 "$@"
    else
        printf '%s\n' "$sql" \
            | docker exec -i "$MYSQL_CONTAINER" mysql --protocol=TCP -h127.0.0.1 -uroot \
                --batch --raw --skip-column-names --default-character-set=utf8mb4 "$@"
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

expect_success() {
    label=$1
    sql=$2
    shift 2

    if ! run_mysql "$sql" "$@" >/dev/null; then
        fail "$label: command failed"
    fi
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
run_mysql "CREATE DATABASE ${DATABASE};" >/dev/null

expect_success \
    "introduced dml and default forms accepted" \
    "USE ${DATABASE}; "\
"CREATE TABLE t (id INT PRIMARY KEY, word VARCHAR(32), c VARCHAR(255)); "\
"INSERT INTO t VALUES (1, _latin1'a' COLLATE latin1_bin, _utf8mb4 0x4142); "\
"UPDATE t SET word = _utf8mb4 0x4142 COLLATE utf8mb4_0900_ai_ci WHERE id = 1; "\
"CREATE TABLE defaults (a CHAR DEFAULT _latin1'a' COLLATE latin1_bin);" \
    "$DATABASE"

expect_success \
    "introduced predicate forms accepted" \
    "USE ${DATABASE}; "\
"CREATE TABLE p_latin (c VARCHAR(32) CHARACTER SET latin1); "\
"CREATE TABLE p_utf16 (c VARCHAR(255) CHARACTER SET utf16); "\
"SELECT * FROM t WHERE word = BINARY 0x4142; "\
"SELECT * FROM p_latin WHERE c > _latin1 'B' COLLATE latin1_bin; "\
"SELECT * FROM p_utf16 WHERE c LIKE _utf16 0x039C0025 COLLATE utf16_general_ci; "\
"SELECT * FROM p_latin WHERE c IN (_latin1'a', _latin1'b' COLLATE latin1_bin);" \
    "$DATABASE"

expect_success \
    "column shorthand and enum set hex bit literals accepted" \
    "USE ${DATABASE}; "\
"CREATE TABLE shorthand (a CHAR(8) ASCII, b VARCHAR(8) BINARY ASCII); "\
"CREATE TABLE enum_hex (c ENUM(0xc3a6, 0xc3b8, b'01100001') CHARSET utf8mb3); "\
"CREATE TABLE set_hex (c SET('b', 0xc3a6, b'01100001') CHARSET utf8mb3);" \
    "$DATABASE"

cleanup

printf '%s\n' "mysql_parser_corpus_charset_collation_surfaces_expectations: ok"
