#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
MYSQL_BIN="${MYLITE_MYSQL_BIN:-mysql}"
MYSQL_SOCKET="${MYLITE_MYSQL_SOCKET:-}"
DATABASE="mylite_text_length_arguments_$$"

fail() {
    printf '%s\n' "mysql_baseline_text_family_length_arguments_expectations: $1" >&2
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
run_mysql "CREATE DATABASE ${DATABASE} DEFAULT CHARACTER SET utf8mb4 COLLATE utf8mb4_0900_ai_ci;" \
    >/dev/null

utf8mb4_expected=$(cat <<\EXPECTED
u0	tinytext	255	255	utf8mb4	utf8mb4_0900_ai_ci	tinytext
u63	tinytext	255	255	utf8mb4	utf8mb4_0900_ai_ci	tinytext
u64	text	65535	65535	utf8mb4	utf8mb4_0900_ai_ci	text
u16383	text	65535	65535	utf8mb4	utf8mb4_0900_ai_ci	text
u16384	mediumtext	16777215	16777215	utf8mb4	utf8mb4_0900_ai_ci	mediumtext
u4194303	mediumtext	16777215	16777215	utf8mb4	utf8mb4_0900_ai_ci	mediumtext
u4194304	longtext	4294967295	4294967295	utf8mb4	utf8mb4_0900_ai_ci	longtext
u4294967295	longtext	4294967295	4294967295	utf8mb4	utf8mb4_0900_ai_ci	longtext
EXPECTED
)
expect_output \
    "utf8mb4 text length normalization" \
    "$utf8mb4_expected" \
    "USE ${DATABASE}; CREATE TABLE utf8_len ("\
"u0 TEXT(0), u63 TEXT(63), u64 TEXT(64), u16383 TEXT(16383), "\
"u16384 TEXT(16384), u4194303 TEXT(4194303), u4194304 TEXT(4194304), "\
"u4294967295 TEXT(4294967295)); "\
"SELECT COLUMN_NAME, DATA_TYPE, CHARACTER_MAXIMUM_LENGTH, CHARACTER_OCTET_LENGTH, "\
"CHARACTER_SET_NAME, COLLATION_NAME, COLUMN_TYPE FROM INFORMATION_SCHEMA.COLUMNS "\
"WHERE TABLE_SCHEMA='${DATABASE}' AND TABLE_NAME='utf8_len' ORDER BY ORDINAL_POSITION;"

ascii_expected=$(cat <<\EXPECTED
a255	tinytext	255	255	ascii	ascii_general_ci	tinytext
a256	text	65535	65535	ascii	ascii_general_ci	text
a65535	text	65535	65535	ascii	ascii_general_ci	text
a65536	mediumtext	16777215	16777215	ascii	ascii_general_ci	mediumtext
EXPECTED
)
expect_output \
    "explicit ascii text length normalization" \
    "$ascii_expected" \
    "USE ${DATABASE}; CREATE TABLE ascii_len ("\
"a255 TEXT(255) CHARACTER SET ascii, a256 TEXT(256) CHARACTER SET ascii, "\
"a65535 TEXT(65535) CHARACTER SET ascii, a65536 TEXT(65536) CHARACTER SET ascii); "\
"SELECT COLUMN_NAME, DATA_TYPE, CHARACTER_MAXIMUM_LENGTH, CHARACTER_OCTET_LENGTH, "\
"CHARACTER_SET_NAME, COLLATION_NAME, COLUMN_TYPE FROM INFORMATION_SCHEMA.COLUMNS "\
"WHERE TABLE_SCHEMA='${DATABASE}' AND TABLE_NAME='ascii_len' ORDER BY ORDINAL_POSITION;"

ascii_table_expected=$(cat <<\EXPECTED
ta255	tinytext	255	255	ascii	ascii_general_ci	tinytext
ta256	text	65535	65535	ascii	ascii_general_ci	text
EXPECTED
)
expect_output \
    "table default ascii text length normalization" \
    "$ascii_table_expected" \
    "USE ${DATABASE}; CREATE TABLE ascii_table ("\
"ta255 TEXT(255), ta256 TEXT(256)) DEFAULT CHARSET=ascii; "\
"SELECT COLUMN_NAME, DATA_TYPE, CHARACTER_MAXIMUM_LENGTH, CHARACTER_OCTET_LENGTH, "\
"CHARACTER_SET_NAME, COLLATION_NAME, COLUMN_TYPE FROM INFORMATION_SCHEMA.COLUMNS "\
"WHERE TABLE_SCHEMA='${DATABASE}' AND TABLE_NAME='ascii_table' ORDER BY ORDINAL_POSITION;"

binary_expected=$(cat <<\EXPECTED
b255	tinyblob	255	255	NULL	NULL	tinyblob
b256	blob	65535	65535	NULL	NULL	blob
b65535	blob	65535	65535	NULL	NULL	blob
b65536	mediumblob	16777215	16777215	NULL	NULL	mediumblob
EXPECTED
)
expect_output \
    "binary text length normalization" \
    "$binary_expected" \
    "USE ${DATABASE}; CREATE TABLE binary_len ("\
"b255 TEXT(255) CHARACTER SET binary, b256 TEXT(256) CHARACTER SET binary, "\
"b65535 TEXT(65535) CHARACTER SET binary, b65536 TEXT(65536) CHARACTER SET binary); "\
"SELECT COLUMN_NAME, DATA_TYPE, CHARACTER_MAXIMUM_LENGTH, CHARACTER_OCTET_LENGTH, "\
"CHARACTER_SET_NAME, COLLATION_NAME, COLUMN_TYPE FROM INFORMATION_SCHEMA.COLUMNS "\
"WHERE TABLE_SCHEMA='${DATABASE}' AND TABLE_NAME='binary_len' ORDER BY ORDINAL_POSITION;"

collate_binary_expected=$(cat <<\EXPECTED
c255	tinyblob	255	255	NULL	NULL	tinyblob
c256	blob	65535	65535	NULL	NULL	blob
c65536	mediumblob	16777215	16777215	NULL	NULL	mediumblob
EXPECTED
)
expect_output \
    "collate binary text length normalization" \
    "$collate_binary_expected" \
    "USE ${DATABASE}; CREATE TABLE collate_binary_len ("\
"c255 TEXT(255) COLLATE binary, c256 TEXT(256) COLLATE binary, "\
"c65536 TEXT(65536) COLLATE binary); "\
"SELECT COLUMN_NAME, DATA_TYPE, CHARACTER_MAXIMUM_LENGTH, CHARACTER_OCTET_LENGTH, "\
"CHARACTER_SET_NAME, COLLATION_NAME, COLUMN_TYPE FROM INFORMATION_SCHEMA.COLUMNS "\
"WHERE TABLE_SCHEMA='${DATABASE}' AND TABLE_NAME='collate_binary_len' ORDER BY ORDINAL_POSITION;"

table_binary_expected=$(cat <<\EXPECTED
t255	tinyblob	255	255	NULL	NULL	tinyblob
t256	blob	65535	65535	NULL	NULL	blob
t65536	mediumblob	16777215	16777215	NULL	NULL	mediumblob
EXPECTED
)
expect_output \
    "table default binary text length normalization" \
    "$table_binary_expected" \
    "USE ${DATABASE}; CREATE TABLE table_binary_len ("\
"t255 TEXT(255), t256 TEXT(256), t65536 TEXT(65536)) DEFAULT CHARSET=binary; "\
"SELECT COLUMN_NAME, DATA_TYPE, CHARACTER_MAXIMUM_LENGTH, CHARACTER_OCTET_LENGTH, "\
"CHARACTER_SET_NAME, COLLATION_NAME, COLUMN_TYPE FROM INFORMATION_SCHEMA.COLUMNS "\
"WHERE TABLE_SCHEMA='${DATABASE}' AND TABLE_NAME='table_binary_len' ORDER BY ORDINAL_POSITION;"

show_create_expected=$(cat <<\EXPECTED
utf8_len	CREATE TABLE `utf8_len` (
  `u0` tinytext,
  `u63` tinytext,
  `u64` text,
  `u16383` text,
  `u16384` mediumtext,
  `u4194303` mediumtext,
  `u4194304` longtext,
  `u4294967295` longtext
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
EXPECTED
)
expect_output \
    "show create renders normalized text descriptors" \
    "$show_create_expected" \
    "USE ${DATABASE}; SHOW CREATE TABLE utf8_len;"

alter_expected=$(cat <<\EXPECTED
id	int
changed	mediumtext
added	text
EXPECTED
)
expect_output \
    "alter add modify change normalize text length" \
    "$alter_expected" \
    "USE ${DATABASE}; CREATE TABLE alter_len (id INT, body TEXT(63)) DEFAULT CHARSET=ascii; "\
"ALTER TABLE alter_len ADD COLUMN added TEXT(256); "\
"ALTER TABLE alter_len MODIFY body TEXT(65536); "\
"ALTER TABLE alter_len CHANGE body changed TEXT(65536); "\
"SELECT COLUMN_NAME, COLUMN_TYPE FROM INFORMATION_SCHEMA.COLUMNS "\
"WHERE TABLE_SCHEMA='${DATABASE}' AND TABLE_NAME='alter_len' ORDER BY ORDINAL_POSITION;"

like_expected=$(cat <<\EXPECTED
u0	tinytext
u63	tinytext
u64	text
u16383	text
u16384	mediumtext
u4194303	mediumtext
u4194304	longtext
u4294967295	longtext
EXPECTED
)
expect_output \
    "create table like clones normalized descriptors" \
    "$like_expected" \
    "USE ${DATABASE}; CREATE TABLE clone_len LIKE utf8_len; "\
"SELECT COLUMN_NAME, COLUMN_TYPE FROM INFORMATION_SCHEMA.COLUMNS "\
"WHERE TABLE_SCHEMA='${DATABASE}' AND TABLE_NAME='clone_len' ORDER BY ORDINAL_POSITION;"

row_expected=$(cat <<\EXPECTED
1	alpha	beta
EXPECTED
)
expect_output \
    "normalized descriptors store rows" \
    "$row_expected" \
    "USE ${DATABASE}; CREATE TABLE row_len (id INT, a TEXT(1), b TEXT(255) CHARACTER SET ascii); "\
"INSERT INTO row_len VALUES (1, 'alpha', 'beta'); SELECT id, a, b FROM row_len;"

expect_error \
    "text length too large" \
    1439 \
    42000 \
    "Display width out of range for column 'too_big' (max = 4294967295)" \
    "USE ${DATABASE}; CREATE TABLE too_large (too_big TEXT(4294967296));"

expect_error \
    "signed text length is syntax error" \
    1064 \
    42000 \
    "You have an error in your SQL syntax" \
    "USE ${DATABASE}; CREATE TABLE signed_len (c TEXT(+1));"

expect_error \
    "quoted text length is syntax error" \
    1064 \
    42000 \
    "You have an error in your SQL syntax" \
    "USE ${DATABASE}; CREATE TABLE quoted_len (c TEXT('1'));"

expect_error \
    "tinytext length is syntax error" \
    1064 \
    42000 \
    "You have an error in your SQL syntax" \
    "USE ${DATABASE}; CREATE TABLE tiny_len (c TINYTEXT(1));"

expect_error \
    "mediumtext length is syntax error" \
    1064 \
    42000 \
    "You have an error in your SQL syntax" \
    "USE ${DATABASE}; CREATE TABLE medium_len (c MEDIUMTEXT(1));"

expect_error \
    "longtext length is syntax error" \
    1064 \
    42000 \
    "You have an error in your SQL syntax" \
    "USE ${DATABASE}; CREATE TABLE long_len (c LONGTEXT(1));"

cleanup

printf '%s\n' "mysql_baseline_text_family_length_arguments_expectations: ok"
