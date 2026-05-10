#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_select_all_modifier_expectations_$$"

fail() {
    printf '%s\n' "mysql_baseline_select_all_modifier_expectations: $1" >&2
    exit 1
}

run_mysql() {
    sql=$1
    shift
    printf '%s\n' "$sql" \
        | docker exec -i "$MYSQL_CONTAINER" mysql --protocol=TCP -h127.0.0.1 -uroot --batch --raw --skip-column-names "$@"
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

expect_value() {
    label=$1
    expected=$2
    actual=$3

    if [ "$actual" != "$expected" ]; then
        fail "$label: expected [$expected], got [$actual]"
    fi
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

run_mysql \
    "CREATE DATABASE ${DATABASE};
     USE ${DATABASE};
     CREATE TABLE t(id INT NOT NULL, n INT NULL, b BOOL NULL);
     INSERT INTO t VALUES
       (1, NULL, TRUE),
       (2, 20, FALSE),
       (3, 20, FALSE),
       (4, 30, NULL),
       (5, NULL, TRUE);" >/dev/null

core=$(run_mysql \
    "USE ${DATABASE};
     DO 0;
     SELECT n FROM t ORDER BY n LIMIT 10;
     SELECT @@warning_count, ROW_COUNT();
     DO 0;
     SELECT ALL n FROM t ORDER BY n LIMIT 10;
     SELECT @@warning_count, ROW_COUNT();"
)
expect_value "default and all duplicate-preserving table select" "NULL
NULL
20
20
30
0	-1
NULL
NULL
20
20
30
0	-1" "$core"

composition=$(run_mysql \
    "USE ${DATABASE};
     SELECT ALL * FROM t ORDER BY id LIMIT 2;
     SELECT ALL n FROM t WHERE n IS NOT NULL ORDER BY n DESC LIMIT 2;
     SELECT ALL n FROM ${DATABASE}.t ORDER BY n LIMIT 3;"
)
expect_value "all table composition" "1	NULL	1
2	20	0
30
20
NULL
NULL
20" "$composition"

scalar_and_aggregate=$(run_mysql \
    "USE ${DATABASE};
     DO 0;
     SELECT ALL 1;
     SELECT @@warning_count, ROW_COUNT();
     DO 0;
     SELECT ALL 1 FROM DUAL;
     SELECT @@warning_count, ROW_COUNT();
     SELECT ALL COUNT(*) FROM t;
     SELECT ALL COUNT(n), COUNT(DISTINCT n), MIN(n), MAX(n) FROM t;"
)
expect_value "all scalar and aggregate" "1
0	-1
1
0	-1
5
3	2	20	30" "$scalar_and_aggregate"

accepted_but_deferred=$(run_mysql \
    "USE ${DATABASE};
     SELECT ALL ALL 1;"
)
expect_value "repeated all accepted by mysql" "1" "$accepted_but_deferred"

expect_error \
    "all distinct mix" \
    1221 \
    HY000 \
    "Incorrect usage of ALL and DISTINCT" \
    "USE ${DATABASE}; SELECT ALL DISTINCT 1;"

expect_error \
    "all distinctrow mix" \
    1221 \
    HY000 \
    "Incorrect usage of ALL and DISTINCT" \
    "USE ${DATABASE}; SELECT ALL DISTINCTROW 1;"

expect_error \
    "unknown selected column" \
    1054 \
    42S22 \
    "Unknown column 'missing' in 'field list'" \
    "USE ${DATABASE}; SELECT ALL missing FROM t;"

expect_error \
    "missing default schema" \
    1046 \
    3D000 \
    "No database selected" \
    "SELECT ALL n FROM t;"

expect_error \
    "unknown schema" \
    1049 \
    42000 \
    "Unknown database '${DATABASE}_missing'" \
    "SELECT ALL n FROM ${DATABASE}_missing.t;"

expect_error \
    "unknown table" \
    1146 \
    42S02 \
    "Table '${DATABASE}.missing' doesn't exist" \
    "USE ${DATABASE}; SELECT ALL n FROM missing;"
