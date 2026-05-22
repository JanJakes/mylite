#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_temp_index_expectations_$$"
MISSING_DATABASE="${DATABASE}_missing"

fail() {
    printf '%s\n' "mysql_baseline_temporary_index_lifecycle_expectations: $1" >&2
    exit 1
}

run_mysql() {
    sql=$1
    shift
    printf '%s\n' "$sql" \
        | docker exec -i "$MYSQL_CONTAINER" mysql -uroot --batch --raw --skip-column-names "$@"
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

expect_contains() {
    label=$1
    needle=$2
    sql=$3
    shift 3

    output=$(run_mysql "$sql" "$@")
    case "$output" in
        *"$needle"*) ;;
        *) fail "$label: expected output to contain [$needle], got [$output]" ;;
    esac
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

version=$(run_mysql 'SELECT VERSION();')
case "$version" in
    8.4.9*) ;;
    *) fail "expected MySQL 8.4.9 runtime, got [$version]" ;;
esac

cleanup
run_mysql "CREATE DATABASE ${DATABASE};" >/dev/null

metadata_expected=$(cat <<\EXPECTED
create-index	3	0
create-unique	3	0
t	CREATE TEMPORARY TABLE `t` (
  `id` int NOT NULL,
  `v` int DEFAULT NULL,
  `name` varchar(10) DEFAULT NULL,
  PRIMARY KEY (`id`),
  UNIQUE KEY `u_name` (`name`),
  KEY `k_v` (`v`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
0
EXPECTED
)
expect_output \
    "temporary create index metadata and information schema omission" \
    "$metadata_expected" \
    "USE ${DATABASE}; "\
"CREATE TEMPORARY TABLE t (id INT NOT NULL, v INT, name VARCHAR(10), PRIMARY KEY(id)); "\
"INSERT INTO t VALUES (1,10,'a'),(2,20,'b'),(3,NULL,'c'); "\
"CREATE INDEX k_v ON t(v); "\
"SELECT 'create-index', ROW_COUNT(), @@warning_count; "\
"CREATE UNIQUE INDEX u_name ON t(name); "\
"SELECT 'create-unique', ROW_COUNT(), @@warning_count; "\
"SHOW CREATE TABLE t; "\
"SELECT COUNT(*) FROM INFORMATION_SCHEMA.STATISTICS "\
"WHERE TABLE_SCHEMA = '${DATABASE}' AND TABLE_NAME = 't';"

expect_contains \
    "temporary show index reflects post-create indexes" \
    "t	1	k_v	1	v	A	3" \
    "USE ${DATABASE}; "\
"CREATE TEMPORARY TABLE t (id INT NOT NULL, v INT, name VARCHAR(10), PRIMARY KEY(id)); "\
"INSERT INTO t VALUES (1,10,'a'),(2,20,'b'),(3,NULL,'c'); "\
"CREATE INDEX k_v ON t(v); "\
"CREATE UNIQUE INDEX u_name ON t(name); "\
"SHOW INDEX FROM t;"

alter_expected=$(cat <<\EXPECTED
alter-add-index	3	0
alter-add-unique	3	0
t	CREATE TEMPORARY TABLE `t` (
  `id` int DEFAULT NULL,
  `v` int DEFAULT NULL,
  `name` varchar(10) DEFAULT NULL,
  UNIQUE KEY `u_name` (`name`),
  KEY `k_v` (`v`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
EXPECTED
)
expect_output \
    "temporary alter add index metadata" \
    "$alter_expected" \
    "USE ${DATABASE}; "\
"CREATE TEMPORARY TABLE t (id INT, v INT, name VARCHAR(10)); "\
"INSERT INTO t VALUES (1,10,'a'),(2,20,'b'),(3,NULL,'c'); "\
"ALTER TABLE t ADD INDEX k_v(v); "\
"SELECT 'alter-add-index', ROW_COUNT(), @@warning_count; "\
"ALTER TABLE t ADD UNIQUE KEY u_name(name); "\
"SELECT 'alter-add-unique', ROW_COUNT(), @@warning_count; "\
"SHOW CREATE TABLE t;"

drop_expected=$(cat <<\EXPECTED
drop-index	3	0
alter-drop-index	3	0
t	CREATE TEMPORARY TABLE `t` (
  `id` int DEFAULT NULL,
  `v` int DEFAULT NULL,
  `name` varchar(10) DEFAULT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
EXPECTED
)
expect_output \
    "temporary drop index row counts and metadata" \
    "$drop_expected" \
    "USE ${DATABASE}; "\
"CREATE TEMPORARY TABLE t (id INT, v INT, name VARCHAR(10), KEY k_v(v), UNIQUE KEY u_name(name)); "\
"INSERT INTO t VALUES (1,10,'a'),(2,20,'b'),(3,NULL,'c'); "\
"DROP INDEX k_v ON t; "\
"SELECT 'drop-index', ROW_COUNT(), @@warning_count; "\
"ALTER TABLE t DROP INDEX u_name; "\
"SELECT 'alter-drop-index', ROW_COUNT(), @@warning_count; "\
"SHOW CREATE TABLE t;"

expect_output \
    "temporary empty table index row counts" \
    "create-empty	0	0
drop-empty	0	0" \
    "USE ${DATABASE}; "\
"CREATE TEMPORARY TABLE empty_t (id INT, v INT); "\
"CREATE INDEX k_v ON empty_t(v); "\
"SELECT 'create-empty', ROW_COUNT(), @@warning_count; "\
"DROP INDEX k_v ON empty_t; "\
"SELECT 'drop-empty', ROW_COUNT(), @@warning_count;"

expect_output \
    "temporary unique index permits duplicate nulls" \
    "unique-null	3	0" \
    "USE ${DATABASE}; "\
"CREATE TEMPORARY TABLE dup_null (id INT, v INT); "\
"INSERT INTO dup_null VALUES (1,NULL),(2,NULL),(3,10); "\
"CREATE UNIQUE INDEX u_v ON dup_null(v); "\
"SELECT 'unique-null', ROW_COUNT(), @@warning_count;"

expect_error \
    "temporary unique index rejects duplicate values" \
    1062 \
    23000 \
    "Duplicate entry '10' for key 'dup.u_v'" \
    "USE ${DATABASE}; "\
"CREATE TEMPORARY TABLE dup (id INT, v INT); "\
"INSERT INTO dup VALUES (1,10),(2,10); "\
"CREATE UNIQUE INDEX u_v ON dup(v);"

shadow_expected=$(cat <<\EXPECTED
temp-create	2	0
0
shadow	CREATE TEMPORARY TABLE `shadow` (
  `id` int DEFAULT NULL,
  `v` int DEFAULT NULL,
  KEY `k_v` (`v`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
shadow	CREATE TABLE `shadow` (
  `id` int DEFAULT NULL,
  `v` int DEFAULT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
EXPECTED
)
expect_output \
    "temporary index creation shadows persistent table" \
    "$shadow_expected" \
    "USE ${DATABASE}; "\
"DROP TABLE IF EXISTS shadow; "\
"CREATE TABLE shadow (id INT, v INT); "\
"CREATE TEMPORARY TABLE shadow (id INT, v INT); "\
"INSERT INTO shadow VALUES (1,10),(2,20); "\
"CREATE INDEX k_v ON shadow(v); "\
"SELECT 'temp-create', ROW_COUNT(), @@warning_count; "\
"SELECT COUNT(*) FROM INFORMATION_SCHEMA.STATISTICS "\
"WHERE TABLE_SCHEMA = '${DATABASE}' AND TABLE_NAME = 'shadow' AND INDEX_NAME = 'k_v'; "\
"SHOW CREATE TABLE shadow; "\
"DROP TEMPORARY TABLE shadow; "\
"SHOW CREATE TABLE shadow;"

expect_output \
    "temporary index ddl commits active transaction like index ddl" \
    "after-create	2	0
persistent-after-rollback	1
after-drop	2	0
persistent-after-rollback2	2" \
    "USE ${DATABASE}; "\
"CREATE TABLE tx_p (id INT); "\
"CREATE TEMPORARY TABLE tx_t (id INT, v INT); "\
"INSERT INTO tx_t VALUES (1,10),(2,20); "\
"START TRANSACTION; "\
"INSERT INTO tx_p VALUES (1); "\
"CREATE INDEX k_v ON tx_t(v); "\
"SELECT 'after-create', ROW_COUNT(), @@warning_count; "\
"ROLLBACK; "\
"SELECT 'persistent-after-rollback', COUNT(*) FROM tx_p; "\
"START TRANSACTION; "\
"INSERT INTO tx_p VALUES (2); "\
"DROP INDEX k_v ON tx_t; "\
"SELECT 'after-drop', ROW_COUNT(), @@warning_count; "\
"ROLLBACK; "\
"SELECT 'persistent-after-rollback2', COUNT(*) FROM tx_p;"

expect_error \
    "temporary fulltext index rejection" \
    1796 \
    HY000 \
    "Cannot create FULLTEXT index on temporary InnoDB table" \
    "USE ${DATABASE}; "\
"CREATE TEMPORARY TABLE ft (name VARCHAR(10)); "\
"CREATE FULLTEXT INDEX ft_name ON ft(name);"

expect_output \
    "mysql accepts temporary spatial index as deferred mylite behavior" \
    "spatial	0	1" \
    "USE ${DATABASE}; "\
"CREATE TEMPORARY TABLE spatial_t (g GEOMETRY NOT NULL); "\
"CREATE SPATIAL INDEX s_g ON spatial_t(g); "\
"SELECT 'spatial', ROW_COUNT(), @@warning_count;"

expect_error \
    "temporary create index without selected schema" \
    1046 \
    3D000 \
    "No database selected" \
    "CREATE TEMPORARY TABLE no_default (id INT, v INT); "\
"CREATE INDEX k_v ON no_default(v);"

expect_error \
    "temporary create index unknown schema" \
    1049 \
    42000 \
    "Unknown database '${MISSING_DATABASE}'" \
    "CREATE INDEX k_v ON ${MISSING_DATABASE}.missing_table(v);"

expect_error \
    "temporary create index unknown table" \
    1146 \
    42S02 \
    "Table '${DATABASE}.missing_table' doesn't exist" \
    "USE ${DATABASE}; CREATE INDEX k_v ON missing_table(v);"

expect_error \
    "temporary create index unknown column" \
    1072 \
    42000 \
    "Key column 'missing_col' doesn't exist in table" \
    "USE ${DATABASE}; "\
"CREATE TEMPORARY TABLE missing_col_t (id INT); "\
"CREATE INDEX k_missing ON missing_col_t(missing_col);"

expect_error \
    "temporary drop index unknown index" \
    1091 \
    42000 \
    "Can't DROP 'missing_idx'; check that column/key exists" \
    "USE ${DATABASE}; "\
"CREATE TEMPORARY TABLE missing_idx_t (id INT); "\
"DROP INDEX missing_idx ON missing_idx_t;"
