#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_named_unique_constraint_expectations_$$"

fail() {
    printf '%s\n' "mysql_baseline_named_unique_constraint_expectations: $1" >&2
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

named_expected=$(cat <<\EXPECTED
named_unique	CREATE TABLE `named_unique` (
  `id` int DEFAULT NULL,
  `name` int DEFAULT NULL,
  UNIQUE KEY `c_name` (`name`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
named_unique	0	c_name	1	name	A	0	NULL	NULL	YES	BTREE			YES	NULL
c_name	UNIQUE	YES
c_name	0	1	name
c_name	name	1
EXPECTED
)
expect_output \
    "named UNIQUE constraint metadata" \
    "$named_expected" \
    "CREATE TABLE named_unique (id INT, name INT, CONSTRAINT c_name UNIQUE (name)); "\
"SHOW CREATE TABLE named_unique; "\
"SHOW INDEX FROM named_unique; "\
"SELECT CONSTRAINT_NAME, CONSTRAINT_TYPE, ENFORCED "\
"FROM INFORMATION_SCHEMA.TABLE_CONSTRAINTS "\
"WHERE TABLE_SCHEMA = '${DATABASE}' AND TABLE_NAME = 'named_unique' "\
"ORDER BY CONSTRAINT_NAME; "\
"SELECT INDEX_NAME, NON_UNIQUE, SEQ_IN_INDEX, COLUMN_NAME "\
"FROM INFORMATION_SCHEMA.STATISTICS "\
"WHERE TABLE_SCHEMA = '${DATABASE}' AND TABLE_NAME = 'named_unique' "\
"ORDER BY INDEX_NAME, SEQ_IN_INDEX; "\
"SELECT CONSTRAINT_NAME, COLUMN_NAME, ORDINAL_POSITION "\
"FROM INFORMATION_SCHEMA.KEY_COLUMN_USAGE "\
"WHERE TABLE_SCHEMA = '${DATABASE}' AND TABLE_NAME = 'named_unique' "\
"ORDER BY CONSTRAINT_NAME, ORDINAL_POSITION;" \
    "$DATABASE"

variants_expected=$(cat <<\EXPECTED
no_symbol	CREATE TABLE `no_symbol` (
  `a` int DEFAULT NULL,
  UNIQUE KEY `a` (`a`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
unique_key_form	CREATE TABLE `unique_key_form` (
  `a` int DEFAULT NULL,
  UNIQUE KEY `c_key` (`a`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
unique_index_form	CREATE TABLE `unique_index_form` (
  `a` int DEFAULT NULL,
  UNIQUE KEY `c_idx` (`a`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
explicit_index_name	CREATE TABLE `explicit_index_name` (
  `a` int DEFAULT NULL,
  UNIQUE KEY `idx` (`a`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
idx
idx
EXPECTED
)
expect_output \
    "constraint UNIQUE syntax variants" \
    "$variants_expected" \
    "CREATE TABLE no_symbol (a INT, CONSTRAINT UNIQUE (a)); "\
"CREATE TABLE unique_key_form (a INT, CONSTRAINT c_key UNIQUE KEY (a)); "\
"CREATE TABLE unique_index_form (a INT, CONSTRAINT c_idx UNIQUE INDEX (a)); "\
"CREATE TABLE explicit_index_name (a INT, CONSTRAINT ignored UNIQUE KEY idx (a)); "\
"SHOW CREATE TABLE no_symbol; "\
"SHOW CREATE TABLE unique_key_form; "\
"SHOW CREATE TABLE unique_index_form; "\
"SHOW CREATE TABLE explicit_index_name; "\
"SELECT CONSTRAINT_NAME FROM INFORMATION_SCHEMA.TABLE_CONSTRAINTS "\
"WHERE TABLE_SCHEMA = '${DATABASE}' AND TABLE_NAME = 'explicit_index_name'; "\
"SELECT INDEX_NAME FROM INFORMATION_SCHEMA.STATISTICS "\
"WHERE TABLE_SCHEMA = '${DATABASE}' AND TABLE_NAME = 'explicit_index_name';" \
    "$DATABASE"

prefix_expected=$(cat <<\EXPECTED
prefix_form	CREATE TABLE `prefix_form` (
  `a` varchar(20) DEFAULT NULL,
  `b` text,
  UNIQUE KEY `c_pref` (`a`(3),`b`(2) DESC)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
prefix_form	0	c_pref	1	a	A	0	3	NULL	YES	BTREE			YES	NULL
prefix_form	0	c_pref	2	b	D	0	2	NULL	YES	BTREE			YES	NULL
EXPECTED
)
expect_output \
    "named unique constraint preserves prefix and direction metadata" \
    "$prefix_expected" \
    "CREATE TABLE prefix_form (a VARCHAR(20), b TEXT, "\
"CONSTRAINT c_pref UNIQUE KEY (a(3), b(2) DESC)); "\
"SHOW CREATE TABLE prefix_form; SHOW INDEX FROM prefix_form;" \
    "$DATABASE"

drop_expected=$(cat <<\EXPECTED
drop_named	CREATE TABLE `drop_named` (
  `a` int DEFAULT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
drop_standalone	CREATE TABLE `drop_standalone` (
  `a` int DEFAULT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
EXPECTED
)
expect_output \
    "named unique constraints drop through index names" \
    "$drop_expected" \
    "CREATE TABLE drop_named (a INT, CONSTRAINT c_drop UNIQUE (a)); "\
"CREATE TABLE drop_standalone (a INT, CONSTRAINT c_standalone UNIQUE (a)); "\
"ALTER TABLE drop_named DROP INDEX c_drop; "\
"DROP INDEX c_standalone ON drop_standalone; "\
"SHOW CREATE TABLE drop_named; SHOW CREATE TABLE drop_standalone;" \
    "$DATABASE"

nullable_expected=$(cat <<\EXPECTED
1	0
1	10
2	NULL
3	NULL
4	NULL
EXPECTED
)
expect_output \
    "named unique constraint enforces duplicates and permits duplicate NULL" \
    "$nullable_expected" \
    "CREATE TABLE nullable_named (id INT, v INT, CONSTRAINT c_v UNIQUE (v)); "\
"INSERT INTO nullable_named VALUES (1,10),(2,20),(3,NULL),(4,NULL); "\
"UPDATE nullable_named SET v = NULL WHERE id = 2; "\
"SELECT ROW_COUNT(), @@warning_count; SELECT * FROM nullable_named ORDER BY id;" \
    "$DATABASE"

expect_error \
    "duplicate insert through named unique constraint fails" \
    1062 \
    "23000" \
    "Duplicate entry '10' for key 'duplicate_named.c_v'" \
    "CREATE TABLE duplicate_named (id INT, v INT, CONSTRAINT c_v UNIQUE (v)); "\
"INSERT INTO duplicate_named VALUES (1,10); INSERT INTO duplicate_named VALUES (2,10);" \
    "$DATABASE"

expect_error \
    "duplicate same-table key name fails" \
    1061 \
    "42000" \
    "Duplicate key name 'same'" \
    "CREATE TABLE duplicate_same_table ("\
"a INT, b INT, CONSTRAINT same UNIQUE (a), UNIQUE KEY same (b));" \
    "$DATABASE"

expect_error \
    "unknown named unique key column fails" \
    1072 \
    "42000" \
    "Key column 'missing' doesn't exist in table" \
    "CREATE TABLE missing_named (a INT, CONSTRAINT c_missing UNIQUE (missing));" \
    "$DATABASE"

expect_output \
    "same named unique key can appear on different tables" \
    "2" \
    "CREATE TABLE same_one (a INT, CONSTRAINT reused UNIQUE (a)); "\
"CREATE TABLE same_two (a INT, CONSTRAINT reused UNIQUE (a)); "\
"SELECT COUNT(*) FROM INFORMATION_SCHEMA.TABLE_CONSTRAINTS "\
"WHERE TABLE_SCHEMA = '${DATABASE}' AND CONSTRAINT_NAME = 'reused';" \
    "$DATABASE"

expect_upstream_accepts \
    "MySQL accepts deferred no-keyword explicit named unique index" \
    "CREATE TABLE deferred_no_keyword_name (a INT, CONSTRAINT c UNIQUE idx (a));" \
    "$DATABASE"

expect_upstream_accepts \
    "MySQL accepts deferred ALTER ADD CONSTRAINT UNIQUE" \
    "CREATE TABLE deferred_alter_add (a INT); "\
"ALTER TABLE deferred_alter_add ADD CONSTRAINT c UNIQUE (a);" \
    "$DATABASE"

printf '%s\n' "mysql_baseline_named_unique_constraint_expectations: ok"
