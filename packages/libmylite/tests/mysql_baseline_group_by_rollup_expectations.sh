#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_group_by_rollup_expectations_$$"

fail() {
    printf '%s\n' "mysql_baseline_group_by_rollup_expectations: $1" >&2
    exit 1
}

run_mysql() {
    sql=$1
    shift
    printf '%s\n' "$sql" \
        | docker exec -i "$MYSQL_CONTAINER" mysql --protocol=TCP -h127.0.0.1 -uroot \
            --batch --raw --skip-column-names "$@"
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
run_mysql "CREATE DATABASE ${DATABASE} DEFAULT CHARACTER SET utf8mb4 "\
"COLLATE utf8mb4_0900_ai_ci; USE ${DATABASE}; "\
"CREATE TABLE t(id INT NOT NULL, g INT NULL, n INT NULL, nn INT NOT NULL); "\
"INSERT INTO t VALUES "\
"(1, NULL, NULL, 5), "\
"(2, 1, 10, 6), "\
"(3, 1, NULL, 7), "\
"(4, 2, 20, 8), "\
"(5, 2, 30, 9);" >/dev/null

count_rollup=$(run_mysql \
    "USE ${DATABASE};
     SELECT g, COUNT(*) FROM t GROUP BY g WITH ROLLUP;"
)
expected_count_rollup=$(cat <<EXPECTED
NULL	1
1	2
2	2
NULL	5
EXPECTED
)
expect_value "single-key count rollup" "$expected_count_rollup" "$count_rollup"

multi_rollup=$(run_mysql \
    "USE ${DATABASE};
     SELECT g, COUNT(*), COUNT(n), SUM(n), MIN(n), MAX(n), AVG(n)
     FROM t GROUP BY g WITH ROLLUP;"
)
expected_multi_rollup=$(cat <<EXPECTED
NULL	1	0	NULL	NULL	NULL	NULL
1	2	1	10	10	10	10.0000
2	2	2	50	20	30	25.0000
NULL	5	3	60	10	30	20.0000
EXPECTED
)
expect_value "single-key numeric aggregate rollup" "$expected_multi_rollup" "$multi_rollup"

where_rollup=$(run_mysql \
    "USE ${DATABASE};
     SELECT g, COUNT(*), SUM(n) FROM t WHERE n > 10 GROUP BY g WITH ROLLUP;"
)
expected_where_rollup=$(cat <<EXPECTED
2	2	50
NULL	2	50
EXPECTED
)
expect_value "single-key rollup source predicate" "$expected_where_rollup" "$where_rollup"

empty_rollup=$(run_mysql \
    "USE ${DATABASE};
     SELECT g, COUNT(*) FROM t WHERE n > 100 GROUP BY g WITH ROLLUP;"
)
expect_value "single-key rollup empty filtered source" "" "$empty_rollup"

projection_rollup=$(run_mysql \
    "USE ${DATABASE};
     SELECT g FROM t GROUP BY g WITH ROLLUP;"
)
expected_projection_rollup=$(cat <<EXPECTED
NULL
1
2
NULL
EXPECTED
)
expect_value "single-key projection-only rollup" "$expected_projection_rollup" "$projection_rollup"

grouping_rollup=$(run_mysql \
    "USE ${DATABASE};
     SELECT g, GROUPING(g), COUNT(*) FROM t GROUP BY g WITH ROLLUP;"
)
expected_grouping_rollup=$(cat <<EXPECTED
NULL	0	1
1	0	2
2	0	2
NULL	1	5
EXPECTED
)
expect_value "GROUPING marks rollup row" "$expected_grouping_rollup" "$grouping_rollup"

printf '%s\n' "mysql_baseline_group_by_rollup_expectations: ok"
