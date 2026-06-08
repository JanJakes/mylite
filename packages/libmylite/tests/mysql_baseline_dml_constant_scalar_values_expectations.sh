#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_dml_constant_scalar_values_$$"

fail() {
    printf '%s\n' "mysql_baseline_dml_constant_scalar_values_expectations: $1" >&2
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
        *) fail "$label: expected error $code/$state containing [$message], got [$output]" ;;
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
run_mysql "CREATE DATABASE ${DATABASE}; USE ${DATABASE};" >/dev/null

scalar_expected=$(cat <<'EXPECTED'
insert	1	0	1	abb	414243	42	12.34	25	1	2024-05-06 07:08:09	01:01:01
duplicate	2	0	1	abbZ	7	7.89
replace	1	0	2	AB	7879	5	1.50	2	2	2024-01-02 00:00:00	00:00:59
update	1	0	2	ABQ	4445	8	2024-05-07 08:09:10	00:00:59
introducers	1	0	AB	4344
auto	1	0	1:ai:10,5:explicit:20,6:next:30
EXPECTED
)
expect_output \
    "constant scalar dml values" \
    "$scalar_expected" \
    "CREATE TABLE scalars("\
"id INT PRIMARY KEY, "\
"v VARCHAR(32) UNIQUE, "\
"b VARBINARY(32), "\
"i INT, "\
"d DECIMAL(6,2), "\
"f DOUBLE, "\
"js JSON, "\
"dt DATETIME, "\
"tm TIME) ENGINE=InnoDB; "\
"INSERT INTO scalars(id, v, b, i, d, f, js, dt, tm) VALUES "\
"(1, CONCAT(_utf8mb4'a', REPEAT('b', 2)), CONVERT('ABC' USING BINARY), "\
"CONVERT('42' USING utf8mb4), CONVERT('12.34' USING utf8mb4), "\
"CONVERT('2.5e1' USING utf8mb4), _utf8mb4'{\"a\": 1}', "\
"STR_TO_DATE('2024-05-06 07:08:09', '%Y-%m-%d %H:%i:%s'), SEC_TO_TIME(3661)); "\
"SELECT 'insert', ROW_COUNT(), @@warning_count, id, v, HEX(b), i, d, f + 0, "\
"JSON_UNQUOTE(JSON_EXTRACT(js, '$.a')), dt, tm FROM scalars; "\
"INSERT INTO scalars(id, v, i, d) VALUES (3, 'abb', 1, 1) "\
"ON DUPLICATE KEY UPDATE v = CONCAT('abb', 'Z'), i = CONVERT('7' USING utf8mb4), "\
"d = CONVERT('7.89' USING utf8mb4); "\
"SELECT 'duplicate', ROW_COUNT(), @@warning_count, id, v, i, d "\
"FROM scalars WHERE id = 1; "\
"REPLACE INTO scalars(id, v, b, i, d, f, js, dt, tm) VALUES "\
"(2, CONCAT('A', REPEAT('B', 1)), _latin1 0x7879, "\
"CONVERT('5' USING utf8mb4), CONVERT('1.50' USING utf8mb4), "\
"CONVERT('2.0' USING utf8mb4), _utf8mb4'{\"b\": 2}', DATE '2024-01-02', "\
"SEC_TO_TIME(59)); "\
"SELECT 'replace', ROW_COUNT(), @@warning_count, id, v, HEX(b), i, d, f + 0, "\
"JSON_UNQUOTE(JSON_EXTRACT(js, '$.b')), dt, tm FROM scalars WHERE id = 2; "\
"UPDATE scalars SET v = CONCAT('AB', 'Q'), b = CONVERT('DE' USING BINARY), "\
"i = CONVERT('8' USING utf8mb4), "\
"dt = STR_TO_DATE('2024-05-07 08:09:10', '%Y-%m-%d %H:%i:%s'), "\
"tm = SEC_TO_TIME(59) WHERE id = 2; "\
"SELECT 'update', ROW_COUNT(), @@warning_count, id, v, HEX(b), i, dt, tm "\
"FROM scalars WHERE id = 2; "\
"CREATE TABLE intro(c VARCHAR(10), b VARBINARY(10)) ENGINE=InnoDB; "\
"INSERT INTO intro VALUES (_latin1 0x4142, _utf8mb4 0x4344); "\
"SELECT 'introducers', ROW_COUNT(), @@warning_count, c, HEX(b) FROM intro; "\
"CREATE TABLE auto_generated("\
"id INT AUTO_INCREMENT PRIMARY KEY, label VARCHAR(20), v INT) ENGINE=InnoDB; "\
"INSERT INTO auto_generated(label, v) "\
"VALUES (CONCAT('a', REPEAT('i', 1)), CONVERT('10' USING utf8mb4)); "\
"INSERT INTO auto_generated(id, label, v) "\
"VALUES (CONVERT('5' USING utf8mb4), _utf8mb4'explicit', "\
"CONVERT('20' USING utf8mb4)); "\
"INSERT INTO auto_generated(label, v) VALUES (_utf8mb4'next', "\
"CONVERT('30' USING utf8mb4)); "\
"SELECT 'auto', ROW_COUNT(), @@warning_count, "\
"GROUP_CONCAT(CONCAT(id, ':', label, ':', v) ORDER BY id) FROM auto_generated;" \
    "$DATABASE"

run_mysql "CREATE TABLE ints(id INT PRIMARY KEY, v INT NOT NULL) ENGINE=InnoDB;" "$DATABASE" \
    >/dev/null

ignore_expected=$(cat <<'EXPECTED'
ignore	1	1	7	0
EXPECTED
)
expect_output \
    "constant scalar dml ignore conversion warning" \
    "$ignore_expected" \
    "INSERT IGNORE INTO ints VALUES (7, CONVERT('abc' USING utf8mb4)); "\
"SELECT 'ignore', ROW_COUNT(), @@warning_count, id, v FROM ints WHERE id = 7;" \
    "$DATABASE"

expect_error \
    "constant scalar dml strict conversion error" \
    1366 \
    HY000 \
    "Incorrect integer value: 'abc' for column 'v' at row 1" \
    "INSERT INTO ints VALUES (8, CONVERT('abc' USING utf8mb4));" \
    "$DATABASE"

printf '%s\n' "mysql_baseline_dml_constant_scalar_values_expectations: ok"
