#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_having_grouped_aggregate_expectations_$$"

fail() {
    printf '%s\n' "mysql_baseline_having_grouped_aggregate_expectations: $1" >&2
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
       g INT NULL,
       n INT NULL,
       nn INT NOT NULL,
       u INT UNSIGNED NULL,
       b BIGINT NULL
     ) ENGINE=InnoDB;
     INSERT INTO t VALUES
       (1, NULL, NULL, 5, 0, -1),
       (2, NULL, 5, 5, 2, 2),
       (3, 1, 10, 7, 4294967295, -9223372036854775808),
       (4, 1, NULL, 7, NULL, 4),
       (5, 1, 20, 1, 7, NULL),
       (6, 2, NULL, 9, NULL, NULL),
       (7, 2, NULL, 9, NULL, NULL);" >/dev/null

core=$(run_mysql \
    "USE ${DATABASE};
     DO 0;
     SELECT g, COUNT(*) AS c FROM t GROUP BY g HAVING c > 1 ORDER BY g;
     SELECT @@warning_count, ROW_COUNT();
     SELECT g, COUNT(n) AS c FROM t GROUP BY g HAVING c = 0 ORDER BY g;
     SELECT g, SUM(n) AS s FROM t GROUP BY g HAVING s IS NULL ORDER BY g;
     SELECT g, MIN(n) AS m FROM t GROUP BY g HAVING MIN(n) < 10 ORDER BY g;
     SELECT g, MAX(n) AS m FROM t GROUP BY g HAVING MAX(n) >= 20 ORDER BY g;
     SELECT g, AVG(n) AS a FROM t GROUP BY g HAVING AVG(n) = 15 ORDER BY g;
     SELECT g, SUM(u) AS s FROM t GROUP BY g HAVING s >= 4294967295 ORDER BY g;
     SELECT g, MIN(b) AS m FROM t GROUP BY g HAVING m <=> -9223372036854775808 ORDER BY g;"
)
expect_value "count alias null group" "NULL	2" "$(printf '%s\n' "$core" | sed -n '1p')"
expect_value "count alias group one" "1	3" "$(printf '%s\n' "$core" | sed -n '2p')"
expect_value "count alias group two" "2	2" "$(printf '%s\n' "$core" | sed -n '3p')"
expect_value "having select status" "0	-1" "$(printf '%s\n' "$core" | sed -n '4p')"
expect_value "count column all null group" "2	0" "$(printf '%s\n' "$core" | sed -n '5p')"
expect_value "sum null group" "2	NULL" "$(printf '%s\n' "$core" | sed -n '6p')"
expect_value "min expression null group" "NULL	5" "$(printf '%s\n' "$core" | sed -n '7p')"
expect_value "max expression group one" "1	20" "$(printf '%s\n' "$core" | sed -n '8p')"
expect_value "avg expression group one" "1	15.0000" "$(printf '%s\n' "$core" | sed -n '9p')"
expect_value "unsigned sum group one" "1	4294967302" "$(printf '%s\n' "$core" | sed -n '10p')"
expect_value "bigint min group one" "1	-9223372036854775808" \
    "$(printf '%s\n' "$core" | sed -n '11p')"

group_key=$(run_mysql \
    "USE ${DATABASE};
     SELECT g, COUNT(*) FROM t GROUP BY g HAVING g IS NULL ORDER BY g;
     SELECT g, COUNT(*) FROM t GROUP BY g HAVING g <=> 1 ORDER BY g;
     SELECT g, COUNT(*) FROM t GROUP BY g HAVING g > 0 ORDER BY g LIMIT 1;
     SELECT g AS k, COUNT(*) FROM t GROUP BY g HAVING k = 2 ORDER BY k;
     SELECT g AS x, COUNT(*) AS x FROM t GROUP BY g HAVING x > 2 ORDER BY g;
     SELECT g, COUNT(*) AS g FROM t GROUP BY g HAVING g > 1;
     SELECT g, COUNT(*) FROM t GROUP BY g HAVING g IN (1,2) ORDER BY g;
     SELECT g AS k, COUNT(*) FROM t GROUP BY g HAVING k IN (1,NULL) ORDER BY k;"
)
expect_value "group is null" "NULL	2" "$(printf '%s\n' "$group_key" | sed -n '1p')"
expect_value "group null safe equal" "1	3" "$(printf '%s\n' "$group_key" | sed -n '2p')"
expect_value "having before limit" "1	3" "$(printf '%s\n' "$group_key" | sed -n '3p')"
expect_value "group alias" "2	2" "$(printf '%s\n' "$group_key" | sed -n '4p')"
expect_value "duplicate alias aggregate precedence group one" "1	3" \
    "$(printf '%s\n' "$group_key" | sed -n '5p')"
