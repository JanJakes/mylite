#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_numeric_format_truncate_crc32_expectations_$$"

fail() {
    printf '%s\n' "mysql_baseline_numeric_format_truncate_crc32_expectations: $1" >&2
    exit 1
}

run_mysql() {
    sql=$1
    shift
    printf '%s\n' "$sql" \
        | docker exec -i "$MYSQL_CONTAINER" mysql --protocol=TCP -h127.0.0.1 -uroot --batch --raw --skip-column-names "$@"
}

run_mysql_error() {
    sql=$1
    shift
    printf '%s\n' "$sql" \
        | docker exec -i "$MYSQL_CONTAINER" mysql --protocol=TCP -h127.0.0.1 -uroot --batch --raw --skip-column-names "$@" 2>&1
}

expect_value() {
    label=$1
    expected=$2
    actual=$3

    if [ "$actual" != "$expected" ]; then
        fail "$label: expected [$expected], got [$actual]"
    fi
}

expect_mysql_error() {
    label=$1
    sql=$2
    expected=$3

    output=$(run_mysql_error "$sql") && status=0 || status=$?
    if [ "$status" -eq 0 ]; then
        fail "$label: expected MySQL error, got success [$output]"
    fi
    case "$output" in
        *"$expected"*) ;;
        *) fail "$label: expected error containing [$expected], got [$output]" ;;
    esac
}

cleanup() {
    run_mysql "DROP DATABASE IF EXISTS ${DATABASE};" >/dev/null 2>&1 || true
}

trap cleanup EXIT

version=$(run_mysql 'SELECT VERSION();')
case "$version" in
    8.4.9*) ;;
    *) fail "expected MySQL 8.4.9 runtime, got [$version]" ;;
esac

cleanup
run_mysql "CREATE DATABASE ${DATABASE}; USE ${DATABASE};
           CREATE TABLE metrics(
               id INT,
               amount DECIMAL(8,3),
               places INT,
               label VARCHAR(20),
               payload VARBINARY(20)
           );
           CREATE TABLE sort_metrics(
               id INT,
               amount DECIMAL(8,3),
               places INT
           );
           INSERT INTO metrics VALUES
               (1,1234.555,2,'MySQL',X'616263'),
               (2,-1.004,2,'mysql',NULL),
               (3,NULL,NULL,NULL,X'');
           INSERT INTO sort_metrics VALUES
               (1,2.0,0),
               (2,10.0,0),
               (3,-1.0,0);" \
    >/dev/null

crc32_values=$(run_mysql \
    "SELECT CRC32('MySQL'), CRC32('mysql'), CRC32(''), CRC32(NULL), "\
"CRC32(123), CRC32(TRUE), CRC32(FALSE), CRC32(-1), CRC32(X'616263');")
expect_value "crc32 values" \
    "3259397556	2501908538	0	NULL	2286445522	2212294583	4108050209	808273962	891568578" \
    "$crc32_values"

format_values=$(run_mysql \
    "SELECT FORMAT(12332.123456,4), FORMAT(12332.1,4), FORMAT(12332.2,0), "\
"FORMAT(-12332.555,2), FORMAT(NULL,2), FORMAT(123,NULL), FORMAT(123.55,-1), "\
"FORMAT(123.55,31), FORMAT(999.995,2), FORMAT(999.994,2), FORMAT(-999.995,2), "\
"FORMAT(1,TRUE), FORMAT(1,FALSE);")
expect_value "format values" \
    "12,332.1235	12,332.1000	12,332	-12,332.56	NULL	NULL	124	123.550000000000000000000000000000	1,000.00	999.99	-1,000.00	1.0	1" \
    "$format_values"

truncate_values=$(run_mysql \
    "SELECT TRUNCATE(1.223,1), TRUNCATE(1.999,1), TRUNCATE(1.999,0), "\
"TRUNCATE(-1.999,1), TRUNCATE(122,-2), TRUNCATE(NULL,1), TRUNCATE(122,NULL), "\
"TRUNCATE(123.456,31), TRUNCATE(123.456,-31), TRUNCATE(1234,2), "\
"TRUNCATE(1234.000,2), TRUNCATE(1234.1,4), TRUNCATE(1234.100,2), "\
"TRUNCATE(1.9,TRUE), TRUNCATE(1.9,FALSE);")
expect_value "truncate values" \
    "1.2	1.9	1	-1.9	100	NULL	NULL	123.456	0	1234	1234.00	1234.1	1234.10	1.9	1" \
    "$truncate_values"

