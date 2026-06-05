#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_temporal_extract_predicates_expectations_$$"

fail() {
    printf '%s\n' "mysql_baseline_temporal_extract_predicates_expectations: $1" >&2
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
1,2
1
1,2
1,3
1
1
1
1,2
1,2
3
2
1
1
2
1
3
1
1
2
1
3
1,2
3
4
4
1,2,3
3
2
1,2
2
0
EXPECTED
)
expect_output \
    "temporal extract predicate values" \
    "$predicate_expected" \
    "SET SESSION sql_mode = ''; "\
"CREATE TABLE t(id INT, d DATE NULL, dt DATETIME NULL, tm TIME NULL, txt VARCHAR(32)); "\
"INSERT INTO t VALUES "\
"(1,'2008-01-02','2008-01-02 13:29:17','13:29:17','2008-01-02 13:29:17.123456'),"\
"(2,'2008-02-03','2008-02-03 00:42:00','00:42:00','2008-02-03 00:42:00'),"\
"(3,'0000-00-00','0000-00-00 01:02:03','-13:29:17','0000-00-00 01:02:03'),"\
"(4,NULL,NULL,NULL,NULL); "\
"CREATE TABLE other(id INT, tag INT); "\
"INSERT INTO other VALUES (1,5),(2,7),(3,5),(5,5); "\
"SELECT GROUP_CONCAT(id ORDER BY id) FROM t WHERE YEAR(d) = 2008; "\
"SELECT GROUP_CONCAT(id ORDER BY id) FROM t WHERE MONTH(dt) = 1 AND DAYOFMONTH(dt) = 2; "\
"SELECT GROUP_CONCAT(id ORDER BY id) FROM t WHERE HOUR(tm) = 0 OR MINUTE(dt) = 29; "\
"SELECT GROUP_CONCAT(id ORDER BY id) FROM t WHERE SECOND(tm) <=> 17; "\
"SELECT GROUP_CONCAT(id ORDER BY id) FROM t WHERE DAY(d) = 2; "\
"SELECT GROUP_CONCAT(id ORDER BY id) FROM t WHERE DAYOFWEEK(d) = 4; "\
"SELECT GROUP_CONCAT(id ORDER BY id) FROM t WHERE DAYOFYEAR(d) = 2; "\
"SELECT GROUP_CONCAT(id ORDER BY id) FROM t WHERE YEAR(d) BETWEEN 2000 AND 2010; "\
"SELECT GROUP_CONCAT(id ORDER BY id) FROM t WHERE DAYOFMONTH(dt) BETWEEN 2 AND 3; "\
"SELECT GROUP_CONCAT(id ORDER BY id) FROM t WHERE YEAR(d) NOT BETWEEN 2000 AND 2010; "\
"SELECT GROUP_CONCAT(id ORDER BY id) FROM t WHERE WEEK(d, 3) = 5; "\
"SELECT GROUP_CONCAT(id ORDER BY id) FROM t WHERE WEEKDAY(d) = 2; "\
"SELECT GROUP_CONCAT(id ORDER BY id) FROM t WHERE WEEKOFYEAR(d) = 1; "\
"SELECT GROUP_CONCAT(id ORDER BY id) FROM t WHERE YEARWEEK(d, 3) = 200805; "\
"SELECT GROUP_CONCAT(id ORDER BY id) FROM t WHERE MICROSECOND(txt) = 123456; "\
"SELECT GROUP_CONCAT(id ORDER BY id) FROM t WHERE TIME_TO_SEC(tm) < 0; "\
"SELECT GROUP_CONCAT(id ORDER BY id) FROM t WHERE TO_DAYS(d) = 733408; "\
"SELECT GROUP_CONCAT(id ORDER BY id) FROM t WHERE TO_SECONDS(dt) = 63366499757; "\
"SELECT GROUP_CONCAT(id ORDER BY id) FROM t WHERE EXTRACT(MONTH FROM d) = 2; "\
"SELECT GROUP_CONCAT(id ORDER BY id) FROM t WHERE EXTRACT(DAY_HOUR FROM dt) = 213; "\
"SELECT GROUP_CONCAT(id ORDER BY id) FROM t WHERE EXTRACT(HOUR_SECOND FROM tm) = -132917; "\
"SELECT GROUP_CONCAT(id ORDER BY id) FROM t WHERE YEAR(d); "\
"SELECT GROUP_CONCAT(id ORDER BY id) FROM t WHERE NOT YEAR(d); "\
"SELECT GROUP_CONCAT(id ORDER BY id) FROM t WHERE YEAR(d) <=> NULL; "\
"SELECT GROUP_CONCAT(id ORDER BY id) FROM t WHERE YEAR(d) IS NULL; "\
"SELECT GROUP_CONCAT(id ORDER BY id) FROM t WHERE YEAR(d) IS NOT NULL ORDER BY id; "\
"SELECT id FROM t WHERE QUARTER(d) <= 1 ORDER BY id DESC LIMIT 2; "\
"SELECT GROUP_CONCAT(t.id ORDER BY t.id) FROM t JOIN other ON t.id = other.id "\
"WHERE YEAR(t.d) = 2008; "\
"SELECT GROUP_CONCAT(t.id ORDER BY t.id) FROM t LEFT JOIN other ON t.id = other.id "\
"WHERE YEAR(t.d) = 2008 AND MONTH(t.dt) = 2 AND other.tag IN (7) "\
"GROUP BY t.id ORDER BY t.id; "\
"SHOW WARNINGS; SELECT @@warning_count;" \
    "$DATABASE"

invalid_expected=$(cat <<EXPECTED
2,3
Warning	1292	Incorrect datetime value: 'not-a-date'
1
1
Warning	1292	Incorrect datetime value: 'not-a-date'
1
EXPECTED
)
expect_output \
    "temporal extract predicate invalid strings" \
    "$invalid_expected" \
    "SET SESSION sql_mode = ''; "\
"CREATE TABLE bad(id INT, txt VARCHAR(32)); "\
"INSERT INTO bad VALUES (1,'2008-01-02'),(2,'not-a-date'),(3,NULL),(4,'0000-00-00'); "\
"SELECT GROUP_CONCAT(id ORDER BY id) FROM bad WHERE YEAR(txt) IS NULL; "\
"SHOW WARNINGS; SELECT @@warning_count; "\
"SELECT GROUP_CONCAT(id ORDER BY id) FROM bad WHERE YEAR(txt) = 2008; "\
"SHOW WARNINGS; SELECT @@warning_count;" \
    "$DATABASE"

deferred_expected=$(cat <<EXPECTED
1
1
1
EXPECTED
)
expect_output \
    "temporal extract deferred mysql shapes accepted upstream" \
    "$deferred_expected" \
    "SELECT 2008 = YEAR('2008-01-02'); "\
"SELECT YEAR('2008-01-02') IN (2008); "\
"SELECT YEAR('2008-01-02') BETWEEN 2000 AND 2010;" \
    "$DATABASE"
