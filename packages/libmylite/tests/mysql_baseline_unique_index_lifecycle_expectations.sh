#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_unique_index_expectations_$$"

fail() {
    printf '%s\n' "mysql_baseline_unique_index_lifecycle_expectations: $1" >&2
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

cleanup
run_mysql "CREATE DATABASE ${DATABASE};" >/dev/null

run_mysql \
    "CREATE TABLE unique_t ("\
"id INT NOT NULL, v INT, amount DECIMAL(5,2), d DATE, n INT NOT NULL, "\
"PRIMARY KEY (id), UNIQUE KEY u_v (v), UNIQUE KEY u_amount (amount), "\
"UNIQUE KEY u_date (d), UNIQUE KEY u_n (n), KEY k_v (v));" \
    "$DATABASE" >/dev/null

show_create_expected=$(cat <<\EXPECTED
unique_t	CREATE TABLE `unique_t` (
  `id` int NOT NULL,
  `v` int DEFAULT NULL,
  `amount` decimal(5,2) DEFAULT NULL,
  `d` date DEFAULT NULL,
  `n` int NOT NULL,
  PRIMARY KEY (`id`),
  UNIQUE KEY `u_n` (`n`),
  UNIQUE KEY `u_v` (`v`),
  UNIQUE KEY `u_amount` (`amount`),
  UNIQUE KEY `u_date` (`d`),
  KEY `k_v` (`v`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
EXPECTED
)
show_columns_expected=$(printf '%b' 'id\tint\tNO\tPRI\tNULL\t\nv\tint\tYES\tUNI\tNULL\t\namount\tdecimal(5,2)\tYES\tUNI\tNULL\t\nd\tdate\tYES\tUNI\tNULL\t\nn\tint\tNO\tUNI\tNULL\t')
show_index_expected=$(cat <<\EXPECTED
unique_t	0	PRIMARY	1	id	A	0	NULL	NULL		BTREE			YES	NULL
unique_t	0	u_n	1	n	A	0	NULL	NULL		BTREE			YES	NULL
unique_t	0	u_v	1	v	A	0	NULL	NULL	YES	BTREE			YES	NULL
unique_t	0	u_amount	1	amount	A	0	NULL	NULL	YES	BTREE			YES	NULL
unique_t	0	u_date	1	d	A	0	NULL	NULL	YES	BTREE			YES	NULL
unique_t	1	k_v	1	v	A	0	NULL	NULL	YES	BTREE			YES	NULL
EXPECTED
)
expect_output \
    "unique index metadata" \
    "$show_create_expected
$show_columns_expected
$show_index_expected" \
    "SHOW CREATE TABLE unique_t; SHOW COLUMNS FROM unique_t; SHOW INDEX FROM unique_t;" \
    "$DATABASE"

statistics_expected=$(cat <<\EXPECTED
unique_t	1	k_v	1	v	A	NULL	YES	BTREE	YES	NULL
unique_t	0	PRIMARY	1	id	A	NULL		BTREE	YES	NULL
unique_t	0	u_amount	1	amount	A	NULL	YES	BTREE	YES	NULL
unique_t	0	u_date	1	d	A	NULL	YES	BTREE	YES	NULL
unique_t	0	u_n	1	n	A	NULL		BTREE	YES	NULL
unique_t	0	u_v	1	v	A	NULL	YES	BTREE	YES	NULL
EXPECTED
)
expect_output \
    "information schema statistics marks unique indexes" \
    "$statistics_expected" \
    "SELECT TABLE_NAME, NON_UNIQUE, INDEX_NAME, SEQ_IN_INDEX, COLUMN_NAME, COLLATION, "\
"SUB_PART, NULLABLE, INDEX_TYPE, IS_VISIBLE, EXPRESSION FROM INFORMATION_SCHEMA.STATISTICS "\
"WHERE TABLE_SCHEMA = '${DATABASE}' AND TABLE_NAME = 'unique_t' "\
"ORDER BY INDEX_NAME, SEQ_IN_INDEX;" \
    "$DATABASE"

inline_expected=$(cat <<\EXPECTED
inline_unique	CREATE TABLE `inline_unique` (
  `v` int DEFAULT NULL,
  `n` int NOT NULL,
  UNIQUE KEY `n` (`n`),
  UNIQUE KEY `v` (`v`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
v	int	YES	UNI	NULL	
n	int	NO	PRI	NULL	
inline_unique	0	n	1	n	A	0	NULL	NULL		BTREE			YES	NULL
inline_unique	0	v	1	v	A	0	NULL	NULL	YES	BTREE			YES	NULL
EXPECTED
)
expect_output \
    "inline unique column attributes" \
    "$inline_expected" \
    "CREATE TABLE inline_unique (v INT UNIQUE, n INT NOT NULL UNIQUE KEY); "\
"SHOW CREATE TABLE inline_unique; SHOW COLUMNS FROM inline_unique; SHOW INDEX FROM inline_unique;" \
    "$DATABASE"

unnamed_expected=$(cat <<\EXPECTED
unnamed_unique	CREATE TABLE `unnamed_unique` (
  `id` int DEFAULT NULL,
  `v` int DEFAULT NULL,
  UNIQUE KEY `v` (`v`),
  UNIQUE KEY `v_2` (`v`),
  UNIQUE KEY `id` (`id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
EXPECTED
)
expect_output \
    "unnamed unique indexes derive names and suffixes" \
    "$unnamed_expected" \
    "CREATE TABLE unnamed_unique (id INT, v INT, UNIQUE (v), UNIQUE KEY (v), UNIQUE (id)); "\
"SHOW CREATE TABLE unnamed_unique;" \
    "$DATABASE"

nullable_unique_expected=$(cat <<\EXPECTED
3	1
1	10
3	20
4	NULL
5	NULL
EXPECTED
)
expect_output \
    "unique indexes permit duplicate NULL and INSERT IGNORE skips duplicates" \
    "$nullable_unique_expected" \
    "CREATE TABLE nullable_unique (id INT, v INT, UNIQUE KEY u_v (v)); "\
"INSERT INTO nullable_unique VALUES (1,10); "\
"INSERT IGNORE INTO nullable_unique VALUES (2,10),(3,20),(4,NULL),(5,NULL); "\
"SELECT ROW_COUNT(), @@warning_count; SELECT * FROM nullable_unique ORDER BY id;" \
    "$DATABASE"

update_null_expected=$(cat <<\EXPECTED
1	0
1	10
2	NULL
3	NULL
4	NULL
EXPECTED
)
expect_output \
    "unique UPDATE permits duplicate NULL" \
    "$update_null_expected" \
    "CREATE TABLE update_unique (id INT, v INT, UNIQUE KEY u_v (v)); "\
"INSERT INTO update_unique VALUES (1,10),(2,20),(3,NULL),(4,NULL); "\
"UPDATE update_unique SET v = NULL WHERE id = 2; "\
"SELECT ROW_COUNT(), @@warning_count; SELECT * FROM update_unique ORDER BY id;" \
    "$DATABASE"

expect_error \
    "duplicate insert fails" \
    1062 \
    "23000" \
    "Duplicate entry '10' for key 'duplicate_insert.u_v'" \
    "CREATE TABLE duplicate_insert (id INT, v INT, UNIQUE KEY u_v (v)); "\
"INSERT INTO duplicate_insert VALUES (1,10); INSERT INTO duplicate_insert VALUES (2,10);" \
    "$DATABASE"

expect_error \
    "duplicate update fails" \
    1062 \
    "23000" \
    "Duplicate entry '10' for key 'update_unique.u_v'" \
    "UPDATE update_unique SET v = 10 WHERE id = 2;" \
    "$DATABASE"

expect_error \
    "multi-row update that creates an internal duplicate fails" \
    1062 \
    "23000" \
    "Duplicate entry '99' for key 'update_internal_duplicate.u_v'" \
    "CREATE TABLE update_internal_duplicate (id INT, v INT, UNIQUE KEY u_v (v)); "\
"INSERT INTO update_internal_duplicate VALUES (1,10),(2,20); "\
"UPDATE update_internal_duplicate SET v = 99;" \
    "$DATABASE"

expect_error \
    "duplicate key names share index namespace" \
    1061 \
    "42000" \
    "Duplicate key name 'k'" \
    "CREATE TABLE duplicate_name (a INT, b INT, UNIQUE KEY k (a), KEY k (b));" \
    "$DATABASE"

expect_error \
    "PRIMARY remains reserved for nonprimary indexes" \
    1280 \
    "42000" \
    "Incorrect index name 'PRIMARY'" \
    "CREATE TABLE primary_name (a INT, UNIQUE KEY \`PRIMARY\` (a));" \
    "$DATABASE"

expect_error \
    "text unique key needs prefix" \
    1170 \
    "42000" \
    "BLOB/TEXT column 'a' used in key specification without a key length" \
    "CREATE TABLE text_unique (a TEXT, UNIQUE KEY k (a));" \
    "$DATABASE"

expect_upstream_accepts \
    "MySQL accepts deferred string unique indexes with collation semantics" \
    "CREATE TABLE deferred_string_unique (c CHAR(3), v VARCHAR(20), UNIQUE KEY uc (c), UNIQUE KEY uv (v));" \
    "$DATABASE"

expect_upstream_accepts \
    "MySQL accepts deferred composite unique indexes" \
    "CREATE TABLE deferred_composite_unique (a INT, b INT, UNIQUE KEY k (a, b));" \
    "$DATABASE"

expect_upstream_accepts \
    "MySQL accepts deferred descending unique parts" \
    "CREATE TABLE deferred_desc_unique (a INT, UNIQUE KEY k (a DESC));" \
    "$DATABASE"

expect_upstream_accepts \
    "MySQL accepts deferred named unique constraints" \
    "CREATE TABLE deferred_constraint_unique (a INT, CONSTRAINT uq_a UNIQUE (a));" \
    "$DATABASE"

printf '%s\n' "mysql_baseline_unique_index_lifecycle_expectations: ok"
