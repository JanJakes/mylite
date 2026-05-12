#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_serial_alias_expectations_$$"

fail() {
    printf '%s\n' "mysql_baseline_serial_alias_lifecycle_expectations: $1" >&2
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

serial_metadata_expected=$(cat <<\EXPECTED
id	bigint unsigned	NO	PRI	NULL	auto_increment
v	int	YES		NULL	
serial_t	0	id	1	id	A	0	NULL	NULL		BTREE			YES	NULL
serial_t	CREATE TABLE `serial_t` (
  `id` bigint unsigned NOT NULL AUTO_INCREMENT,
  `v` int DEFAULT NULL,
  UNIQUE KEY `id` (`id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
id	bigint unsigned	NO	PRI	auto_increment
v	int	YES		
id	UNIQUE
id	id	1
id	0	1	id	
EXPECTED
)
expect_output \
    "serial metadata" \
    "$serial_metadata_expected" \
    "CREATE TABLE serial_t (id SERIAL, v INT); "\
"SHOW COLUMNS FROM serial_t; "\
"SHOW INDEX FROM serial_t; "\
"SHOW CREATE TABLE serial_t; "\
"SELECT COLUMN_NAME, COLUMN_TYPE, IS_NULLABLE, COLUMN_KEY, EXTRA "\
"FROM INFORMATION_SCHEMA.COLUMNS "\
"WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = 'serial_t' "\
"ORDER BY ORDINAL_POSITION; "\
"SELECT CONSTRAINT_NAME, CONSTRAINT_TYPE "\
"FROM INFORMATION_SCHEMA.TABLE_CONSTRAINTS "\
"WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = 'serial_t' "\
"ORDER BY CONSTRAINT_NAME; "\
"SELECT CONSTRAINT_NAME, COLUMN_NAME, ORDINAL_POSITION "\
"FROM INFORMATION_SCHEMA.KEY_COLUMN_USAGE "\
"WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = 'serial_t' "\
"ORDER BY CONSTRAINT_NAME, ORDINAL_POSITION; "\
"SELECT INDEX_NAME, NON_UNIQUE, SEQ_IN_INDEX, COLUMN_NAME, NULLABLE "\
"FROM INFORMATION_SCHEMA.STATISTICS "\
"WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = 'serial_t' "\
"ORDER BY INDEX_NAME, SEQ_IN_INDEX;" \
    "$DATABASE"

serial_dml_expected=$(cat <<\EXPECTED
2	0	1
1:10,2:20
1	0	1
1	0	11
1:10,2:20,10:100,11:110
serial_t	CREATE TABLE `serial_t` (
  `id` bigint unsigned NOT NULL AUTO_INCREMENT,
  `v` int DEFAULT NULL,
  UNIQUE KEY `id` (`id`)
) ENGINE=InnoDB AUTO_INCREMENT=12 DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
EXPECTED
)
expect_output \
    "serial generated and explicit inserts" \
    "$serial_dml_expected" \
    "INSERT INTO serial_t (v) VALUES (10), (20); "\
"SELECT ROW_COUNT(), @@warning_count, LAST_INSERT_ID(); "\
"SELECT GROUP_CONCAT(CONCAT(id, ':', v) ORDER BY id) FROM serial_t; "\
"INSERT INTO serial_t VALUES (10,100); "\
"SELECT ROW_COUNT(), @@warning_count, LAST_INSERT_ID(); "\
"INSERT INTO serial_t (v) VALUES (110); "\
"SELECT ROW_COUNT(), @@warning_count, LAST_INSERT_ID(); "\
"SELECT GROUP_CONCAT(CONCAT(id, ':', v) ORDER BY id) FROM serial_t; "\
"SHOW CREATE TABLE serial_t;" \
    "$DATABASE"

serial_forms_expected=$(cat <<\EXPECTED
id	bigint unsigned	YES	UNI	NULL	auto_increment
v	int	YES		NULL	
serial_null	CREATE TABLE `serial_null` (
  `id` bigint unsigned AUTO_INCREMENT,
  `v` int DEFAULT NULL,
  UNIQUE KEY `id` (`id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
serial_primary	CREATE TABLE `serial_primary` (
  `id` bigint unsigned NOT NULL AUTO_INCREMENT,
  `v` int DEFAULT NULL,
  PRIMARY KEY (`id`),
  UNIQUE KEY `id` (`id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
serial_key	CREATE TABLE `serial_key` (
  `id` bigint unsigned NOT NULL AUTO_INCREMENT,
  `v` int DEFAULT NULL,
  UNIQUE KEY `id` (`id`),
  KEY `id_2` (`id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
9:90	9
serial_option	CREATE TABLE `serial_option` (
  `id` bigint unsigned NOT NULL AUTO_INCREMENT,
  `v` int DEFAULT NULL,
  UNIQUE KEY `id` (`id`)
) ENGINE=InnoDB AUTO_INCREMENT=10 DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
EXPECTED
)
expect_output \
    "serial forms" \
    "$serial_forms_expected" \
    "CREATE TABLE serial_null (id SERIAL NULL, v INT); "\
"SHOW COLUMNS FROM serial_null; "\
"SHOW CREATE TABLE serial_null; "\
"CREATE TABLE serial_primary (id SERIAL PRIMARY KEY, v INT); "\
"SHOW CREATE TABLE serial_primary; "\
"CREATE TABLE serial_key (id SERIAL, v INT, KEY(id)); "\
"SHOW CREATE TABLE serial_key; "\
"CREATE TABLE serial_option (id SERIAL, v INT) AUTO_INCREMENT=9; "\
"INSERT INTO serial_option (v) VALUES (90); "\
"SELECT GROUP_CONCAT(CONCAT(id, ':', v) ORDER BY id), LAST_INSERT_ID() FROM serial_option; "\
"SHOW CREATE TABLE serial_option;" \
    "$DATABASE"

secondary_auto_expected=$(cat <<\EXPECTED
id	int	NO	PRI	NULL	auto_increment
v	int	YES		NULL	
auto_unique	CREATE TABLE `auto_unique` (
  `id` int NOT NULL AUTO_INCREMENT,
  `v` int DEFAULT NULL,
  UNIQUE KEY `id` (`id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
id	int	NO	MUL	NULL	auto_increment
v	int	YES		NULL	
auto_key	CREATE TABLE `auto_key` (
  `id` int NOT NULL AUTO_INCREMENT,
  `v` int DEFAULT NULL,
  KEY `id` (`id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
1:10,2:20
1:10,2:20
EXPECTED
)
expect_output \
    "secondary key auto increment" \
    "$secondary_auto_expected" \
    "CREATE TABLE auto_unique (id INT AUTO_INCREMENT UNIQUE, v INT); "\
"SHOW COLUMNS FROM auto_unique; "\
"SHOW CREATE TABLE auto_unique; "\
"CREATE TABLE auto_key (id INT AUTO_INCREMENT, KEY(id), v INT); "\
"SHOW COLUMNS FROM auto_key; "\
"SHOW CREATE TABLE auto_key; "\
"INSERT INTO auto_unique (v) VALUES (10), (20); "\
"SELECT GROUP_CONCAT(CONCAT(id, ':', v) ORDER BY id) FROM auto_unique; "\
"INSERT INTO auto_key (v) VALUES (10), (20); "\
"SELECT GROUP_CONCAT(CONCAT(id, ':', v) ORDER BY id) FROM auto_key;" \
    "$DATABASE"

expect_error \
    "unindexed auto increment rejected" \
    1075 \
    42000 \
    "there can be only one auto column and it must be defined as a key" \
    "CREATE TABLE no_key (id INT AUTO_INCREMENT, v INT);" \
    "$DATABASE"

expect_error \
    "serial invalid default rejected" \
    1067 \
    42000 \
    "Invalid default value for 'id'" \
    "CREATE TABLE serial_default_int (id SERIAL DEFAULT 7, v INT);" \
    "$DATABASE"

expect_error \
    "serial default value syntax rejected" \
    1064 \
    42000 \
    "near 'VALUE" \
    "CREATE TABLE serial_default_value (id SERIAL DEFAULT VALUE, v INT);" \
    "$DATABASE"

expect_error \
    "two auto increment columns rejected" \
    1075 \
    42000 \
    "there can be only one auto column and it must be defined as a key" \
    "CREATE TABLE two_auto (id SERIAL, other INT AUTO_INCREMENT, KEY(other));" \
    "$DATABASE"

expect_upstream_accepts \
    "integer serial default value deferred" \
    "CREATE TABLE serial_default_value_attr (id INT SERIAL DEFAULT VALUE, v INT);" \
    "$DATABASE"
