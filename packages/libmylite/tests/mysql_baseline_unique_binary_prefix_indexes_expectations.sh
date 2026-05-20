#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
MYSQL_BIN="${MYLITE_MYSQL_BIN:-}"
MYSQL_SOCKET="${MYLITE_MYSQL_SOCKET:-}"
DATABASE="mylite_unique_binary_prefix_indexes_expectations_$$"

fail() {
    printf '%s\n' "mysql_baseline_unique_binary_prefix_indexes_expectations: $1" >&2
    exit 1
}

run_mysql() {
    sql=$1
    shift
    if [ -n "$MYSQL_SOCKET" ]; then
        printf '%s\n' "$sql" \
            | "${MYSQL_BIN:-mysql}" --protocol=SOCKET --socket="$MYSQL_SOCKET" -uroot \
                --default-character-set=utf8mb4 --batch --raw --skip-column-names "$@"
        return
    fi
    if [ -n "$MYSQL_BIN" ]; then
        printf '%s\n' "$sql" \
            | "$MYSQL_BIN" --protocol=TCP -h127.0.0.1 -uroot --default-character-set=utf8mb4 \
                --batch --raw --skip-column-names "$@"
        return
    fi
    printf '%s\n' "$sql" \
        | docker exec -i "$MYSQL_CONTAINER" mysql -uroot --default-character-set=utf8mb4 \
            --batch --raw --skip-column-names "$@"
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
run_mysql "CREATE DATABASE ${DATABASE};" >/dev/null

metadata_expected=$(cat <<\EXPECTED
0	0
0	0
0	0
ubp	CREATE TABLE `ubp` (
  `id` int DEFAULT NULL,
  `b` binary(4) DEFAULT NULL,
  `vb` varbinary(8) DEFAULT NULL,
  `bl` blob,
  `tb` tinyblob,
  UNIQUE KEY `u_b` (`b`(2)),
  UNIQUE KEY `u_vb` (`vb`(3)),
  UNIQUE KEY `u_bl` (`bl`(4)),
  UNIQUE KEY `u_tb` (`tb`(5))
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
ubp	0	u_b	1	b	A	0	2	NULL	YES	BTREE			YES	NULL
ubp	0	u_vb	1	vb	A	0	3	NULL	YES	BTREE			YES	NULL
ubp	0	u_bl	1	bl	A	0	4	NULL	YES	BTREE			YES	NULL
ubp	0	u_tb	1	tb	A	0	5	NULL	YES	BTREE			YES	NULL
u_b	0	1	b	2	YES	BTREE	YES	NULL
u_bl	0	1	bl	4	YES	BTREE	YES	NULL
u_tb	0	1	tb	5	YES	BTREE	YES	NULL
u_vb	0	1	vb	3	YES	BTREE	YES	NULL
EXPECTED
)
expect_output \
    "unique binary prefix metadata across create table alter add and create index" \
    "$metadata_expected" \
    "CREATE TABLE ubp ("\
"id INT, b BINARY(4), vb VARBINARY(8), bl BLOB, tb TINYBLOB, "\
"UNIQUE KEY u_b (b(2)), UNIQUE KEY u_vb (vb(3))"\
"); "\
"SELECT ROW_COUNT(), @@warning_count; "\
"ALTER TABLE ubp ADD UNIQUE KEY u_bl (bl(4)); "\
"SELECT ROW_COUNT(), @@warning_count; "\
"CREATE UNIQUE INDEX u_tb ON ubp (tb(5)); "\
"SELECT ROW_COUNT(), @@warning_count; "\
"SHOW CREATE TABLE ubp; "\
"SHOW INDEX FROM ubp; "\
"SELECT INDEX_NAME, NON_UNIQUE, SEQ_IN_INDEX, COLUMN_NAME, SUB_PART, NULLABLE, "\
"INDEX_TYPE, IS_VISIBLE, EXPRESSION FROM INFORMATION_SCHEMA.STATISTICS "\
"WHERE TABLE_SCHEMA = '${DATABASE}' AND TABLE_NAME = 'ubp' "\
"ORDER BY INDEX_NAME, SEQ_IN_INDEX;" \
    "$DATABASE"

composite_expected=$(cat <<\EXPECTED
0	0
mix	CREATE TABLE `mix` (
  `id` int DEFAULT NULL,
  `b` binary(4) DEFAULT NULL,
  `vb` varbinary(8) DEFAULT NULL,
  `body` blob,
  UNIQUE KEY `u_mix` (`b`(2),`vb`(3),`body`(4)),
  UNIQUE KEY `u_created` (`vb`(2),`body`(2))
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
u_created	1	vb	2
u_created	2	body	2
u_mix	1	b	2
u_mix	2	vb	3
u_mix	3	body	4
EXPECTED
)
expect_output \
    "composite unique binary prefixes render key-part metadata" \
    "$composite_expected" \
    "CREATE TABLE mix ("\
"id INT, b BINARY(4), vb VARBINARY(8), body BLOB, "\
"UNIQUE KEY u_mix (b(2), vb(3), body(4))"\
"); "\
"SELECT ROW_COUNT(), @@warning_count; "\
"CREATE UNIQUE INDEX u_created ON mix (vb(2), body(2)); "\
"SHOW CREATE TABLE mix; "\
"SELECT INDEX_NAME, SEQ_IN_INDEX, COLUMN_NAME, SUB_PART "\
"FROM INFORMATION_SCHEMA.STATISTICS "\
"WHERE TABLE_SCHEMA = '${DATABASE}' AND TABLE_NAME = 'mix' "\
"ORDER BY INDEX_NAME, SEQ_IN_INDEX;" \
    "$DATABASE"

ignore_expected=$(cat <<\EXPECTED
4	0
2	2
1	61626364	78795A
2	646566	7A7A
3	NULL	7171FF
4	NULL	NULL
7	NULL	6B6B
8	NULL	NULL
EXPECTED
)
expect_output \
    "insert ignore skips duplicate binary prefixes and keeps duplicate null keys" \
    "$ignore_expected" \
    "CREATE TABLE ignore_binary ("\
"id INT, vb VARBINARY(8), bl BLOB, "\
"UNIQUE KEY u_vb (vb(3)), UNIQUE KEY u_bl (bl(2))"\
"); "\
"INSERT INTO ignore_binary VALUES "\
"(1, X'61626364', X'78795A'), "\
"(2, X'646566', X'7A7A'), "\
"(3, NULL, X'7171FF'), "\
"(4, NULL, NULL); "\
"SELECT ROW_COUNT(), @@warning_count; "\
"INSERT IGNORE INTO ignore_binary VALUES "\
"(5, X'616263FF', X'6D6D'), "\
"(6, X'676869', X'7879AA'), "\
"(7, NULL, X'6B6B'), "\
"(8, NULL, NULL); "\
"SELECT ROW_COUNT(), @@warning_count; "\
"SELECT id, HEX(vb), HEX(bl) FROM ignore_binary ORDER BY id;" \
    "$DATABASE"

odku_expected=$(cat <<\EXPECTED
2	1	0
61626364	2
EXPECTED
)
expect_output \
    "odku resolves conflict through unique binary prefix" \
    "$odku_expected" \
    "CREATE TABLE odku_binary (vb VARBINARY(8), n INT, UNIQUE KEY u_vb (vb(3))); "\
"INSERT INTO odku_binary VALUES (X'61626364', 1); "\
"INSERT INTO odku_binary VALUES (X'616263FF', 2) ON DUPLICATE KEY UPDATE n=VALUES(n); "\
"SELECT ROW_COUNT(), @@warning_count, @@error_count; "\
"SELECT HEX(vb), n FROM odku_binary;" \
    "$DATABASE"

update_expected=$(cat <<\EXPECTED
1	0
1	61626364	78795A
2	717273	7A7A
EXPECTED
)
expect_output \
    "successful update changes unique binary prefix key" \
    "$update_expected" \
    "CREATE TABLE update_binary ("\
"id INT, vb VARBINARY(8), bl BLOB, "\
"UNIQUE KEY u_vb (vb(3)), UNIQUE KEY u_bl (bl(2))"\
"); "\
"INSERT INTO update_binary VALUES (1, X'61626364', X'78795A'), (2, X'646566', X'7A7A'); "\
"UPDATE update_binary SET vb = X'717273' WHERE id = 2; "\
"SELECT ROW_COUNT(), @@warning_count; "\
"SELECT id, HEX(vb), HEX(bl) FROM update_binary ORDER BY id;" \
    "$DATABASE"

expect_error \
    "insert duplicate binary prefix fails" \
    1062 \
    23000 \
    "Duplicate entry 'ab' for key 'dup_binary.u_b'" \
    "CREATE TABLE dup_binary (b BINARY(4), UNIQUE KEY u_b (b(2))); "\
"INSERT INTO dup_binary VALUES (X'61626364'); "\
"INSERT INTO dup_binary VALUES (X'61627878');" \
    "$DATABASE"

expect_error \
    "nonprintable binary duplicate diagnostic uses hex escapes" \
    1062 \
    23000 \
    "Duplicate entry '\\x00\\x01' for key 'escape_binary.u_vb'" \
    "CREATE TABLE escape_binary (vb VARBINARY(4), UNIQUE KEY u_vb (vb(2))); "\
"INSERT INTO escape_binary VALUES (X'0001AA'); "\
"INSERT INTO escape_binary VALUES (X'0001BB');" \
    "$DATABASE"

expect_error \
    "update duplicate varbinary prefix fails" \
    1062 \
    23000 \
    "Duplicate entry 'abc' for key 'update_dup_vb.u_vb'" \
    "CREATE TABLE update_dup_vb (id INT, vb VARBINARY(8), UNIQUE KEY u_vb (vb(3))); "\
"INSERT INTO update_dup_vb VALUES (1, X'61626364'), (2, X'646566'); "\
"UPDATE update_dup_vb SET vb = X'616263EE' WHERE id = 2;" \
    "$DATABASE"

expect_error \
    "update duplicate blob prefix fails" \
    1062 \
    23000 \
    "Duplicate entry 'xy' for key 'update_dup_blob.u_bl'" \
    "CREATE TABLE update_dup_blob (id INT, bl BLOB, UNIQUE KEY u_bl (bl(2))); "\
"INSERT INTO update_dup_blob VALUES (1, X'78795A'), (2, X'7A7A'); "\
"UPDATE update_dup_blob SET bl = X'787900' WHERE id = 2;" \
    "$DATABASE"

expect_error \
    "alter unique binary prefix validates existing rows" \
    1062 \
    23000 \
    "Duplicate entry 'abc' for key 'alter_existing.u_vb'" \
    "CREATE TABLE alter_existing (vb VARBINARY(8)); "\
"INSERT INTO alter_existing VALUES (X'61626364'), (X'616263FF'); "\
"ALTER TABLE alter_existing ADD UNIQUE KEY u_vb (vb(3));" \
    "$DATABASE"

expect_error \
    "create unique index binary prefix validates existing rows" \
    1062 \
    23000 \
    "Duplicate entry 'ab' for key 'create_existing.u_b'" \
    "CREATE TABLE create_existing (b BINARY(4)); "\
"INSERT INTO create_existing VALUES (X'61626364'), (X'61627878'); "\
"CREATE UNIQUE INDEX u_b ON create_existing (b(2));" \
    "$DATABASE"

expect_error \
    "blob without unique prefix fails" \
    1170 \
    42000 \
    "BLOB/TEXT column 'bl' used in key specification without a key length" \
    "CREATE TABLE bad_blob_key (bl BLOB, UNIQUE KEY u_bl (bl));" \
    "$DATABASE"

expect_error \
    "varbinary unique prefix over declared length fails" \
    1089 \
    HY000 \
    "Incorrect prefix key" \
    "CREATE TABLE bad_varbinary_prefix (v VARBINARY(10), UNIQUE KEY u_v (v(11)));" \
    "$DATABASE"

expect_error \
    "tinyblob unique prefix over 255-byte cap fails" \
    1071 \
    42000 \
    "Specified key was too long; max key length is 255 bytes" \
    "CREATE TABLE bad_tinyblob_prefix (tb TINYBLOB, UNIQUE KEY u_tb (tb(256)));" \
    "$DATABASE"

expect_error \
    "binary unique zero prefix fails" \
    1391 \
    HY000 \
    "Key part 'b' length cannot be 0" \
    "CREATE TABLE bad_zero_prefix (b BINARY(4), UNIQUE KEY u_b (b(0)));" \
    "$DATABASE"

cleanup
