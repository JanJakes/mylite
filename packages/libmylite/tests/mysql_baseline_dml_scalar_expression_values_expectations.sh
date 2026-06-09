#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_dml_scalar_expression_values_$$"

fail() {
    printf '%s\n' "mysql_baseline_dml_scalar_expression_values_expectations: $1" >&2
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
insert	1	0	1	7	beta	4142	12.00	8	1	2024-01-02 03:04:05	00:00:59	1
duplicate	2	0	1	8	dup	1
replace	1	0	2	10	x-b	4546	3.00	3	b	2024-01-03 04:05:06	00:01:01	1
update	1	0	1	4	b	4344	19.00	0	true	2024-01-02 03:04:06	00:01:00	2
ignore	1	1	3	0
EXPECTED
)
expect_output \
    "scalar expression dml values" \
    "$scalar_expected" \
    "CREATE TABLE exprs("\
"id INT PRIMARY KEY, "\
"i INT, "\
"v VARCHAR(64), "\
"b VARBINARY(16), "\
"d DECIMAL(8,2), "\
"f DOUBLE, "\
"js JSON, "\
"dt DATETIME, "\
"tm TIME, "\
"flag INT) ENGINE=InnoDB; "\
"INSERT INTO exprs(id, i, v, b, d, f, js, dt, tm, flag) VALUES "\
"(1, 1 + 2 * 3, GREATEST('alpha', 'beta'), UNHEX('4142'), ABS(-12), "\
"POW(2, 3), JSON_OBJECT('a', 1), "\
"DATE_ADD('2024-01-02 03:04:04', INTERVAL 1 SECOND), SEC_TO_TIME(59), IF(1, 1, 0)); "\
"SELECT 'insert', ROW_COUNT(), @@warning_count, id, i, v, HEX(b), d, f + 0, "\
"JSON_UNQUOTE(JSON_EXTRACT(js, '$.a')), dt, tm, flag FROM exprs WHERE id = 1; "\
"INSERT INTO exprs(id, i, v, b, d, f, js, dt, tm, flag) VALUES "\
"(1, 0, 'ignored', X'', 0, 0, JSON_OBJECT(), '2024-01-01', '00:00:00', 0) "\
"ON DUPLICATE KEY UPDATE i = GREATEST(5, 8), v = GREATEST('abc', 'dup'), "\
"flag = IF(1, 1, 0); "\
"SELECT 'duplicate', ROW_COUNT(), @@warning_count, id, i, v, flag "\
"FROM exprs WHERE id = 1; "\
"REPLACE INTO exprs(id, i, v, b, d, f, js, dt, tm, flag) VALUES "\
"(2, IF(1, 10, 20), CONCAT_WS('-', 'x', 'b'), UNHEX('4546'), "\
"FLOOR(3), LOG2(8), JSON_ARRAY(2, 'b'), "\
"TIMESTAMP('2024-01-03', '04:05:06'), SEC_TO_TIME(61), IF(0, 0, 1)); "\
"SELECT 'replace', ROW_COUNT(), @@warning_count, id, i, v, HEX(b), d, f + 0, "\
"JSON_UNQUOTE(JSON_EXTRACT(js, '$[1]')), dt, tm, flag FROM exprs WHERE id = 2; "\
"UPDATE exprs SET i = BIT_COUNT(15) WHERE id = 1; "\
"UPDATE exprs SET v = LEAST('b', 'c') WHERE id = 1; "\
"UPDATE exprs SET b = UNHEX('4344') WHERE id = 1; "\
"UPDATE exprs SET d = ROUND(19, 0) WHERE id = 1; "\
"UPDATE exprs SET f = ACOS(1) WHERE id = 1; "\
"UPDATE exprs SET js = JSON_OBJECT('updated', TRUE) WHERE id = 1; "\
"UPDATE exprs SET dt = TIMESTAMP('2024-01-02', '03:04:06') WHERE id = 1; "\
"UPDATE exprs SET tm = ADDTIME('00:00:31', '00:00:29') WHERE id = 1; "\
"UPDATE exprs SET flag = IF(1, 2, 0) WHERE id = 1; "\
"SELECT 'update', ROW_COUNT(), @@warning_count, id, i, v, HEX(b), d, f + 0, "\
"JSON_UNQUOTE(JSON_EXTRACT(js, '$.updated')), dt, tm, flag FROM exprs WHERE id = 1; "\
"INSERT IGNORE INTO exprs(id, i) VALUES (3, GREATEST('abc', 'def')); "\
"SELECT 'ignore', ROW_COUNT(), @@warning_count, id, i FROM exprs WHERE id = 3;" \
    "$DATABASE"

expect_error \
    "scalar expression dml strict conversion error" \
    1366 \
    HY000 \
    "Incorrect integer value: 'def' for column 'i' at row 1" \
    "INSERT INTO exprs(id, i) VALUES (4, GREATEST('abc', 'def'));" \
    "$DATABASE"

printf '%s\n' "mysql_baseline_dml_scalar_expression_values_expectations: ok"
