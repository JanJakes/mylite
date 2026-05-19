#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
MYSQL_BIN="${MYLITE_MYSQL_BIN:-}"
MYSQL_SOCKET="${MYLITE_MYSQL_SOCKET:-}"
DATABASE="mylite_binary_prefix_indexes_expectations_$$"
MISSING_DATABASE="${DATABASE}_missing"

fail() {
    printf '%s\n' "mysql_baseline_binary_prefix_indexes_expectations: $1" >&2
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
    run_mysql "DROP DATABASE IF EXISTS ${MISSING_DATABASE};" >/dev/null 2>&1 || true
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
bin_prefix	CREATE TABLE `bin_prefix` (
  `id` int DEFAULT NULL,
  `b` binary(4) DEFAULT NULL,
  `vb` varbinary(10) DEFAULT NULL,
  `bl` blob,
  `tb` tinyblob,
  KEY `k_b` (`b`(2)),
  KEY `k_vb` (`vb`(3)),
  KEY `k_bl` (`bl`(4)),
  KEY `k_tb` (`tb`(255)),
  KEY `k_alt` (`vb`(5)),
  KEY `k_created` (`bl`(6))
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
bin_prefix	1	k_b	1	b	A	0	2	NULL	YES	BTREE			YES	NULL
bin_prefix	1	k_vb	1	vb	A	0	3	NULL	YES	BTREE			YES	NULL
bin_prefix	1	k_bl	1	bl	A	0	4	NULL	YES	BTREE			YES	NULL
bin_prefix	1	k_tb	1	tb	A	0	255	NULL	YES	BTREE			YES	NULL
bin_prefix	1	k_alt	1	vb	A	0	5	NULL	YES	BTREE			YES	NULL
bin_prefix	1	k_created	1	bl	A	0	6	NULL	YES	BTREE			YES	NULL
k_alt	1	1	vb	5	YES	BTREE	YES	NULL
k_b	1	1	b	2	YES	BTREE	YES	NULL
k_bl	1	1	bl	4	YES	BTREE	YES	NULL
k_created	1	1	bl	6	YES	BTREE	YES	NULL
k_tb	1	1	tb	255	YES	BTREE	YES	NULL
k_vb	1	1	vb	3	YES	BTREE	YES	NULL
EXPECTED
)
expect_output \
    "binary prefix metadata across create table alter add key and create index" \
    "$metadata_expected" \
    "CREATE TABLE bin_prefix ("\
"id INT, b BINARY(4), vb VARBINARY(10), bl BLOB, tb TINYBLOB, "\
"KEY k_b (b(2)), KEY k_vb (vb(3)), KEY k_bl (bl(4)), KEY k_tb (tb(255))"\
"); "\
"SELECT ROW_COUNT(), @@warning_count; "\
"ALTER TABLE bin_prefix ADD KEY k_alt (vb(5)); "\
"SELECT ROW_COUNT(), @@warning_count; "\
"CREATE INDEX k_created ON bin_prefix (bl(6)); "\
"SELECT ROW_COUNT(), @@warning_count; "\
"SHOW CREATE TABLE bin_prefix; "\
"SHOW INDEX FROM bin_prefix; "\
"SELECT INDEX_NAME, NON_UNIQUE, SEQ_IN_INDEX, COLUMN_NAME, SUB_PART, NULLABLE, "\
"INDEX_TYPE, IS_VISIBLE, EXPRESSION FROM INFORMATION_SCHEMA.STATISTICS "\
"WHERE TABLE_SCHEMA = '${DATABASE}' AND TABLE_NAME = 'bin_prefix' "\
"ORDER BY INDEX_NAME, SEQ_IN_INDEX;" \
    "$DATABASE"

composite_expected=$(cat <<\EXPECTED
0	0
composite_prefix	CREATE TABLE `composite_prefix` (
  `b` binary(4) DEFAULT NULL,
  `v` varbinary(8) DEFAULT NULL,
  `body` blob,
  KEY `k_mix` (`b`(2),`v`(3),`body`(4)),
  KEY `v` (`v`(2)),
  KEY `body` (`body`(5))
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
body	1	body	5
k_mix	1	b	2
k_mix	2	v	3
k_mix	3	body	4
v	1	v	2
EXPECTED
)
expect_output \
    "composite binary prefixes and generated names" \
    "$composite_expected" \
    "CREATE TABLE composite_prefix ("\
"b BINARY(4), v VARBINARY(8), body BLOB, KEY k_mix (b(2), v(3), body(4)), "\
"KEY (v(2)), INDEX (body(5))"\
"); "\
"SELECT ROW_COUNT(), @@warning_count; "\
"SHOW CREATE TABLE composite_prefix; "\
"SELECT INDEX_NAME, SEQ_IN_INDEX, COLUMN_NAME, SUB_PART "\
"FROM INFORMATION_SCHEMA.STATISTICS "\
"WHERE TABLE_SCHEMA = '${DATABASE}' AND TABLE_NAME = 'composite_prefix' "\
"ORDER BY INDEX_NAME, SEQ_IN_INDEX;" \
    "$DATABASE"

expect_error \
    "blob without prefix fails" \
    1170 \
    42000 \
    "BLOB/TEXT column 'bl' used in key specification without a key length" \
    "CREATE TABLE bad_blob_key (bl BLOB, KEY k (bl));" \
    "$DATABASE"

expect_error \
    "tinyblob prefix over 255-byte cap fails" \
    1071 \
    42000 \
    "Specified key was too long; max key length is 255 bytes" \
    "CREATE TABLE bad_tinyblob_prefix (tb TINYBLOB, KEY k (tb(256)));" \
    "$DATABASE"

expect_error \
    "varbinary prefix over declared length fails" \
    1089 \
    HY000 \
    "Incorrect prefix key" \
    "CREATE TABLE bad_varbinary_prefix (v VARBINARY(10), KEY k (v(11)));" \
    "$DATABASE"

expect_error \
    "binary prefix over declared length fails" \
    1089 \
    HY000 \
    "Incorrect prefix key" \
    "CREATE TABLE bad_binary_prefix (b BINARY(4), KEY k (b(5)));" \
    "$DATABASE"

expect_error \
    "binary zero prefix fails" \
    1391 \
    HY000 \
    "Key part 'b' length cannot be 0" \
    "CREATE TABLE bad_zero_prefix (b BINARY(4), KEY k (b(0)));" \
    "$DATABASE"

expect_error \
    "blob prefix over 3072-byte key limit fails" \
    1071 \
    42000 \
    "Specified key was too long; max key length is 3072 bytes" \
    "CREATE TABLE bad_blob_key_length (bl BLOB, KEY k (bl(3073)));" \
    "$DATABASE"

expect_error \
    "composite binary prefix over 3072-byte key limit fails" \
    1071 \
    42000 \
    "Specified key was too long; max key length is 3072 bytes" \
    "CREATE TABLE bad_composite_key_length (a BLOB, b BLOB, KEY k (a(2000), b(1073)));" \
    "$DATABASE"

expect_error \
    "duplicate binary prefix key part fails" \
    1060 \
    42S21 \
    "Duplicate column name 'v'" \
    "CREATE TABLE bad_duplicate_part (v VARBINARY(10), KEY k (v(2), v(3)));" \
    "$DATABASE"

expect_error \
    "duplicate binary prefix index name fails" \
    1061 \
    42000 \
    "Duplicate key name 'k'" \
    "CREATE TABLE bad_duplicate_name (v VARBINARY(10), KEY k (v(2)), KEY k (v(3)));" \
    "$DATABASE"

expect_error \
    "unknown binary prefix key column fails" \
    1072 \
    42000 \
    "Key column 'missing' doesn't exist in table" \
    "CREATE TABLE bad_unknown (v VARBINARY(10), KEY k (missing(2)));" \
    "$DATABASE"

expect_error \
    "missing default schema fails" \
    1046 \
    3D000 \
    "No database selected" \
    "CREATE INDEX k_no_default ON no_default (v(2));"

expect_error \
    "unknown schema fails" \
    1049 \
    42000 \
    "Unknown database '${MISSING_DATABASE}'" \
    "CREATE INDEX k_missing_schema ON ${MISSING_DATABASE}.missing (v(2));"

expect_error \
    "unknown table fails" \
    1146 \
    42S02 \
    "Table '${DATABASE}.missing_table' doesn't exist" \
    "CREATE INDEX k_missing_table ON missing_table (v(2));" \
    "$DATABASE"

printf '%s\n' "mysql_baseline_binary_prefix_indexes_expectations: ok"
