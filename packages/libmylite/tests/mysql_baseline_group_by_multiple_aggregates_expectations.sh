#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_group_by_multiple_aggregates_expectations_$$"

fail() {
    printf '%s\n' "mysql_baseline_group_by_multiple_aggregates_expectations: $1" >&2
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
     CREATE TABLE t(
       id INT NOT NULL,
       g INT NULL,
       n INT NULL,
       nn INT NOT NULL,
       iu INT UNSIGNED NULL,
       b BIGINT NULL,
       bu BIGINT UNSIGNED NULL
     ) ENGINE=InnoDB;
     INSERT INTO t VALUES
       (1, NULL, NULL, 5, 0, -1, 0),
       (2, NULL, 5, 6, 2, 2, 2),
       (3, 1, 10, 7, 4294967295, -9223372036854775808, 9223372036854775807),
       (4, 1, NULL, 8, NULL, 4, NULL),
       (5, 1, 20, 9, 7, NULL, 7),
       (6, 2, NULL, 10, NULL, NULL, NULL),
       (7, 2, NULL, 11, NULL, NULL, NULL);
     CREATE TABLE avg_order(g INT NOT NULL, n BIGINT NOT NULL) ENGINE=InnoDB;
     INSERT INTO avg_order VALUES
       (-1, -9007199254740993),
       (1, 9007199254740992),
       (2, 9007199254740993);
     CREATE TABLE bitwise_order(
       g INT NULL,
       bor INT NOT NULL,
       band INT NOT NULL,
       bxor INT NOT NULL
     ) ENGINE=InnoDB;
     INSERT INTO bitwise_order VALUES
       (NULL, 7, 7, 7),
       (1, 7, 15, 7),
       (1, 8, 13, 8),
       (2, 10, 15, 10),
       (2, 1, 11, 1);" >/dev/null

core=$(run_mysql \
    "USE ${DATABASE};
     DO 0;
     SELECT g, COUNT(*) AS c, COUNT(n) AS cn, COUNT(DISTINCT n) AS cd,
            MIN(n) AS mn, MAX(n) AS mx, SUM(n) AS s, AVG(n) AS a,
            BIT_AND(n) AS ba, BIT_OR(nn) AS bo, BIT_XOR(nn) AS bx
       FROM t GROUP BY g ORDER BY g;
     SELECT @@warning_count, ROW_COUNT();
     SELECT g, SUM(iu) AS su, MIN(b) AS mb, MAX(bu) AS xb
       FROM t GROUP BY g ORDER BY g;
     SELECT g, COUNT(*) AS c, SUM(n) AS s FROM t WHERE id >= 3 GROUP BY g ORDER BY g;"
)
expect_value "null group multi row" \
    "NULL	2	1	1	5	5	5	5.0000	5	7	3" "$(printf '%s\n' "$core" | sed -n '1p')"
expect_value "group one multi row" \
    "1	3	2	2	10	20	30	15.0000	0	15	6" "$(printf '%s\n' "$core" | sed -n '2p')"
expect_value "group two multi row" \
    "2	2	0	0	NULL	NULL	NULL	NULL	18446744073709551615	11	1" "$(printf '%s\n' "$core" | sed -n '3p')"
expect_value "multi status" "0	-1" "$(printf '%s\n' "$core" | sed -n '4p')"
expect_value "integer family null group" "NULL	2	-1	2" "$(printf '%s\n' "$core" | sed -n '5p')"
expect_value "integer family group one" \
    "1	4294967302	-9223372036854775808	9223372036854775807" \
    "$(printf '%s\n' "$core" | sed -n '6p')"
expect_value "integer family group two" "2	NULL	NULL	NULL" \
    "$(printf '%s\n' "$core" | sed -n '7p')"
expect_value "where group one row" "1	3	30" "$(printf '%s\n' "$core" | sed -n '8p')"
expect_value "where group two row" "2	2	NULL" "$(printf '%s\n' "$core" | sed -n '9p')"

having_order=$(run_mysql \
    "USE ${DATABASE};
     SELECT g, COUNT(*) AS c, SUM(n) AS s FROM t GROUP BY g HAVING s IS NOT NULL ORDER BY c DESC;
     SELECT g, COUNT(*) AS c, SUM(n) AS s FROM t GROUP BY g HAVING SUM(n) >= 30 ORDER BY s DESC;
     SELECT g, COUNT(*) AS c, SUM(n) AS s FROM t GROUP BY g ORDER BY s ASC;
     SELECT g, COUNT(*) AS c, SUM(n) AS s FROM t GROUP BY g ORDER BY s DESC LIMIT 2;
     SELECT g, COUNT(n) AS cn FROM t GROUP BY g ORDER BY cn DESC LIMIT 2;"
)
expect_value "having alias keeps group one" "1	3	30" "$(printf '%s\n' "$having_order" | sed -n '1p')"
expect_value "having alias keeps null group" "NULL	2	5" "$(printf '%s\n' "$having_order" | sed -n '2p')"
expect_value "having expression keeps group one" "1	3	30" "$(printf '%s\n' "$having_order" | sed -n '3p')"
expect_value "order aggregate null first" "2	2	NULL" "$(printf '%s\n' "$having_order" | sed -n '4p')"
expect_value "order aggregate low next" "NULL	2	5" "$(printf '%s\n' "$having_order" | sed -n '5p')"
expect_value "order aggregate high last" "1	3	30" "$(printf '%s\n' "$having_order" | sed -n '6p')"
expect_value "order aggregate desc first" "1	3	30" "$(printf '%s\n' "$having_order" | sed -n '7p')"
expect_value "order aggregate desc second" "NULL	2	5" "$(printf '%s\n' "$having_order" | sed -n '8p')"
expect_value "count column alias order first" "1	2" "$(printf '%s\n' "$having_order" | sed -n '9p')"
expect_value "count column alias order second" "NULL	1" "$(printf '%s\n' "$having_order" | sed -n '10p')"

expression_order=$(run_mysql \
    "USE ${DATABASE};
     SELECT g, COUNT(*) AS c, SUM(n) AS s FROM t GROUP BY g ORDER BY SUM(n) ASC;
     SELECT g, COUNT(*) AS c, SUM(n) AS s FROM t GROUP BY g ORDER BY COUNT(*) DESC, g DESC;
     SELECT g, COUNT(n) AS cn FROM t GROUP BY g ORDER BY COUNT(n) DESC LIMIT 2;
     SELECT g, COUNT(DISTINCT n) AS cd FROM t GROUP BY g ORDER BY COUNT(DISTINCT n) DESC LIMIT 2;
     SELECT g, MIN(n) AS mn, MAX(n) AS mx FROM t GROUP BY g ORDER BY MIN(n) ASC;
     SELECT g, MIN(n) AS mn, MAX(n) AS mx FROM t GROUP BY g ORDER BY MAX(n) DESC;
     SELECT g, AVG(n) AS a FROM t GROUP BY g ORDER BY AVG(n) DESC LIMIT 2;"
)
expect_value "sum expression order null first" "2	2	NULL" \
    "$(printf '%s\n' "$expression_order" | sed -n '1p')"
expect_value "sum expression order low next" "NULL	2	5" \
    "$(printf '%s\n' "$expression_order" | sed -n '2p')"
expect_value "sum expression order high last" "1	3	30" \
    "$(printf '%s\n' "$expression_order" | sed -n '3p')"
expect_value "count star expression order first" "1	3	30" \
    "$(printf '%s\n' "$expression_order" | sed -n '4p')"
expect_value "count star expression order tiebreak second" "2	2	NULL" \
    "$(printf '%s\n' "$expression_order" | sed -n '5p')"
expect_value "count star expression order tiebreak third" "NULL	2	5" \
    "$(printf '%s\n' "$expression_order" | sed -n '6p')"
expect_value "count column expression order first" "1	2" \
    "$(printf '%s\n' "$expression_order" | sed -n '7p')"
expect_value "count column expression order second" "NULL	1" \
    "$(printf '%s\n' "$expression_order" | sed -n '8p')"
expect_value "count distinct expression order first" "1	2" \
    "$(printf '%s\n' "$expression_order" | sed -n '9p')"
expect_value "count distinct expression order second" "NULL	1" \
    "$(printf '%s\n' "$expression_order" | sed -n '10p')"
expect_value "min expression order null first" "2	NULL	NULL" \
    "$(printf '%s\n' "$expression_order" | sed -n '11p')"
expect_value "min expression order low next" "NULL	5	5" \
    "$(printf '%s\n' "$expression_order" | sed -n '12p')"
expect_value "min expression order high last" "1	10	20" \
    "$(printf '%s\n' "$expression_order" | sed -n '13p')"
expect_value "max expression order high first" "1	10	20" \
    "$(printf '%s\n' "$expression_order" | sed -n '14p')"
expect_value "max expression order low next" "NULL	5	5" \
    "$(printf '%s\n' "$expression_order" | sed -n '15p')"
expect_value "max expression order null last" "2	NULL	NULL" \
    "$(printf '%s\n' "$expression_order" | sed -n '16p')"
expect_value "avg expression order high first" "1	15.0000" \
    "$(printf '%s\n' "$expression_order" | sed -n '17p')"
expect_value "avg expression order low second" "NULL	5.0000" \
    "$(printf '%s\n' "$expression_order" | sed -n '18p')"

aggregate_order=$(run_mysql \
    "USE ${DATABASE};
     SELECT g, MIN(n) AS mn, MAX(n) AS mx FROM t GROUP BY g ORDER BY mn ASC;
     SELECT g, MIN(n) AS mn, MAX(n) AS mx FROM t GROUP BY g ORDER BY mx DESC;
     SELECT g, AVG(n) AS a FROM avg_order GROUP BY g ORDER BY a;
     SELECT g, AVG(n) AS a FROM avg_order GROUP BY g ORDER BY a DESC LIMIT 1;
     SELECT g, BIT_AND(band) AS ba FROM bitwise_order GROUP BY g ORDER BY ba ASC;
     SELECT g, BIT_OR(bor) AS bo FROM bitwise_order GROUP BY g ORDER BY bo ASC;
     SELECT g, BIT_XOR(bxor) AS bx FROM bitwise_order GROUP BY g ORDER BY bx ASC;"
)
expect_value "min alias order null first" "2	NULL	NULL" \
    "$(printf '%s\n' "$aggregate_order" | sed -n '1p')"
expect_value "min alias order middle" "NULL	5	5" \
    "$(printf '%s\n' "$aggregate_order" | sed -n '2p')"
expect_value "min alias order high" "1	10	20" \
    "$(printf '%s\n' "$aggregate_order" | sed -n '3p')"
expect_value "max alias order high" "1	10	20" \
    "$(printf '%s\n' "$aggregate_order" | sed -n '4p')"
expect_value "max alias order middle" "NULL	5	5" \
    "$(printf '%s\n' "$aggregate_order" | sed -n '5p')"
expect_value "max alias order null last" "2	NULL	NULL" \
    "$(printf '%s\n' "$aggregate_order" | sed -n '6p')"
expect_value "mysql exact avg alias order negative" "-1	-9007199254740993.0000" \
    "$(printf '%s\n' "$aggregate_order" | sed -n '7p')"
expect_value "mysql exact avg alias order lower" "1	9007199254740992.0000" \
    "$(printf '%s\n' "$aggregate_order" | sed -n '8p')"
expect_value "mysql exact avg alias order higher" "2	9007199254740993.0000" \
    "$(printf '%s\n' "$aggregate_order" | sed -n '9p')"
expect_value "mysql exact avg alias order desc limit" "2	9007199254740993.0000" \
    "$(printf '%s\n' "$aggregate_order" | sed -n '10p')"
expect_value "mysql bit and alias order low" "NULL	7" \
    "$(printf '%s\n' "$aggregate_order" | sed -n '11p')"
expect_value "mysql bit and alias order middle" "2	11" \
    "$(printf '%s\n' "$aggregate_order" | sed -n '12p')"
expect_value "mysql bit and alias order high" "1	13" \
    "$(printf '%s\n' "$aggregate_order" | sed -n '13p')"
expect_value "mysql bit or alias order low" "NULL	7" \
    "$(printf '%s\n' "$aggregate_order" | sed -n '14p')"
expect_value "mysql bit or alias order middle" "2	11" \
    "$(printf '%s\n' "$aggregate_order" | sed -n '15p')"
expect_value "mysql bit or alias order high" "1	15" \
    "$(printf '%s\n' "$aggregate_order" | sed -n '16p')"
expect_value "mysql bit xor alias order low" "NULL	7" \
    "$(printf '%s\n' "$aggregate_order" | sed -n '17p')"
expect_value "mysql bit xor alias order middle" "2	11" \
    "$(printf '%s\n' "$aggregate_order" | sed -n '18p')"
expect_value "mysql bit xor alias order high" "1	15" \
    "$(printf '%s\n' "$aggregate_order" | sed -n '19p')"

headers=$(run_mysql_with_headers \
    "USE ${DATABASE};
     SELECT g AS k, COUNT(*) AS c, SUM(n) AS s FROM t GROUP BY g ORDER BY c DESC LIMIT 1;"
)
expect_value "header labels" "k	c	s" "$(printf '%s\n' "$headers" | sed -n '1p')"
expect_value "header row" "1	3	30" "$(printf '%s\n' "$headers" | sed -n '2p')"

expect_error \
    "only full group by extra column" \
    1055 \
    "42000" \
    "Expression #2 of SELECT list is not in GROUP BY clause" \
    "SET SESSION sql_mode = 'ONLY_FULL_GROUP_BY,STRICT_TRANS_TABLES';
     USE ${DATABASE}; SELECT g, n, COUNT(*) FROM t GROUP BY g;"
expect_error \
    "unknown aggregate argument" \
    1054 \
    "42S22" \
    "Unknown column 'missing' in 'field list'" \
    "USE ${DATABASE}; SELECT g, COUNT(*), SUM(missing) FROM t GROUP BY g;"
expect_error \
    "unknown aggregate alias order" \
    1054 \
    "42S22" \
    "Unknown column 'missing' in 'order clause'" \
    "USE ${DATABASE}; SELECT g, COUNT(*) AS c, SUM(n) AS s FROM t GROUP BY g ORDER BY missing;"

cleanup

printf '%s\n' "mysql_baseline_group_by_multiple_aggregates_expectations: ok"
