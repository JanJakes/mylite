#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_auto_increment_step_expectations_$$"
TAB=$(printf '\t')

fail() {
    printf '%s\n' "mysql_baseline_auto_increment_step_system_variables_expectations: $1" >&2
    exit 1
}

run_mysql() {
    sql=$1
    shift
    printf '%s\n' "$sql" \
        | docker exec -i "$MYSQL_CONTAINER" mysql \
            --protocol=TCP \
            -h127.0.0.1 \
            -uroot \
            --batch \
            --raw \
            --skip-column-names \
            --default-character-set=utf8mb4 \
            "$@"
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

normalize_tsv() {
    sed "s/${TAB}/|/g"
}

reset_global_defaults() {
    run_mysql \
        "SET GLOBAL auto_increment_increment = 1; SET GLOBAL auto_increment_offset = 1;" \
        >/dev/null 2>&1 || true
}

cleanup() {
    run_mysql "DROP DATABASE IF EXISTS ${DATABASE};" >/dev/null 2>&1 || true
    reset_global_defaults
}

trap cleanup EXIT HUP INT TERM
cleanup

version=$(run_mysql "SELECT VERSION();")
case "$version" in
    8.4.9*) ;;
    *) fail "expected MySQL 8.4.9 runtime, got [$version]" ;;
esac

run_mysql "CREATE DATABASE ${DATABASE};" >/dev/null

expect_output \
    "default scalar readback" \
    "1${TAB}1${TAB}1${TAB}1" \
    "SELECT @@auto_increment_increment, @@SESSION.auto_increment_increment, "\
"@@LOCAL.auto_increment_offset, @@GLOBAL.auto_increment_offset;"

expect_output \
    "case and quoted scalar readback" \
    "1${TAB}1${TAB}1" \
    "SELECT @@AUTO_INCREMENT_INCREMENT, @@session.\`auto_increment_increment\`, "\
"@@\`auto_increment_offset\`;"

