#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_group_by_single_column_aggregate_expectations_$$"

fail() {
    printf '%s\n' "mysql_baseline_group_by_single_column_aggregate_expectations: $1" >&2
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
       i INT NULL,
       ii INTEGER NULL,
       iu INT UNSIGNED NULL,
       b BIGINT NULL,
       bu BIGINT UNSIGNED NULL
     ) ENGINE=InnoDB;
     INSERT INTO t VALUES
       (1, NULL, NULL, NULL, 0, -1, 0),
       (2, NULL, 5, 5, 2, 2, 2),
       (3, 1, 10, 10, 4294967295, -9223372036854775808, 9223372036854775807),
       (4, 1, NULL, NULL, NULL, 4, NULL),
       (5, 1, 20, 20, 7, NULL, 7),
       (6, 2, NULL, NULL, NULL, NULL, NULL),
       (7, 2, NULL, NULL, NULL, NULL, NULL);
     CREATE TABLE readable_posts(
       id INT,
       post_status VARCHAR(20),
       post_type VARCHAR(32),
       post_author INT
     ) ENGINE=InnoDB;
     INSERT INTO readable_posts VALUES
       (1,'publish','article',10),
       (2,'publish','article',10),
       (3,'publish','article',10),
       (4,'publish','article',10),
       (5,'publish','article',10),
       (6,'private','article',10),
       (7,'private','article',10),
       (8,'private','article',11),
       (10,'publish','page',10);
     CREATE TABLE string_grouped(
       id INT NOT NULL,
       name VARCHAR(20) NULL,
       label CHAR(5) NULL,
       body TEXT NULL,
       n INT NULL
     ) ENGINE=InnoDB;
     INSERT INTO string_grouped VALUES
       (1, NULL, NULL, NULL, 10),
       (2, 'alice', 'A', 'essay', 20),
       (3, 'Alice', 'A   ', 'Essay', 30),
       (4, 'bob', 'B', 'note', NULL),
       (5, 'BOB', 'B    ', 'Note', 5),
       (6, 'carol', 'C', NULL, 7);" >/dev/null

core=$(run_mysql \
    "USE ${DATABASE};
     DO 0;
     SELECT g, COUNT(*), COUNT(i), MIN(i), MAX(i), SUM(i), AVG(i),
            BIT_AND(i), BIT_OR(i), BIT_XOR(i)
       FROM t GROUP BY g ORDER BY g;
     SELECT @@warning_count, ROW_COUNT();
     SELECT g, SUM(ii), SUM(iu), MIN(b), MAX(bu)
       FROM t GROUP BY g ORDER BY g;
     SELECT g, COUNT(*), SUM(i) FROM t WHERE id >= 3 GROUP BY g ORDER BY g;"
)
expect_value \
    "null group aggregate row" \
    "NULL	2	1	5	5	5	5.0000	5	5	5" \
    "$(printf '%s\n' "$core" | sed -n '1p')"
expect_value \
    "group one aggregate row" \
    "1	3	2	10	20	30	15.0000	0	30	30" \
    "$(printf '%s\n' "$core" | sed -n '2p')"
expect_value \
    "all null aggregate row" \
    "2	2	0	NULL	NULL	NULL	NULL	18446744073709551615	0	0" \
    "$(printf '%s\n' "$core" | sed -n '3p')"
expect_value "grouped select status" "0	-1" "$(printf '%s\n' "$core" | sed -n '4p')"
expect_value \
    "integer family null group" \
    "NULL	5	2	-1	2" \
    "$(printf '%s\n' "$core" | sed -n '5p')"
expect_value \
    "integer family group one" \
    "1	30	4294967302	-9223372036854775808	9223372036854775807" \
    "$(printf '%s\n' "$core" | sed -n '6p')"
expect_value \
    "integer family group two" \
    "2	NULL	NULL	NULL	NULL" \
    "$(printf '%s\n' "$core" | sed -n '7p')"
expect_value "where group one row" "1	3	30" "$(printf '%s\n' "$core" | sed -n '8p')"
expect_value "where group two row" "2	2	NULL" "$(printf '%s\n' "$core" | sed -n '9p')"

distinct_aggregates=$(run_mysql \
    "USE ${DATABASE};
     SELECT g, MIN(DISTINCT i), MAX(DISTINCT i), SUM(DISTINCT i), AVG(DISTINCT i)
       FROM t GROUP BY g ORDER BY g;"
)
expect_value "distinct aggregate null group" "NULL	5	5	5	5.0000" \
    "$(printf '%s\n' "$distinct_aggregates" | sed -n '1p')"
expect_value "distinct aggregate group one" "1	10	20	30	15.0000" \
    "$(printf '%s\n' "$distinct_aggregates" | sed -n '2p')"
expect_value "distinct aggregate group two" "2	NULL	NULL	NULL	NULL" \
    "$(printf '%s\n' "$distinct_aggregates" | sed -n '3p')"

