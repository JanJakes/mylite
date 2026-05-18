#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
MYSQL_BIN="${MYLITE_MYSQL_BIN:-mysql}"
MYSQL_SOCKET="${MYLITE_MYSQL_SOCKET:-}"
DATABASE="mylite_alter_index_visibility_expectations_$$"
MISSING_DATABASE="${DATABASE}_missing"

fail() {
    printf '%s\n' "mysql_baseline_alter_index_visibility_expectations: $1" >&2
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

cleanup
run_mysql "CREATE DATABASE ${DATABASE} DEFAULT CHARACTER SET utf8mb4 "\
"COLLATE utf8mb4_0900_ai_ci;" >/dev/null

visibility_expected=$(cat <<\EXPECTED
0	0
k_v	NO
PRIMARY	YES
uk_u	YES
t	CREATE TABLE `t` (
  `id` int NOT NULL,
  `v` int DEFAULT NULL,
  `u` int NOT NULL,
  PRIMARY KEY (`id`),
  UNIQUE KEY `uk_u` (`u`),
  KEY `k_v` (`v`) /*!80000 INVISIBLE */
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
t	1	k_v	1	v	A	2	NULL	NULL	YES	BTREE			NO	NULL
0	0
k_v	YES
t	CREATE TABLE `t` (
  `id` int NOT NULL,
  `v` int DEFAULT NULL,
  `u` int NOT NULL,
  PRIMARY KEY (`id`),
  UNIQUE KEY `uk_u` (`u`),
  KEY `k_v` (`v`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
EXPECTED
)
expect_output \
    "secondary index visibility metadata" \
    "$visibility_expected" \
    "CREATE TABLE t ("\
"id INT NOT NULL, v INT, u INT NOT NULL, PRIMARY KEY(id), KEY k_v(v), UNIQUE KEY uk_u(u)"\
"); "\
"INSERT INTO t VALUES (1,10,100),(2,20,200); "\
"ALTER TABLE t ALTER INDEX k_v INVISIBLE; "\
"SELECT ROW_COUNT(), @@warning_count; "\
"SELECT INDEX_NAME, IS_VISIBLE FROM INFORMATION_SCHEMA.STATISTICS "\
"WHERE TABLE_SCHEMA='${DATABASE}' AND TABLE_NAME='t' "\
"ORDER BY INDEX_NAME, SEQ_IN_INDEX; "\
"SHOW CREATE TABLE t; "\
"SHOW INDEX FROM t WHERE Key_name = 'k_v'; "\
"ALTER TABLE t ALTER INDEX k_v VISIBLE; "\
"SELECT ROW_COUNT(), @@warning_count; "\
"SELECT INDEX_NAME, IS_VISIBLE FROM INFORMATION_SCHEMA.STATISTICS "\
"WHERE TABLE_SCHEMA='${DATABASE}' AND TABLE_NAME='t' AND INDEX_NAME='k_v'; "\
"SHOW CREATE TABLE t;" \
    "$DATABASE"

noop_expected=$(cat <<\EXPECTED
0	0
0	0
NO
EXPECTED
)
expect_output \
    "idempotent visibility updates report no rows" \
    "$noop_expected" \
    "ALTER TABLE t ALTER INDEX k_v INVISIBLE; "\
"SELECT ROW_COUNT(), @@warning_count; "\
"ALTER TABLE t ALTER INDEX k_v INVISIBLE; "\
"SELECT ROW_COUNT(), @@warning_count; "\
"SELECT IS_VISIBLE FROM INFORMATION_SCHEMA.STATISTICS "\
"WHERE TABLE_SCHEMA='${DATABASE}' AND TABLE_NAME='t' AND INDEX_NAME='k_v';" \
    "$DATABASE"

unique_expected=$(cat <<\EXPECTED
0	0
uk_u	NO
EXPECTED
)
expect_output \
    "unique index can become invisible" \
    "$unique_expected" \
    "ALTER TABLE t ALTER INDEX uk_u INVISIBLE; "\
"SELECT ROW_COUNT(), @@warning_count; "\
"SELECT INDEX_NAME, IS_VISIBLE FROM INFORMATION_SCHEMA.STATISTICS "\
"WHERE TABLE_SCHEMA='${DATABASE}' AND TABLE_NAME='t' AND INDEX_NAME='uk_u';" \
    "$DATABASE"

expect_error \
    "invisible unique index is still enforced" \
    1062 \
    23000 \
    "Duplicate entry '100' for key 't.uk_u'" \
    "INSERT INTO t VALUES (3,30,100);" \
    "$DATABASE"

rename_expected=$(cat <<\EXPECTED
0	0
renamed_k_v	NO
rename_visibility	CREATE TABLE `rename_visibility` (
  `id` int NOT NULL,
  `v` int DEFAULT NULL,
  PRIMARY KEY (`id`),
  KEY `renamed_k_v` (`v`) /*!80000 INVISIBLE */
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
EXPECTED
)
expect_output \
    "rename preserves invisible index metadata" \
    "$rename_expected" \
    "CREATE TABLE rename_visibility (id INT PRIMARY KEY, v INT, KEY k_v(v)); "\
"ALTER TABLE rename_visibility ALTER INDEX k_v INVISIBLE; "\
"ALTER TABLE rename_visibility RENAME INDEX k_v TO renamed_k_v; "\
"SELECT ROW_COUNT(), @@warning_count; "\
"SELECT INDEX_NAME, IS_VISIBLE FROM INFORMATION_SCHEMA.STATISTICS "\
"WHERE TABLE_SCHEMA='${DATABASE}' AND TABLE_NAME='rename_visibility' AND INDEX_NAME <> 'PRIMARY'; "\
"SHOW CREATE TABLE rename_visibility;" \
    "$DATABASE"

foreign_key_expected=$(cat <<\EXPECTED
pid_idx	NO
u	NO
EXPECTED
)
expect_output \
    "foreign key indexes can become invisible" \
    "$foreign_key_expected" \
    "CREATE TABLE p (id INT PRIMARY KEY, u INT NOT NULL UNIQUE); "\
"CREATE TABLE c ("\
"id INT PRIMARY KEY, pid INT, u INT, KEY pid_idx(pid), "\
"CONSTRAINT fk_pid FOREIGN KEY(pid) REFERENCES p(id), "\
"CONSTRAINT fk_u FOREIGN KEY(u) REFERENCES p(u)"\
"); "\
"ALTER TABLE c ALTER INDEX pid_idx INVISIBLE; "\
"ALTER TABLE p ALTER INDEX u INVISIBLE; "\
"SELECT INDEX_NAME, IS_VISIBLE FROM INFORMATION_SCHEMA.STATISTICS "\
"WHERE TABLE_SCHEMA='${DATABASE}' AND "\
"((TABLE_NAME='c' AND INDEX_NAME='pid_idx') OR (TABLE_NAME='p' AND INDEX_NAME='u')) "\
"ORDER BY INDEX_NAME;" \
    "$DATABASE"

foreign_key_enforcement_expected=$(cat <<\EXPECTED
1	0
1
EXPECTED
)
expect_output \
    "foreign key enforcement uses invisible indexes" \
    "$foreign_key_enforcement_expected" \
    "INSERT INTO p VALUES (1,10); "\
"INSERT INTO c VALUES (1,1,10); "\
"SELECT ROW_COUNT(), @@warning_count; "\
"SELECT COUNT(*) FROM c;" \
    "$DATABASE"

expect_error \
    "invisible child index still rejects missing parent primary key" \
    1452 \
    23000 \
    "Cannot add or update a child row" \
    "INSERT INTO c VALUES (2,999,10);" \
    "$DATABASE"

expect_error \
    "invisible parent unique index still rejects missing parent unique key" \
    1452 \
    23000 \
    "Cannot add or update a child row" \
    "INSERT INTO c VALUES (3,1,999);" \
    "$DATABASE"

expect_error \
    "invisible foreign key indexes still block referenced parent delete" \
    1451 \
    23000 \
    "Cannot delete or update a parent row" \
    "DELETE FROM p WHERE id = 1;" \
    "$DATABASE"

fulltext_expected=$(cat <<\EXPECTED
0	0
ft_body	NO	FULLTEXT
ft	CREATE TABLE `ft` (
  `body` text,
  FULLTEXT KEY `ft_body` (`body`) /*!80000 INVISIBLE */
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
EXPECTED
)
expect_output \
    "fulltext visibility metadata" \
    "$fulltext_expected" \
    "CREATE TABLE ft (body TEXT, FULLTEXT KEY ft_body(body)); "\
"ALTER TABLE ft ALTER INDEX ft_body INVISIBLE; "\
"SELECT ROW_COUNT(), @@warning_count; "\
"SELECT INDEX_NAME, IS_VISIBLE, INDEX_TYPE FROM INFORMATION_SCHEMA.STATISTICS "\
"WHERE TABLE_SCHEMA='${DATABASE}' AND TABLE_NAME='ft'; "\
"SHOW CREATE TABLE ft;" \
    "$DATABASE"

option_expected=$(cat <<\EXPECTED
2	0
0	0
YES
EXPECTED
)
expect_output \
    "algorithm and lock tails are accepted" \
    "$option_expected" \
    "ALTER TABLE t ALTER INDEX k_v INVISIBLE, ALGORITHM=COPY; "\
"SELECT ROW_COUNT(), @@warning_count; "\
"ALTER TABLE t ALTER INDEX k_v VISIBLE, LOCK=NONE, ALGORITHM=INPLACE; "\
"SELECT ROW_COUNT(), @@warning_count; "\
"SELECT IS_VISIBLE FROM INFORMATION_SCHEMA.STATISTICS "\
"WHERE TABLE_SCHEMA='${DATABASE}' AND TABLE_NAME='t' AND INDEX_NAME='k_v';" \
    "$DATABASE"

expect_error \
    "instant algorithm is rejected upstream" \
    1845 \
    0A000 \
    "ALGORITHM=INSTANT is not supported for this operation" \
    "ALTER TABLE t ALTER INDEX k_v INVISIBLE, ALGORITHM=INSTANT;" \
    "$DATABASE"

expect_error \
    "missing default schema fails" \
    1046 \
    3D000 \
    "No database selected" \
    "ALTER TABLE no_default ALTER INDEX k INVISIBLE;"

expect_error \
    "unknown schema fails" \
    1049 \
    42000 \
    "Unknown database '${MISSING_DATABASE}'" \
    "ALTER TABLE ${MISSING_DATABASE}.missing ALTER INDEX k INVISIBLE;" \
    "$DATABASE"

expect_error \
    "unknown table fails" \
    1146 \
    42S02 \
    "Table '${DATABASE}.missing_table' doesn't exist" \
    "ALTER TABLE missing_table ALTER INDEX k INVISIBLE;" \
    "$DATABASE"

expect_error \
    "unknown index fails" \
    1176 \
    42000 \
    "Key 'missing_idx' doesn't exist in table 't'" \
    "ALTER TABLE t ALTER INDEX missing_idx INVISIBLE;" \
    "$DATABASE"

expect_error \
    "quoted primary index cannot be invisible" \
    3522 \
    HY000 \
    "A primary key index cannot be invisible" \
    "ALTER TABLE t ALTER INDEX \`PRIMARY\` INVISIBLE;" \
    "$DATABASE"

expect_error \
    "quoted primary index visible is still rejected" \
    3522 \
    HY000 \
    "A primary key index cannot be invisible" \
    "ALTER TABLE t ALTER INDEX \`PRIMARY\` VISIBLE;" \
    "$DATABASE"

expect_error \
    "unquoted primary is syntax error" \
    1064 \
    42000 \
    "near 'PRIMARY INVISIBLE'" \
    "ALTER TABLE t ALTER INDEX PRIMARY INVISIBLE;" \
    "$DATABASE"

expect_error \
    "alter key syntax is rejected" \
    1064 \
    42000 \
    "near 'KEY k_v VISIBLE'" \
    "ALTER TABLE t ALTER KEY k_v VISIBLE;" \
    "$DATABASE"

expect_upstream_accepts \
    "mysql accepts create index visibility deferred by MyLite" \
    "CREATE INDEX k_create_invisible ON t(v) INVISIBLE;" \
    "$DATABASE"

expect_upstream_accepts \
    "mysql accepts add index visibility deferred by MyLite" \
    "ALTER TABLE t ADD INDEX k_add_invisible(v) INVISIBLE;" \
    "$DATABASE"

printf '%s\n' "mysql_baseline_alter_index_visibility_expectations: ok"
