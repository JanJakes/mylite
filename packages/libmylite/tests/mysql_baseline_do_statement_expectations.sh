#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_do_statement_expectations_$$"

fail() {
    printf '%s\n' "mysql_baseline_do_statement_expectations: $1" >&2
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
run_mysql "CREATE DATABASE ${DATABASE}; USE ${DATABASE}; CREATE TABLE t(id INT, n INT NULL); INSERT INTO t VALUES (1, NULL), (2, 5);" >/dev/null

expect_output_with_headers \
    "single expression row count" \
    "@@warning_count	ROW_COUNT()
0	0" \
    "DO 1; SELECT @@warning_count, ROW_COUNT();" \
    "$DATABASE"

expect_output_with_headers \
    "multi expression row count" \
    "@@warning_count	ROW_COUNT()
0	0" \
    "DO 1, NULL, TRUE, IF(0,5,6), IFNULL(NULL,7), COALESCE(NULL,8),
        NULLIF(9,9), ISNULL(NULL), 1+2, 5 DIV 2, 5%2, 1=1,
        1<=>NULL, 1 AND NOT 0, 1 IS TRUE,
        CASE WHEN 1 THEN 10 ELSE 11 END;
     SELECT @@warning_count, ROW_COUNT();" \
    "$DATABASE"

expect_output_with_headers \
    "division warning row count" \
    "@@warning_count	ROW_COUNT()
1	0" \
    "DO 5 DIV 0; SELECT @@warning_count, ROW_COUNT();" \
    "$DATABASE"

expect_output_with_headers \
    "warning details" \
    "Level	Code	Message
Warning	1365	Division by 0
Warning	1365	Division by 0
@@warning_count	ROW_COUNT()
2	-1" \
    "DO 5 DIV 0, MOD(5,0); SHOW WARNINGS; SELECT @@warning_count, ROW_COUNT();" \
    "$DATABASE"

expect_output_with_headers \
    "short circuit warnings" \
    "@@warning_count	ROW_COUNT()
0	0" \
    "DO 0 AND 5 DIV 0, 1 OR 5 DIV 0, CASE WHEN 1 THEN 2 ELSE 5 DIV 0 END;
     SELECT @@warning_count, ROW_COUNT();" \
    "$DATABASE"

expect_output_with_headers \
    "selected case warning" \
    "@@warning_count	ROW_COUNT()
1	0" \
    "DO CASE WHEN 1 THEN 5 DIV 0 ELSE 2 END; SELECT @@warning_count, ROW_COUNT();" \
    "$DATABASE"

accepted_but_deferred=$(run_mysql_with_headers \
    "DO 1 AS x;
     DO 'x';
     DO 1.5;
     DO 0x31;
     DO b'1';
     DO (SELECT COUNT(*) FROM t);
     DO @x := 1;
     SELECT @x;" \
    "$DATABASE"
)
expect_value \
    "mysql accepted forms deferred by this slice" \
    "@x
1" \
    "$accepted_but_deferred"

expect_error \
    "empty do" \
    1064 \
    42000 \
    "You have an error in your SQL syntax" \
    "DO;" \
    "$DATABASE"

expect_error \
    "unknown column" \
    1054 \
    42S22 \
    "Unknown column 'id' in 'field list'" \
    "DO id;" \
    "$DATABASE"

expect_error \
    "from clause is invalid" \
    1064 \
    42000 \
    "You have an error in your SQL syntax" \
    "DO id FROM t;" \
    "$DATABASE"

expect_error \
    "dual from clause is invalid" \
    1064 \
    42000 \
    "You have an error in your SQL syntax" \
    "DO 1 FROM DUAL;" \
    "$DATABASE"

expect_error \
    "wrong ifnull arity" \
    1582 \
    42000 \
    "Incorrect parameter count in the call to native function 'IFNULL'" \
    "DO IFNULL(1);" \
    "$DATABASE"

expect_error \
    "arithmetic overflow" \
    1690 \
    22003 \
    "BIGINT value is out of range" \
    "DO 9223372036854775807 + 1;" \
    "$DATABASE"

expect_error \
    "parameter marker rejected" \
    1064 \
    42000 \
    "You have an error in your SQL syntax" \
    "DO ?;" \
    "$DATABASE"

printf '%s\n' "mysql_baseline_do_statement_expectations: ok"