count_distinct_group=$(run_mysql \
    "USE ${DATABASE};
     SELECT g, COUNT(DISTINCT i) FROM t GROUP BY g ORDER BY g;
     SELECT g, COUNT(DISTINCT i) AS c FROM t GROUP BY g HAVING c > 1 ORDER BY g;
     SELECT g, COUNT(DISTINCT i)
       FROM t GROUP BY g HAVING COUNT(DISTINCT i) > 1 ORDER BY g;"
)
expect_value "count distinct null group" "NULL	1" \
    "$(printf '%s\n' "$count_distinct_group" | sed -n '1p')"
expect_value "count distinct group one" "1	2" \
    "$(printf '%s\n' "$count_distinct_group" | sed -n '2p')"
expect_value "count distinct group two" "2	0" \
    "$(printf '%s\n' "$count_distinct_group" | sed -n '3p')"
expect_value "count distinct having alias" "1	2" \
    "$(printf '%s\n' "$count_distinct_group" | sed -n '4p')"
expect_value "count distinct having expression" "1	2" \
    "$(printf '%s\n' "$count_distinct_group" | sed -n '5p')"

count_distinct_string_group=$(run_mysql \
    "USE ${DATABASE};
     SELECT name, COUNT(DISTINCT label), COUNT(DISTINCT body)
       FROM string_grouped GROUP BY name ORDER BY name;
     SELECT name, COUNT(DISTINCT body) AS c
       FROM string_grouped GROUP BY name HAVING c = 0 ORDER BY name;
     SELECT name
       FROM string_grouped GROUP BY name ORDER BY COUNT(DISTINCT body) DESC, name;"
)
expect_value "string count distinct null group" "NULL	0	0" \
    "$(printf '%s\n' "$count_distinct_string_group" | sed -n '1p')"
expect_value "string count distinct alice group" "alice	1	1" \
    "$(printf '%s\n' "$count_distinct_string_group" | sed -n '2p')"
expect_value "string count distinct bob group" "bob	1	1" \
    "$(printf '%s\n' "$count_distinct_string_group" | sed -n '3p')"
expect_value "string count distinct carol group" "carol	1	0" \
    "$(printf '%s\n' "$count_distinct_string_group" | sed -n '4p')"
expect_value "string count distinct having null group" "NULL	0" \
    "$(printf '%s\n' "$count_distinct_string_group" | sed -n '5p')"
expect_value "string count distinct having carol group" "carol	0" \
    "$(printf '%s\n' "$count_distinct_string_group" | sed -n '6p')"
expect_value "string hidden count distinct order alice" "alice" \
    "$(printf '%s\n' "$count_distinct_string_group" | sed -n '7p')"
expect_value "string hidden count distinct order bob" "bob" \
    "$(printf '%s\n' "$count_distinct_string_group" | sed -n '8p')"
expect_value "string hidden count distinct order null" "NULL" \
    "$(printf '%s\n' "$count_distinct_string_group" | sed -n '9p')"
expect_value "string hidden count distinct order carol" "carol" \
    "$(printf '%s\n' "$count_distinct_string_group" | sed -n '10p')"

distinct_group=$(run_mysql \
    "USE ${DATABASE};
     SET SESSION sql_mode = 'ONLY_FULL_GROUP_BY,STRICT_TRANS_TABLES';
     SELECT DISTINCT g, COUNT(*) FROM t GROUP BY g ORDER BY g;"
)
expect_value "strict distinct null group" "NULL	2" \
    "$(printf '%s\n' "$distinct_group" | sed -n '1p')"
expect_value "strict distinct group one" "1	3" \
    "$(printf '%s\n' "$distinct_group" | sed -n '2p')"
expect_value "strict distinct group two" "2	2" \
    "$(printf '%s\n' "$distinct_group" | sed -n '3p')"

derived_union_group=$(run_mysql \
    "USE ${DATABASE};
     SELECT post_status, COUNT(*) AS num_posts
       FROM (
         SELECT post_status
           FROM readable_posts
          WHERE post_type = 'article' AND post_status != 'private'
         UNION ALL
         SELECT post_status
           FROM readable_posts
          WHERE post_type = 'article' AND post_status = 'private' AND post_author = 11
       ) AS filtered_posts
      GROUP BY post_status
      ORDER BY post_status;"
)
expect_value "derived union group private" "private	1" \
    "$(printf '%s\n' "$derived_union_group" | sed -n '1p')"
expect_value "derived union group publish" "publish	5" \
    "$(printf '%s\n' "$derived_union_group" | sed -n '2p')"

no_match=$(run_mysql \
    "USE ${DATABASE};
     SELECT g, COUNT(i) FROM t WHERE id > 99 GROUP BY g ORDER BY g;"
)
expect_value "no matched grouped rows" "" "$no_match"

