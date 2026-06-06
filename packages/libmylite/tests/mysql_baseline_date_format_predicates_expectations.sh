#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_date_format_predicates_expectations_$$"

fail() {
    printf '%s\n' "mysql_baseline_date_format_predicates_expectations: $1" >&2
    exit 1
}

run_mysql() {
    sql=$1
    shift
    printf '%s\n' "$sql" \
        | docker exec -i "$MYSQL_CONTAINER" mysql --protocol=TCP -h127.0.0.1 -uroot \
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
run_mysql "CREATE DATABASE ${DATABASE}; USE ${DATABASE}; SET SESSION sql_mode = '';" >/dev/null

predicate_expected=$(cat <<EXPECTED
1,5
1,5
NULL
2
1,2,5
2
2
2
1,5
1,2
2
2
5
Warning	1292	Incorrect datetime value: 'not-a-date'
1
EXPECTED
)
expect_output \
    "date_format predicate values" \
    "$predicate_expected" \
    "SET SESSION sql_mode = ''; "\
"CREATE TABLE options(id INT, option_value VARCHAR(32), d DATE NULL, dt DATETIME NULL, "\
"ts TIMESTAMP NULL, tm TIME NULL); "\
"INSERT INTO options VALUES "\
"(1,'2008-01-02 00:42:00','2008-01-02','2008-01-02 00:42:00',"\
"'2008-01-02 00:42:00','00:42:00'),"\
"(2,'2008-01-02 13:29:17','2008-01-02','2008-01-02 13:29:17',"\
"'2008-01-02 13:29:17','13:29:17'),"\
"(3,NULL,NULL,NULL,NULL,NULL),"\
"(4,'not-a-date',NULL,NULL,NULL,NULL),"\
"(5,'2008-01-02 00:42:59',NULL,NULL,NULL,NULL); "\
"SELECT GROUP_CONCAT(id ORDER BY id) FROM options "\
"WHERE DATE_FORMAT(option_value, '%H.%i') = 0.42; "\
"SELECT GROUP_CONCAT(id ORDER BY id) FROM options "\
"WHERE DATE_FORMAT(option_value, '%H.%i') = +0.42; "\
"SELECT GROUP_CONCAT(id ORDER BY id) FROM options "\
"WHERE DATE_FORMAT(option_value, '%H.%i') = -0.42; "\
"SELECT GROUP_CONCAT(id ORDER BY id) FROM options "\
"WHERE DATE_FORMAT(option_value, '%H.%i') = 13.29; "\
"SELECT GROUP_CONCAT(id ORDER BY id) FROM options "\
"WHERE DATE_FORMAT(option_value, '%H.%i') >= 0.42; "\
"SELECT GROUP_CONCAT(id ORDER BY id) FROM options "\
"WHERE DATE_FORMAT(option_value, '%H.%i') >= 9.00; "\
"SELECT GROUP_CONCAT(id ORDER BY id) FROM options "\
"WHERE DATE_FORMAT(option_value, '%H.%i') >= 9.00 "\
"AND DATE_FORMAT(option_value, '%H.%i') <= 17.00; "\
"SELECT GROUP_CONCAT(id ORDER BY id) FROM options "\
"WHERE DATE_FORMAT(option_value, '%H.%i') <> 0.42; "\
"SELECT GROUP_CONCAT(id ORDER BY id) FROM options "\
"WHERE DATE_FORMAT(option_value, '%H.%i') < 1.00; "\
"SELECT GROUP_CONCAT(id ORDER BY id) FROM options WHERE DATE_FORMAT(d, '%H.%i') = 0.00; "\
"SELECT GROUP_CONCAT(id ORDER BY id) FROM options WHERE DATE_FORMAT(dt, '%H.%i') = 13.29; "\
"SELECT GROUP_CONCAT(id ORDER BY id) FROM options WHERE DATE_FORMAT(ts, '%H.%i') = 13.29; "\
"SELECT id FROM options WHERE DATE_FORMAT(option_value, '%H.%i') = 0.42 "\
"ORDER BY id DESC LIMIT 1; "\
"SHOW WARNINGS; SELECT @@warning_count;" \
    "$DATABASE"

warning_expected=$(cat <<EXPECTED
3
Warning	1292	Incorrect datetime value: 'not-a-date'
1
EXPECTED
)
expect_output \
    "date_format predicate invalid warnings" \
    "$warning_expected" \
    "SET SESSION sql_mode = ''; "\
"CREATE TABLE bad(id INT, option_value VARCHAR(32)); "\
"INSERT INTO bad VALUES (1,'not-a-date'),(2,NULL),(3,'2008-01-02 00:42:00'); "\
"SELECT GROUP_CONCAT(id ORDER BY id) FROM bad "\
"WHERE DATE_FORMAT(option_value, '%H.%i') = 0.42; "\
"SHOW WARNINGS; SELECT @@warning_count;" \
    "$DATABASE"
