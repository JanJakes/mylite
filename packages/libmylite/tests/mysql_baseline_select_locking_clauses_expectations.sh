#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_select_locking_clauses_$$"

fail() {
    printf '%s\n' "mysql_baseline_select_locking_clauses_expectations: $1" >&2
    exit 1
}

run_mysql() {
    sql=$1
    shift
    printf '%s\n' "$sql" \
        | docker exec -i "$MYSQL_CONTAINER" mysql --protocol=TCP -h127.0.0.1 -uroot --batch --raw --skip-column-names "$@"
}

expect_value() {
    label=$1
    expected=$2
    actual=$3

    if [ "$actual" != "$expected" ]; then
        fail "$label: expected [$expected], got [$actual]"
    fi
}

expect_contains() {
    label=$1
    haystack=$2
    needle=$3

    case "$haystack" in
        *"$needle"*) ;;
        *) fail "$label: expected output containing [$needle], got [$haystack]" ;;
    esac
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

run_mysql \
    "CREATE DATABASE ${DATABASE};
     USE ${DATABASE};
     CREATE TABLE t(id INT NOT NULL, n INT NULL) ENGINE=InnoDB;
     CREATE TABLE dst(id INT NOT NULL) ENGINE=InnoDB;
     INSERT INTO t VALUES (1, 10), (2, NULL), (3, 10);" >/dev/null

locking_selects=$(
    run_mysql \
        "USE ${DATABASE};
         SELECT id FROM t WHERE n <=> 10 ORDER BY id LIMIT 2 FOR UPDATE;
         SELECT @@warning_count, ROW_COUNT();
         SELECT id FROM t ORDER BY id LIMIT 1 FOR SHARE;
         SELECT @@warning_count, ROW_COUNT();
         SELECT id FROM t ORDER BY id LIMIT 1 LOCK IN SHARE MODE;
         SELECT @@warning_count, ROW_COUNT();
         SELECT 1 FOR UPDATE;
         SELECT @@warning_count, ROW_COUNT();
         SELECT 1 FROM DUAL FOR SHARE;
         SELECT @@warning_count, ROW_COUNT();
         SELECT COUNT(*) FROM t FOR UPDATE;
         SELECT @@warning_count, ROW_COUNT();
         SELECT n, COUNT(*) FROM t GROUP BY n ORDER BY n FOR SHARE;
         SELECT @@warning_count, ROW_COUNT();
         SELECT DISTINCT n FROM t ORDER BY n FOR UPDATE;
         SELECT @@warning_count, ROW_COUNT();"
)
expect_value \
    "locking select no-op results" \
    "1
3
0	-1
1
0	-1
1
0	-1
1
0	-1
1
0	-1
3
0	-1
NULL	1
10	2
0	-1
NULL
10
0	-1" \
    "$locking_selects"

calc_locking=$(
    run_mysql \
        "USE ${DATABASE};
         SELECT SQL_CALC_FOUND_ROWS id FROM t ORDER BY id LIMIT 1 FOR UPDATE;
         SHOW COUNT(*) WARNINGS;
         SHOW WARNINGS;"
)
expect_value "sql calc locking visible row" "1" "$(printf '%s\n' "$calc_locking" | sed -n '1p')"
expect_value "sql calc locking warning count" "1" "$(printf '%s\n' "$calc_locking" | sed -n '2p')"
expect_contains \
    "sql calc locking warning" \
    "$(printf '%s\n' "$calc_locking" | sed -n '3p')" \
    "Warning	1287	SQL_CALC_FOUND_ROWS is deprecated"

calc_locking_found_rows=$(
    run_mysql \
        "USE ${DATABASE};
         SELECT SQL_CALC_FOUND_ROWS id FROM t ORDER BY id LIMIT 1 FOR UPDATE;
         SELECT FOUND_ROWS(), @@warning_count, ROW_COUNT();"
)
expect_value \
    "sql calc locking found rows" \
    "1
3	1	-1" \
    "$calc_locking_found_rows"

source_dml=$(
    run_mysql \
        "USE ${DATABASE};
         INSERT INTO dst SELECT id FROM t ORDER BY id LIMIT 1 FOR UPDATE;
         SELECT ROW_COUNT(), @@warning_count;
         SELECT COUNT(*), MIN(id) FROM dst;
         REPLACE INTO dst SELECT id FROM t ORDER BY id LIMIT 1 FOR SHARE;
         SELECT ROW_COUNT(), @@warning_count;
         SELECT COUNT(*), MIN(id) FROM dst;"
)
expect_value \
    "locking source DML no-op results" \
    "1	0
1	1
1	0
2	1" \
    "$source_dml"

accepted_deferred_options=$(
    run_mysql \
        "USE ${DATABASE};
         SELECT id FROM t ORDER BY id FOR UPDATE NOWAIT;
         SELECT id FROM t ORDER BY id FOR SHARE SKIP LOCKED;
         SELECT id FROM t ORDER BY id FOR UPDATE OF t;"
)
expect_value \
    "mysql accepted locking options deferred by MyLite" \
    "1
2
3
1
2
3
1
2
3" \
    "$accepted_deferred_options"

expect_error \
    "ctas locking clause rejected" \
    1746 \
    HY000 \
    "Can't update table 't' while 'copy' is being created" \
    "USE ${DATABASE}; CREATE TABLE copy AS SELECT id FROM t FOR UPDATE;"

expect_error \
    "repeated locking clause rejected" \
    3569 \
    HY000 \
    "Table t appears in multiple locking clauses" \
    "USE ${DATABASE}; SELECT id FROM t FOR UPDATE FOR SHARE;"

expect_error \
    "misplaced locking clause rejected" \
    1064 \
    42000 \
    "near 'ORDER BY id'" \
    "USE ${DATABASE}; SELECT id FROM t FOR UPDATE ORDER BY id;"

printf '%s\n' "mysql_baseline_select_locking_clauses_expectations: ok"
