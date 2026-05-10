#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_dml_default_expectations_$$"

fail() {
    printf '%s\n' "mysql_baseline_dml_default_keyword_values_expectations: $1" >&2
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

version=$(run_mysql 'SELECT VERSION();')
case "$version" in
    8.4.9*) ;;
    *) fail "expected MySQL 8.4.9 runtime, got [$version]" ;;
esac

cleanup
run_mysql "CREATE DATABASE ${DATABASE};" >/dev/null

expect_output \
    "strict insert values defaults" \
    "1	0
100	5	NULL	7	NULL	9" \
    "CREATE TABLE t("\
"id INT NOT NULL DEFAULT 100, i INT DEFAULT 5, ii INTEGER DEFAULT -6, "\
"iu INT UNSIGNED DEFAULT 4294967295, integeru INTEGER UNSIGNED DEFAULT 8, "\
"b BIGINT DEFAULT -9223372036854775808, bu BIGINT UNSIGNED DEFAULT 9223372036854775807, "\
"n INT NULL DEFAULT NULL, nn INT NOT NULL DEFAULT 7, plain INT NULL, nod INT NOT NULL) "\
"ENGINE=InnoDB; "\
"INSERT INTO t(id, i, n, nn, nod) VALUES(DEFAULT, DEFAULT, DEFAULT, DEFAULT, 9); "\
"SELECT ROW_COUNT(), @@warning_count; "\
"SELECT id, i, IFNULL(n, 'NULL'), nn, IFNULL(plain, 'NULL'), nod FROM t;" \
    "$DATABASE"

expect_output \
    "strict insert set defaults" \
    "1	0
100	5	NULL	7	NULL	1" \
    "TRUNCATE t; "\
"INSERT INTO t SET id = DEFAULT, i = DEFAULT, n = DEFAULT, nn = DEFAULT, nod = 1; "\
"SELECT ROW_COUNT(), @@warning_count; "\
"SELECT id, i, IFNULL(n, 'NULL'), nn, IFNULL(plain, 'NULL'), nod FROM t;" \
    "$DATABASE"

expect_output \
    "strict replace values defaults and integer families" \
    "1	0
100	5	-6	4294967295	8	-9223372036854775808	9223372036854775807	NULL	7	2" \
    "TRUNCATE t; "\
"REPLACE INTO t(id, i, ii, iu, integeru, b, bu, n, nn, nod) "\
"VALUES(DEFAULT, DEFAULT, DEFAULT, DEFAULT, DEFAULT, DEFAULT, DEFAULT, DEFAULT, DEFAULT, 2); "\
"SELECT ROW_COUNT(), @@warning_count; "\
"SELECT id, i, ii, iu, integeru, b, bu, IFNULL(n, 'NULL'), nn, nod FROM t;" \
    "$DATABASE"

expect_output \
    "strict replace set defaults" \
    "1	0
100	5	NULL	7	NULL	3" \
    "TRUNCATE t; "\
"REPLACE INTO t SET id = DEFAULT, i = DEFAULT, n = DEFAULT, nn = DEFAULT, nod = 3; "\
"SELECT ROW_COUNT(), @@warning_count; "\
"SELECT id, i, IFNULL(n, 'NULL'), nn, IFNULL(plain, 'NULL'), nod FROM t;" \
    "$DATABASE"

expect_output \
    "update default changed and no-op counts" \
    "1	0
1:5:9:9
2:5:NULL:8
0	0" \
    "TRUNCATE t; "\
"INSERT INTO t(id, i, n, nn, nod) VALUES(1, 9, 9, 9, 9), (2, 5, NULL, 7, 8); "\
"UPDATE t SET i = DEFAULT WHERE id IN (1, 2); "\
"SELECT ROW_COUNT(), @@warning_count; "\
"SELECT CONCAT(id, ':', i, ':', IFNULL(n, 'NULL'), ':', nod) FROM t ORDER BY id; "\
"UPDATE t SET i = DEFAULT WHERE id IN (1, 2); "\
"SELECT ROW_COUNT(), @@warning_count;" \
    "$DATABASE"

expect_output \
    "nullable implicit default keyword update" \
    "1	0	NULL" \
    "TRUNCATE t; "\
"INSERT INTO t(id, i, n, nn, plain, nod) VALUES(1, 1, 1, 1, 1, 1); "\
"UPDATE t SET plain = DEFAULT; "\
"SELECT ROW_COUNT(), @@warning_count, IFNULL(plain, 'NULL') FROM t;" \
    "$DATABASE"

expect_output \
    "ordered limited update default" \
    "2	0
