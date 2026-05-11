#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_auto_increment_expectations_$$"

fail() {
    printf '%s\n' "mysql_baseline_auto_increment_lifecycle_expectations: $1" >&2
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

expect_show_table_status_auto_increment() {
    label=$1
    table_name=$2
    expected=$3
    shift 3

    output=$(
        run_mysql "SHOW TABLE STATUS LIKE '${table_name}';" "$@" \
            | awk -F '\t' '{ print $11 }'
    )
    if [ "$output" != "$expected" ]; then
        fail "$label: expected SHOW TABLE STATUS Auto_increment [$expected], got [$output]"
    fi
}

expect_show_table_status_auto_increment_after_sql() {
    label=$1
    table_name=$2
    expected=$3
    sql=$4
    shift 4

    output=$(
        run_mysql "${sql} SHOW TABLE STATUS LIKE '${table_name}';" "$@" \
            | awk -F '\t' 'NF > 10 { value = $11 } END { print value }'
    )
    if [ "$output" != "$expected" ]; then
        fail "$label: expected SHOW TABLE STATUS Auto_increment [$expected], got [$output]"
    fi
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

show_columns_expected=$(printf '%b' 'id\tint\tNO\tPRI\tNULL\tauto_increment\nv\tint\tYES\t\tNULL\t')
show_index_expected=$(cat <<\EXPECTED
t	0	PRIMARY	1	id	A	0	NULL	NULL		BTREE			YES	NULL
EXPECTED
)
show_create_expected=$(cat <<\EXPECTED
t	CREATE TABLE `t` (
  `id` int NOT NULL AUTO_INCREMENT,
  `v` int DEFAULT NULL,
  PRIMARY KEY (`id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
EXPECTED
)
expect_output \
    "auto increment metadata" \
    "$show_columns_expected
$show_index_expected
$show_create_expected" \
    "CREATE TABLE t (id INT AUTO_INCREMENT PRIMARY KEY, v INT); "\
"SHOW COLUMNS FROM t; "\
"SHOW INDEX FROM t; "\
"SHOW CREATE TABLE t;" \
    "$DATABASE"
expect_show_table_status_auto_increment "initial auto increment status" "t" "1" "$DATABASE"

insert_expected=$(cat <<\EXPECTED
0
1	0	1
1:10
3	0	2
1:10,2:20,3:30,4:40
1	0	2
1	0	11
1:10,2:20,3:30,4:40,10:100,11:110
t	CREATE TABLE `t` (
  `id` int NOT NULL AUTO_INCREMENT,
  `v` int DEFAULT NULL,
  PRIMARY KEY (`id`)
) ENGINE=InnoDB AUTO_INCREMENT=12 DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
EXPECTED
)
expect_output \
    "generated and explicit insert ids" \
    "$insert_expected" \
    "SELECT LAST_INSERT_ID(); "\
"INSERT INTO t (v) VALUES (10); "\
"SELECT ROW_COUNT(), @@warning_count, LAST_INSERT_ID(); "\
"SELECT GROUP_CONCAT(CONCAT(id, ':', v) ORDER BY id) FROM t; "\
"INSERT INTO t VALUES (NULL,20), (0,30), (DEFAULT,40); "\
"SELECT ROW_COUNT(), @@warning_count, LAST_INSERT_ID(); "\
"SELECT GROUP_CONCAT(CONCAT(id, ':', v) ORDER BY id) FROM t; "\
"INSERT INTO t VALUES (10,100); "\
"SELECT ROW_COUNT(), @@warning_count, LAST_INSERT_ID(); "\
"INSERT INTO t (v) VALUES (110); "\
"SELECT ROW_COUNT(), @@warning_count, LAST_INSERT_ID(); "\
"SELECT GROUP_CONCAT(CONCAT(id, ':', v) ORDER BY id) FROM t; "\
"SHOW CREATE TABLE t;" \
    "$DATABASE"
expect_show_table_status_auto_increment_after_sql \
    "advanced auto increment status" \
    "status_t" \
    "12" \
    "CREATE TABLE status_t (id INT AUTO_INCREMENT PRIMARY KEY, v INT); "\
"INSERT INTO status_t (v) VALUES (10); "\
"INSERT INTO status_t VALUES (NULL,20), (0,30), (DEFAULT,40); "\
"INSERT INTO status_t VALUES (10,100); "\
"INSERT INTO status_t (v) VALUES (110);" \
    "$DATABASE"

insert_set_expected=$(cat <<\EXPECTED
1	0	1
1:10
1	0	6
1:10,5:50,6:60
EXPECTED
)
expect_output \
    "insert set generated and explicit ids" \
    "$insert_set_expected" \
    "CREATE TABLE set_t (id INT AUTO_INCREMENT PRIMARY KEY, v INT); "\
"INSERT INTO set_t SET v = 10; "\
"SELECT ROW_COUNT(), @@warning_count, LAST_INSERT_ID(); "\
"SELECT GROUP_CONCAT(CONCAT(id, ':', v) ORDER BY id) FROM set_t; "\
"INSERT INTO set_t SET id = 5, v = 50; "\
"INSERT INTO set_t SET v = 60; "\
"SELECT ROW_COUNT(), @@warning_count, LAST_INSERT_ID(); "\
"SELECT GROUP_CONCAT(CONCAT(id, ':', v) ORDER BY id) FROM set_t;" \
    "$DATABASE"

update_expected=$(cat <<\EXPECTED
1	0	1
1	0	6
5:10,6:20
EXPECTED
)
expect_output \
    "update auto increment column advances counter" \
    "$update_expected" \
    "CREATE TABLE update_t (id INT AUTO_INCREMENT PRIMARY KEY, v INT); "\
"INSERT INTO update_t (v) VALUES (10); "\
"UPDATE update_t SET id = 5 WHERE id = 1; "\
"SELECT ROW_COUNT(), @@warning_count, LAST_INSERT_ID(); "\
"INSERT INTO update_t (v) VALUES (20); "\
"SELECT ROW_COUNT(), @@warning_count, LAST_INSERT_ID(); "\
"SELECT GROUP_CONCAT(CONCAT(id, ':', v) ORDER BY id) FROM update_t;" \
    "$DATABASE"

hidden_default_columns_expected=$(printf '%b' 'id\tint\tNO\tPRI\tNULL\tauto_increment\nv\tint\tYES\t\tNULL\t')
expect_output \
    "alter set default on auto increment hides metadata default" \
    "$hidden_default_columns_expected" \
    "CREATE TABLE default_set (id INT AUTO_INCREMENT PRIMARY KEY, v INT); "\
"ALTER TABLE default_set ALTER id SET DEFAULT 7; "\
"SHOW COLUMNS FROM default_set;" \
    "$DATABASE"

hidden_default_expected=$(cat <<\EXPECTED
default_set	CREATE TABLE `default_set` (
  `id` int NOT NULL AUTO_INCREMENT,
  `v` int DEFAULT NULL,
  PRIMARY KEY (`id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
1	0
7:70
default_set	CREATE TABLE `default_set` (
  `id` int NOT NULL AUTO_INCREMENT,
  `v` int DEFAULT NULL,
  PRIMARY KEY (`id`)
) ENGINE=InnoDB AUTO_INCREMENT=8 DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
EXPECTED
)
expect_output \
    "alter set default on auto increment is explicit hidden default" \
    "$hidden_default_expected" \
    "SHOW CREATE TABLE default_set; "\
"INSERT INTO default_set (v) VALUES (70); "\
"SELECT ROW_COUNT(), @@warning_count; "\
"SELECT GROUP_CONCAT(CONCAT(id, ':', v) ORDER BY id) FROM default_set; "\
"SHOW CREATE TABLE default_set;" \
    "$DATABASE"
expect_show_table_status_auto_increment \
    "alter set default on auto increment advances status after explicit default" \
    "default_set" \
    "8" \
    "$DATABASE"
expect_error \
    "alter set default on auto increment duplicate default" \
    1062 \
    23000 \
    "Duplicate entry '7' for key 'default_set.PRIMARY'" \
    "INSERT INTO default_set (v) VALUES (80);" \
    "$DATABASE"

option_expected=$(cat <<\EXPECTED
opt	CREATE TABLE `opt` (
  `id` int NOT NULL AUTO_INCREMENT,
  `v` int DEFAULT NULL,
  PRIMARY KEY (`id`)
) ENGINE=InnoDB AUTO_INCREMENT=7 DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
1	0	7
7:70
like_opt	CREATE TABLE `like_opt` (
  `id` int NOT NULL AUTO_INCREMENT,
  `v` int DEFAULT NULL,
  PRIMARY KEY (`id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
1
1:1
1
1:90
EXPECTED
)
expect_output \
    "table option like and truncate counters" \
    "$option_expected" \
    "CREATE TABLE opt (id INT AUTO_INCREMENT PRIMARY KEY, v INT) AUTO_INCREMENT=7; "\
"SHOW CREATE TABLE opt; "\
"INSERT INTO opt (v) VALUES (70); "\
"SELECT ROW_COUNT(), @@warning_count, LAST_INSERT_ID(); "\
"SELECT GROUP_CONCAT(CONCAT(id, ':', v) ORDER BY id) FROM opt; "\
"CREATE TABLE like_opt LIKE opt; "\
"SHOW CREATE TABLE like_opt; "\
"INSERT INTO like_opt (v) VALUES (1); "\
"SELECT LAST_INSERT_ID(); "\
"SELECT GROUP_CONCAT(CONCAT(id, ':', v) ORDER BY id) FROM like_opt; "\
"TRUNCATE TABLE opt; "\
"INSERT INTO opt (v) VALUES (90); "\
"SELECT LAST_INSERT_ID(); "\
"SELECT GROUP_CONCAT(CONCAT(id, ':', v) ORDER BY id) FROM opt;" \
    "$DATABASE"

ignore_expected=$(cat <<\EXPECTED
1
0	1	1
1:10
EXPECTED
)
expect_output \
    "insert ignore explicit duplicate preserves insert id" \
    "$ignore_expected" \
    "CREATE TABLE ignore_t (id INT AUTO_INCREMENT PRIMARY KEY, v INT); "\
"INSERT INTO ignore_t (v) VALUES (10); "\
"SELECT LAST_INSERT_ID(); "\
"INSERT IGNORE INTO ignore_t VALUES (1, 11); "\
"SELECT ROW_COUNT(), @@warning_count, LAST_INSERT_ID(); "\
"SELECT GROUP_CONCAT(CONCAT(id, ':', v) ORDER BY id) FROM ignore_t;" \
    "$DATABASE"

exhaustion_expected=$(cat <<\EXPECTED
1	255
255
EXPECTED
)
expect_output \
    "tinyint unsigned auto increment maximum" \
    "$exhaustion_expected" \
    "CREATE TABLE tiny_t (id TINYINT UNSIGNED AUTO_INCREMENT PRIMARY KEY, v INT) "\
"AUTO_INCREMENT=255; "\
"INSERT INTO tiny_t (v) VALUES (1); "\
"SELECT ROW_COUNT(), LAST_INSERT_ID(); "\
"SELECT id FROM tiny_t;" \
    "$DATABASE"

expect_error \
    "tinyint unsigned auto increment exhaustion" \
    1062 \
    23000 \
    "Duplicate entry '255' for key 'tiny_t.PRIMARY'" \
    "INSERT INTO tiny_t (v) VALUES (2);" \
    "$DATABASE"

expect_show_table_status_auto_increment_after_sql \
    "tinyint unsigned initial counter beyond maximum status" \
    "tiny_over" \
    "256" \
    "CREATE TABLE tiny_over (id TINYINT UNSIGNED AUTO_INCREMENT PRIMARY KEY, v INT) "\
"AUTO_INCREMENT=256;" \
    "$DATABASE"
expect_error \
    "tinyint unsigned initial counter beyond maximum insert" \
    1467 \
    HY000 \
    "Failed to read auto-increment value from storage engine" \
    "INSERT INTO tiny_over (v) VALUES (1);" \
    "$DATABASE"

expect_error \
    "auto increment without key" \
    1075 \
    42000 \
    "there can be only one auto column and it must be defined as a key" \
    "CREATE TABLE no_key (id INT AUTO_INCREMENT, v INT);" \
    "$DATABASE"

expect_error \
    "varchar auto increment" \
    1063 \
    42000 \
    "Incorrect column specifier for column 'id'" \
    "CREATE TABLE varchar_auto (id VARCHAR(3) AUTO_INCREMENT PRIMARY KEY);" \
    "$DATABASE"

expect_error \
    "auto increment default" \
    1067 \
    42000 \
    "Invalid default value for 'id'" \
    "CREATE TABLE default_auto (id INT AUTO_INCREMENT DEFAULT 7 PRIMARY KEY);" \
    "$DATABASE"

expect_error \
    "nullable auto increment primary key" \
    1171 \
    42000 \
    "All parts of a PRIMARY KEY must be NOT NULL" \
    "CREATE TABLE nullable_auto (id INT NULL AUTO_INCREMENT PRIMARY KEY);" \
    "$DATABASE"

expect_error \
    "multiple auto increment columns" \
    1075 \
    42000 \
    "there can be only one auto column and it must be defined as a key" \
    "CREATE TABLE two_auto (id INT AUTO_INCREMENT PRIMARY KEY, n INT AUTO_INCREMENT);" \
    "$DATABASE"

expect_error \
    "negative auto increment table option" \
    1064 \
    42000 \
    "right syntax to use near '-1'" \
    "CREATE TABLE negative_option (id INT AUTO_INCREMENT PRIMARY KEY) AUTO_INCREMENT=-1;" \
    "$DATABASE"

expect_upstream_accepts \
    "mysql accepts mixed-mode multi-row auto increment inserts deferred by MyLite" \
    "CREATE TABLE mixed_t (id INT AUTO_INCREMENT PRIMARY KEY, v INT); "\
"INSERT INTO mixed_t VALUES (1, 10), (NULL, 20), (5, 50), (NULL, 60);" \
    "$DATABASE"
