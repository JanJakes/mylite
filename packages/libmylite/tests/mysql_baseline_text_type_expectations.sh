#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
MYSQL_BIN="${MYLITE_MYSQL_BIN:-}"
MYSQL_SOCKET="${MYLITE_MYSQL_SOCKET:-}"
DATABASE="mylite_text_type_expectations_$$"

fail() {
    printf '%s\n' "mysql_baseline_text_type_expectations: $1" >&2
    exit 1
}

run_mysql() {
    sql=$1
    shift
    if [ -n "$MYSQL_BIN" ]; then
        if [ -n "$MYSQL_SOCKET" ]; then
            printf '%s\n' "$sql" \
                | "$MYSQL_BIN" --protocol=SOCKET --socket="$MYSQL_SOCKET" -uroot \
                    --batch --raw --skip-column-names --default-character-set=utf8mb4 "$@"
        else
            printf '%s\n' "$sql" \
                | "$MYSQL_BIN" --protocol=TCP -h127.0.0.1 -uroot \
                    --batch --raw --skip-column-names --default-character-set=utf8mb4 "$@"
        fi
    else
        printf '%s\n' "$sql" \
            | docker exec -i "$MYSQL_CONTAINER" mysql -uroot \
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

expect_upstream_accepts() {
    label=$1
    sql=$2
    shift 2

    set +e
    output=$(run_mysql "$sql" "$@" 2>&1)
    status_code=$?
    set -e

    if [ "$status_code" -ne 0 ]; then
        fail "$label: expected MySQL to accept deferred behavior, got [$output]"
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

case "$(run_mysql "SELECT @@sql_mode;")" in
    *STRICT_TRANS_TABLES*) ;;
    *) fail "expected strict default sql_mode" ;;
esac

cleanup
run_mysql "CREATE DATABASE ${DATABASE};" >/dev/null

text_identifier_expected=$(cat <<\EXPECTED
text	int	YES		NULL	
body	text	YES		NULL	
EXPECTED
)
expect_output \
    "mysql accepts nonreserved text identifiers" \
    "$text_identifier_expected" \
    "CREATE TABLE text (text INT, body TEXT); SHOW COLUMNS FROM text; DROP TABLE text;" \
    "$DATABASE"

show_columns_expected=$(cat <<\EXPECTED
id	int	NO		NULL	
tt	tinytext	YES		NULL	
t	text	YES		NULL	
mt	mediumtext	YES		NULL	
lt	longtext	YES		NULL	
nn	text	NO		NULL	
EXPECTED
)
expect_output \
    "show columns renders text family descriptors" \
    "$show_columns_expected" \
    "CREATE TABLE text_family ("\
"id INT NOT NULL, tt TINYTEXT, t TEXT, mt MEDIUMTEXT, lt LONGTEXT, nn TEXT NOT NULL); "\
"SHOW COLUMNS FROM text_family;" \
    "$DATABASE"