show_default=$(run_mysql "SHOW VARIABLES LIKE 'auto_inc%';" | normalize_tsv)
if [ "$show_default" != "auto_increment_increment|1
auto_increment_offset|1" ]; then
    fail "show variables default: got [$show_default]"
fi

show_session=$(run_mysql "SHOW SESSION VARIABLES LIKE 'auto_inc%';" | normalize_tsv)
if [ "$show_session" != "auto_increment_increment|1
auto_increment_offset|1" ]; then
    fail "show session variables default: got [$show_session]"
fi

show_global=$(run_mysql "SHOW GLOBAL VARIABLES LIKE 'auto_inc%';" | normalize_tsv)
if [ "$show_global" != "auto_increment_increment|1
auto_increment_offset|1" ]; then
    fail "show global variables default: got [$show_global]"
fi

where_rows=$(
    run_mysql \
        "SHOW VARIABLES WHERE Variable_name IN ('auto_increment_increment','auto_increment_offset') "\
"AND Value = '1';" \
        | normalize_tsv
)
if [ "$where_rows" != "auto_increment_increment|1
auto_increment_offset|1" ]; then
    fail "show variables where default: got [$where_rows]"
fi

expect_output \
    "session set forms" \
    "7${TAB}5${TAB}0${TAB}0${TAB}0" \
    "SET auto_increment_increment = 7; "\
"SET SESSION auto_increment_offset = 5; "\
"SELECT @@auto_increment_increment, @@auto_increment_offset, @@warning_count, @@error_count, "\
"ROW_COUNT();"

expect_output \
    "local and system set forms" \
    "11${TAB}3${TAB}0${TAB}0${TAB}0" \
    "SET LOCAL auto_increment_increment = +11; "\
"SET @@SESSION.auto_increment_offset = 3; "\
"SELECT @@SESSION.auto_increment_increment, @@LOCAL.auto_increment_offset, @@warning_count, "\
"@@error_count, ROW_COUNT();"

expect_output \
    "default set forms" \
    "1${TAB}1${TAB}0" \
    "SET @@auto_increment_increment = DEFAULT; "\
"SET @@LOCAL.auto_increment_offset = DEFAULT; "\
"SELECT @@auto_increment_increment, @@auto_increment_offset, @@warning_count;"

expect_output \
    "true and false assignments" \
    "1${TAB}0
Warning${TAB}1292${TAB}Truncated incorrect auto_increment_offset value: '0'
1${TAB}1" \
    "SET @@auto_increment_increment = TRUE; "\
"SELECT @@auto_increment_increment, @@warning_count; "\
"SET @@auto_increment_offset = FALSE; "\
"SHOW WARNINGS; "\
"SELECT @@auto_increment_offset, @@warning_count;"

expect_output \
    "clamped assignments" \
    "Warning${TAB}1292${TAB}Truncated incorrect auto_increment_increment value: '0'
1${TAB}1
Warning${TAB}1292${TAB}Truncated incorrect auto_increment_offset value: '-1'
1${TAB}1
Warning${TAB}1292${TAB}Truncated incorrect auto_increment_increment value: '65536'
65535${TAB}1" \
    "SET @@auto_increment_increment = 0; "\
"SHOW WARNINGS; "\
"SELECT @@auto_increment_increment, @@warning_count; "\
"SET @@auto_increment_offset = -1; "\
"SHOW WARNINGS; "\
"SELECT @@auto_increment_offset, @@warning_count; "\
"SET @@auto_increment_increment = 65536; "\
"SHOW WARNINGS; "\
"SELECT @@auto_increment_increment, @@warning_count;"

expect_error \
    "string assignment rejected" \
    1232 \
    42000 \
    "Incorrect argument type to variable 'auto_increment_increment'" \
    "SET @@auto_increment_increment = '5';"

expect_error \
    "decimal assignment rejected" \
    1232 \
    42000 \
    "Incorrect argument type to variable 'auto_increment_offset'" \
    "SET @@auto_increment_offset = 1.5;"

expect_error \
    "null assignment rejected" \
    1232 \
    42000 \
    "Incorrect argument type to variable 'auto_increment_offset'" \
    "SET @@auto_increment_offset = NULL;"

expect_output \
    "upstream global assignment changes global only" \
    "2${TAB}1${TAB}0" \
    "SET @@GLOBAL.auto_increment_increment = 2; "\
"SELECT @@GLOBAL.auto_increment_increment, @@SESSION.auto_increment_increment, @@warning_count; "\
"SET @@GLOBAL.auto_increment_increment = 1;"

step_expected=$(cat <<\EXPECTED
5	5,15,25,35
t	CREATE TABLE `t` (
  `id` int NOT NULL AUTO_INCREMENT,
  `v` int DEFAULT NULL,
  PRIMARY KEY (`id`)
) ENGINE=InnoDB AUTO_INCREMENT=45 DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
45	5,15,25,35,45,55
explicit	CREATE TABLE `explicit` (
  `id` int NOT NULL AUTO_INCREMENT,
  `v` int DEFAULT NULL,
  PRIMARY KEY (`id`)
) ENGINE=InnoDB AUTO_INCREMENT=101 DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
105	100,105
EXPECTED
)
expect_output \
    "increment offset generated and explicit insert allocation" \
    "$step_expected" \
    "SET @@auto_increment_increment=10; "\
"SET @@auto_increment_offset=5; "\
"CREATE TABLE t(id INT AUTO_INCREMENT PRIMARY KEY, v INT); "\
"INSERT INTO t(v) VALUES(1),(2),(3),(4); "\
"SELECT LAST_INSERT_ID(), GROUP_CONCAT(id ORDER BY id) FROM t; "\
"SHOW CREATE TABLE t; "\
"INSERT INTO t(v) VALUES(5),(6); "\
"SELECT LAST_INSERT_ID(), GROUP_CONCAT(id ORDER BY id) FROM t; "\
"CREATE TABLE explicit(id INT AUTO_INCREMENT PRIMARY KEY, v INT); "\
"INSERT INTO explicit(id,v) VALUES(100,1); "\
"SHOW CREATE TABLE explicit; "\
"INSERT INTO explicit(v) VALUES(2); "\
"SELECT LAST_INSERT_ID(), GROUP_CONCAT(id ORDER BY id) FROM explicit;" \
    "$DATABASE"

option_expected=$(cat <<\EXPECTED
opt6	CREATE TABLE `opt6` (
  `id` int NOT NULL AUTO_INCREMENT,
  PRIMARY KEY (`id`)
) ENGINE=InnoDB AUTO_INCREMENT=6 DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
15	15
opt6	CREATE TABLE `opt6` (
  `id` int NOT NULL AUTO_INCREMENT,
  PRIMARY KEY (`id`)
) ENGINE=InnoDB AUTO_INCREMENT=25 DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
opt100	CREATE TABLE `opt100` (
  `id` int NOT NULL AUTO_INCREMENT,
  PRIMARY KEY (`id`)
) ENGINE=InnoDB AUTO_INCREMENT=100 DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
105	105
EXPECTED
)
expect_output \
    "table option lower bound allocation" \
    "$option_expected" \
    "SET @@auto_increment_increment=10; "\
"SET @@auto_increment_offset=5; "\
"CREATE TABLE opt6(id INT AUTO_INCREMENT PRIMARY KEY) AUTO_INCREMENT=6; "\
"SHOW CREATE TABLE opt6; "\
"INSERT INTO opt6 VALUES(NULL); "\
"SELECT LAST_INSERT_ID(), GROUP_CONCAT(id ORDER BY id) FROM opt6; "\
"SHOW CREATE TABLE opt6; "\
"CREATE TABLE opt100(id INT AUTO_INCREMENT PRIMARY KEY) AUTO_INCREMENT=100; "\
"SHOW CREATE TABLE opt100; "\
"INSERT INTO opt100 VALUES(NULL); "\
"SELECT LAST_INSERT_ID(), GROUP_CONCAT(id ORDER BY id) FROM opt100;" \
    "$DATABASE"

alter_update_expected=$(cat <<\EXPECTED
altered	CREATE TABLE `altered` (
  `id` int NOT NULL AUTO_INCREMENT,
  `v` int DEFAULT NULL,
  PRIMARY KEY (`id`)
) ENGINE=InnoDB AUTO_INCREMENT=16 DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
25	5,15,25
1	0	15,25,100
altered	CREATE TABLE `altered` (
  `id` int NOT NULL AUTO_INCREMENT,
  `v` int DEFAULT NULL,
  PRIMARY KEY (`id`)
) ENGINE=InnoDB AUTO_INCREMENT=105 DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
105	15,25,100,105
EXPECTED
)
expect_output \
    "alter and update allocation" \
    "$alter_update_expected" \
    "SET @@auto_increment_increment=10; "\
"SET @@auto_increment_offset=5; "\
"CREATE TABLE altered(id INT AUTO_INCREMENT PRIMARY KEY, v INT); "\
"INSERT INTO altered(v) VALUES(1),(2); "\
"ALTER TABLE altered AUTO_INCREMENT=6; "\
"SHOW CREATE TABLE altered; "\
"INSERT INTO altered(v) VALUES(3); "\
"SELECT LAST_INSERT_ID(), GROUP_CONCAT(id ORDER BY id) FROM altered; "\
"UPDATE altered SET id=100 WHERE id=5; "\
"SELECT ROW_COUNT(), @@warning_count, GROUP_CONCAT(id ORDER BY id) FROM altered; "\
"SHOW CREATE TABLE altered; "\
"INSERT INTO altered(v) VALUES(4); "\
"SELECT LAST_INSERT_ID(), GROUP_CONCAT(id ORDER BY id) FROM altered;" \
    "$DATABASE"

offset_gt_increment=$(
    run_mysql \
        "SET @@auto_increment_increment=10; "\
"SET @@auto_increment_offset=20; "\
"CREATE TABLE deferred_offset(id INT AUTO_INCREMENT PRIMARY KEY); "\
"INSERT INTO deferred_offset VALUES(NULL),(NULL),(NULL); "\
"SELECT @@auto_increment_increment, @@auto_increment_offset, LAST_INSERT_ID(), "\
"GROUP_CONCAT(id ORDER BY id) FROM deferred_offset;" \
        "$DATABASE"
)
if [ "$offset_gt_increment" != "10${TAB}20${TAB}4${TAB}4,14,20" ]; then
    fail "upstream offset greater than increment observation changed: got [$offset_gt_increment]"
fi
