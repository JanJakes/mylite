#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_rename_index_expectations_$$"
MISSING_DATABASE="${DATABASE}_missing"

fail() {
    printf '%s\n' "mysql_baseline_rename_index_lifecycle_expectations: $1" >&2
    exit 1
}

run_mysql() {
    sql=$1
    shift
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
    run_mysql "DROP DATABASE IF EXISTS ${MISSING_DATABASE};" >/dev/null 2>&1 || true
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

metadata_expected=$(cat <<\EXPECTED
0	0
0	0
0	0
t	CREATE TABLE `t` (
  `id` int NOT NULL,
  `a` int DEFAULT NULL,
  `b` int DEFAULT NULL,
  `v` varchar(20) DEFAULT NULL,
  PRIMARY KEY (`id`),
  UNIQUE KEY `u_a` (`a`),
  KEY `K_B` (`b`),
  KEY `k_v` (`v`(5) DESC)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
t	0	PRIMARY	1	id	A	3	NULL	NULL		BTREE			YES	NULL
t	0	u_a	1	a	A	3	NULL	NULL	YES	BTREE			YES	NULL
t	1	K_B	1	b	A	3	NULL	NULL	YES	BTREE			YES	NULL
t	1	k_v	1	v	D	3	5	NULL	YES	BTREE			YES	NULL
K_B	1	1	b	A	NULL	YES	BTREE	YES	NULL
k_v	1	1	v	D	5	YES	BTREE	YES	NULL
PRIMARY	0	1	id	A	NULL		BTREE	YES	NULL
u_a	0	1	a	A	NULL	YES	BTREE	YES	NULL
1:10:100:abc,2:20:200:def,3:30:300:ghi
EXPECTED
)
expect_output \
    "rename nonunique index metadata and preserve rows" \
    "$metadata_expected" \
    "CREATE TABLE t ("\
"id INT NOT NULL, a INT, b INT, v VARCHAR(20), "\
"PRIMARY KEY (id), UNIQUE KEY u_a (a), KEY k_b (b), KEY k_v (v(5) DESC)"\
"); "\
"INSERT INTO t VALUES (1,10,100,'abc'),(2,20,200,'def'),(3,30,300,'ghi'); "\
"ALTER TABLE t RENAME INDEX k_b TO renamed_b; "\
"SELECT ROW_COUNT(), @@warning_count; "\
"ALTER TABLE t RENAME KEY renamed_b TO k_b_again; "\
"SELECT ROW_COUNT(), @@warning_count; "\
"ALTER TABLE t RENAME INDEX k_b_again TO K_B; "\
"SELECT ROW_COUNT(), @@warning_count; "\
"SHOW CREATE TABLE t; "\
"SHOW INDEX FROM t; "\
"SELECT INDEX_NAME, NON_UNIQUE, SEQ_IN_INDEX, COLUMN_NAME, COLLATION, SUB_PART, "\
"NULLABLE, INDEX_TYPE, IS_VISIBLE, EXPRESSION FROM INFORMATION_SCHEMA.STATISTICS "\
"WHERE TABLE_SCHEMA = '${DATABASE}' AND TABLE_NAME = 't' "\
"ORDER BY INDEX_NAME, SEQ_IN_INDEX; "\
"SELECT GROUP_CONCAT(CONCAT(id, ':', a, ':', b, ':', v) ORDER BY id) FROM t;" \
    "$DATABASE"

same_name_expected=$(cat <<\EXPECTED
0	0
0	0
K_B
EXPECTED
)
expect_output \
    "same-name and case-only renames are accepted" \
    "$same_name_expected" \
    "CREATE TABLE same_name (id INT PRIMARY KEY, b INT, KEY k_b (b)); "\
"ALTER TABLE same_name RENAME INDEX k_b TO k_b; "\
"SELECT ROW_COUNT(), @@warning_count; "\
"ALTER TABLE same_name RENAME INDEX k_b TO K_B; "\
"SELECT ROW_COUNT(), @@warning_count; "\
"SELECT INDEX_NAME FROM INFORMATION_SCHEMA.STATISTICS "\
"WHERE TABLE_SCHEMA = '${DATABASE}' AND TABLE_NAME = 'same_name' AND INDEX_NAME <> 'PRIMARY';" \
    "$DATABASE"

unique_expected=$(cat <<\EXPECTED
0	0
uq	CREATE TABLE `uq` (
  `id` int NOT NULL,
  `a` int DEFAULT NULL,
  PRIMARY KEY (`id`),
  UNIQUE KEY `renamed_u` (`a`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
EXPECTED
)
expect_output \
    "unique index rename metadata" \
    "$unique_expected" \
    "CREATE TABLE uq (id INT PRIMARY KEY, a INT, UNIQUE KEY u_a (a)); "\
"INSERT INTO uq VALUES (1,10); "\
"ALTER TABLE uq RENAME INDEX u_a TO renamed_u; "\
"SELECT ROW_COUNT(), @@warning_count; "\
"SHOW CREATE TABLE uq;" \
    "$DATABASE"

expect_error \
    "renamed unique index is enforced under the new name" \
    1062 \
    23000 \
    "Duplicate entry '10' for key 'uq.renamed_u'" \
    "INSERT INTO uq VALUES (2,10);" \
    "$DATABASE"

foreign_key_expected=$(cat <<\EXPECTED
0	0
c	CREATE TABLE `c` (
  `id` int NOT NULL,
  `pid` int DEFAULT NULL,
  PRIMARY KEY (`id`),
  KEY `renamed_fk_pid` (`pid`),
  CONSTRAINT `fk_c_p` FOREIGN KEY (`pid`) REFERENCES `p` (`id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
fk_c_p	renamed_fk_pid
EXPECTED
)
expect_output \
    "foreign-key-backed child index can be renamed" \
    "$foreign_key_expected" \
    "CREATE TABLE p (id INT PRIMARY KEY); "\
"CREATE TABLE c ("\
"id INT PRIMARY KEY, pid INT, KEY fk_pid (pid), "\
"CONSTRAINT fk_c_p FOREIGN KEY (pid) REFERENCES p(id)"\
"); "\
"ALTER TABLE c RENAME INDEX fk_pid TO renamed_fk_pid; "\
"SELECT ROW_COUNT(), @@warning_count; "\
"SHOW CREATE TABLE c; "\
"SELECT rc.CONSTRAINT_NAME, s.INDEX_NAME "\
"FROM INFORMATION_SCHEMA.REFERENTIAL_CONSTRAINTS rc "\
"JOIN INFORMATION_SCHEMA.STATISTICS s ON s.TABLE_SCHEMA = rc.CONSTRAINT_SCHEMA "\
"AND s.TABLE_NAME = rc.TABLE_NAME AND s.COLUMN_NAME = 'pid' "\
"WHERE rc.CONSTRAINT_SCHEMA = '${DATABASE}' AND rc.TABLE_NAME = 'c' "\
"ORDER BY rc.CONSTRAINT_NAME, s.INDEX_NAME;" \
    "$DATABASE"

expect_error \
    "missing default schema fails" \
    1046 \
    3D000 \
    "No database selected" \
    "ALTER TABLE no_default RENAME INDEX k_v TO renamed;"

expect_error \
    "unknown schema fails" \
    1049 \
    42000 \
    "Unknown database '${MISSING_DATABASE}'" \
    "ALTER TABLE ${MISSING_DATABASE}.missing RENAME INDEX k_v TO renamed;" \
    "$DATABASE"

expect_error \
    "unknown table fails" \
    1146 \
    42S02 \
    "Table '${DATABASE}.missing_table' doesn't exist" \
    "ALTER TABLE missing_table RENAME INDEX k_v TO renamed;" \
    "$DATABASE"

expect_error \
    "unknown index fails" \
    1176 \
    42000 \
    "Key 'missing_idx' doesn't exist in table 'diag'" \
    "CREATE TABLE diag (id INT PRIMARY KEY, a INT, v INT, UNIQUE KEY u_a (a), KEY k_v (v)); "\
"ALTER TABLE diag RENAME INDEX missing_idx TO renamed;" \
    "$DATABASE"

expect_error \
    "duplicate new index name fails" \
    1061 \
    42000 \
    "Duplicate key name 'u_a'" \
    "ALTER TABLE diag RENAME INDEX k_v TO u_a;" \
    "$DATABASE"

expect_error \
    "quoted primary old name fails" \
    1280 \
    42000 \
    "Incorrect index name 'PRIMARY'" \
    "ALTER TABLE diag RENAME INDEX \`PRIMARY\` TO renamed;" \
    "$DATABASE"

expect_error \
    "quoted primary new name fails" \
    1280 \
    42000 \
    "Incorrect index name 'PRIMARY'" \
    "ALTER TABLE diag RENAME INDEX k_v TO \`PRIMARY\`;" \
    "$DATABASE"

expect_error \
    "unquoted primary old name is syntax error" \
    1064 \
    42000 \
    "near 'PRIMARY TO renamed'" \
    "ALTER TABLE diag RENAME INDEX PRIMARY TO renamed;" \
    "$DATABASE"

expect_error \
    "qualified old index name is syntax error" \
    1064 \
    42000 \
    "near '.k_v TO renamed'" \
    "ALTER TABLE diag RENAME INDEX diag.k_v TO renamed;" \
    "$DATABASE"

expect_error \
    "qualified new index name is syntax error" \
    1064 \
    42000 \
    "near '.renamed'" \
    "ALTER TABLE diag RENAME INDEX k_v TO diag.renamed;" \
    "$DATABASE"

expect_error \
    "missing TO is syntax error" \
    1064 \
    42000 \
    "near 'renamed'" \
    "ALTER TABLE diag RENAME INDEX k_v renamed;" \
    "$DATABASE"

expect_upstream_accepts \
    "mysql accepts multi-action rename indexes" \
    "CREATE TABLE multi_action (id INT PRIMARY KEY, a INT, b INT, KEY k_a (a), KEY k_b (b)); "\
"ALTER TABLE multi_action RENAME INDEX k_a TO renamed_a, RENAME INDEX k_b TO renamed_b;" \
    "$DATABASE"

expect_upstream_accepts \
    "mysql accepts algorithm clause with rename index" \
    "CREATE TABLE algorithm_clause (id INT PRIMARY KEY, b INT, KEY k_b (b)); "\
"ALTER TABLE algorithm_clause RENAME INDEX k_b TO renamed_b, ALGORITHM=INPLACE;" \
    "$DATABASE"

expect_upstream_accepts \
    "mysql accepts lock clause with rename index" \
    "CREATE TABLE lock_clause (id INT PRIMARY KEY, b INT, KEY k_b (b)); "\
"ALTER TABLE lock_clause RENAME INDEX k_b TO renamed_b, LOCK=NONE;" \
    "$DATABASE"
