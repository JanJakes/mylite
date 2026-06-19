#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_found_rows_expectations_$$"

fail() {
    printf '%s\n' "mysql_baseline_found_rows_function_expectations: $1" >&2
    exit 1
}

run_mysql() {
    sql=$1
    shift
    printf '%s\n' "$sql" \
        | docker exec -i "$MYSQL_CONTAINER" mysql --protocol=TCP -h127.0.0.1 -uroot --batch --raw --skip-column-names "$@"
}

run_mysql_with_headers() {
    sql=$1
    shift
    printf '%s\n' "$sql" \
        | docker exec -i "$MYSQL_CONTAINER" mysql --protocol=TCP -h127.0.0.1 -uroot --batch --raw "$@"
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
    "CREATE DATABASE ${DATABASE}; USE ${DATABASE};
     CREATE TABLE t(id INT NOT NULL, n INT NULL);
     INSERT INTO t VALUES (1, NULL), (2, 20), (3, 20), (4, 30);" >/dev/null

function_headers=$(run_mysql_with_headers \
    "USE ${DATABASE};
     SELECT FOUND_ROWS(), Found_Rows(), found_rows(), FOUND_ROWS (), (FOUND_ROWS()) FROM DUAL;
     SHOW WARNINGS;"
)
expect_value \
    "found rows labels" \
    "FOUND_ROWS()	Found_Rows()	found_rows()	FOUND_ROWS ()	(FOUND_ROWS())" \
    "$(printf '%s\n' "$function_headers" | sed -n '1p')"
expect_value \
    "found rows initial values" \
    "1	1	1	1	1" \
    "$(printf '%s\n' "$function_headers" | sed -n '2p')"
warning_lines=$(printf '%s\n' "$function_headers" | sed -n '4,$p')
expect_value "found rows warning count" "5" "$(printf '%s\n' "$warning_lines" | wc -l | tr -d ' ')"
expect_contains \
    "found rows deprecation warning" \
    "$warning_lines" \
    "FOUND_ROWS() is deprecated and will be removed in a future release. Consider using COUNT(*) instead."

calc_limit=$(run_mysql \
    "USE ${DATABASE};
     SELECT SQL_CALC_FOUND_ROWS id FROM t ORDER BY id LIMIT 2;
     SHOW WARNINGS;
     SELECT FOUND_ROWS(), @@warning_count, ROW_COUNT();
     SHOW WARNINGS;
     SELECT FOUND_ROWS(), @@warning_count, ROW_COUNT();"
)
expect_value "sql calc visible row 1" "1" "$(printf '%s\n' "$calc_limit" | sed -n '1p')"
expect_value "sql calc visible row 2" "2" "$(printf '%s\n' "$calc_limit" | sed -n '2p')"
expect_contains \
    "sql calc warning" \
    "$(printf '%s\n' "$calc_limit" | sed -n '3p')" \
    "SQL_CALC_FOUND_ROWS is deprecated and will be removed in a future release. Consider using two separate queries instead."
expect_value "found rows after calc limit" "4	1	-1" "$(printf '%s\n' "$calc_limit" | sed -n '4p')"
expect_contains \
    "found rows warning after calc" \
    "$(printf '%s\n' "$calc_limit" | sed -n '5p')" \
    "FOUND_ROWS() is deprecated and will be removed in a future release. Consider using COUNT(*) instead."
expect_value "found rows select updates state" "1	1	-1" "$(printf '%s\n' "$calc_limit" | sed -n '6p')"

source_backed=$(run_mysql \
    "USE ${DATABASE};
     SELECT SQL_CALC_FOUND_ROWS id FROM t ORDER BY id LIMIT 2;
     SELECT FOUND_ROWS() FROM t ORDER BY id LIMIT 2;
     SHOW WARNINGS;
     SELECT FOUND_ROWS(), @@warning_count, ROW_COUNT();
     SELECT id FROM t WHERE FOUND_ROWS() = 1 ORDER BY id LIMIT 1;
     SHOW WARNINGS;
     SELECT @@warning_count, ROW_COUNT();"
)
expect_value "source-backed seed row 1" "1" "$(printf '%s\n' "$source_backed" | sed -n '1p')"
expect_value "source-backed seed row 2" "2" "$(printf '%s\n' "$source_backed" | sed -n '2p')"
expect_value "source-backed found rows projection row 1" "4" \
    "$(printf '%s\n' "$source_backed" | sed -n '3p')"
expect_value "source-backed found rows projection row 2" "4" \
    "$(printf '%s\n' "$source_backed" | sed -n '4p')"
expect_contains \
    "source-backed found rows projection warning" \
    "$(printf '%s\n' "$source_backed" | sed -n '5p')" \
    "FOUND_ROWS() is deprecated and will be removed in a future release. Consider using COUNT(*) instead."
expect_value "source-backed found rows state" "2	1	-1" \
    "$(printf '%s\n' "$source_backed" | sed -n '6p')"
expect_value "source-backed found rows predicate row" "1" \
    "$(printf '%s\n' "$source_backed" | sed -n '7p')"
expect_contains \
    "source-backed found rows predicate warning" \
    "$(printf '%s\n' "$source_backed" | sed -n '8p')" \
    "FOUND_ROWS() is deprecated and will be removed in a future release. Consider using COUNT(*) instead."
expect_value "source-backed found rows predicate diagnostics" "1	-1" \
    "$(printf '%s\n' "$source_backed" | sed -n '9p')"

calc_count=$(run_mysql \
    "USE ${DATABASE};
     SELECT SQL_CALC_FOUND_ROWS COUNT(*) FROM t;
     SELECT FOUND_ROWS(), @@warning_count, ROW_COUNT();"
)
expect_value "sql calc count value" "4" "$(printf '%s\n' "$calc_count" | sed -n '1p')"
expect_value "sql calc count found rows" "1	1	-1" \
    "$(printf '%s\n' "$calc_count" | sed -n '2p')"

calc_row_scalar=$(run_mysql \
    "USE ${DATABASE};
     SELECT SQL_CALC_FOUND_ROWS id, 1 AS marker FROM t WHERE n <=> 20 ORDER BY id LIMIT 1;
     SELECT FOUND_ROWS(), @@warning_count, ROW_COUNT();
     SELECT SQL_CALC_FOUND_ROWS t.*, 1 AS marker FROM t ORDER BY id LIMIT 2;
     SELECT FOUND_ROWS();"
)
expect_value "sql calc row scalar visible row" "2	1" \
    "$(printf '%s\n' "$calc_row_scalar" | sed -n '1p')"
expect_value "sql calc row scalar found rows" "2	1	-1" \
    "$(printf '%s\n' "$calc_row_scalar" | sed -n '2p')"
expect_value "sql calc wildcard row 1" "1	NULL	1" \
    "$(printf '%s\n' "$calc_row_scalar" | sed -n '3p')"
expect_value "sql calc wildcard row 2" "2	20	1" \
    "$(printf '%s\n' "$calc_row_scalar" | sed -n '4p')"
expect_value "sql calc wildcard found rows" "4" \
    "$(printf '%s\n' "$calc_row_scalar" | sed -n '5p')"

ordinary_limit=$(run_mysql \
    "USE ${DATABASE};
     SELECT id FROM t ORDER BY id LIMIT 2;
     SELECT FOUND_ROWS(), @@warning_count, ROW_COUNT();
     SELECT id FROM t ORDER BY id LIMIT 1, 2;
     SELECT FOUND_ROWS();
     SELECT id FROM t ORDER BY id LIMIT 10, 2;
     SELECT FOUND_ROWS();
     SELECT id FROM t ORDER BY id LIMIT 2, 0;
     SELECT FOUND_ROWS();
     SELECT id FROM t ORDER BY id LIMIT 0;
     SELECT FOUND_ROWS();
     SELECT id FROM t ORDER BY id;
     SELECT FOUND_ROWS();"
)
expect_value "ordinary limit row 1" "1" "$(printf '%s\n' "$ordinary_limit" | sed -n '1p')"
expect_value "ordinary limit row 2" "2" "$(printf '%s\n' "$ordinary_limit" | sed -n '2p')"
expect_value "ordinary limit found rows" "2	1	-1" "$(printf '%s\n' "$ordinary_limit" | sed -n '3p')"
expect_value "ordinary offset row 1" "2" "$(printf '%s\n' "$ordinary_limit" | sed -n '4p')"
expect_value "ordinary offset row 2" "3" "$(printf '%s\n' "$ordinary_limit" | sed -n '5p')"
expect_value "ordinary offset found rows" "3" "$(printf '%s\n' "$ordinary_limit" | sed -n '6p')"
expect_value "ordinary beyond offset found rows" "4" "$(printf '%s\n' "$ordinary_limit" | sed -n '7p')"
expect_value "ordinary zero row count found rows" "2" "$(printf '%s\n' "$ordinary_limit" | sed -n '8p')"
expect_value "ordinary limit zero found rows" "0" "$(printf '%s\n' "$ordinary_limit" | sed -n '9p')"
expect_value "ordinary no-limit row 1" "1" "$(printf '%s\n' "$ordinary_limit" | sed -n '10p')"
expect_value "ordinary no-limit row 4" "4" "$(printf '%s\n' "$ordinary_limit" | sed -n '13p')"
expect_value "ordinary no-limit found rows" "4" "$(printf '%s\n' "$ordinary_limit" | sed -n '14p')"

calc_filtered=$(run_mysql \
    "USE ${DATABASE};
     SELECT SQL_CALC_FOUND_ROWS id FROM t WHERE n <=> 20 ORDER BY id LIMIT 1;
     SELECT FOUND_ROWS();
     SELECT SQL_CALC_FOUND_ROWS id FROM t ORDER BY id LIMIT 0;
     SHOW WARNINGS;
     SELECT FOUND_ROWS();
     SELECT SQL_CALC_FOUND_ROWS id FROM t ORDER BY id;
     SELECT FOUND_ROWS();"
)
expect_value "filtered calc visible row" "2" "$(printf '%s\n' "$calc_filtered" | sed -n '1p')"
expect_value "filtered calc found rows" "2" "$(printf '%s\n' "$calc_filtered" | sed -n '2p')"
expect_contains \
    "limit zero calc warning" \
    "$(printf '%s\n' "$calc_filtered" | sed -n '3p')" \
    "SQL_CALC_FOUND_ROWS is deprecated"
expect_value "limit zero calc found rows" "4" "$(printf '%s\n' "$calc_filtered" | sed -n '4p')"
expect_value "no limit calc visible row 1" "1" "$(printf '%s\n' "$calc_filtered" | sed -n '5p')"
expect_value "no limit calc visible row 4" "4" "$(printf '%s\n' "$calc_filtered" | sed -n '8p')"
expect_value "no limit calc found rows" "4" "$(printf '%s\n' "$calc_filtered" | sed -n '9p')"

joined_calc=$(run_mysql \
    "USE ${DATABASE};
     CREATE TABLE lefts(id INT NOT NULL, k INT NULL, v INT NULL);
     CREATE TABLE rights(id INT NOT NULL, k INT NULL, w INT NULL);
     INSERT INTO lefts VALUES (1,10,100),(2,20,200),(3,NULL,300);
     INSERT INTO rights VALUES (7,10,700),(8,10,800),(9,NULL,900);
     SELECT SQL_CALC_FOUND_ROWS lefts.id, rights.id
       FROM lefts JOIN rights ON lefts.k = rights.k
       ORDER BY rights.id LIMIT 1;
     SHOW WARNINGS;
     SELECT FOUND_ROWS(), @@warning_count, ROW_COUNT();
     SELECT SQL_CALC_FOUND_ROWS lefts.id, rights.id
       FROM lefts, rights
       WHERE lefts.k = rights.k
       ORDER BY rights.id LIMIT 0;
     SHOW WARNINGS;
     SELECT FOUND_ROWS(), @@warning_count, ROW_COUNT();
     SELECT SQL_CALC_FOUND_ROWS lefts.id, rights.id
       FROM lefts, rights
       ORDER BY lefts.id, rights.id LIMIT 2;
     SELECT FOUND_ROWS(), @@warning_count, ROW_COUNT();
     SELECT SQL_CALC_FOUND_ROWS lefts.id, rights.id
       FROM lefts LEFT JOIN rights ON lefts.k = rights.k
       ORDER BY lefts.id, rights.id LIMIT 2;
     SHOW WARNINGS;
     SELECT FOUND_ROWS(), @@warning_count, ROW_COUNT();
     SELECT SQL_CALC_FOUND_ROWS lefts.id, rights.id
       FROM lefts RIGHT JOIN rights ON lefts.k = rights.k
       ORDER BY rights.id, lefts.id LIMIT 2;
     SHOW WARNINGS;
     SELECT FOUND_ROWS(), @@warning_count, ROW_COUNT();
     SELECT DISTINCT SQL_CALC_FOUND_ROWS lefts.id
       FROM lefts JOIN rights ON lefts.k = rights.k
       ORDER BY lefts.id LIMIT 1;
     SELECT FOUND_ROWS(), @@warning_count, ROW_COUNT();
     SELECT SQL_CALC_FOUND_ROWS lefts.id
       FROM lefts LEFT JOIN rights ON lefts.k = rights.k
       GROUP BY lefts.id ORDER BY lefts.id LIMIT 1;
     SHOW WARNINGS;
     SELECT FOUND_ROWS(), @@warning_count, ROW_COUNT();
     SELECT SQL_CALC_FOUND_ROWS lefts.id
       FROM lefts LEFT JOIN rights ON (lefts.k = rights.k)
       WHERE NOT EXISTS (
         SELECT 1 FROM rights r2 WHERE r2.k = rights.k LIMIT 1
       )
       GROUP BY lefts.id ORDER BY lefts.id LIMIT 1;
     SHOW WARNINGS;
     SELECT FOUND_ROWS(), @@warning_count, ROW_COUNT();
     SELECT lefts.id, rights.id
       FROM lefts JOIN rights ON lefts.k = rights.k
       ORDER BY rights.id LIMIT 1, 1;
     SELECT FOUND_ROWS(), @@warning_count, ROW_COUNT();"
)
expect_value "joined sql calc visible row" "1	7" "$(printf '%s\n' "$joined_calc" | sed -n '1p')"
expect_contains \
    "joined sql calc warning" \
    "$(printf '%s\n' "$joined_calc" | sed -n '2p')" \
    "SQL_CALC_FOUND_ROWS is deprecated"
expect_value "joined sql calc found rows" "2	1	-1" "$(printf '%s\n' "$joined_calc" | sed -n '3p')"
expect_contains \
    "comma joined limit zero sql calc warning" \
    "$(printf '%s\n' "$joined_calc" | sed -n '4p')" \
    "SQL_CALC_FOUND_ROWS is deprecated"
expect_value \
    "comma joined limit zero found rows" \
    "2	1	-1" \
    "$(printf '%s\n' "$joined_calc" | sed -n '5p')"
expect_value \
    "cartesian joined sql calc visible row 1" \
    "1	7" \
    "$(printf '%s\n' "$joined_calc" | sed -n '6p')"
expect_value \
    "cartesian joined sql calc visible row 2" \
    "1	8" \
    "$(printf '%s\n' "$joined_calc" | sed -n '7p')"
expect_value \
    "cartesian joined sql calc found rows" \
    "9	1	-1" \
    "$(printf '%s\n' "$joined_calc" | sed -n '8p')"
expect_value \
    "left joined sql calc visible row 1" \
    "1	7" \
    "$(printf '%s\n' "$joined_calc" | sed -n '9p')"
expect_value \
    "left joined sql calc visible row 2" \
    "1	8" \
    "$(printf '%s\n' "$joined_calc" | sed -n '10p')"
expect_contains \
    "left joined sql calc warning" \
    "$(printf '%s\n' "$joined_calc" | sed -n '11p')" \
    "SQL_CALC_FOUND_ROWS is deprecated"
expect_value "left joined sql calc found rows" "4	1	-1" "$(printf '%s\n' "$joined_calc" | sed -n '12p')"
expect_value \
    "right joined sql calc visible row 1" \
    "1	7" \
    "$(printf '%s\n' "$joined_calc" | sed -n '13p')"
expect_value \
    "right joined sql calc visible row 2" \
    "1	8" \
    "$(printf '%s\n' "$joined_calc" | sed -n '14p')"
expect_contains \
    "right joined sql calc warning" \
    "$(printf '%s\n' "$joined_calc" | sed -n '15p')" \
    "SQL_CALC_FOUND_ROWS is deprecated"
expect_value "right joined sql calc found rows" "3	1	-1" "$(printf '%s\n' "$joined_calc" | sed -n '16p')"
expect_value \
    "distinct joined sql calc visible row" \
    "1" \
    "$(printf '%s\n' "$joined_calc" | sed -n '17p')"
expect_value \
    "distinct joined sql calc found rows" \
    "1	1	-1" \
    "$(printf '%s\n' "$joined_calc" | sed -n '18p')"
expect_value \
    "grouped joined sql calc visible row" \
    "1" \
    "$(printf '%s\n' "$joined_calc" | sed -n '19p')"
expect_contains \
    "grouped joined sql calc warning" \
    "$(printf '%s\n' "$joined_calc" | sed -n '20p')" \
    "SQL_CALC_FOUND_ROWS is deprecated"
expect_value "grouped joined sql calc found rows" "3	1	-1" "$(printf '%s\n' "$joined_calc" | sed -n '21p')"
expect_value \
    "grouped joined not exists sql calc visible row" \
    "2" \
    "$(printf '%s\n' "$joined_calc" | sed -n '22p')"
expect_contains \
    "grouped joined not exists sql calc warning" \
    "$(printf '%s\n' "$joined_calc" | sed -n '23p')" \
    "SQL_CALC_FOUND_ROWS is deprecated"
expect_value \
    "grouped joined not exists sql calc found rows" \
    "2	1	-1" \
    "$(printf '%s\n' "$joined_calc" | sed -n '24p')"
expect_value \
    "ordinary joined offset visible row" \
    "1	8" \
    "$(printf '%s\n' "$joined_calc" | sed -n '25p')"
expect_value \
    "ordinary joined offset found rows" \
    "2	1	-1" \
    "$(printf '%s\n' "$joined_calc" | sed -n '26p')"

non_select=$(run_mysql \
    "USE ${DATABASE};
     SELECT SQL_CALC_FOUND_ROWS id FROM t ORDER BY id LIMIT 1;
     SELECT FOUND_ROWS();
     DO 0;
     SELECT FOUND_ROWS(), ROW_COUNT();
     UPDATE t SET n = n WHERE id = 1;
     SELECT FOUND_ROWS(), ROW_COUNT();"
)
expect_value "non-select setup row" "1" "$(printf '%s\n' "$non_select" | sed -n '1p')"
expect_value "non-select setup found rows" "4" "$(printf '%s\n' "$non_select" | sed -n '2p')"
expect_value "do preserves found rows" "1	0" "$(printf '%s\n' "$non_select" | sed -n '3p')"
expect_value "update preserves found rows" "1	0" "$(printf '%s\n' "$non_select" | sed -n '4p')"

deferred_shapes=$(run_mysql \
    "USE ${DATABASE};
     SELECT SQL_CALC_FOUND_ROWS DISTINCT n FROM t ORDER BY n LIMIT 1;
     SELECT FOUND_ROWS();
     SELECT SQL_CALC_FOUND_ROWS DISTINCT lefts.id
       FROM lefts JOIN rights ON lefts.k = rights.k
       ORDER BY lefts.id LIMIT 1;
     SELECT FOUND_ROWS();
     SELECT SQL_CALC_FOUND_ROWS n, COUNT(*) FROM t GROUP BY n ORDER BY n LIMIT 1;
     SELECT FOUND_ROWS();"
)
expect_value "mysql accepts distinct calc row" "NULL" "$(printf '%s\n' "$deferred_shapes" | sed -n '1p')"
expect_value "mysql distinct calc found rows" "3" "$(printf '%s\n' "$deferred_shapes" | sed -n '2p')"
expect_value "mysql accepts joined distinct calc row" "1" "$(printf '%s\n' "$deferred_shapes" | sed -n '3p')"
expect_value "mysql joined distinct calc found rows" "1" "$(printf '%s\n' "$deferred_shapes" | sed -n '4p')"
expect_value "mysql accepts grouped calc first group" "NULL	1" "$(printf '%s\n' "$deferred_shapes" | sed -n '5p')"
expect_value "mysql grouped calc found rows" "3" "$(printf '%s\n' "$deferred_shapes" | sed -n '6p')"

expect_error \
    "found rows one argument" \
    1582 \
    42000 \
    "Incorrect parameter count in the call to native function 'FOUND_ROWS'" \
    "USE ${DATABASE}; SELECT FOUND_ROWS(1);"

expect_error \
    "found rows two arguments" \
    1582 \
    42000 \
    "Incorrect parameter count in the call to native function 'FOUND_ROWS'" \
    "USE ${DATABASE}; SELECT FOUND_ROWS(1, 2);"

printf '%s\n' "mysql_baseline_found_rows_function_expectations: ok"