1:5
2:4
3:5" \
    "TRUNCATE t; "\
"INSERT INTO t(id, i, n, nn, nod) VALUES(1, 3, NULL, 1, 1), (2, 4, 4, 2, 2), (3, 9, NULL, 3, 3); "\
"UPDATE t SET i = DEFAULT ORDER BY n LIMIT 2; "\
"SELECT ROW_COUNT(), @@warning_count; "\
"SELECT CONCAT(id, ':', i) FROM t ORDER BY id;" \
    "$DATABASE"

expect_error \
    "strict insert default no explicit default" \
    1364 \
    HY000 \
    "Field 'nod' doesn't have a default value" \
    "INSERT INTO t(id, i, n, nn, nod) VALUES(1, 1, 1, 1, DEFAULT);" \
    "$DATABASE"

expect_error \
    "strict update default no explicit default matched" \
    1364 \
    HY000 \
    "Field 'nod' doesn't have a default value" \
    "UPDATE t SET nod = DEFAULT WHERE id = 1;" \
    "$DATABASE"

expect_output \
    "strict update default no explicit default no match" \
    "0" \
    "UPDATE t SET nod = DEFAULT WHERE id = 999; SELECT ROW_COUNT();" \
    "$DATABASE"

expect_output \
    "strict update default no explicit default limit zero" \
    "0" \
    "UPDATE t SET nod = DEFAULT LIMIT 0; SELECT ROW_COUNT();" \
    "$DATABASE"

expect_output \
    "insert ignore explicit default adjustment rows" \
    "2
Warning	1364	Field 'id' doesn't have a default value
Warning	1364	Field 'nn' doesn't have a default value
0	7	NULL	0" \
    "CREATE TABLE ignore_t(id INT NOT NULL, d INT DEFAULT 7, n INT NULL, nn INT NOT NULL) "\
"ENGINE=InnoDB; "\
"INSERT IGNORE INTO ignore_t(id, d, n, nn) VALUES(DEFAULT, DEFAULT, DEFAULT, DEFAULT); "\
"SHOW COUNT(*) WARNINGS; "\
"SHOW WARNINGS; "\
"SELECT id, d, IFNULL(n, 'NULL'), nn FROM ignore_t;" \
    "$DATABASE"

expect_output \
    "insert ignore set explicit default adjustment rows" \
    "2
Warning	1364	Field 'id' doesn't have a default value
Warning	1364	Field 'nn' doesn't have a default value
0	7	NULL	0" \
    "TRUNCATE ignore_t; "\
"INSERT IGNORE INTO ignore_t SET id = DEFAULT, d = DEFAULT, n = DEFAULT, nn = DEFAULT; "\
"SHOW COUNT(*) WARNINGS; "\
"SHOW WARNINGS; "\
"SELECT id, d, IFNULL(n, 'NULL'), nn FROM ignore_t;" \
    "$DATABASE"

expect_error \
    "nullable dropped default strict insert" \
    1364 \
    HY000 \
    "Field 'n' doesn't have a default value" \
    "CREATE TABLE drop_t(id INT NOT NULL, n INT NULL) ENGINE=InnoDB; "\
"ALTER TABLE drop_t ALTER n DROP DEFAULT; "\
"INSERT INTO drop_t(id, n) VALUES(1, DEFAULT);" \
    "$DATABASE"

expect_error \
    "nullable dropped default strict update" \
    1364 \
    HY000 \
    "Field 'n' doesn't have a default value" \
    "INSERT INTO drop_t(id, n) VALUES(2, 9); "\
"UPDATE drop_t SET n = DEFAULT WHERE id = 2;" \
    "$DATABASE"

expect_output \
    "nullable dropped default insert ignore" \
    "2
Warning	1364	Field 'n' doesn't have a default value
Warning	1364	Field 'n' doesn't have a default value
1	NULL
2	NULL" \
    "TRUNCATE drop_t; "\
"INSERT IGNORE INTO drop_t(id, n) VALUES(1, DEFAULT), (2, DEFAULT); "\
"SHOW COUNT(*) WARNINGS; "\
"SHOW WARNINGS; "\
"SELECT id, IFNULL(n, 'NULL') FROM drop_t ORDER BY id;" \
    "$DATABASE"

expect_output \
    "mysql accepts DEFAULT column function upstream" \
    "1	0	1:5:1" \
    "TRUNCATE t; "\
"INSERT INTO t(id, i, n, nn, nod) VALUES(1, DEFAULT(i), 1, 1, 1); "\
"SELECT ROW_COUNT(), @@warning_count, CONCAT(id, ':', i, ':', nod) FROM t;" \
    "$DATABASE"

expect_error \
    "arithmetic default assignment remains expression form" \
    1064 \
    42000 \
    "You have an error in your SQL syntax" \
    "UPDATE t SET i = DEFAULT + 1;" \
    "$DATABASE"

printf '%s\n' "mysql_baseline_dml_default_keyword_values_expectations: ok"
