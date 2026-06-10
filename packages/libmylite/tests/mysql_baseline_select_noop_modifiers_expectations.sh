#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_select_noop_modifiers_$$"

fail() {
    printf '%s\n' "mysql_baseline_select_noop_modifiers_expectations: $1" >&2
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
     INSERT INTO t VALUES (1, 10), (2, NULL), (3, 10);" >/dev/null

scalar_modifiers=$(
    run_mysql \
        "USE ${DATABASE};
         SELECT HIGH_PRIORITY 1;
         SELECT STRAIGHT_JOIN 1;
         SELECT SQL_SMALL_RESULT 1;
         SELECT SQL_BIG_RESULT 1;
         SELECT SQL_BUFFER_RESULT 1;
         SELECT SQL_NO_CACHE 1;
         SHOW COUNT(*) WARNINGS;
         SHOW WARNINGS;"
)
expect_value \
    "scalar no-op modifier rows and sql no-cache warning" \
    "1
1
1
1
1
1
1
Warning	1681	'SQL_NO_CACHE' is deprecated and will be removed in a future release." \
    "$scalar_modifiers"

table_modifiers=$(
    run_mysql \
        "USE ${DATABASE};
         SELECT HIGH_PRIORITY STRAIGHT_JOIN SQL_SMALL_RESULT SQL_BIG_RESULT SQL_BUFFER_RESULT id
           FROM t WHERE n <=> 10 ORDER BY id LIMIT 2;
         SELECT FOUND_ROWS(), ROW_COUNT(), @@warning_count, @@error_count;"
)
expect_value \
    "table no-op modifiers" \
    "1
3
2	-1	1	0" \
    "$table_modifiers"

sql_no_cache_table=$(
    run_mysql \
        "USE ${DATABASE};
         SELECT ALL HIGH_PRIORITY STRAIGHT_JOIN SQL_SMALL_RESULT SQL_BIG_RESULT SQL_BUFFER_RESULT
           SQL_NO_CACHE id FROM t ORDER BY id LIMIT 1;
         SHOW COUNT(*) WARNINGS;
         SHOW WARNINGS;
         SELECT FOUND_ROWS();"
)
expect_value \
    "sql no-cache table warning and found rows" \
    "1
1
Warning	1681	'SQL_NO_CACHE' is deprecated and will be removed in a future release.
1" \
    "$sql_no_cache_table"

found_rows_warning_order=$(
    run_mysql \
        "USE ${DATABASE};
         SELECT SQL_NO_CACHE FOUND_ROWS();
         SHOW COUNT(*) WARNINGS;
         SHOW WARNINGS;"
)
expect_value "found rows with sql no-cache value" "1" "$(printf '%s\n' "$found_rows_warning_order" | sed -n '1p')"
expect_value "found rows with sql no-cache count" "2" "$(printf '%s\n' "$found_rows_warning_order" | sed -n '2p')"
expect_contains \
    "sql no-cache warning first" \
    "$(printf '%s\n' "$found_rows_warning_order" | sed -n '3p')" \
    "Warning	1681	'SQL_NO_CACHE' is deprecated"
expect_contains \
    "found rows warning second" \
    "$(printf '%s\n' "$found_rows_warning_order" | sed -n '4p')" \
    "Warning	1287	FOUND_ROWS() is deprecated"

calc_warning_order=$(
    run_mysql \
        "USE ${DATABASE};
         SELECT SQL_NO_CACHE SQL_CALC_FOUND_ROWS id FROM t ORDER BY id LIMIT 1;
         SHOW COUNT(*) WARNINGS;
         SHOW WARNINGS;"
)
expect_value "sql calc with no-cache visible row" "1" "$(printf '%s\n' "$calc_warning_order" | sed -n '1p')"
expect_value "sql calc with no-cache warning count" "2" "$(printf '%s\n' "$calc_warning_order" | sed -n '2p')"
expect_contains \
    "sql no-cache before sql calc" \
    "$(printf '%s\n' "$calc_warning_order" | sed -n '3p')" \
    "Warning	1681	'SQL_NO_CACHE' is deprecated"
expect_contains \
    "sql calc warning after sql no-cache" \
    "$(printf '%s\n' "$calc_warning_order" | sed -n '4p')" \
    "Warning	1287	SQL_CALC_FOUND_ROWS is deprecated"

calc_found_rows=$(
    run_mysql \
        "USE ${DATABASE};
         SELECT SQL_NO_CACHE SQL_CALC_FOUND_ROWS id FROM t ORDER BY id LIMIT 1;
         SELECT FOUND_ROWS(), @@warning_count, ROW_COUNT();"
)
expect_value \
    "sql calc with no-cache found rows" \
    "1
3	1	-1" \
    "$calc_found_rows"

distinct_aggregate_grouped=$(
    run_mysql \
        "USE ${DATABASE};
         SELECT DISTINCT HIGH_PRIORITY STRAIGHT_JOIN SQL_SMALL_RESULT SQL_BIG_RESULT
           SQL_BUFFER_RESULT n FROM t ORDER BY n;
         SELECT SQL_BIG_RESULT DISTINCT n FROM t ORDER BY n;
         SELECT SQL_SMALL_RESULT SQL_BUFFER_RESULT DISTINCT n FROM t ORDER BY n;
         SELECT SQL_SMALL_RESULT SQL_BIG_RESULT SQL_BUFFER_RESULT COUNT(*) FROM t
           WHERE n IS NOT NULL;
         SELECT SQL_SMALL_RESULT SQL_BIG_RESULT SQL_BUFFER_RESULT n, COUNT(*) FROM t
           GROUP BY n ORDER BY n;"
)
expect_value \
    "distinct aggregate grouped no-op modifiers" \
    "NULL
10
NULL
10
NULL
10
2
NULL	1
10	2" \
    "$distinct_aggregate_grouped"

no_cache_before_distinct=$(
    run_mysql \
        "USE ${DATABASE};
         SELECT SQL_NO_CACHE DISTINCT n FROM t WHERE n IS NOT NULL ORDER BY n;
         SHOW COUNT(*) WARNINGS;
         SHOW WARNINGS;"
)
expect_value \
    "sql no-cache before distinct warning" \
    "10
1
Warning	1681	'SQL_NO_CACHE' is deprecated and will be removed in a future release." \
    "$no_cache_before_distinct"

calc_before_distinct=$(
    run_mysql \
        "USE ${DATABASE};
         SELECT SQL_CALC_FOUND_ROWS DISTINCT n FROM t WHERE n IS NOT NULL ORDER BY n LIMIT 1;
         SHOW COUNT(*) WARNINGS;
         SHOW WARNINGS;
         SELECT FOUND_ROWS();"
)
expect_value \
    "sql calc before distinct visible row" \
    "10" \
    "$(printf '%s\n' "$calc_before_distinct" | sed -n '1p')"
expect_value \
    "sql calc before distinct warning count" \
    "1" \
    "$(printf '%s\n' "$calc_before_distinct" | sed -n '2p')"
expect_contains \
    "sql calc before distinct warning" \
    "$(printf '%s\n' "$calc_before_distinct" | sed -n '3p')" \
    "Warning	1287	SQL_CALC_FOUND_ROWS is deprecated"
expect_value \
    "sql calc before distinct found rows" \
    "1" \
    "$(printf '%s\n' "$calc_before_distinct" | sed -n '4p')"

source_selects=$(
    run_mysql \
        "USE ${DATABASE};
         CREATE TABLE copy AS SELECT SQL_BUFFER_RESULT id FROM t ORDER BY id LIMIT 1;
         SELECT COUNT(*), MIN(id) FROM copy;
         CREATE TABLE inserted(id INT NOT NULL);
         INSERT INTO inserted SELECT SQL_NO_CACHE id FROM t ORDER BY id LIMIT 1;
         SHOW COUNT(*) WARNINGS;
         SHOW WARNINGS;
         SELECT COUNT(*), MIN(id) FROM inserted;
         CREATE TABLE replaced(id INT NOT NULL);
         REPLACE INTO replaced SELECT SQL_NO_CACHE id FROM t ORDER BY id LIMIT 1;
         SHOW COUNT(*) WARNINGS;
         SHOW WARNINGS;
         SELECT COUNT(*), MIN(id) FROM replaced;"
)
expect_value \
    "source select no-op modifiers" \
    "1	1
1
Warning	1681	'SQL_NO_CACHE' is deprecated and will be removed in a future release.
1	1
1
Warning	1681	'SQL_NO_CACHE' is deprecated and will be removed in a future release.
1	1" \
    "$source_selects"

expect_error \
    "all distinct unsupported combination" \
    1221 \
    HY000 \
    "Incorrect usage of ALL and DISTINCT" \
    "USE ${DATABASE}; SELECT ALL DISTINCT id FROM t;"

expect_error \
    "sql cache removed" \
    1054 \
    42S22 \
    "Unknown column 'SQL_CACHE' in 'field list'" \
    "USE ${DATABASE}; SELECT SQL_CACHE id FROM t;"

printf '%s\n' "mysql_baseline_select_noop_modifiers_expectations: ok"
