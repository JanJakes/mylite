#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_pi_function_expectations_$$"

fail() {
    printf '%s\n' "mysql_baseline_pi_function_expectations: $1" >&2
    exit 1
}

run_mysql() {
    sql=$1
    shift
    printf '%s\n' "$sql" \
        | docker exec -i "$MYSQL_CONTAINER" mysql --protocol=TCP -h127.0.0.1 -uroot \
            --batch --raw --binary-as-hex=1 --skip-column-names "$@"
}

run_mysql_with_headers() {
    sql=$1
    shift
    printf '%s\n' "$sql" \
        | docker exec -i "$MYSQL_CONTAINER" mysql --protocol=TCP -h127.0.0.1 -uroot \
            --batch --raw --binary-as-hex=1 "$@"
}

expect_value() {
    label=$1
    expected=$2
    actual=$3

    if [ "$actual" != "$expected" ]; then
        fail "$label: expected [$expected], got [$actual]"
    fi
}

expect_output() {
    label=$1
    expected=$2
    sql=$3
    shift 3

    output=$(run_mysql "$sql" "$@")
    expect_value "$label" "$expected" "$output"
}

expect_output_with_headers() {
    label=$1
    expected=$2
    sql=$3
    shift 3

    output=$(run_mysql_with_headers "$sql" "$@")
    expect_value "$label" "$expected" "$output"
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

version=$(run_mysql 'SELECT VERSION();')
case "$version" in
    8.4.9*) ;;
    *) fail "expected MySQL 8.4.9 runtime, got [$version]" ;;
esac

cleanup
run_mysql "CREATE DATABASE ${DATABASE}; USE ${DATABASE};
           CREATE TABLE t(id INT);
           INSERT INTO t VALUES (7), (8);" \
    >/dev/null

expect_output_with_headers \
    "core pi values" \
    "PI()	pi()	Pi()	PI ()	(PI())	@@warning_count	ROW_COUNT()
3.141593	3.141593	3.141593	3.141593	3.141593	0	0" \
    "DO 0; SELECT PI(),pi(),Pi(),PI (), (PI()), @@warning_count, ROW_COUNT();" \
    "$DATABASE"

expect_output_with_headers \
    "dual pi value" \
    "PI()
3.141593" \
    "SELECT PI() FROM DUAL;" \
    "$DATABASE"

expect_output \
    "do pi status" \
    "0	0" \
    "DO PI(), pi(); SELECT @@warning_count, ROW_COUNT();" \
    "$DATABASE"

expect_output \
    "pi warnings status" \
    "3.141593
0	0	-1" \
    "SELECT PI(); SHOW WARNINGS; SELECT @@warning_count, @@error_count, ROW_COUNT();" \
    "$DATABASE"

expect_error \
    "one argument pi arity" \
    1582 \
    42000 \
    "Incorrect parameter count in the call to native function 'PI'" \
    "SELECT PI(1);" \
    "$DATABASE"

expect_error \
    "null argument pi arity" \
    1582 \
    42000 \
    "Incorrect parameter count in the call to native function 'PI'" \
    "SELECT PI(NULL);" \
    "$DATABASE"

expect_error \
    "two argument pi arity" \
    1582 \
    42000 \
    "Incorrect parameter count in the call to native function 'PI'" \
    "SELECT PI(1, 2);" \
    "$DATABASE"

expect_error \
    "bare pi identifier" \
    1054 \
    42S22 \
    "Unknown column 'PI' in 'field list'" \
    "SELECT PI;" \
    "$DATABASE"

accepted_but_deferred=$(run_mysql_with_headers \
    "SELECT id, PI() AS p, CONCAT('p=', PI()) AS label FROM t ORDER BY id;
     SELECT PI()+0.000000000000000000;" \
    "$DATABASE"
)
expect_value \
    "row pi values and deferred arithmetic" \
    "id	p	label
7	3.141593	p=3.141593
8	3.141593	p=3.141593
PI()+0.000000000000000000
3.141592653589793000" \
    "$accepted_but_deferred"

printf '%s\n' "mysql_baseline_pi_function_expectations: ok"
