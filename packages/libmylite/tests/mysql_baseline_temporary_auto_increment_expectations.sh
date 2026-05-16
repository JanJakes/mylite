#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_temporary_auto_increment_expectations_$$"

fail() {
    printf '%s\n' "mysql_baseline_temporary_auto_increment_expectations: $1" >&2
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
id	int	NO	PRI	NULL	auto_increment
v	int	YES		NULL	NULL
t	0	PRIMARY	1	id	A	0	NULL	NULL		BTREE			YES	NULL
t	CREATE TEMPORARY TABLE `t` (
  `id` int NOT NULL AUTO_INCREMENT,
  `v` int DEFAULT NULL,
  PRIMARY KEY (`id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
0
0
0
EXPECTED
)
expect_output \
    "temporary auto increment metadata and durable omission" \
    "$metadata_expected" \
    "CREATE TEMPORARY TABLE t (id INT AUTO_INCREMENT PRIMARY KEY, v INT); "\
"SHOW COLUMNS FROM t; "\
"SHOW INDEX FROM t; "\
"SHOW CREATE TABLE t; "\
"SELECT COUNT(*) FROM information_schema.tables "\
"WHERE table_schema = '${DATABASE}' AND table_name = 't'; "\
"SELECT COUNT(*) FROM information_schema.columns "\
"WHERE table_schema = '${DATABASE}' AND table_name = 't'; "\
"SELECT COUNT(*) FROM information_schema.statistics "\
"WHERE table_schema = '${DATABASE}' AND table_name = 't';" \
    "$DATABASE"

generated_expected=$(cat <<\EXPECTED
0
1	0	1
3	0	2
1:10,2:20,3:30,4:40
1	0	2
0:50,1:10,2:20,3:30,4:40
1	0	11
0:50,1:10,2:20,3:30,4:40,10:100,11:110
EXPECTED
)
expect_output \
    "temporary generated explicit and sql mode values" \
    "$generated_expected" \
    "CREATE TEMPORARY TABLE t (id INT AUTO_INCREMENT PRIMARY KEY, v INT); "\
"SELECT LAST_INSERT_ID(); "\
"INSERT INTO t (v) VALUES (10); "\
"SELECT ROW_COUNT(), @@warning_count, LAST_INSERT_ID(); "\
"INSERT INTO t VALUES (NULL,20), (0,30), (DEFAULT,40); "\
"SELECT ROW_COUNT(), @@warning_count, LAST_INSERT_ID(); "\
"SELECT GROUP_CONCAT(CONCAT(id, ':', v) ORDER BY id) FROM t; "\
"SET sql_mode = 'NO_AUTO_VALUE_ON_ZERO'; "\
"INSERT INTO t VALUES (0,50); "\
"SELECT ROW_COUNT(), @@warning_count, LAST_INSERT_ID(); "\
"SELECT GROUP_CONCAT(CONCAT(id, ':', v) ORDER BY id) FROM t; "\
"INSERT INTO t SET id = 10, v = 100; "\
"INSERT INTO t SET v = 110; "\
"SELECT ROW_COUNT(), @@warning_count, LAST_INSERT_ID(); "\
"SELECT GROUP_CONCAT(CONCAT(id, ':', v) ORDER BY id) FROM t;" \
    "$DATABASE"

option_expected=$(cat <<\EXPECTED
opt	CREATE TEMPORARY TABLE `opt` (
  `id` int NOT NULL AUTO_INCREMENT,
  `v` int DEFAULT NULL,
  PRIMARY KEY (`id`)
) ENGINE=InnoDB AUTO_INCREMENT=7 DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
1	0	7
7:70
opt	CREATE TEMPORARY TABLE `opt` (
  `id` int NOT NULL AUTO_INCREMENT,
  `v` int DEFAULT NULL,
  PRIMARY KEY (`id`)
) ENGINE=InnoDB AUTO_INCREMENT=8 DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
EXPECTED
)
expect_output \
    "temporary auto increment table option" \
    "$option_expected" \
    "CREATE TEMPORARY TABLE opt (id INT AUTO_INCREMENT PRIMARY KEY, v INT) "\
"AUTO_INCREMENT=7; "\
"SHOW CREATE TABLE opt; "\
"INSERT INTO opt(v) VALUES(70); "\
"SELECT ROW_COUNT(), @@warning_count, LAST_INSERT_ID(); "\
"SELECT GROUP_CONCAT(CONCAT(id, ':', v) ORDER BY id) FROM opt; "\
"SHOW CREATE TABLE opt;" \
    "$DATABASE"

key_backed_expected=$(cat <<\EXPECTED
table_pk	CREATE TEMPORARY TABLE `table_pk` (
  `id` int NOT NULL AUTO_INCREMENT,
  `v` int DEFAULT NULL,
  PRIMARY KEY (`id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
1	0	1
table_pk	CREATE TEMPORARY TABLE `table_pk` (
  `id` int NOT NULL AUTO_INCREMENT,
  `v` int DEFAULT NULL,
  PRIMARY KEY (`id`)
) ENGINE=InnoDB AUTO_INCREMENT=2 DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
1:10
unique_ai	0	id_uq	1	id	A	0	NULL	NULL		BTREE			YES	NULL
1:20
key_ai	1	id_key	1	id	A	0	NULL	NULL		BTREE			YES	NULL
1:30
EXPECTED
)
expect_output \
    "temporary table-level and secondary-key backed auto increment" \
    "$key_backed_expected" \
    "CREATE TEMPORARY TABLE table_pk "\
"(id INT AUTO_INCREMENT, v INT, PRIMARY KEY(id)) AUTO_INCREMENT=0; "\
"SHOW CREATE TABLE table_pk; "\
"INSERT INTO table_pk(v) VALUES(10); "\
"SELECT ROW_COUNT(), @@warning_count, LAST_INSERT_ID(); "\
"SHOW CREATE TABLE table_pk; "\
"SELECT GROUP_CONCAT(CONCAT(id, ':', v) ORDER BY id) FROM table_pk; "\
"CREATE TEMPORARY TABLE unique_ai "\
"(id INT AUTO_INCREMENT, v INT, UNIQUE KEY id_uq (id)); "\
"SHOW INDEX FROM unique_ai; "\
"INSERT INTO unique_ai(v) VALUES(20); "\
"SELECT GROUP_CONCAT(CONCAT(id, ':', v) ORDER BY id) FROM unique_ai; "\
"CREATE TEMPORARY TABLE key_ai "\
"(id INT AUTO_INCREMENT, v INT, KEY id_key (id)); "\
"SHOW INDEX FROM key_ai; "\
"INSERT INTO key_ai(v) VALUES(30); "\
"SELECT GROUP_CONCAT(CONCAT(id, ':', v) ORDER BY id) FROM key_ai;" \
    "$DATABASE"

insert_select_expected=$(cat <<\EXPECTED
1	0	1
1	0	2
1	0	3
1:40,2:50,3:60
EXPECTED
)
expect_output \
    "temporary row scalar insert select auto increment" \
    "$insert_select_expected" \
    "CREATE TEMPORARY TABLE sel (id INT AUTO_INCREMENT PRIMARY KEY, v INT); "\
"INSERT INTO sel(v) SELECT 40 FROM DUAL; "\
"SELECT ROW_COUNT(), @@warning_count, LAST_INSERT_ID(); "\
"INSERT INTO sel(id, v) SELECT NULL, 50 FROM DUAL; "\
"SELECT ROW_COUNT(), @@warning_count, LAST_INSERT_ID(); "\
"INSERT INTO sel(id, v) SELECT 0, 60 FROM DUAL; "\
"SELECT ROW_COUNT(), @@warning_count, LAST_INSERT_ID(); "\
"SELECT GROUP_CONCAT(CONCAT(id, ':', v) ORDER BY id) FROM sel;" \
    "$DATABASE"

like_expected=$(cat <<\EXPECTED
tmp_like	CREATE TEMPORARY TABLE `tmp_like` (
  `id` int NOT NULL AUTO_INCREMENT,
  `v` int DEFAULT NULL,
  PRIMARY KEY (`id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
1
1:1
persistent_from_tmp	CREATE TABLE `persistent_from_tmp` (
  `id` int NOT NULL AUTO_INCREMENT,
  `v` int DEFAULT NULL,
  PRIMARY KEY (`id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
1
1:2
temp_from_temp	CREATE TEMPORARY TABLE `temp_from_temp` (
  `id` int NOT NULL AUTO_INCREMENT,
  `v` int DEFAULT NULL,
  PRIMARY KEY (`id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
1
1:3
EXPECTED
)
expect_output \
    "temporary and persistent LIKE clone counters reset" \
    "$like_expected" \
    "CREATE TABLE src (id INT AUTO_INCREMENT PRIMARY KEY, v INT) AUTO_INCREMENT=7; "\
"INSERT INTO src(v) VALUES(70); "\
"CREATE TEMPORARY TABLE tmp_like LIKE src; "\
"SHOW CREATE TABLE tmp_like; "\
"INSERT INTO tmp_like(v) VALUES(1); "\
"SELECT LAST_INSERT_ID(); "\
"SELECT GROUP_CONCAT(CONCAT(id, ':', v) ORDER BY id) FROM tmp_like; "\
"CREATE TEMPORARY TABLE tmp_src (id INT AUTO_INCREMENT PRIMARY KEY, v INT) "\
"AUTO_INCREMENT=9; "\
"INSERT INTO tmp_src(v) VALUES(90); "\
"CREATE TABLE persistent_from_tmp LIKE tmp_src; "\
"SHOW CREATE TABLE persistent_from_tmp; "\
"INSERT INTO persistent_from_tmp(v) VALUES(2); "\
"SELECT LAST_INSERT_ID(); "\
"SELECT GROUP_CONCAT(CONCAT(id, ':', v) ORDER BY id) FROM persistent_from_tmp; "\
"CREATE TEMPORARY TABLE temp_from_temp LIKE tmp_src; "\
"SHOW CREATE TABLE temp_from_temp; "\
"INSERT INTO temp_from_temp(v) VALUES(3); "\
"SELECT LAST_INSERT_ID(); "\
"SELECT GROUP_CONCAT(CONCAT(id, ':', v) ORDER BY id) FROM temp_from_temp;" \
    "$DATABASE"

transaction_expected=$(cat <<\EXPECTED
inside	1	1
1:10
after_rollback	1
0	NULL
after_insert	1	2
2:20
EXPECTED
)
expect_output \
    "temporary auto increment transaction rollback keeps counter advanced" \
    "$transaction_expected" \
    "CREATE TEMPORARY TABLE tx (id INT AUTO_INCREMENT PRIMARY KEY, v INT); "\
"START TRANSACTION; "\
"INSERT INTO tx(v) VALUES(10); "\
"SELECT 'inside', ROW_COUNT(), LAST_INSERT_ID(); "\
"SELECT GROUP_CONCAT(CONCAT(id, ':', v) ORDER BY id) FROM tx; "\
"ROLLBACK; "\
"SELECT 'after_rollback', LAST_INSERT_ID(); "\
"SELECT COUNT(*), GROUP_CONCAT(CONCAT(id, ':', v) ORDER BY id) FROM tx; "\
"INSERT INTO tx(v) VALUES(20); "\
"SELECT 'after_insert', ROW_COUNT(), LAST_INSERT_ID(); "\
"SELECT GROUP_CONCAT(CONCAT(id, ':', v) ORDER BY id) FROM tx;" \
    "$DATABASE"

update_expected=$(cat <<\EXPECTED
1	0	1
1	0	6
5:10,6:20
1	0	6
7
-1:10,6:20,7:30
EXPECTED
)
expect_output \
    "temporary update advances auto increment counter" \
    "$update_expected" \
    "CREATE TEMPORARY TABLE u (id INT AUTO_INCREMENT PRIMARY KEY, v INT); "\
"INSERT INTO u(v) VALUES(10); "\
"UPDATE u SET id=5 WHERE id=1; "\
"SELECT ROW_COUNT(), @@warning_count, LAST_INSERT_ID(); "\
"INSERT INTO u(v) VALUES(20); "\
"SELECT ROW_COUNT(), @@warning_count, LAST_INSERT_ID(); "\
"SELECT GROUP_CONCAT(CONCAT(id, ':', v) ORDER BY id) FROM u; "\
"UPDATE u SET id=-1 WHERE id=5; "\
"SELECT ROW_COUNT(), @@warning_count, LAST_INSERT_ID(); "\
"INSERT INTO u(v) VALUES(30); "\
"SELECT LAST_INSERT_ID(); "\
"SELECT GROUP_CONCAT(CONCAT(id, ':', v) ORDER BY id) FROM u;" \
    "$DATABASE"

expect_error \
    "temporary auto increment without key" \
    1075 \
    42000 \
    "there can be only one auto column and it must be defined as a key" \
    "CREATE TEMPORARY TABLE no_key (id INT AUTO_INCREMENT, v INT);" \
    "$DATABASE"

expect_error \
    "temporary varchar auto increment" \
    1063 \
    42000 \
    "Incorrect column specifier for column 'id'" \
    "CREATE TEMPORARY TABLE varchar_auto (id VARCHAR(3) AUTO_INCREMENT PRIMARY KEY);" \
    "$DATABASE"

expect_error \
    "temporary negative auto increment table option" \
    1064 \
    42000 \
    "right syntax to use near '-1'" \
    "CREATE TEMPORARY TABLE negative_option (id INT AUTO_INCREMENT PRIMARY KEY) "\
"AUTO_INCREMENT=-1;" \
    "$DATABASE"

boundary_expected=$(cat <<\EXPECTED
1	255
255
EXPECTED
)
expect_output \
    "temporary tinyint unsigned auto increment maximum" \
    "$boundary_expected" \
    "CREATE TEMPORARY TABLE tiny_t "\
"(id TINYINT UNSIGNED AUTO_INCREMENT PRIMARY KEY, v INT) AUTO_INCREMENT=255; "\
"INSERT INTO tiny_t(v) VALUES(1); "\
"SELECT ROW_COUNT(), LAST_INSERT_ID(); "\
"SELECT id FROM tiny_t;" \
    "$DATABASE"

expect_error \
    "temporary tinyint unsigned auto increment exhaustion" \
    1062 \
    23000 \
    "Duplicate entry '255' for key 'tiny_t.PRIMARY'" \
    "CREATE TEMPORARY TABLE tiny_t "\
"(id TINYINT UNSIGNED AUTO_INCREMENT PRIMARY KEY, v INT) AUTO_INCREMENT=255; "\
"INSERT INTO tiny_t(v) VALUES(1); "\
"INSERT INTO tiny_t(v) VALUES(2);" \
    "$DATABASE"
