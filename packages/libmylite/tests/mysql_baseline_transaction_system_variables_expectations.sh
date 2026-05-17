#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
MYSQL_ARGS="--protocol=TCP -h127.0.0.1 -uroot --batch --raw --skip-column-names"
DATABASE="mylite_transaction_system_variables_expectations_$$"
TAB=$(printf '\t')

fail() {
    printf '%s\n' "mysql_baseline_transaction_system_variables_expectations: $1" >&2
    exit 1
}

run_mysql() {
    sql=$1
    shift
    printf '%s\n' "$sql" | docker exec -i "$MYSQL_CONTAINER" mysql $MYSQL_ARGS "$@"
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

normalize_tsv() {
    sed "s/${TAB}/|/g"
}

cleanup() {
    run_mysql "SET SESSION transaction_read_only = OFF; DROP DATABASE IF EXISTS ${DATABASE};" \
        >/dev/null 2>&1 || true
    run_mysql "SET GLOBAL transaction_isolation = 'REPEATABLE-READ'; SET GLOBAL transaction_read_only = OFF;" \
        >/dev/null 2>&1 || true
}

trap cleanup EXIT HUP INT TERM

version=$(run_mysql "SELECT VERSION();")
case "$version" in
    8.4.9*) ;;
    *) fail "expected MySQL 8.4.9 runtime, got [$version]" ;;
esac

cleanup
run_mysql "CREATE DATABASE ${DATABASE}; USE ${DATABASE}; CREATE TABLE t (id INT PRIMARY KEY, v INT); INSERT INTO t VALUES (1, 10);" \
    >/dev/null

expect_output \
    "default scalar values" \
    "REPEATABLE-READ${TAB}REPEATABLE-READ${TAB}REPEATABLE-READ${TAB}REPEATABLE-READ${TAB}0${TAB}0${TAB}0${TAB}0" \
    "SET SESSION transaction_isolation = 'REPEATABLE-READ'; SET SESSION transaction_read_only = OFF; "\
"SELECT @@transaction_isolation, @@GLOBAL.transaction_isolation, @@SESSION.transaction_isolation, "\
"@@LOCAL.transaction_isolation, @@transaction_read_only, @@GLOBAL.transaction_read_only, "\
"@@SESSION.transaction_read_only, @@LOCAL.transaction_read_only;" \
    "$DATABASE"

show_session=$(
    run_mysql \
        "SHOW VARIABLES WHERE Variable_name IN ('transaction_isolation','transaction_read_only');" \
        "$DATABASE" | normalize_tsv
)
expect_value \
    "show session variables" \
    "transaction_isolation|REPEATABLE-READ
transaction_read_only|OFF" \
    "$show_session"

show_global=$(
    run_mysql \
        "SHOW GLOBAL VARIABLES WHERE Variable_name IN ('transaction_isolation','transaction_read_only');" \
        "$DATABASE" | normalize_tsv
)
expect_value \
    "show global variables" \
    "transaction_isolation|REPEATABLE-READ
transaction_read_only|OFF" \
    "$show_global"

expect_output \
    "session isolation assignments" \
    "READ-COMMITTED${TAB}REPEATABLE-READ${TAB}0${TAB}0
SERIALIZABLE${TAB}0${TAB}0
REPEATABLE-READ${TAB}0${TAB}0" \
    "SET SESSION transaction_isolation = 'READ-COMMITTED'; "\
"SELECT @@transaction_isolation, @@GLOBAL.transaction_isolation, ROW_COUNT(), @@warning_count; "\
"SET SESSION transaction_isolation = SERIALIZABLE; "\
"SELECT @@transaction_isolation, ROW_COUNT(), @@warning_count; "\
"SET SESSION transaction_isolation = DEFAULT; "\
"SELECT @@transaction_isolation, ROW_COUNT(), @@warning_count;" \
    "$DATABASE"

expect_output \
    "session read only assignments" \
    "1${TAB}0${TAB}0${TAB}0
0${TAB}0${TAB}0
1${TAB}0${TAB}0
0${TAB}0${TAB}0
1${TAB}0${TAB}0
0${TAB}0${TAB}0" \
    "SET SESSION transaction_read_only = ON; "\
"SELECT @@transaction_read_only, @@GLOBAL.transaction_read_only, ROW_COUNT(), @@warning_count; "\
"SET LOCAL transaction_read_only = OFF; SELECT @@transaction_read_only, ROW_COUNT(), @@warning_count; "\
"SET @@LOCAL.transaction_read_only = ON; SELECT @@transaction_read_only, ROW_COUNT(), @@warning_count; "\
"SET @@SESSION.transaction_read_only = OFF; SELECT @@transaction_read_only, ROW_COUNT(), @@warning_count; "\
"SET transaction_read_only = 'ON'; SELECT @@transaction_read_only, ROW_COUNT(), @@warning_count; "\
"SET transaction_read_only = DEFAULT; SELECT @@transaction_read_only, ROW_COUNT(), @@warning_count;" \
    "$DATABASE"

