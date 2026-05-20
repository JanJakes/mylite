#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
MYSQL_BIN="${MYLITE_MYSQL_BIN:-}"
MYSQL_SOCKET="${MYLITE_MYSQL_SOCKET:-}"
DATABASE="mylite_binary_full_column_indexes_expectations_$$"

fail() {
    printf '%s\n' "mysql_baseline_binary_full_column_indexes_expectations: $1" >&2
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
bin_full	CREATE TABLE `bin_full` (
  `id` int NOT NULL,
  `b` binary(4) DEFAULT NULL,
  `vb` varbinary(8) DEFAULT NULL,
  PRIMARY KEY (`id`),
  UNIQUE KEY `u_b` (`b`),
  UNIQUE KEY `u_vb` (`vb`),
  KEY `k_vb` (`vb`),
  KEY `k_mix` (`b`,`vb`),
  KEY `k_vb_desc` (`vb` DESC)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
bin_full	0	PRIMARY	1	id	A	0	NULL	NULL		BTREE			YES	NULL
bin_full	0	u_b	1	b	A	0	NULL	NULL	YES	BTREE			YES	NULL
bin_full	0	u_vb	1	vb	A	0	NULL	NULL	YES	BTREE			YES	NULL
bin_full	1	k_vb	1	vb	A	0	NULL	NULL	YES	BTREE			YES	NULL
bin_full	1	k_mix	1	b	A	0	NULL	NULL	YES	BTREE			YES	NULL
bin_full	1	k_mix	2	vb	A	0	NULL	NULL	YES	BTREE			YES	NULL
bin_full	1	k_vb_desc	1	vb	D	0	NULL	NULL	YES	BTREE			YES	NULL
k_mix	1	1	b	NULL	A	BTREE	YES
k_mix	1	2	vb	NULL	A	BTREE	YES
k_vb	1	1	vb	NULL	A	BTREE	YES
k_vb_desc	1	1	vb	NULL	D	BTREE	YES
PRIMARY	0	1	id	NULL	A	BTREE	YES
u_b	0	1	b	NULL	A	BTREE	YES
u_vb	0	1	vb	NULL	A	BTREE	YES
EXPECTED
)
expect_output \
    "full binary index metadata" \
    "$metadata_expected" \
    "USE ${DATABASE}; "\
"CREATE TABLE bin_full ("\
"id INT PRIMARY KEY, b BINARY(4), vb VARBINARY(8), "\
"UNIQUE KEY u_b (b), KEY k_vb (vb), KEY k_mix (b, vb)"\
"); "\
"ALTER TABLE bin_full ADD KEY k_vb_desc (vb DESC); "\
"CREATE UNIQUE INDEX u_vb ON bin_full (vb); "\
"SHOW CREATE TABLE bin_full; "\
"SHOW INDEX FROM bin_full; "\
"SELECT INDEX_NAME, NON_UNIQUE, SEQ_IN_INDEX, COLUMN_NAME, SUB_PART, "\
"COLLATION, INDEX_TYPE, IS_VISIBLE "\
"FROM information_schema.statistics "\
"WHERE TABLE_SCHEMA = '${DATABASE}' AND TABLE_NAME = 'bin_full' "\
"ORDER BY INDEX_NAME, SEQ_IN_INDEX;"

dml_expected=$(cat <<\EXPECTED
3	NULL	NULL
1	41000000	41
2	61000000	4100
1	0	4
0	2	4
2	1	0
616263	2
EXPECTED
)
expect_output \
    "full binary index dml behavior" \
    "$dml_expected" \
    "USE ${DATABASE}; "\
"CREATE TABLE dml_binary ("\
"id INT PRIMARY KEY, b BINARY(4), vb VARBINARY(8), "\
"UNIQUE KEY u_b (b), UNIQUE KEY u_vb (vb), KEY k_mix (b, vb)"\
"); "\
"INSERT INTO dml_binary VALUES (1, X'4100', X'41'), (2, X'61', X'4100'), "\
"(3, NULL, NULL); "\
"SELECT id, HEX(b), HEX(vb) FROM dml_binary ORDER BY vb; "\
"INSERT INTO dml_binary VALUES (4, X'6262', X'62'); "\
"SELECT ROW_COUNT(), @@warning_count, COUNT(*) FROM dml_binary; "\
"INSERT IGNORE INTO dml_binary VALUES (5, X'6262', X'63'), (6, X'64', X'62'); "\
"SELECT ROW_COUNT(), @@warning_count, COUNT(*) FROM dml_binary; "\
"CREATE TABLE odku_binary (vb VARBINARY(8), n INT, UNIQUE KEY u_vb (vb)); "\
"INSERT INTO odku_binary VALUES (X'616263', 1); "\
"INSERT INTO odku_binary VALUES (X'616263', 2) "\
"ON DUPLICATE KEY UPDATE n = VALUES(n); "\
"SELECT ROW_COUNT(), @@warning_count, @@error_count; "\
"SELECT HEX(vb), n FROM odku_binary;"

replace_update_expected=$(cat <<\EXPECTED
1	0	2	4200
EXPECTED
)
expect_output \
    "full binary unique replace and update" \
    "$replace_update_expected" \
    "USE ${DATABASE}; "\
"CREATE TABLE replace_update_binary ("\
"id INT PRIMARY KEY, vb VARBINARY(4), UNIQUE KEY u_vb (vb)"\
"); "\
"INSERT INTO replace_update_binary VALUES (1, X'41'), (2, X'42'); "\
"REPLACE INTO replace_update_binary VALUES (3, X'41'); "\
"UPDATE replace_update_binary SET vb = X'4200' WHERE id = 2; "\
"SELECT ROW_COUNT(), @@warning_count, COUNT(*), "\
"(SELECT HEX(vb) FROM replace_update_binary WHERE id = 2) "\
"FROM replace_update_binary;"

null_expected=$(cat <<\EXPECTED
2
EXPECTED
)
expect_output \
    "full binary unique allows multiple nulls" \
    "$null_expected" \
    "USE ${DATABASE}; "\
"CREATE TABLE null_binary (b BINARY(4), vb VARBINARY(4), UNIQUE KEY u_mix (b, vb)); "\
"INSERT INTO null_binary VALUES (NULL, X'41'), (NULL, X'41'); "\
"SELECT COUNT(*) FROM null_binary WHERE b IS NULL;"

added_index_expected=$(cat <<\EXPECTED
2
EXPECTED
)
expect_output \
    "full binary alter-added unique and standalone nonunique indexes" \
    "$added_index_expected" \
    "USE ${DATABASE}; "\
"CREATE TABLE added_binary_indexes (b BINARY(2), v VARBINARY(2)); "\
"ALTER TABLE added_binary_indexes ADD UNIQUE KEY u_b (b); "\
"CREATE INDEX k_v ON added_binary_indexes (v); "\
"SELECT COUNT(*) FROM INFORMATION_SCHEMA.STATISTICS "\
"WHERE TABLE_SCHEMA = '${DATABASE}' AND TABLE_NAME = 'added_binary_indexes';"

expect_error \
    "full varbinary duplicate diagnostic" \
    1062 \
    23000 \
    "Duplicate entry '\\x00\\x01\\xAA' for key 'dup_vb.u_vb'" \
    "USE ${DATABASE}; "\
"CREATE TABLE dup_vb (v VARBINARY(4), UNIQUE KEY u_vb (v)); "\
"INSERT INTO dup_vb VALUES (X'0001AA'); "\
"INSERT INTO dup_vb VALUES (X'0001AA');"

expect_error \
    "full binary duplicate diagnostic trims trailing nul bytes" \
    1062 \
    23000 \
    "Duplicate entry 'A\\x00B' for key 'dup_b.u_b'" \
    "USE ${DATABASE}; "\
"CREATE TABLE dup_b (b BINARY(4), UNIQUE KEY u_b (b)); "\
"INSERT INTO dup_b VALUES (X'41004200'); "\
"INSERT INTO dup_b VALUES (X'41004200');"

expect_error \
    "update duplicate full binary key fails" \
    1062 \
    23000 \
    "Duplicate entry 'B\\x00' for key 'update_dup.u_vb'" \
    "USE ${DATABASE}; "\
"CREATE TABLE update_dup (id INT PRIMARY KEY, vb VARBINARY(4), UNIQUE KEY u_vb (vb)); "\
"INSERT INTO update_dup VALUES (1, X'41'), (2, X'4200'); "\
"UPDATE update_dup SET vb = X'4200' WHERE id = 1;"

expect_error \
    "alter unique full binary validates existing rows" \
    1062 \
    23000 \
    "Duplicate entry 'ab' for key 'alter_existing.u_vb'" \
    "USE ${DATABASE}; "\
"CREATE TABLE alter_existing (vb VARBINARY(4)); "\
"INSERT INTO alter_existing VALUES (X'6162'), (X'6162'); "\
"ALTER TABLE alter_existing ADD UNIQUE KEY u_vb (vb);"

expect_error \
    "create unique index full binary validates existing rows" \
    1062 \
    23000 \
    "Duplicate entry 'ab' for key 'create_existing.u_vb'" \
    "USE ${DATABASE}; "\
"CREATE TABLE create_existing (vb VARBINARY(4)); "\
"INSERT INTO create_existing VALUES (X'6162'), (X'6162'); "\
"CREATE UNIQUE INDEX u_vb ON create_existing (vb);"

expect_error \
    "blob full key still requires prefix" \
    1170 \
    42000 \
    "BLOB/TEXT column 'b' used in key specification without a key length" \
    "USE ${DATABASE}; CREATE TABLE blob_bad (b BLOB, KEY k (b));"

expect_error \
    "varbinary full key over 3072 bytes fails" \
    1071 \
    42000 \
    "Specified key was too long; max key length is 3072 bytes" \
    "USE ${DATABASE}; CREATE TABLE bad_vb (v VARBINARY(3073), KEY k (v));"

expect_error \
    "zero length full binary key fails" \
    1167 \
    42000 \
    "The used storage engine can't index column 'v'" \
    "USE ${DATABASE}; CREATE TABLE bad_zero (v VARBINARY(0), KEY k (v));"

expect_error \
    "composite full binary key over 3072 bytes fails" \
    1071 \
    42000 \
    "Specified key was too long; max key length is 3072 bytes" \
    "USE ${DATABASE}; "\
"CREATE TABLE bad_mix (a VARBINARY(2000), b VARBINARY(1073), KEY k (a, b));"

printf '%s\n' "mysql_baseline_binary_full_column_indexes_expectations: ok"
