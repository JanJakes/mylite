#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"

fail() {
    printf '%s\n' "mysql_baseline_loadable_function_diagnostics_expectations: $1" >&2
    exit 1
}

run_mysql() {
    sql=$1
    shift
    printf '%s\n' "$sql" \
        | docker exec -i "$MYSQL_CONTAINER" mysql --protocol=TCP -h127.0.0.1 -uroot \
            --batch --raw --skip-column-names "$@"
}

run_mysql_with_headers() {
    sql=$1
    shift
    printf '%s\n' "$sql" \
        | docker exec -i "$MYSQL_CONTAINER" mysql --protocol=TCP -h127.0.0.1 -uroot \
            --batch --raw "$@"
}

expect_error() {
    label=$1
    code=$2
    state=$3
    message=$4
    sql=$5

    set +e
    output=$(run_mysql "$sql" 2>&1)
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

version=$(run_mysql 'SELECT VERSION();')
case "$version" in
    8.4.9*) ;;
    *) fail "expected MySQL 8.4.9 runtime, got [$version]" ;;
esac

expect_error \
    "CREATE FUNCTION missing loadable library" \
    1126 \
    "HY000" \
    "Can't open shared library" \
    "CREATE FUNCTION mylite_missing_udf RETURNS INTEGER SONAME 'missing_mylite_udf.so';"

expect_error \
    "CREATE FUNCTION IF NOT EXISTS missing loadable library" \
    1126 \
    "HY000" \
    "Can't open shared library" \
    "CREATE FUNCTION IF NOT EXISTS mylite_missing_udf RETURNS STRING SONAME 'missing_mylite_udf.so';"

expect_error \
    "CREATE AGGREGATE FUNCTION missing loadable library" \
    1126 \
    "HY000" \
    "Can't open shared library" \
    "CREATE AGGREGATE FUNCTION mylite_missing_aggr RETURNS REAL SONAME 'missing_mylite_udf.so';"

expect_error \
    "CREATE FUNCTION invalid loadable return type" \
    1064 \
    "42000" \
    "near 'BOGUS SONAME" \
    "CREATE FUNCTION mylite_bad_udf RETURNS BOGUS SONAME 'missing_mylite_udf.so';"

expect_error \
    "DROP FUNCTION missing loadable function" \
    1305 \
    "42000" \
    "FUNCTION (UDF) mylite_missing_udf does not exist" \
    "DROP FUNCTION mylite_missing_udf;"

drop_if_exists=$(run_mysql_with_headers \
    "DROP FUNCTION IF EXISTS mylite_missing_udf; SHOW WARNINGS; SELECT @@warning_count, ROW_COUNT();")
case "$drop_if_exists" in
    *"Note"*"1305"*"FUNCTION (UDF) mylite_missing_udf does not exist"*) ;;
    *)
        fail "DROP FUNCTION IF EXISTS missing-function note did not match expected surface: [$drop_if_exists]"
        ;;
esac
case "$drop_if_exists" in
    *"@@warning_count"*"ROW_COUNT()"*) ;;
    *)
        fail "DROP FUNCTION IF EXISTS count header did not match expected surface: [$drop_if_exists]"
        ;;
esac
case "$drop_if_exists" in
    *"1	-1"*) ;;
    *)
        fail "DROP FUNCTION IF EXISTS count row did not match expected surface: [$drop_if_exists]"
        ;;
esac

printf '%s\n' "mysql_baseline_loadable_function_diagnostics_expectations: ok"
