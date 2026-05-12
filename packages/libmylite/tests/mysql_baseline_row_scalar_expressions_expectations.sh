#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_row_scalar_expressions_expectations_$$"

fail() {
    printf '%s\n' "mysql_baseline_row_scalar_expressions_expectations: $1" >&2
    exit 1
}

run_mysql() {
    sql=$1
    shift
    printf '%s\n' "$sql" \
        | docker exec -i "$MYSQL_CONTAINER" mysql -uroot --batch --raw --skip-column-names "$@"
}

run_mysql_with_headers() {
    sql=$1
    shift
    printf '%s\n' "$sql" \
        | docker exec -i "$MYSQL_CONTAINER" mysql -uroot --batch --raw "$@"
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

expect_output_with_headers() {
    label=$1
    expected=$2
    sql=$3
    shift 3

    output=$(run_mysql_with_headers "$sql" "$@")
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
run_mysql "CREATE DATABASE ${DATABASE}; USE ${DATABASE};" >/dev/null

no_source_expected=$(cat <<EXPECTED
${DATABASE}	${DATABASE}	test-${DATABASE}	ab	NULL	123	10	0
-1	0
EXPECTED
)
expect_output \
    "no-source concat values" \
    "$no_source_expected" \
    "DO 0; SELECT DATABASE(), SCHEMA(), CONCAT('test-', DATABASE()), CONCAT('a', 'b'), "\
"CONCAT('a', NULL), CONCAT(1, 2, 3), CONCAT(TRUE, FALSE), @@warning_count; "\
"SELECT ROW_COUNT(), @@warning_count;" \
    "$DATABASE"

expect_output \
    "from dual concat values" \
    "dual	xy" \
    "SELECT CONCAT('du', 'al'), CONCAT('x', 'y') FROM DUAL;" \
    "$DATABASE"

run_mysql \
    "CREATE TABLE t ("\
"id INT, v VARCHAR(20), n VARCHAR(20), i INT, d DECIMAL(6,2), dt DATE, txt TEXT"\
"); "\
"INSERT INTO t VALUES "\
"(1, 'a', 'x', 7, 12.30, '2024-01-02', 'alpha'), "\
"(2, 'b', NULL, -3, -4.50, NULL, 'beta'), "\
"(3, '', '', 0, 0.00, '2024-12-31', NULL);" \
    "$DATABASE" >/dev/null

mixed_projection_expected=$(cat <<\EXPECTED
1	a-7
2	b--3
3	-0
EXPECTED
)
expect_output \
    "table concat projection" \
    "$mixed_projection_expected" \
    "SELECT id, CONCAT(v, '-', i) AS label FROM t ORDER BY id;" \
    "$DATABASE"

null_projection_expected=$(cat <<\EXPECTED
1	[ax]
2	NULL
3	[]
EXPECTED
)
expect_output \
    "table concat null propagation" \
    "$null_projection_expected" \
    "SELECT id, CONCAT('[', v, n, ']') AS merged FROM t ORDER BY id;" \
    "$DATABASE"

typed_projection_expected=$(cat <<\EXPECTED
1	12.30:2024-01-02:alpha
2	NULL
3	NULL
EXPECTED
)
expect_output \
    "table concat typed values" \
    "$typed_projection_expected" \
    "SELECT id, CONCAT(d, ':', dt, ':', txt) AS mixed FROM t ORDER BY id;" \
    "$DATABASE"

expect_output \
    "table concat where order limit" \
    ":3
b:2" \
    "SELECT CONCAT(v, ':', id) FROM t WHERE id >= 1 ORDER BY id DESC LIMIT 2;" \
    "$DATABASE"

labels_expected=$(cat <<EXPECTED
CONCAT(v, '-', id)	alias_name
a-1	x${DATABASE}
EXPECTED
)
expect_output_with_headers \
    "concat labels" \
    "$labels_expected" \
    "SELECT CONCAT(v, '-', id), CONCAT('x', DATABASE()) AS alias_name FROM t WHERE id = 1;" \
    "$DATABASE"

expect_error \
    "concat rejects zero arguments" \
    1582 \
    42000 \
    "Incorrect parameter count in the call to native function 'CONCAT'" \
    "SELECT CONCAT();" \
    "$DATABASE"

expect_error \
    "concat unknown column" \
    1054 \
    42S22 \
    "Unknown column 'missing' in 'field list'" \
    "SELECT CONCAT(v, missing) FROM t;" \
    "$DATABASE"

printf '%s\n' "mysql_baseline_row_scalar_expressions_expectations: ok"
