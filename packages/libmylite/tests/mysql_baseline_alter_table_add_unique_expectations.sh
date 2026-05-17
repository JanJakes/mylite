#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_add_unique_expectations_$$"
MISSING_DATABASE="${DATABASE}_missing"

fail() {
    printf '%s\n' "mysql_baseline_alter_table_add_unique_expectations: $1" >&2
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
  `v` int DEFAULT NULL,
  `u` bigint unsigned DEFAULT NULL,
  `name` varchar(10) DEFAULT NULL,
  `c` char(3) DEFAULT NULL,
  `amount` decimal(5,2) DEFAULT NULL,
  `d` date DEFAULT NULL,
  `dt` datetime DEFAULT NULL,
  `ts` timestamp NULL DEFAULT NULL,
  `txt` text,
  PRIMARY KEY (`id`),
  UNIQUE KEY `u_v` (`v`),
  UNIQUE KEY `u_u` (`u`),
  UNIQUE KEY `name` (`name`),
  UNIQUE KEY `u_c` (`c`),
  UNIQUE KEY `u_amount` (`amount`),
  UNIQUE KEY `u_d` (`d`),
  UNIQUE KEY `u_dt` (`dt`),
  UNIQUE KEY `u_ts` (`ts`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
amount	UNI
c	UNI
d	UNI
dt	UNI
id	PRI
name	UNI
ts	UNI
txt	
u	UNI
v	UNI
name	0	1	name	YES
u_amount	0	1	amount	YES
u_c	0	1	c	YES
u_d	0	1	d	YES
u_dt	0	1	dt	YES
u_ts	0	1	ts	YES
u_u	0	1	u	YES
u_v	0	1	v	YES
name	UNIQUE
PRIMARY	PRIMARY KEY
u_amount	UNIQUE
u_c	UNIQUE
u_d	UNIQUE
u_dt	UNIQUE
u_ts	UNIQUE
u_u	UNIQUE
u_v	UNIQUE
name	name	1
PRIMARY	id	1
u_amount	amount	1
u_c	c	1
u_d	d	1
u_dt	dt	1
u_ts	ts	1
u_u	u	1
u_v	v	1
EXPECTED
)
expect_output \
    "add unique metadata for supported descriptor families" \
    "$metadata_expected" \
    "CREATE TABLE t ("\
"id INT PRIMARY KEY, v INT, u BIGINT UNSIGNED, name VARCHAR(10), c CHAR(3), "\
"amount DECIMAL(5,2), d DATE, dt DATETIME, ts TIMESTAMP NULL, txt TEXT"\
"); "\
"INSERT INTO t VALUES "\
"(1,10,100,'aa','bb',12.30,'2024-01-01','2024-01-01 01:02:03',"\
"'2024-01-01 01:02:03','hello'),"\
"(2,NULL,200,'cc','dd',45.60,'2024-01-02','2024-01-02 01:02:03',NULL,'body'); "\
"ALTER TABLE t ADD UNIQUE u_v (v); "\
"SELECT ROW_COUNT(), @@warning_count; "\
"ALTER TABLE t ADD UNIQUE KEY u_u (u); "\
"SELECT ROW_COUNT(), @@warning_count; "\
"ALTER TABLE t ADD UNIQUE INDEX (name); "\
"SELECT ROW_COUNT(), @@warning_count; "\
"ALTER TABLE t ADD UNIQUE u_c (c); "\
"ALTER TABLE t ADD UNIQUE u_amount (amount); "\
"ALTER TABLE t ADD UNIQUE u_d (d); "\
"ALTER TABLE t ADD UNIQUE u_dt (dt); "\
"ALTER TABLE t ADD UNIQUE u_ts (ts); "\
"SHOW CREATE TABLE t; "\
"SELECT COLUMN_NAME, COLUMN_KEY FROM INFORMATION_SCHEMA.COLUMNS "\
"WHERE TABLE_SCHEMA = '${DATABASE}' AND TABLE_NAME = 't' ORDER BY COLUMN_NAME; "\
"SELECT INDEX_NAME, NON_UNIQUE, SEQ_IN_INDEX, COLUMN_NAME, NULLABLE "\
"FROM INFORMATION_SCHEMA.STATISTICS "\
"WHERE TABLE_SCHEMA = '${DATABASE}' AND TABLE_NAME = 't' AND INDEX_NAME <> 'PRIMARY' "\
"ORDER BY INDEX_NAME, SEQ_IN_INDEX; "\
"SELECT CONSTRAINT_NAME, CONSTRAINT_TYPE FROM INFORMATION_SCHEMA.TABLE_CONSTRAINTS "\
"WHERE TABLE_SCHEMA = '${DATABASE}' AND TABLE_NAME = 't' ORDER BY CONSTRAINT_NAME; "\
"SELECT CONSTRAINT_NAME, COLUMN_NAME, ORDINAL_POSITION "\
"FROM INFORMATION_SCHEMA.KEY_COLUMN_USAGE "\
"WHERE TABLE_SCHEMA = '${DATABASE}' AND TABLE_NAME = 't' ORDER BY CONSTRAINT_NAME, ORDINAL_POSITION;" \
    "$DATABASE"

omitted_forms_expected=$(cat <<\EXPECTED
0	0
0	0
forms	CREATE TABLE `forms` (
  `id` int DEFAULT NULL,
  `v` int DEFAULT NULL,
  `u` int DEFAULT NULL,
  `n` int DEFAULT NULL,
  UNIQUE KEY `v` (`v`),
  UNIQUE KEY `u` (`u`),
  UNIQUE KEY `n_2` (`n`),
  KEY `n` (`id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
EXPECTED
)
expect_output \
    "add unique omitted keyword and generated suffix forms" \
    "$omitted_forms_expected" \
    "CREATE TABLE forms (id INT, v INT, u INT, n INT, KEY n (id)); "\
"ALTER TABLE forms ADD UNIQUE (v); "\
"SELECT ROW_COUNT(), @@warning_count; "\
"ALTER TABLE forms ADD UNIQUE KEY (u); "\
"SELECT ROW_COUNT(), @@warning_count; "\
"ALTER TABLE forms ADD UNIQUE INDEX (n); "\
"SHOW CREATE TABLE forms;" \
    "$DATABASE"

named_constraint_expected=$(cat <<\EXPECTED
0	0
constraint_forms	CREATE TABLE `constraint_forms` (
  `id` int DEFAULT NULL,
  `a` int DEFAULT NULL,
  `b` int DEFAULT NULL,
  `c` int DEFAULT NULL,
  `d` int DEFAULT NULL,
  `e` int DEFAULT NULL,
  `f` int DEFAULT NULL,
  UNIQUE KEY `uq_a` (`a`),
  UNIQUE KEY `b` (`b`),
  UNIQUE KEY `k_c` (`c`),
  UNIQUE KEY `uq_d` (`d`),
  UNIQUE KEY `k_e` (`e`),
  UNIQUE KEY `visible` (`f`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
b	UNIQUE
k_c	UNIQUE
k_e	UNIQUE
uq_a	UNIQUE
uq_d	UNIQUE
visible	UNIQUE
EXPECTED
)
expect_output \
    "add constraint unique visible names" \
    "$named_constraint_expected" \
    "CREATE TABLE constraint_forms (id INT, a INT, b INT, c INT, d INT, e INT, f INT); "\
"ALTER TABLE constraint_forms ADD CONSTRAINT uq_a UNIQUE (a); "\
"SELECT ROW_COUNT(), @@warning_count; "\
"ALTER TABLE constraint_forms ADD CONSTRAINT UNIQUE (b); "\
"ALTER TABLE constraint_forms ADD CONSTRAINT UNIQUE KEY k_c (c); "\
"ALTER TABLE constraint_forms ADD CONSTRAINT uq_d UNIQUE KEY (d); "\
"ALTER TABLE constraint_forms ADD CONSTRAINT uq_e UNIQUE KEY k_e (e); "\
"ALTER TABLE constraint_forms ADD CONSTRAINT ignored UNIQUE visible (f); "\
"SHOW CREATE TABLE constraint_forms; "\
"SELECT CONSTRAINT_NAME, CONSTRAINT_TYPE FROM INFORMATION_SCHEMA.TABLE_CONSTRAINTS "\
"WHERE TABLE_SCHEMA = '${DATABASE}' AND TABLE_NAME = 'constraint_forms' "\
"ORDER BY CONSTRAINT_NAME;" \
    "$DATABASE"

schema_expected=$(cat <<\EXPECTED
q	CREATE TABLE `q` (
  `id` int DEFAULT NULL,
  `v` int DEFAULT NULL,
  UNIQUE KEY `u_v` (`v`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
EXPECTED
)
expect_output \
    "schema-qualified add unique succeeds without selected schema" \
    "$schema_expected" \
    "CREATE TABLE ${DATABASE}.q (id INT, v INT); "\
"ALTER TABLE ${DATABASE}.q ADD UNIQUE u_v (v); "\
"SHOW CREATE TABLE ${DATABASE}.q;"

duplicate_null_expected=$(cat <<\EXPECTED
0	0
duplicate_null	CREATE TABLE `duplicate_null` (
  `id` int DEFAULT NULL,
  `v` int DEFAULT NULL,
  UNIQUE KEY `u_v` (`v`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
EXPECTED
)
expect_output \
    "add unique permits duplicate nulls" \
    "$duplicate_null_expected" \
    "CREATE TABLE duplicate_null (id INT, v INT); "\
"INSERT INTO duplicate_null VALUES (1,NULL),(2,NULL),(3,10); "\
"ALTER TABLE duplicate_null ADD UNIQUE u_v (v); "\
"SELECT ROW_COUNT(), @@warning_count; SHOW CREATE TABLE duplicate_null;" \
    "$DATABASE"

varchar_space_expected=$(cat <<\EXPECTED
0	0
EXPECTED
)
expect_output \
    "add unique varchar preserves no-pad trailing spaces" \
    "$varchar_space_expected" \
    "CREATE TABLE varchar_space (id INT, name VARCHAR(10)); "\
"INSERT INTO varchar_space VALUES (1,'a'),(2,'a '); "\
"ALTER TABLE varchar_space ADD UNIQUE u_name (name); "\
"SELECT ROW_COUNT(), @@warning_count;" \
    "$DATABASE"

expect_error \
    "add unique rejects duplicate populated values" \
    1062 \
    23000 \
    "Duplicate entry '10' for key 'duplicate_values.u_v'" \
    "CREATE TABLE duplicate_values (id INT, v INT); "\
"INSERT INTO duplicate_values VALUES (1,10),(2,10); "\
"ALTER TABLE duplicate_values ADD UNIQUE u_v (v);" \
    "$DATABASE"

expect_error \
    "add unique varchar index is case-insensitive" \
    1062 \
    23000 \
    "Duplicate entry 'a' for key 'varchar_case.u_name'" \
    "CREATE TABLE varchar_case (id INT, name VARCHAR(10)); "\
"INSERT INTO varchar_case VALUES (1,'a'),(2,'A'); "\
"ALTER TABLE varchar_case ADD UNIQUE u_name (name);" \
    "$DATABASE"

expect_error \
    "add unique char index observes stored char canonicalization" \
    1062 \
    23000 \
    "Duplicate entry 'a' for key 'char_space.u_name'" \
    "CREATE TABLE char_space (id INT, name CHAR(10)); "\
"INSERT INTO char_space VALUES (1,'a'),(2,'a '); "\
"ALTER TABLE char_space ADD UNIQUE u_name (name);" \
    "$DATABASE"

expect_error \
    "missing default schema fails" \
    1046 \
    3D000 \
    "No database selected" \
    "ALTER TABLE no_default ADD UNIQUE u_v (v);"

expect_error \
    "unknown schema fails" \
    1049 \
    42000 \
    "Unknown database '${MISSING_DATABASE}'" \
    "ALTER TABLE ${MISSING_DATABASE}.missing ADD UNIQUE u_v (v);"

expect_error \
    "unknown table fails" \
    1146 \
    42S02 \
    "Table '${DATABASE}.missing_table' doesn't exist" \
    "ALTER TABLE missing_table ADD UNIQUE u_v (v);" \
    "$DATABASE"

expect_error \
    "duplicate explicit index name fails" \
    1061 \
    42000 \
    "Duplicate key name 'u_v'" \
    "CREATE TABLE duplicate_name (id INT, v INT, UNIQUE KEY u_v (v)); "\
"ALTER TABLE duplicate_name ADD UNIQUE u_v (id);" \
    "$DATABASE"

expect_error \
    "duplicate explicit index name outranks unknown key column" \
    1061 \
    42000 \
    "Duplicate key name 'u_v'" \
    "CREATE TABLE duplicate_unknown_column (id INT, v INT, UNIQUE KEY u_v (v)); "\
"ALTER TABLE duplicate_unknown_column ADD UNIQUE u_v (missing);" \
    "$DATABASE"

expect_error \
    "quoted primary index name fails" \
    1280 \
    42000 \
    "Incorrect index name 'PRIMARY'" \
    "CREATE TABLE quoted_primary (id INT, v INT); "\
"ALTER TABLE quoted_primary ADD UNIQUE \`PRIMARY\` (v);" \
    "$DATABASE"

expect_error \
    "unknown key column fails" \
    1072 \
    42000 \
    "Key column 'missing' doesn't exist in table" \
    "CREATE TABLE unknown_column (id INT, v INT); "\
"ALTER TABLE unknown_column ADD UNIQUE u_missing (missing);" \
    "$DATABASE"

expect_error \
    "text without prefix fails" \
    1170 \
    42000 \
    "BLOB/TEXT column 'txt' used in key specification without a key length" \
    "CREATE TABLE text_key (id INT, txt TEXT); "\
"ALTER TABLE text_key ADD UNIQUE u_txt (txt);" \
    "$DATABASE"

expect_error \
    "char zero length unique fails" \
    1167 \
    42000 \
    "The used storage engine can't index column 'c'" \
    "CREATE TABLE char_zero (c CHAR(0)); ALTER TABLE char_zero ADD UNIQUE u_c (c);" \
    "$DATABASE"

expect_error \
    "varchar zero length unique fails" \
    1167 \
    42000 \
    "The used storage engine can't index column 'v'" \
    "CREATE TABLE varchar_zero (v VARCHAR(0)); ALTER TABLE varchar_zero ADD UNIQUE u_v (v);" \
    "$DATABASE"

expect_error \
    "unquoted primary index name is syntax error" \
    1064 \
    42000 \
    "near 'PRIMARY (v)'" \
    "CREATE TABLE unquoted_primary (id INT, v INT); "\
"ALTER TABLE unquoted_primary ADD UNIQUE PRIMARY (v);" \
    "$DATABASE"

expect_upstream_accepts \
    "mysql accepts multi-action add unique" \
    "CREATE TABLE deferred_multi_action (id INT, v INT, u INT); "\
"ALTER TABLE deferred_multi_action ADD UNIQUE u_v (v), ADD UNIQUE u_u (u);" \
    "$DATABASE"

expect_upstream_accepts \
    "mysql accepts multi-column add unique" \
    "CREATE TABLE deferred_multi_part (id INT, v INT); "\
"ALTER TABLE deferred_multi_part ADD UNIQUE u_multi (id, v);" \
    "$DATABASE"

expect_upstream_accepts \
    "mysql accepts unique prefix parts" \
    "CREATE TABLE deferred_prefix (id INT, name VARCHAR(10)); "\
"ALTER TABLE deferred_prefix ADD UNIQUE u_name (name(2));" \
    "$DATABASE"

expect_upstream_accepts \
    "mysql accepts descending unique parts" \
    "CREATE TABLE deferred_desc (id INT, v INT); "\
"ALTER TABLE deferred_desc ADD UNIQUE u_v (v DESC);" \
    "$DATABASE"

expect_upstream_accepts \
    "mysql accepts add unique options" \
    "CREATE TABLE deferred_options (id INT, name VARCHAR(10)); "\
"ALTER TABLE deferred_options ADD UNIQUE u_name USING BTREE (name) COMMENT 'hello' VISIBLE;" \
    "$DATABASE"

printf '%s\n' "mysql_baseline_alter_table_add_unique_expectations: ok"
