#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_scalar_subquery_projection_expectations_$$"

fail() {
    printf '%s\n' "mysql_baseline_scalar_subquery_projection_expectations: $1" >&2
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
run_mysql "CREATE TABLE t (id INT, v VARCHAR(20)); INSERT INTO t VALUES (1, 'a'), (2, NULL);" \
    "$DATABASE" >/dev/null

scalar_expected=$(cat <<EXPECTED
${DATABASE}	test-${DATABASE}	${DATABASE}	NULL	1	2	-3	1	0	0	0
-1	0
EXPECTED
)
expect_output \
    "scalar subquery projection values" \
    "$scalar_expected" \
    "DO 0; SELECT (SELECT DATABASE()), CONCAT('test-', (SELECT DATABASE())), "\
"((SELECT DATABASE())), (SELECT NULL), (SELECT 1), (SELECT +2), (SELECT -3), "\
"(SELECT TRUE), (SELECT FALSE), (SELECT @@warning_count), @@warning_count; "\
"SELECT ROW_COUNT(), @@warning_count;" \
    "$DATABASE"

dual_expected=$(cat <<EXPECTED
${DATABASE}	1
EXPECTED
)
expect_output \
    "dual scalar subquery projection values" \
    "$dual_expected" \
    "SELECT (SELECT DATABASE() FROM DUAL), (SELECT 1 FROM DUAL);" \
    "$DATABASE"

row_scalar_expected=$(cat <<EXPECTED
1	a-${DATABASE}
2	NULL
EXPECTED
)
expect_output \
    "row-scalar concat scalar subquery values" \
    "$row_scalar_expected" \
    "SELECT id, CONCAT(v, '-', (SELECT DATABASE())) FROM t ORDER BY id;" \
    "$DATABASE"

labels_expected=$(cat <<EXPECTED
(SELECT DATABASE())	named
${DATABASE}	${DATABASE}
EXPECTED
)
expect_output_with_headers \
    "scalar subquery labels" \
    "$labels_expected" \
    "SELECT (SELECT DATABASE()), (SELECT DATABASE()) AS named;" \
    "$DATABASE"

expect_error \
    "scalar subquery rejects multiple columns" \
    1241 \
    21000 \
    "Operand should contain 1 column(s)" \
    "SELECT (SELECT DATABASE(), SCHEMA());" \
    "$DATABASE"

expect_error \
    "scalar subquery rejects wildcard without table" \
    1096 \
    HY000 \
    "No tables used" \
    "SELECT (SELECT * FROM DUAL);" \
    "$DATABASE"

printf '%s\n' "mysql_baseline_scalar_subquery_projection_expectations: ok"