row_values=$(run_mysql \
    "SELECT id, CRC32(label), CRC32(payload), FORMAT(amount, places), "\
"TRUNCATE(amount, places), CONCAT('c=', CRC32(label)), PI(), CONCAT('p=', PI()) "\
"FROM metrics ORDER BY id;" \
    "$DATABASE")
expect_value "row-backed numeric functions" \
    "1	3259397556	891568578	1,234.56	1234.550	c=3259397556	3.141593	p=3.141593
2	2501908538	NULL	-1.00	-1.000	c=2501908538	3.141593	p=3.141593
3	NULL	0	NULL	NULL	NULL	3.141593	p=3.141593" \
    "$row_values"

crc32_predicates=$(run_mysql \
    "SELECT id FROM metrics "\
"WHERE CRC32(label)=3259397556 OR CRC32(payload)=0 ORDER BY id;" \
    "$DATABASE")
expect_value "crc32 predicates" \
    "1
3" \
    "$crc32_predicates"

format_predicates=$(run_mysql \
    "SELECT id FROM metrics "\
"WHERE FORMAT(amount,places)='1,234.56' OR FORMAT(amount,places) IS NULL ORDER BY id;" \
    "$DATABASE")
expect_value "format predicates" \
    "1
3" \
    "$format_predicates"

truncate_predicates=$(run_mysql \
    "SELECT id FROM metrics WHERE TRUNCATE(amount,places) IS NULL ORDER BY id;" \
    "$DATABASE")
expect_value "truncate predicates" "3" "$truncate_predicates"

pi_predicates=$(run_mysql \
    "SELECT id FROM metrics WHERE PI()>3 AND PI()<4 ORDER BY id;" \
    "$DATABASE")
expect_value "pi predicates" \
    "1
2
3" \
    "$pi_predicates"

truncate_numeric_predicates=$(run_mysql \
    "SELECT id FROM sort_metrics WHERE TRUNCATE(amount,places)<3 ORDER BY id;" \
    "$DATABASE")
expect_value "truncate numeric predicates" \
    "1
3" \
    "$truncate_numeric_predicates"

crc32_order=$(run_mysql \
    "SELECT id FROM metrics ORDER BY CRC32(label), id;" \
    "$DATABASE")
expect_value "crc32 order" \
    "3
2
1" \
    "$crc32_order"

format_order=$(run_mysql \
    "SELECT id FROM metrics ORDER BY FORMAT(amount,places), id;" \
    "$DATABASE")
expect_value "format order" \
    "3
2
1" \
    "$format_order"

truncate_order=$(run_mysql \
    "SELECT id FROM sort_metrics ORDER BY TRUNCATE(amount,places), id;" \
    "$DATABASE")
expect_value "truncate order" \
    "3
1
2" \
    "$truncate_order"

pi_order=$(run_mysql \
    "SELECT id FROM metrics ORDER BY PI(), id DESC;" \
    "$DATABASE")
expect_value "pi order" \
    "3
2
1" \
    "$pi_order"

status=$(run_mysql "SELECT CRC32('ok'), FORMAT(1,2), TRUNCATE(1.9,0); SELECT @@warning_count, ROW_COUNT();" | tail -n 1)
expect_value "supported status" "0	-1" "$status"

warning_values=$(run_mysql \
    "SELECT FORMAT('abc',2), TRUNCATE('abc',2); SHOW WARNINGS;")
expect_value "mysql deferred string coercion warnings" \
    "0.00	0
Warning	1292	Truncated incorrect DOUBLE value: 'abc'
Warning	1292	Truncated incorrect DOUBLE value: 'abc'" \
    "$warning_values"

locale_warning=$(run_mysql \
    "SELECT FORMAT(12332.2,2,NULL); SHOW WARNINGS;")
expect_value "mysql deferred locale warning" \
    "12,332.20
Warning	1649	Unknown locale: 'NULL'" \
    "$locale_warning"

deferred_broader_forms=$(run_mysql \
    "SELECT FORMAT(1,2.6), FORMAT(1,2,'de_DE'), CRC32(1+2);")
expect_value "mysql deferred broader forms" "1.000	1,00	1842515611" "$deferred_broader_forms"

expect_mysql_error "crc32 missing argument" \
    "SELECT CRC32();" \
    "ERROR 1582 (42000)"
expect_mysql_error "crc32 too many arguments" \
    "SELECT CRC32(1,2);" \
    "ERROR 1582 (42000)"
expect_mysql_error "format one argument syntax" \
    "SELECT FORMAT(1);" \
    "ERROR 1064 (42000)"
expect_mysql_error "truncate one argument syntax" \
    "SELECT TRUNCATE(1);" \
    "ERROR 1064 (42000)"

printf '%s\n' "mysql_baseline_numeric_format_truncate_crc32_expectations: ok"
