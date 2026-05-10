#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_select_distinctrow_column_expectations_$$"

fail() {
    printf '%s\n' "mysql_baseline_select_distinctrow_column_expectations: $1" >&2
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
     SELECT DISTINCT n FROM t ORDER BY n;
     SELECT @@warning_count, ROW_COUNT();
     DO 0;
     SELECT DISTINCTROW n FROM t ORDER BY n;
     SELECT @@warning_count, ROW_COUNT();"
)
expect_value "distinct and distinctrow core semantics" "NULL
20
30
0	-1
NULL
20
30
0	-1" "$core"

composition=$(run_mysql \
    "USE ${DATABASE};
     SELECT DISTINCTROW n FROM t WHERE n IS NOT NULL ORDER BY n DESC LIMIT 1 OFFSET 1;
     SELECT DISTINCTROW b FROM t ORDER BY b;
     SELECT DISTINCTROW n FROM ${DATABASE}.t ORDER BY n LIMIT 2;"
)
expect_value "distinctrow composition" "20
NULL
0
1
NULL
20" "$composition"

accepted_but_deferred=$(run_mysql \
    "USE ${DATABASE};
     SELECT DISTINCTROW t.n FROM t ORDER BY t.n;
     SELECT DISTINCTROW n, id FROM t ORDER BY n, id;
     SELECT DISTINCTROW 1 FROM t;
     SELECT DISTINCTROW n + 1 FROM t ORDER BY n + 1;"
)
expect_value "accepted deferred distinctrow forms" "NULL
20
30
NULL	1
NULL	5
20	2
20	3
30	4
1
NULL
21
31" "$accepted_but_deferred"

expect_error \
    "non-selected order column" \
    3065 \
    HY000 \
    "which is not in SELECT list; this is incompatible with DISTINCT" \
    "USE ${DATABASE}; SELECT DISTINCTROW n FROM t ORDER BY id;"

expect_error \
    "unknown selected column" \
    1054 \
    42S22 \
    "Unknown column 'missing' in 'field list'" \
    "USE ${DATABASE}; SELECT DISTINCTROW missing FROM t;"

expect_error \
    "unknown predicate column" \
    1054 \
    42S22 \
    "Unknown column 'missing' in 'where clause'" \
    "USE ${DATABASE}; SELECT DISTINCTROW n FROM t WHERE missing = 1;"

expect_error \
    "unknown order column" \
    1054 \
    42S22 \
    "Unknown column 'missing' in 'order clause'" \
    "USE ${DATABASE}; SELECT DISTINCTROW n FROM t ORDER BY missing;"

expect_error \
    "missing default schema" \
    1046 \
    3D000 \
    "No database selected" \
    "SELECT DISTINCTROW n FROM t;"

expect_error \
    "unknown schema" \
    1049 \
    42000 \
    "Unknown database '${DATABASE}_missing'" \
    "SELECT DISTINCTROW n FROM ${DATABASE}_missing.t;"

expect_error \
    "unknown table" \
    1146 \
    42S02 \
    "Table '${DATABASE}.missing' doesn't exist" \
    "USE ${DATABASE}; SELECT DISTINCTROW n FROM missing;"
