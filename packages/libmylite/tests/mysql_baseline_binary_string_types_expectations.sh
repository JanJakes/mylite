#!/usr/bin/env sh

set -eu

MYSQL_BIN="${MYLITE_MYSQL_BIN:-mysql}"
MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
MYSQL_SOCKET="${MYLITE_MYSQL_SOCKET:-}"
DATABASE="mylite_binary_string_types_expectations_$$"

fail() {
    printf '%s\n' "mysql_baseline_binary_string_types_expectations: $1" >&2
    exit 1
}

run_mysql() {
    sql=$1
    shift
    if [ -n "$MYSQL_SOCKET" ]; then
        printf '%s\n' "$sql" \
            | "$MYSQL_BIN" --protocol=SOCKET --socket="$MYSQL_SOCKET" \
                -uroot --batch --raw --skip-column-names --binary-as-hex=1 "$@"
    else
        printf '%s\n' "$sql" \
            | docker exec -i "$MYSQL_CONTAINER" mysql \
                -uroot --batch --raw --skip-column-names --binary-as-hex=1 "$@"
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

show_columns_expected=$(cat <<\EXPECTED
id	int	NO		NULL	
b	binary(1)	YES		NULL	
b0	binary(0)	YES		NULL	
b3	binary(3)	YES		NULL	
v0	varbinary(0)	YES		NULL	
v3	varbinary(3)	YES		NULL	
tb	tinyblob	YES		NULL	
bl	blob	YES		NULL	
mb	mediumblob	YES		NULL	
lb	longblob	YES		NULL	
nn	binary(2)	NO		NULL	
EXPECTED
)
expect_output \
    "show columns renders binary descriptors" \
    "$show_columns_expected" \
    "CREATE TABLE bin_types ("\
"id INT NOT NULL, b BINARY, b0 BINARY(0), b3 BINARY(3), v0 VARBINARY(0), "\
"v3 VARBINARY(3), tb TINYBLOB, bl BLOB, mb MEDIUMBLOB, lb LONGBLOB, "\
"nn BINARY(2) NOT NULL); SHOW COLUMNS FROM bin_types;" \
    "$DATABASE"

show_create_expected=$(cat <<\EXPECTED
bin_types	CREATE TABLE `bin_types` (
  `id` int NOT NULL,
  `b` binary(1) DEFAULT NULL,
  `b0` binary(0) DEFAULT NULL,
  `b3` binary(3) DEFAULT NULL,
  `v0` varbinary(0) DEFAULT NULL,
  `v3` varbinary(3) DEFAULT NULL,
  `tb` tinyblob,
  `bl` blob,
  `mb` mediumblob,
  `lb` longblob,
  `nn` binary(2) NOT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
EXPECTED
)
expect_output \
    "show create renders binary descriptors" \
    "$show_create_expected" \
    "SHOW CREATE TABLE bin_types;" \
    "$DATABASE"

information_schema_expected=$(cat <<\EXPECTED
b	binary	binary(1)	1	1	NULL	NULL	YES	NULL
b0	binary	binary(0)	0	0	NULL	NULL	YES	NULL
b3	binary	binary(3)	3	3	NULL	NULL	YES	NULL
v0	varbinary	varbinary(0)	0	0	NULL	NULL	YES	NULL
v3	varbinary	varbinary(3)	3	3	NULL	NULL	YES	NULL
tb	tinyblob	tinyblob	255	255	NULL	NULL	YES	NULL
bl	blob	blob	65535	65535	NULL	NULL	YES	NULL
mb	mediumblob	mediumblob	16777215	16777215	NULL	NULL	YES	NULL
lb	longblob	longblob	4294967295	4294967295	NULL	NULL	YES	NULL
nn	binary	binary(2)	2	2	NULL	NULL	NO	NULL
EXPECTED
)
expect_output \
    "information schema renders binary metadata" \
    "$information_schema_expected" \
    "SELECT COLUMN_NAME, DATA_TYPE, COLUMN_TYPE, CHARACTER_MAXIMUM_LENGTH, "\
"CHARACTER_OCTET_LENGTH, CHARACTER_SET_NAME, COLLATION_NAME, IS_NULLABLE, COLUMN_DEFAULT "\
"FROM INFORMATION_SCHEMA.COLUMNS WHERE TABLE_SCHEMA='${DATABASE}' "\
"AND TABLE_NAME='bin_types' AND COLUMN_NAME <> 'id' ORDER BY ORDINAL_POSITION;" \
    "$DATABASE"

blob_length_expected=$(cat <<\EXPECTED
a	tinyblob	YES		NULL	
b	tinyblob	YES		NULL	
c	blob	YES		NULL	
d	blob	YES		NULL	
e	mediumblob	YES		NULL	
EXPECTED
)
expect_output \
    "blob length maps to smallest blob family" \
    "$blob_length_expected" \
    "CREATE TABLE blob_lengths (a BLOB(0), b BLOB(255), c BLOB(256), "\
"d BLOB(65535), e BLOB(65536)); "\
"SHOW COLUMNS FROM blob_lengths;" \
    "$DATABASE"

char_byte_expected=$(cat <<\EXPECTED
cb	binary(1)	YES		NULL	
cb3	binary(3)	YES		NULL	
EXPECTED
)
expect_output \
    "char byte aliases binary descriptors" \
    "$char_byte_expected" \
    "CREATE TABLE char_byte_alias (cb CHAR BYTE, cb3 CHAR(3) BYTE); "\
"SHOW COLUMNS FROM char_byte_alias;" \
    "$DATABASE"

alter_add_expected=$(cat <<\EXPECTED
0000	2		0		0
EXPECTED
)
expect_output \
    "alter add binary not null backfills implicit values" \
    "$alter_add_expected" \
    "CREATE TABLE alter_binary (id INT); "\
"INSERT INTO alter_binary VALUES (1); "\
"ALTER TABLE alter_binary ADD COLUMN b BINARY(2) NOT NULL; "\
"ALTER TABLE alter_binary ADD COLUMN v VARBINARY(2) NOT NULL; "\
"ALTER TABLE alter_binary ADD COLUMN bl BLOB NOT NULL; "\
"SELECT HEX(b), LENGTH(b), HEX(v), LENGTH(v), HEX(bl), LENGTH(bl) FROM alter_binary;" \
    "$DATABASE"

insert_readback_expected=$(cat <<\EXPECTED
1	410000	3	4100	2	00	1	00	1	0000	2
2	000000	3		0		0		0	4243	2
3	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	4445	2
EXPECTED
)
expect_output \
    "insert and read binary bytes" \
    "$insert_readback_expected" \
    "INSERT INTO bin_types(id, b3, v3, tb, bl, nn) VALUES "\
"(1, X'41', X'4100', X'00', 0x00, X''), "\
"(2, X'', X'', X'', X'', X'4243'), "\
"(3, NULL, NULL, NULL, NULL, 'DE'); "\
"SELECT id, HEX(b3), LENGTH(b3), HEX(v3), LENGTH(v3), HEX(tb), LENGTH(tb), "\
"HEX(bl), LENGTH(bl), HEX(nn), LENGTH(nn) FROM bin_types ORDER BY id;" \
    "$DATABASE"

string_decode_expected=$(cat <<\EXPECTED
410042	3
410A42	3
415C2542	4
415C3042	4
415C42	3
415C5C42	4
415C6E42	4
EXPECTED
)
expect_output \
    "binary ordinary string decoding follows sql mode" \
    "$string_decode_expected" \
    "CREATE TABLE string_decode (v VARBINARY(8)); "\
"INSERT INTO string_decode VALUES ('A\\0B'), ('A\\nB'), ('A\\%B'), ('A\\\\B'); "\
"SET sql_mode='NO_BACKSLASH_ESCAPES'; "\
"INSERT INTO string_decode VALUES ('A\\0B'), ('A\\nB'), ('A\\\\B'); "\
"SELECT HEX(v), LENGTH(v) FROM string_decode ORDER BY HEX(v), LENGTH(v); "\
"SET sql_mode=DEFAULT;" \
    "$DATABASE"

update_expected=$(cat <<\EXPECTED
0	0	410000	3
1	0	420000	3
0	0	420000	3
1	0	420000	3
EXPECTED
)
expect_output \
    "update binary affected rows use changed bytes" \
    "$update_expected" \
    "UPDATE bin_types SET b3 = X'41' WHERE id = 1; "\
"SELECT ROW_COUNT(), @@warning_count, HEX(b3), LENGTH(b3) FROM bin_types WHERE id = 1; "\
"UPDATE bin_types SET b3 = X'42' WHERE id = 1; "\
"SELECT ROW_COUNT(), @@warning_count, HEX(b3), LENGTH(b3) FROM bin_types WHERE id = 1; "\
"UPDATE bin_types SET b3 = X'420000' WHERE id = 1; "\
"SELECT ROW_COUNT(), @@warning_count, HEX(b3), LENGTH(b3) FROM bin_types WHERE id = 1; "\
"UPDATE bin_types SET v3 = X'420000' WHERE id = 1; "\
"SELECT ROW_COUNT(), @@warning_count, HEX(v3), LENGTH(v3) FROM bin_types WHERE id = 1;" \
    "$DATABASE"

replace_expected=$(cat <<\EXPECTED
1	4100	2	0102	2
2	4200	2	03	1
EXPECTED
)
expect_output \
    "replace values and set convert binary strings" \
    "$replace_expected" \
    "CREATE TABLE replace_binary (id INT, b BINARY(2), v VARBINARY(3)); "\
"REPLACE INTO replace_binary VALUES (1, 'A', X'0102'); "\
"REPLACE INTO replace_binary SET id = 2, b = 'B', v = X'03'; "\
"SELECT id, HEX(b), LENGTH(b), HEX(v), LENGTH(v) FROM replace_binary ORDER BY id;" \
    "$DATABASE"

insert_select_expected=$(cat <<\EXPECTED
510000	3
510000	3
EXPECTED
)
expect_output \
    "insert and replace select convert binary target descriptors" \
    "$insert_select_expected" \
    "CREATE TABLE copy_source (id INT, v VARBINARY(1), b BINARY(3)); "\
"INSERT INTO copy_source VALUES (1, X'51', X'515253'); "\
"CREATE TABLE copy_target (id INT, b BINARY(3)); "\
"INSERT INTO copy_target SELECT id, v FROM copy_source; "\
"SELECT HEX(b), LENGTH(b) FROM copy_target; "\
"CREATE TABLE replace_target (id INT, b BINARY(3)); "\
"REPLACE INTO replace_target SELECT id, v FROM copy_source; "\
"SELECT HEX(b), LENGTH(b) FROM replace_target;" \
    "$DATABASE"

expect_error \
    "insert select overlong binary target fails" \
    1406 \
    22001 \
    "Data too long for column 'v' at row 1" \
    "CREATE TABLE narrow_target (id INT, v VARBINARY(1)); "\
"INSERT INTO narrow_target SELECT id, b FROM copy_source;" \
    "$DATABASE"

update_subquery_expected=$(cat <<\EXPECTED
1	0	510000	3
0	0	510000	3
EXPECTED
)
expect_output \
    "update scalar subquery converts binary target descriptors" \
    "$update_subquery_expected" \
    "CREATE TABLE scalar_source (id INT, v VARBINARY(1), b BINARY(3)); "\
"INSERT INTO scalar_source VALUES (1, X'51', X'515253'); "\
"CREATE TABLE scalar_target (id INT, b BINARY(3), v VARBINARY(1)); "\
"INSERT INTO scalar_target VALUES (1, X'410000', X'41'); "\
"UPDATE scalar_target SET b = (SELECT v FROM scalar_source WHERE id = 1) WHERE id = 1; "\
"SELECT ROW_COUNT(), @@warning_count, HEX(b), LENGTH(b) FROM scalar_target WHERE id = 1; "\
"UPDATE scalar_target SET b = (SELECT v FROM scalar_source WHERE id = 1) WHERE id = 1; "\
"SELECT ROW_COUNT(), @@warning_count, HEX(b), LENGTH(b) FROM scalar_target WHERE id = 1;" \
    "$DATABASE"

expect_error \
    "update scalar subquery overlong binary target fails" \
    1406 \
    22001 \
    "Data too long for column 'v'" \
    "UPDATE scalar_target SET v = (SELECT b FROM scalar_source WHERE id = 1) WHERE id = 1;" \
    "$DATABASE"

expect_output \
    "is null predicates over binary strings" \
    "3
1,2" \
    "SELECT GROUP_CONCAT(id ORDER BY id) FROM bin_types WHERE v3 IS NULL; "\
"SELECT GROUP_CONCAT(id ORDER BY id) FROM bin_types WHERE v3 IS NOT NULL;" \
    "$DATABASE"

ignore_expected=$(cat <<\EXPECTED
Warning	1265	Data truncated for column 'b3' at row 1
Warning	1265	Data truncated for column 'v3' at row 1
Warning	1265	Data truncated for column 'tb' at row 1
Warning	1048	Column 'nn' cannot be null
4	414243	3	414243	3	255	414141	414141	0000	2
EXPECTED
)
expect_output \
    "insert ignore binary adjustments" \
    "$ignore_expected" \
"INSERT IGNORE INTO bin_types(id, b3, v3, tb, nn) "\
"VALUES (4, X'41424344', X'41424344', REPEAT('A', 256), NULL); "\
"SHOW WARNINGS; "\
"SELECT id, HEX(b3), LENGTH(b3), HEX(v3), LENGTH(v3), LENGTH(tb), "\
"LEFT(HEX(tb), 6), RIGHT(HEX(tb), 6), "\
"HEX(nn), LENGTH(nn) FROM bin_types WHERE id = 4;" \
    "$DATABASE"

expect_error \
    "binary length above 255 fails" \
    1074 \
    42000 \
    "Column length too big for column 'b'" \
    "CREATE TABLE bad_binary_length (b BINARY(256));" \
    "$DATABASE"

expect_error \
    "bare varbinary is syntax error" \
    1064 \
    42000 \
    "near ')'" \
    "CREATE TABLE bad_varbinary (v VARBINARY);" \
    "$DATABASE"

expect_error \
    "varbinary row size too large" \
    1118 \
    42000 \
    "Row size too large" \
    "CREATE TABLE bad_varbinary_row_size (v VARBINARY(65533));" \
    "$DATABASE"

expect_error \
    "strict binary overlength fails" \
    1406 \
    22001 \
    "Data too long for column 'b3' at row 1" \
    "INSERT INTO bin_types(id, b3, nn) VALUES (10, X'41424344', X'00');" \
    "$DATABASE"

expect_error \
    "null into binary not null fails" \
    1048 \
    23000 \
    "Column 'nn' cannot be null" \
    "INSERT INTO bin_types(id, nn) VALUES (11, NULL);" \
    "$DATABASE"

expect_error \
    "omitted binary not null no default fails" \
    1364 \
    HY000 \
    "Field 'nn' doesn't have a default value" \
    "INSERT INTO bin_types(id) VALUES (12);" \
    "$DATABASE"

expect_error \
    "blob literal default is rejected by mysql" \
    1101 \
    42000 \
    "BLOB, TEXT, GEOMETRY or JSON column 'b' can't have a default value" \
    "CREATE TABLE bad_blob_default (b BLOB DEFAULT X'41');" \
    "$DATABASE"

expect_upstream_accepts \
    "mysql accepts binary literal defaults" \
    "CREATE TABLE deferred_binary_default (b BINARY(3) DEFAULT X'41', v VARBINARY(3) DEFAULT X'42'); "\
"INSERT INTO deferred_binary_default () VALUES (); "\
"SELECT HEX(b), LENGTH(b), HEX(v), LENGTH(v) FROM deferred_binary_default;" \
    "$DATABASE"

expect_upstream_accepts \
    "mysql accepts deferred blob expression defaults" \
    "CREATE TABLE deferred_blob_default (b BLOB DEFAULT (X'41')); "\
"INSERT INTO deferred_blob_default () VALUES (); "\
"SELECT HEX(b), LENGTH(b) FROM deferred_blob_default;" \
    "$DATABASE"

expect_upstream_accepts \
    "mysql accepts deferred bit literals for binary strings" \
    "CREATE TABLE deferred_bit_literal (b BINARY(2)); "\
"INSERT INTO deferred_bit_literal VALUES (b'1010'); "\
"SELECT HEX(b), LENGTH(b) FROM deferred_bit_literal;" \
    "$DATABASE"

printf '%s\n' "mysql_baseline_binary_string_types_expectations: ok"