headers_output=$(run_mysql_with_headers \
    "USE ${DATABASE};
     SELECT t.g, COUNT(*) AS c FROM ${DATABASE}.t GROUP BY t.g ORDER BY t.g;
     SELECT a.g AS k, SUM(a.i) AS s FROM ${DATABASE}.t AS a
       WHERE a.id >= 3 GROUP BY a.g ORDER BY k DESC LIMIT 2;"
)
expect_value "qualified headers" "g	c" "$(printf '%s\n' "$headers_output" | sed -n '1p')"
expect_value "qualified null row" "NULL	2" "$(printf '%s\n' "$headers_output" | sed -n '2p')"
expect_value "qualified group one" "1	3" "$(printf '%s\n' "$headers_output" | sed -n '3p')"
expect_value "qualified group two" "2	2" "$(printf '%s\n' "$headers_output" | sed -n '4p')"
expect_value "alias headers" "k	s" "$(printf '%s\n' "$headers_output" | sed -n '5p')"
expect_value "alias desc first" "2	NULL" "$(printf '%s\n' "$headers_output" | sed -n '6p')"
expect_value "alias desc second" "1	30" "$(printf '%s\n' "$headers_output" | sed -n '7p')"

limits=$(run_mysql \
    "USE ${DATABASE};
     SELECT g, COUNT(*) FROM t GROUP BY g ORDER BY g DESC;
     SELECT g, COUNT(*) FROM t GROUP BY g ORDER BY g LIMIT 0;
     SELECT g, COUNT(*) FROM t GROUP BY g ORDER BY g LIMIT 1;
     SELECT g, COUNT(*) FROM t GROUP BY g ORDER BY g LIMIT 1 OFFSET 1;
     SELECT g, COUNT(*) FROM t GROUP BY g ORDER BY g LIMIT 1, 1;"
)
expect_value "desc group two" "2	2" "$(printf '%s\n' "$limits" | sed -n '1p')"
expect_value "desc group one" "1	3" "$(printf '%s\n' "$limits" | sed -n '2p')"
expect_value "desc null group" "NULL	2" "$(printf '%s\n' "$limits" | sed -n '3p')"
expect_value "limit one emits null group" "NULL	2" "$(printf '%s\n' "$limits" | sed -n '4p')"
expect_value "limit one offset" "1	3" "$(printf '%s\n' "$limits" | sed -n '5p')"
expect_value "limit comma offset" "1	3" "$(printf '%s\n' "$limits" | sed -n '6p')"

limit_zero=$(run_mysql \
    "USE ${DATABASE};
     SELECT g, COUNT(*) FROM t GROUP BY g ORDER BY g LIMIT 0;"
)
expect_value "limit zero emits no rows" "" "$limit_zero"

accepted_but_deferred=$(run_mysql \
    "USE ${DATABASE};
     SELECT g AS k, COUNT(*) FROM t GROUP BY k ORDER BY k;
     SELECT g, COUNT(*) FROM t GROUP BY 1 ORDER BY 1;
     SELECT g, COUNT(*) FROM t GROUP BY g HAVING COUNT(*) > 1;
     SELECT g, COUNT(*) FROM t GROUP BY g WITH ROLLUP;
     SELECT g, COUNT(*) AS c FROM t GROUP BY g ORDER BY c DESC, g;"
)
expect_value "group alias accepted" "NULL	2" \
    "$(printf '%s\n' "$accepted_but_deferred" | sed -n '1p')"
expect_value "group ordinal accepted" "NULL	2" \
    "$(printf '%s\n' "$accepted_but_deferred" | sed -n '4p')"
expect_value "having accepted" "NULL	2" \
    "$(printf '%s\n' "$accepted_but_deferred" | sed -n '7p')"
expect_value "rollup total row" "NULL	7" \
    "$(printf '%s\n' "$accepted_but_deferred" | sed -n '13p')"
expect_value "order aggregate alias accepted" "1	3" \
    "$(printf '%s\n' "$accepted_but_deferred" | sed -n '14p')"

expect_error \
    "unknown field column" \
    1054 \
    "42S22" \
    "Unknown column 'missing' in 'field list'" \
    "USE ${DATABASE}; SELECT missing, COUNT(*) FROM t GROUP BY missing;"
expect_error \
    "unknown group column" \
    1054 \
    "42S22" \
    "Unknown column 'missing' in 'group statement'" \
    "USE ${DATABASE}; SELECT g, COUNT(*) FROM t GROUP BY missing;"
expect_error \
    "unknown order column" \
    1054 \
    "42S22" \
    "Unknown column 'missing' in 'order clause'" \
    "USE ${DATABASE}; SELECT g, COUNT(*) FROM t GROUP BY g ORDER BY missing;"
expect_error \
    "only full group by selected column" \
    1055 \
    "42000" \
    "Expression #1 of SELECT list is not in GROUP BY clause" \
    "SET SESSION sql_mode = 'ONLY_FULL_GROUP_BY,STRICT_TRANS_TABLES';
     USE ${DATABASE}; SELECT i, COUNT(*) FROM t GROUP BY g;"

cleanup

printf '%s\n' "mysql_baseline_group_by_single_column_aggregate_expectations: ok"
