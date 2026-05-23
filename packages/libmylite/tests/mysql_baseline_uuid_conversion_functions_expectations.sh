#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_uuid_conversion_functions_expectations_$$"

fail() {
    printf '%s\n' "mysql_baseline_uuid_conversion_functions_expectations: $1" >&2
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
run_mysql "CREATE DATABASE ${DATABASE} CHARACTER SET utf8mb4 COLLATE utf8mb4_0900_ai_ci; USE ${DATABASE}; SET NAMES utf8mb4; SET SESSION sql_mode = 'NO_ENGINE_SUBSTITUTION';" >/dev/null

is_uuid_expected=$(cat <<EXPECTED
NULL	0	1	1	1	1	0	0	0	0	1	0	0
EXPECTED
)
expect_output \
    "is_uuid scalar values" \
    "$is_uuid_expected" \
    "DO 0; SELECT IS_UUID(NULL), IS_UUID(''), "\
"IS_UUID('6ccd780c-baba-1026-9564-5b8c656024db'), "\
"IS_UUID('6CCD780C-BABA-1026-9564-5B8C656024DB'), "\
"IS_UUID('6ccd780cbaba102695645b8c656024db'), "\
"IS_UUID('{6ccd780c-baba-1026-9564-5b8c656024db}'), "\
"IS_UUID('gccd780c-baba-1026-9564-5b8c656024db'), "\
"IS_UUID('6ccd780c-baba-1026-9564-5b8c6560'), "\
"IS_UUID(123), IS_UUID(TRUE), "\
"IS_UUID(X'36636364373830632D626162612D313032362D393536342D356238633635363032346462'), "\
"IS_UUID(X'00'), @@warning_count;" \
    "$DATABASE"

uuid_to_bin_expected=$(cat <<EXPECTED
6CCD780CBABA102695645B8C656024DB	6CCD780CBABA102695645B8C656024DB	6CCD780CBABA102695645B8C656024DB	6CCD780CBABA102695645B8C656024DB	1026BABA6CCD780C95645B8C656024DB	1026BABA6CCD780C95645B8C656024DB	6CCD780CBABA102695645B8C656024DB	1	0
EXPECTED
)
expect_output \
    "uuid_to_bin scalar values" \
    "$uuid_to_bin_expected" \
    "DO 0; SELECT HEX(UUID_TO_BIN('6ccd780c-baba-1026-9564-5b8c656024db')), "\
"HEX(UUID_TO_BIN('6CCD780C-BABA-1026-9564-5B8C656024DB')), "\
"HEX(UUID_TO_BIN('6ccd780cbaba102695645b8c656024db')), "\
"HEX(UUID_TO_BIN('{6ccd780c-baba-1026-9564-5b8c656024db}')), "\
"HEX(UUID_TO_BIN('6ccd780c-baba-1026-9564-5b8c656024db', 1)), "\
"HEX(UUID_TO_BIN('6ccd780c-baba-1026-9564-5b8c656024db', -1)), "\
"HEX(UUID_TO_BIN('6ccd780c-baba-1026-9564-5b8c656024db', NULL)), "\
"UUID_TO_BIN(NULL) IS NULL, @@warning_count;" \
    "$DATABASE"

bin_to_uuid_expected=$(cat <<EXPECTED
6ccd780c-baba-1026-9564-5b8c656024db	6ccd780c-baba-1026-9564-5b8c656024db	baba1026-780c-6ccd-9564-5b8c656024db	baba1026-780c-6ccd-9564-5b8c656024db	6ccd780c-baba-1026-9564-5b8c656024db	61626364-6566-6768-696a-6b6c6d6e6f70	1	0
EXPECTED
)
expect_output \
    "bin_to_uuid scalar values" \
    "$bin_to_uuid_expected" \
    "DO 0; SELECT BIN_TO_UUID(X'6CCD780CBABA102695645B8C656024DB'), "\
"BIN_TO_UUID(UUID_TO_BIN('6ccd780c-baba-1026-9564-5b8c656024db', 1), 1), "\
"BIN_TO_UUID(UUID_TO_BIN('6ccd780c-baba-1026-9564-5b8c656024db'), 1), "\
"BIN_TO_UUID(UUID_TO_BIN('6ccd780c-baba-1026-9564-5b8c656024db'), -1), "\
"BIN_TO_UUID(UUID_TO_BIN('6ccd780c-baba-1026-9564-5b8c656024db'), NULL), "\
"BIN_TO_UUID('abcdefghijklmnop'), BIN_TO_UUID(NULL) IS NULL, @@warning_count;" \
    "$DATABASE"

run_mysql \
    "CREATE TABLE t(id INT, s VARCHAR(64), b VARBINARY(32), fixed_bin BINARY(16), "\
"txt TEXT, blob_value BLOB); "\
"INSERT INTO t VALUES "\
"(1, '6ccd780c-baba-1026-9564-5b8c656024db', X'6CCD780CBABA102695645B8C656024DB', "\
"X'6CCD780CBABA102695645B8C656024DB', '6ccd780c-baba-1026-9564-5b8c656024db', "\
"X'6CCD780CBABA102695645B8C656024DB'), "\
"(2, '6ccd780cbaba102695645b8c656024db', X'1026BABA6CCD780C95645B8C656024DB', "\
"X'6CCD780CBABA102695645B8C656024DB', '6ccd780c-baba-1026-9564-5b8c656024db', "\
"X'6CCD780CBABA102695645B8C656024DB'), "\
"(3, '{6ccd780c-baba-1026-9564-5b8c656024db}', 'abcdefghijklmnop', "\
"X'6CCD780CBABA102695645B8C656024DB', '6ccd780c-baba-1026-9564-5b8c656024db', "\
"X'6CCD780CBABA102695645B8C656024DB'), "\
"(4, NULL, NULL, X'6CCD780CBABA102695645B8C656024DB', "\
"'6ccd780c-baba-1026-9564-5b8c656024db', X'6CCD780CBABA102695645B8C656024DB');" \
    "$DATABASE" >/dev/null

table_expected=$(cat <<EXPECTED
1	1	6CCD780CBABA102695645B8C656024DB	1026BABA6CCD780C95645B8C656024DB	6ccd780c-baba-1026-9564-5b8c656024db	baba1026-780c-6ccd-9564-5b8c656024db
2	1	6CCD780CBABA102695645B8C656024DB	1026BABA6CCD780C95645B8C656024DB	1026baba-6ccd-780c-9564-5b8c656024db	6ccd780c-baba-1026-9564-5b8c656024db
3	1	6CCD780CBABA102695645B8C656024DB	1026BABA6CCD780C95645B8C656024DB	61626364-6566-6768-696a-6b6c6d6e6f70	65666768-6364-6162-696a-6b6c6d6e6f70
EXPECTED
)
expect_output \
    "table uuid conversion values" \
    "$table_expected" \
    "SELECT id, IS_UUID(s), HEX(UUID_TO_BIN(s)), HEX(UUID_TO_BIN(s, 1)), "\
"BIN_TO_UUID(b), BIN_TO_UUID(b, 1) FROM t WHERE id < 4 ORDER BY id;" \
    "$DATABASE"

null_row_expected=$(cat <<EXPECTED
4	NULL	1	1
EXPECTED
)
expect_output \
    "table uuid null row" \
    "$null_row_expected" \
    "SELECT id, IS_UUID(s), UUID_TO_BIN(s) IS NULL, BIN_TO_UUID(b) IS NULL "\
"FROM t WHERE id = 4;" \
    "$DATABASE"

limited_expected=$(cat <<EXPECTED
4	NULL	NULL
3	1	61626364-6566-6768-696a-6b6c6d6e6f70
EXPECTED
)
expect_output \
    "table uuid row envelope" \
    "$limited_expected" \
    "SELECT id, IS_UUID(s), BIN_TO_UUID(b) FROM t WHERE id >= 1 ORDER BY id DESC LIMIT 2;" \
    "$DATABASE"

families_expected=$(cat <<EXPECTED
1	6ccd780c-baba-1026-9564-5b8c656024db	6ccd780c-baba-1026-9564-5b8c656024db	6ccd780c-baba-1026-9564-5b8c656024db	6ccd780c-baba-1026-9564-5b8c656024db
EXPECTED
)
expect_output \
    "table uuid descriptor families and nesting" \
    "$families_expected" \
    "SELECT IS_UUID(txt), BIN_TO_UUID(fixed_bin), BIN_TO_UUID(blob_value), "\
"BIN_TO_UUID(UUID_TO_BIN(s)), BIN_TO_UUID(UUID_TO_BIN(s, 1), 1) "\
"FROM t WHERE id = 1;" \
    "$DATABASE"

do_expected=$(cat <<EXPECTED
0	0
EXPECTED
)
expect_output \
    "uuid conversion do statement" \
    "$do_expected" \
    "DO UUID_TO_BIN('6ccd780c-baba-1026-9564-5b8c656024db'), "\
"BIN_TO_UUID(X'6CCD780CBABA102695645B8C656024DB'), IS_UUID('x'); "\
"SELECT ROW_COUNT(), @@warning_count;" \
    "$DATABASE"

expect_error \
    "is_uuid rejects zero arguments" \
    1582 \
    42000 \
    "Incorrect parameter count in the call to native function 'IS_UUID'" \
    "SELECT IS_UUID();" \
    "$DATABASE"

expect_error \
    "is_uuid rejects too many arguments" \
    1582 \
    42000 \
    "Incorrect parameter count in the call to native function 'IS_UUID'" \
    "SELECT IS_UUID('a', 'b');" \
    "$DATABASE"

expect_error \
    "uuid_to_bin rejects zero arguments" \
    1582 \
    42000 \
    "Incorrect parameter count in the call to native function 'UUID_TO_BIN'" \
    "SELECT UUID_TO_BIN();" \
    "$DATABASE"

expect_error \
    "uuid_to_bin rejects invalid uuid" \
    1411 \
    HY000 \
    "Incorrect string value: 'bad' for function uuid_to_bin" \
    "SELECT UUID_TO_BIN('bad');" \
    "$DATABASE"

expect_error \
    "uuid_to_bin rejects too many arguments" \
    1582 \
    42000 \
    "Incorrect parameter count in the call to native function 'UUID_TO_BIN'" \
    "SELECT UUID_TO_BIN('6ccd780c-baba-1026-9564-5b8c656024db', 0, 0);" \
    "$DATABASE"

expect_error \
    "bin_to_uuid rejects zero arguments" \
    1582 \
    42000 \
    "Incorrect parameter count in the call to native function 'BIN_TO_UUID'" \
    "SELECT BIN_TO_UUID();" \
    "$DATABASE"

expect_error \
    "bin_to_uuid rejects short value" \
    1411 \
    HY000 \
    "Incorrect string value:" \
    "SELECT BIN_TO_UUID(X'00');" \
    "$DATABASE"

expect_error \
    "bin_to_uuid rejects too many arguments" \
    1582 \
    42000 \
    "Incorrect parameter count in the call to native function 'BIN_TO_UUID'" \
    "SELECT BIN_TO_UUID(X'6CCD780CBABA102695645B8C656024DB', 0, 0);" \
    "$DATABASE"

printf '%s\n' "mysql_baseline_uuid_conversion_functions_expectations: ok"
