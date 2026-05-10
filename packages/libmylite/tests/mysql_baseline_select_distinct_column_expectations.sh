#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_select_distinct_column_expectations_$$"

fail() {
    printf '%s\n' "mysql_baseline_select_distinct_column_expectations: $1" >&2
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
    "CREATE DATABASE ${DATABASE}; USE ${DATABASE};
     CREATE TABLE t(
       id INT NOT NULL,
       i INTEGER NULL,
       iu INT UNSIGNED NULL,
       b BIGINT NULL,
       bu BIGINT UNSIGNED NULL,
       n INT NULL,
       nn INT NOT NULL,
       bool_col BOOL NULL,
       other_col INT NULL
     );
     INSERT INTO t VALUES
       (1, -2147483648, 0, -9223372036854775808, 0, NULL, 10, TRUE, 30),
       (2, 0, 2, -1, 2, 20, 20, FALSE, 20),
       (3, 2147483647, 4294967295, -1, 9223372036854775807, 20, 30, FALSE, 40),
       (4, 5, NULL, 9223372036854775807, NULL, 30, 40, NULL, 10),
       (5, NULL, 0, NULL, 0, NULL, 50, TRUE, 50);" >/dev/null

core=$(run_mysql \
    "USE ${DATABASE};
     DO 0; SELECT DISTINCT n FROM t ORDER BY n; SELECT @@warning_count, ROW_COUNT();
     SELECT DISTINCT n FROM t ORDER BY n ASC;
     SELECT DISTINCT n FROM t ORDER BY n DESC;"
)
expect_value "distinct nullable int ascending first row" "NULL" "$(printf '%s\n' "$core" | sed -n '1p')"
expect_value "distinct nullable int ascending second row" "20" "$(printf '%s\n' "$core" | sed -n '2p')"
expect_value "distinct nullable int ascending third row" "30" "$(printf '%s\n' "$core" | sed -n '3p')"
expect_value "distinct status" "0	-1" "$(printf '%s\n' "$core" | sed -n '4p')"
expect_value "distinct asc explicit" "NULL
20
30" "$(printf '%s\n' "$core" | sed -n '5,7p')"
expect_value "distinct desc explicit" "30
20
NULL" "$(printf '%s\n' "$core" | sed -n '8,10p')"

schema_qualified=$(run_mysql \
    "SELECT DISTINCT n FROM ${DATABASE}.t ORDER BY n;"
)
expect_value "schema-qualified distinct" "NULL
20
30" "$schema_qualified"

families=$(run_mysql \
    "USE ${DATABASE};
     SELECT DISTINCT i FROM t ORDER BY i;
     SELECT DISTINCT iu FROM t ORDER BY iu;
     SELECT DISTINCT b FROM t ORDER BY b;
     SELECT DISTINCT bu FROM t ORDER BY bu;
     SELECT DISTINCT bool_col FROM t ORDER BY bool_col;"
)
expect_value "distinct integer families" "NULL
-2147483648
0
5
2147483647
NULL
0
2
4294967295
NULL
-9223372036854775808
-1
9223372036854775807
NULL
0
2
9223372036854775807
NULL
0
1" "$families"

limits=$(run_mysql \
    "USE ${DATABASE};
     SELECT DISTINCT n FROM t ORDER BY n LIMIT 0;
     SELECT 'after-limit-zero';
     SELECT DISTINCT n FROM t ORDER BY n LIMIT 2;
     SELECT DISTINCT n FROM t ORDER BY n LIMIT 99;
     SELECT DISTINCT n FROM t ORDER BY n LIMIT 1 OFFSET 1;
     SELECT DISTINCT n FROM t ORDER BY n LIMIT 1, 2;"
)
expect_value "limit forms" "after-limit-zero
NULL
20
NULL
20
30
20
20
30" "$limits"

predicates=$(run_mysql \
    "USE ${DATABASE};
     SELECT DISTINCT n FROM t WHERE n = 20 ORDER BY n;
     SELECT DISTINCT n FROM t WHERE n <=> 20 ORDER BY n;
     SELECT DISTINCT n FROM t WHERE n IS NULL ORDER BY n;
     SELECT DISTINCT n FROM t WHERE n IS NOT NULL ORDER BY n;
     SELECT DISTINCT n FROM t WHERE id <> 2 ORDER BY n;
     SELECT DISTINCT n FROM t WHERE id < 3 ORDER BY n;
     SELECT DISTINCT n FROM t WHERE id > 3 ORDER BY n;"
)
expect_value "predicate reuse" "20
20
NULL
20
30
NULL
20
30
NULL
20
NULL
30" "$predicates"

labels=$(run_mysql_with_headers \
    "USE ${DATABASE};
     SELECT DISTINCT n FROM t ORDER BY n;
     SELECT DISTINCT N FROM t ORDER BY N;
     SELECT DISTINCT \`n\` FROM t ORDER BY \`n\`;"
)
expect_value "label n" "n" "$(printf '%s\n' "$labels" | sed -n '1p')"
expect_value "label uppercase source spelling" "N" "$(printf '%s\n' "$labels" | sed -n '5p')"
expect_value "label quoted" "n" "$(printf '%s\n' "$labels" | sed -n '9p')"

accepted_but_deferred=$(run_mysql \
    "USE ${DATABASE};
     SELECT DISTINCT t.n FROM t ORDER BY t.n;
     SELECT DISTINCT n, nn FROM t ORDER BY n, nn;
     SELECT DISTINCT 1 FROM t;
     SELECT DISTINCT n + 1 FROM t ORDER BY n + 1;
     SELECT DISTINCTROW n FROM t ORDER BY n;
     SELECT ALL n FROM t ORDER BY id;
     SELECT DISTINCT n FROM t ORDER BY 1 DESC;
     SELECT DISTINCT n FROM t ORDER BY n + 0;"
)
expect_value "accepted deferred forms" "NULL
20
30
NULL	10
NULL	50
20	20
20	30
30	40
1
NULL
21
31
NULL
20
30
NULL
20
20
30
NULL
30
20
NULL
NULL
20
30" "$accepted_but_deferred"

expect_error \
    "unknown selected column" \
    1054 \
    42S22 \
    "Unknown column 'missing' in 'field list'" \
    "USE ${DATABASE}; SELECT DISTINCT missing FROM t;"

expect_error \
    "unknown predicate column" \
    1054 \
    42S22 \
    "Unknown column 'missing' in 'where clause'" \
    "USE ${DATABASE}; SELECT DISTINCT n FROM t WHERE missing = 1;"

expect_error \
    "unknown order column" \
    1054 \
    42S22 \
    "Unknown column 'missing' in 'order clause'" \
    "USE ${DATABASE}; SELECT DISTINCT n FROM t ORDER BY missing;"

expect_error \
    "non-selected order column" \
    3065 \
    HY000 \
    "which is not in SELECT list; this is incompatible with DISTINCT" \
    "USE ${DATABASE}; SELECT DISTINCT n FROM t ORDER BY id;"

expect_error \
    "missing default schema" \
    1046 \
    3D000 \
    "No database selected" \
    "SELECT DISTINCT n FROM t;"

expect_error \
    "unknown schema" \
    1049 \
    42000 \
    "Unknown database '${DATABASE}_missing'" \
    "SELECT DISTINCT n FROM ${DATABASE}_missing.t;"

expect_error \
    "unknown table" \
    1146 \
    42S02 \
    "Table '${DATABASE}.missing' doesn't exist" \
    "USE ${DATABASE}; SELECT DISTINCT n FROM missing;"