show_create_expected=$(cat <<\EXPECTED
text_family	CREATE TABLE `text_family` (
  `id` int NOT NULL,
  `tt` tinytext,
  `t` text,
  `mt` mediumtext,
  `lt` longtext,
  `nn` text NOT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
EXPECTED
)
expect_output \
    "show create omits text default null clauses" \
    "$show_create_expected" \
    "SHOW CREATE TABLE text_family;" \
    "$DATABASE"

information_schema_expected=$(cat <<\EXPECTED
id	int	NULL	NULL	NULL	NULL	int	NO	NULL
tt	tinytext	255	255	utf8mb4	utf8mb4_0900_ai_ci	tinytext	YES	NULL
t	text	65535	65535	utf8mb4	utf8mb4_0900_ai_ci	text	YES	NULL
mt	mediumtext	16777215	16777215	utf8mb4	utf8mb4_0900_ai_ci	mediumtext	YES	NULL
lt	longtext	4294967295	4294967295	utf8mb4	utf8mb4_0900_ai_ci	longtext	YES	NULL
nn	text	65535	65535	utf8mb4	utf8mb4_0900_ai_ci	text	NO	NULL
EXPECTED
)
expect_output \
    "information schema renders text family metadata" \
    "$information_schema_expected" \
    "SELECT COLUMN_NAME, DATA_TYPE, CHARACTER_MAXIMUM_LENGTH, CHARACTER_OCTET_LENGTH, "\
"CHARACTER_SET_NAME, COLLATION_NAME, COLUMN_TYPE, IS_NULLABLE, COLUMN_DEFAULT "\
"FROM INFORMATION_SCHEMA.COLUMNS "\
"WHERE TABLE_SCHEMA='${DATABASE}' AND TABLE_NAME='text_family' ORDER BY ORDINAL_POSITION;" \
    "$DATABASE"

insert_readback_expected=$(cat <<\EXPECTED
3	0
1	[tiny]	4	4	[text  ]	6	6	[notnull]
2	[]	0	0	[]	0	0	[]
3	NULL	NULL	NULL	NULL	NULL	NULL	[nn]
EXPECTED
)
expect_output \
    "insert values and read back text rows" \
    "$insert_readback_expected" \
    "INSERT INTO text_family VALUES "\
"(1, 'tiny', 'text  ', 'medium', 'long', 'notnull'), "\
"(2, '', '', '', '', ''), "\
"(3, NULL, NULL, NULL, NULL, 'nn'); "\
"SELECT ROW_COUNT(), @@warning_count; "\
"SELECT id, IF(tt IS NULL, 'NULL', CONCAT('[', tt, ']')), LENGTH(tt), CHAR_LENGTH(tt), "\
"IF(t IS NULL, 'NULL', CONCAT('[', t, ']')), LENGTH(t), CHAR_LENGTH(t), "\
"IF(nn IS NULL, 'NULL', CONCAT('[', nn, ']')) FROM text_family ORDER BY id;" \
    "$DATABASE"

update_expected=$(cat <<\EXPECTED
1	0	[updated]
0	0	[updated]
1	0	NULL
1	0	1:NULL,2:,3:ordered
0	0
EXPECTED
)
expect_output \
    "update text assignment affected rows" \
    "$update_expected" \
    "UPDATE text_family SET t = 'updated' WHERE id = 1; "\
"SELECT ROW_COUNT(), @@warning_count, CONCAT('[', t, ']') FROM text_family WHERE id = 1; "\
"UPDATE text_family SET t = 'updated' WHERE id = 1; "\
"SELECT ROW_COUNT(), @@warning_count, CONCAT('[', t, ']') FROM text_family WHERE id = 1; "\
"UPDATE text_family SET t = NULL WHERE id = 1; "\
"SELECT ROW_COUNT(), @@warning_count, IF(t IS NULL, 'NULL', t) FROM text_family WHERE id = 1; "\
"UPDATE text_family SET t = 'ordered' ORDER BY id DESC LIMIT 1; "\
"SELECT ROW_COUNT(), @@warning_count, GROUP_CONCAT(CONCAT(id, ':', IF(t IS NULL, 'NULL', t)) "\
"ORDER BY id SEPARATOR ',') FROM text_family; "\
"UPDATE text_family SET t = 'ignored' LIMIT 0; "\
"SELECT ROW_COUNT(), @@warning_count;" \
    "$DATABASE"

expect_output \
    "is null predicates over text" \
    "1
2,3" \
    "SELECT GROUP_CONCAT(id ORDER BY id) FROM text_family WHERE t IS NULL; "\
"SELECT GROUP_CONCAT(id ORDER BY id) FROM text_family WHERE t IS NOT NULL;" \
    "$DATABASE"

expect_error \
    "insert overlength tinytext value" \
    1406 \
    22001 \
    "Data too long for column 'tt' at row 1" \
    "INSERT INTO text_family VALUES (10, CONCAT(REPEAT('x', 255), 'y'), 'ok', 'ok', 'ok', 'ok');" \
    "$DATABASE"

trailing_space_expected=$(printf "%b" \
    "1\t1\t255\tx")
expect_output \
    "mysql truncates trailing-space tinytext overlength with warning" \
    "$trailing_space_expected" \
    "INSERT INTO text_family VALUES "\
"(11, CONCAT(REPEAT('x', 255), ' '), 'ok', 'ok', 'ok', 'ok'); "\
"SELECT ROW_COUNT(), @@warning_count, LENGTH(tt), RIGHT(tt, 1) FROM text_family WHERE id = 11;" \
    "$DATABASE"

nonstrict_text_truncation_expected=$(printf "%b" \
    "1\t3\t255\ta\t65535\tb\t65535\tc")
expect_output \
    "nonstrict insert truncates text family overlength values" \
    "$nonstrict_text_truncation_expected" \
"SET sql_mode = ''; TRUNCATE text_family; "\
"INSERT INTO text_family (id, tt, t, mt, lt, nn) VALUES "\
"(1, REPEAT('a', 256), REPEAT('b', 65536), 'ok', 'ok', REPEAT('c', 65536)); "\
"SELECT ROW_COUNT(), @@warning_count, LENGTH(tt), RIGHT(tt, 1), LENGTH(t), RIGHT(t, 1), "\
"LENGTH(nn), RIGHT(nn, 1) FROM text_family WHERE id = 1;" \
    "$DATABASE"

nonstrict_text_truncation_warnings_expected=$(printf "%b" \
    "Warning\t1265\tData truncated for column 'tt' at row 1\n"\
"Warning\t1265\tData truncated for column 't' at row 1\n"\
"Warning\t1265\tData truncated for column 'nn' at row 1")
expect_output \
    "nonstrict insert text family truncation warnings" \
    "$nonstrict_text_truncation_warnings_expected" \
"SET sql_mode = ''; TRUNCATE text_family; "\
"INSERT INTO text_family (id, tt, t, mt, lt, nn) VALUES "\
"(1, REPEAT('a', 256), REPEAT('b', 65536), 'ok', 'ok', REPEAT('c', 65536)); "\
"SHOW WARNINGS;" \
    "$DATABASE"

utf8_text_truncation_expected=$(printf "%b" \
    "1\t1\t255\t85\tE282AC")
expect_output \
    "nonstrict text truncation preserves UTF-8 character boundary" \
    "$utf8_text_truncation_expected" \
    "SET sql_mode = ''; TRUNCATE text_family; "\
"INSERT INTO text_family (id, tt, nn) VALUES "\
"(1, CONCAT(REPEAT(CONVERT(0xE282AC USING utf8mb4), 85), 'x'), 'ok'); "\
"SELECT ROW_COUNT(), @@warning_count, LENGTH(tt), CHAR_LENGTH(tt), HEX(RIGHT(tt, 1)) "\
"FROM text_family WHERE id = 1;" \
    "$DATABASE"

ignore_text_truncation_expected=$(printf "%b" \
    "1\t1\t255\ti")
expect_output \
    "insert ignore truncates text family overlength values" \
    "$ignore_text_truncation_expected" \
    "TRUNCATE text_family; "\
"INSERT IGNORE INTO text_family (id, tt, nn) VALUES "\
"(1, CONCAT(REPEAT('i', 255), 'j'), 'ok'); "\
"SELECT ROW_COUNT(), @@warning_count, LENGTH(tt), RIGHT(tt, 1) FROM text_family WHERE id = 1;" \
    "$DATABASE"

nonstrict_text_update_expected=$(printf "%b" \
    "2\t2\n1:255:q,2:255:q\n0\t1")
expect_output \
    "nonstrict update truncates text family values per matched row" \
    "$nonstrict_text_update_expected" \
    "SET sql_mode = ''; TRUNCATE text_family; "\
"INSERT INTO text_family (id, tt, nn) VALUES (1, 'a', 'ok'), (2, 'b', 'ok'); "\
"UPDATE text_family SET tt = CONCAT(REPEAT('q', 255), 'z') ORDER BY id; "\
"SELECT ROW_COUNT(), @@warning_count; "\
"SELECT GROUP_CONCAT(CONCAT(id, ':', LENGTH(tt), ':', RIGHT(tt, 1)) ORDER BY id) FROM text_family; "\
"UPDATE text_family SET tt = CONCAT(REPEAT('q', 255), 'z') WHERE id = 1; "\
"SELECT ROW_COUNT(), @@warning_count;" \
    "$DATABASE"

text_insert_select_expected=$(printf "%b" \
    "2\t2\n1:255:s,2:255:t")
expect_output \
    "nonstrict insert select truncates text family values" \
    "$text_insert_select_expected" \
    "DROP TABLE IF EXISTS text_src; DROP TABLE IF EXISTS text_dst; SET sql_mode = ''; "\
"CREATE TABLE text_src (id INT, t TEXT); CREATE TABLE text_dst (id INT, tt TINYTEXT); "\
"INSERT INTO text_src VALUES (1, CONCAT(REPEAT('s', 255), 'x')), "\
"(2, CONCAT(REPEAT('t', 255), ' ')); "\
"INSERT INTO text_dst SELECT id, t FROM text_src ORDER BY id; "\
"SELECT ROW_COUNT(), @@warning_count; "\
"SELECT GROUP_CONCAT(CONCAT(id, ':', LENGTH(tt), ':', RIGHT(tt, 1)) ORDER BY id) FROM text_dst;" \
    "$DATABASE"

expect_error \
    "strict insert select rejects table-backed overlength text values" \
    1406 \
    22001 \
    "Data too long for column 'tt' at row 1" \
    "DROP TABLE IF EXISTS text_src; DROP TABLE IF EXISTS text_dst; "\
"CREATE TABLE text_src (id INT, t TEXT); CREATE TABLE text_dst (id INT, tt TINYTEXT); "\
"INSERT INTO text_src VALUES (1, CONCAT(REPEAT('s', 255), ' ')); "\
"INSERT INTO text_dst SELECT id, t FROM text_src;" \
    "$DATABASE"

expect_error \
    "null into text not null" \
    1048 \
    23000 \
    "Column 'nn' cannot be null" \
    "INSERT INTO text_family (id, tt, t, mt, lt, nn) VALUES (20, 'ok', 'ok', 'ok', 'ok', NULL);" \
    "$DATABASE"

expect_error \
    "omitted text not null no default" \
    1364 \
    HY000 \
    "Field 'nn' doesn't have a default value" \
    "INSERT INTO text_family (id, tt, t, mt, lt) VALUES (20, 'ok', 'ok', 'ok', 'ok');" \
    "$DATABASE"

expect_error \
    "update null into text not null" \
    1048 \
    23000 \
    "Column 'nn' cannot be null" \
    "UPDATE text_family SET nn = NULL WHERE id = 1;" \
    "$DATABASE"

ignore_warnings_expected=$(printf "%b" \
    "Warning\t1048\tColumn 'nn' cannot be null\n"\
"Warning\t1364\tField 'nn' doesn't have a default value")
expect_output \
    "insert ignore adjusts text null and no-default failures" \
    "$ignore_warnings_expected" \
    "INSERT IGNORE INTO text_family (id, tt, t, mt, lt, nn) VALUES "\
"(21, 'ok', 'ok', 'ok', 'ok', NULL), "\
"(22, 'ok', 'ok', 'ok', 'ok', DEFAULT); "\
"SHOW WARNINGS;" \
    "$DATABASE"

ignore_rows_expected=$(cat <<\EXPECTED
21	[]	0
22	[]	0
EXPECTED
)
expect_output \
    "insert ignore stores implicit empty text values" \
    "$ignore_rows_expected" \
    "SELECT id, CONCAT('[', nn, ']'), LENGTH(nn) FROM text_family WHERE id IN (21, 22) ORDER BY id;" \
    "$DATABASE"

expect_upstream_accepts \
    "mysql accepts deferred text length form" \
    "DROP TABLE IF EXISTS text_m; CREATE TABLE text_m (a TEXT(10)); SHOW COLUMNS FROM text_m;" \
    "$DATABASE"

expect_upstream_accepts \
    "mysql accepts deferred text expression default" \
    "DROP TABLE IF EXISTS text_default_expr; CREATE TABLE text_default_expr (a TEXT DEFAULT ('abc'));" \
    "$DATABASE"

expect_upstream_accepts \
    "mysql accepts deferred text column charset" \
    "DROP TABLE IF EXISTS text_charset; CREATE TABLE text_charset (a TEXT CHARACTER SET utf8mb4);" \
    "$DATABASE"

expect_upstream_accepts \
    "mysql accepts deferred long varchar alias" \
    "DROP TABLE IF EXISTS text_long_alias; CREATE TABLE text_long_alias (a LONG VARCHAR);" \
    "$DATABASE"

expect_error \
    "literal text default is rejected by mysql" \
    1101 \
    42000 \
    "can't have a default value" \
    "DROP TABLE IF EXISTS bad_default; CREATE TABLE bad_default (a TEXT DEFAULT 'abc');" \
    "$DATABASE"