expect_output \
    "next read only scalar unchanged" \
    "0${TAB}0${TAB}0${TAB}0" \
    "SET SESSION transaction_read_only = OFF; SET @@transaction_read_only = ON; "\
"SELECT @@transaction_read_only, @@SESSION.transaction_read_only, ROW_COUNT(), @@warning_count;" \
    "$DATABASE"

expect_error \
    "next read only survives scalar select and blocks persistent write" \
    1792 \
    25006 \
    "Cannot execute statement in a READ ONLY transaction." \
    "SET SESSION transaction_read_only = OFF; SET @@transaction_read_only = ON; "\
"SELECT @@transaction_read_only; INSERT INTO t VALUES (2, 20);" \
    "$DATABASE"

expect_output \
    "next isolation affects consistent snapshot warning" \
    "REPEATABLE-READ${TAB}REPEATABLE-READ${TAB}0${TAB}0
Warning${TAB}138${TAB}InnoDB: WITH CONSISTENT SNAPSHOT was ignored because this phrase can only be used with REPEATABLE READ isolation level.
1" \
    "SET SESSION transaction_isolation = 'REPEATABLE-READ'; "\
"SET @@transaction_isolation = 'READ-COMMITTED'; "\
"SELECT @@transaction_isolation, @@SESSION.transaction_isolation, ROW_COUNT(), @@warning_count; "\
"START TRANSACTION WITH CONSISTENT SNAPSHOT; SHOW WARNINGS; SELECT @@warning_count; COMMIT;" \
    "$DATABASE"

expect_error \
    "next transaction assignment fails while active" \
    1568 \
    25001 \
    "Transaction characteristics can't be changed while a transaction is in progress" \
    "START TRANSACTION; SET @@transaction_read_only = ON;" \
    "$DATABASE"

expect_output \
    "session assignment inside active transaction affects later only" \
    "2${TAB}1${TAB}0
1" \
    "SET SESSION transaction_read_only = OFF; START TRANSACTION; "\
"SET transaction_read_only = ON; INSERT INTO t VALUES (3, 30); COMMIT; "\
"SELECT COUNT(*), SUM(id = 3), @@warning_count FROM t; "\
"SELECT @@transaction_read_only;" \
    "$DATABASE"

expect_error \
    "session read only blocks later write" \
    1792 \
    25006 \
    "Cannot execute statement in a READ ONLY transaction." \
    "SET SESSION transaction_read_only = ON; INSERT INTO t VALUES (4, 40);" \
    "$DATABASE"

expect_output \
    "global mutable upstream evidence" \
    "REPEATABLE-READ${TAB}REPEATABLE-READ${TAB}0${TAB}0
0${TAB}0${TAB}0${TAB}0
SERIALIZABLE${TAB}REPEATABLE-READ${TAB}0${TAB}0
1${TAB}0${TAB}0${TAB}0" \
    "SET SESSION transaction_isolation = 'REPEATABLE-READ'; SET SESSION transaction_read_only = OFF; "\
"SET GLOBAL transaction_isolation = 'REPEATABLE-READ'; "\
"SELECT @@GLOBAL.transaction_isolation, @@SESSION.transaction_isolation, ROW_COUNT(), @@warning_count; "\
"SET GLOBAL transaction_read_only = OFF; "\
"SELECT @@GLOBAL.transaction_read_only, @@SESSION.transaction_read_only, ROW_COUNT(), @@warning_count; "\
"SET @@GLOBAL.transaction_isolation = 'SERIALIZABLE'; "\
"SELECT @@GLOBAL.transaction_isolation, @@SESSION.transaction_isolation, ROW_COUNT(), @@warning_count; "\
"SET @@GLOBAL.transaction_read_only = ON; "\
"SELECT @@GLOBAL.transaction_read_only, @@SESSION.transaction_read_only, ROW_COUNT(), @@warning_count;" \
    "$DATABASE"
cleanup

expect_error \
    "invalid isolation spaced string" \
    1231 \
    42000 \
    "Variable 'transaction_isolation' can't be set to the value of 'READ COMMITTED'" \
    "SET SESSION transaction_isolation = 'READ COMMITTED';"

expect_error \
    "invalid isolation null" \
    1231 \
    42000 \
    "Variable 'transaction_isolation' can't be set to the value of 'NULL'" \
    "SET SESSION transaction_isolation = NULL;"

expect_error \
    "invalid read only integer" \
    1231 \
    42000 \
    "Variable 'transaction_read_only' can't be set to the value of '2'" \
    "SET SESSION transaction_read_only = 2;"

expect_error \
    "invalid read only string integer" \
    1231 \
    42000 \
    "Variable 'transaction_read_only' can't be set to the value of '1'" \
    "SET SESSION transaction_read_only = '1';"

expect_error \
    "invalid read only null" \
    1231 \
    42000 \
    "Variable 'transaction_read_only' can't be set to the value of 'NULL'" \
    "SET SESSION transaction_read_only = NULL;"