expect_value "descriptor name before aggregate alias" "2	2" \
    "$(printf '%s\n' "$group_key" | sed -n '6p')"
expect_value "group in first" "1	3" "$(printf '%s\n' "$group_key" | sed -n '7p')"
expect_value "group in second" "2	2" "$(printf '%s\n' "$group_key" | sed -n '8p')"
expect_value "group alias in skips null list item" "1	3" \
    "$(printf '%s\n' "$group_key" | sed -n '9p')"

where_and_limit=$(run_mysql \
    "USE ${DATABASE};
     SELECT g, COUNT(*) AS c FROM t WHERE id >= 3 GROUP BY g HAVING c > 1 ORDER BY g;
     SELECT g, COUNT(*) AS c FROM t GROUP BY g HAVING c > 1 ORDER BY g LIMIT 0;
     SELECT g, COUNT(*) AS c FROM t GROUP BY g HAVING c > 2 ORDER BY g DESC;"
)
expect_value "where before having group one" "1	3" "$(printf '%s\n' "$where_and_limit" | sed -n '1p')"
expect_value "where before having group two" "2	2" "$(printf '%s\n' "$where_and_limit" | sed -n '2p')"
expect_value "limit zero then desc remaining" "1	3" "$(printf '%s\n' "$where_and_limit" | sed -n '3p')"

headers=$(run_mysql_with_headers \
    "USE ${DATABASE};
     SELECT a.g AS k, SUM(a.n) AS s FROM ${DATABASE}.t AS a
       GROUP BY a.g HAVING s IS NOT NULL ORDER BY k DESC;"
)
expect_value "header labels" "k	s" "$(printf '%s\n' "$headers" | sed -n '1p')"
expect_value "qualified alias desc group one" "1	30" "$(printf '%s\n' "$headers" | sed -n '2p')"
expect_value "qualified alias desc null group" "NULL	5" "$(printf '%s\n' "$headers" | sed -n '3p')"

no_match=$(run_mysql \
    "USE ${DATABASE};
     SELECT g, COUNT(*) AS c FROM t GROUP BY g HAVING c > 99 ORDER BY g;"
)
expect_value "having no matching groups" "" "$no_match"

accepted_but_deferred=$(run_mysql \
    "USE ${DATABASE};
     SELECT g, COUNT(*) FROM t GROUP BY g HAVING SUM(n) > 20 ORDER BY g;
     SELECT g, COUNT(*) FROM t GROUP BY g HAVING COUNT(DISTINCT n) > 1 ORDER BY g;
     SELECT g, COUNT(*) FROM t GROUP BY g HAVING COUNT(*) + 1 > 2 ORDER BY g;
     SELECT g, BIT_OR(nn) AS bits FROM t GROUP BY g HAVING bits > 1 ORDER BY g;
     SELECT COUNT(*) AS c FROM t HAVING c > 1;"
)
expect_value "different aggregate accepted" "1	3" \
    "$(printf '%s\n' "$accepted_but_deferred" | sed -n '1p')"
expect_value "count distinct accepted" "1	3" \
    "$(printf '%s\n' "$accepted_but_deferred" | sed -n '2p')"
expect_value "expression predicate null group" "NULL	2" \
    "$(printf '%s\n' "$accepted_but_deferred" | sed -n '3p')"
expect_value "bitwise alias accepted null group" "NULL	5" \
    "$(printf '%s\n' "$accepted_but_deferred" | sed -n '6p')"
expect_value "non-grouped having accepted" "7" \
    "$(printf '%s\n' "$accepted_but_deferred" | sed -n '9p')"

expect_error \
    "unknown having column" \
    1054 \
    "42S22" \
    "Unknown column 'missing' in 'having clause'" \
    "USE ${DATABASE}; SELECT g, COUNT(*) FROM t GROUP BY g HAVING missing > 1;"
expect_error \
    "non-group source column in having" \
    1054 \
    "42S22" \
    "Unknown column 'id' in 'having clause'" \
    "USE ${DATABASE}; SELECT g, COUNT(*) FROM t GROUP BY g HAVING id > 1;"
expect_error \
    "unknown aggregate argument" \
    1054 \
    "42S22" \
    "Unknown column 'missing' in 'having clause'" \
    "USE ${DATABASE}; SELECT g, SUM(n) FROM t GROUP BY g HAVING SUM(missing) > 1;"

cleanup

printf '%s\n' "mysql_baseline_having_grouped_aggregate_expectations: ok"
