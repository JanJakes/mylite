#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_group_by_multiple_keys_expectations_$$"

fail() {
    printf '%s\n' "mysql_baseline_group_by_multiple_keys_expectations: $1" >&2
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
       a INT NULL,
       b INT NULL,
       name VARCHAR(20) NULL,
       label CHAR(5) NULL,
       n INT NULL
     ) ENGINE=InnoDB;
     INSERT INTO t VALUES
       (1, NULL, NULL, NULL, NULL, 1),
       (2, 1, 1, 'alice', 'A', 10),
       (3, 1, 1, 'Alice', 'A   ', 20),
       (4, 1, 2, 'alice', 'B', 30),
       (5, 2, 1, 'bob', 'B', 40),
       (6, 2, 1, 'BOB', 'B   ', NULL),
       (7, 2, 2, 'bob', 'C', 50),
       (8, 2, 2, 'carol', 'C', 60);" >/dev/null

run_mysql \
    "USE ${DATABASE};
     CREATE TABLE grouped_posts(
       ID INT,
       post_date DATETIME,
       post_type VARCHAR(20),
       post_status VARCHAR(20)
     ) ENGINE=InnoDB;
     INSERT INTO grouped_posts VALUES
       (1, '2024-01-10 12:00:00', 'post', 'publish'),
       (2, '2024-01-20 09:00:00', 'post', 'publish'),
       (3, '2024-02-05 12:00:00', 'post', 'publish'),
       (4, '2023-12-31 23:00:00', 'post', 'publish'),
       (5, '2024-02-07 12:00:00', 'page', 'publish');" >/dev/null

core=$(run_mysql \
    "USE ${DATABASE};
     DO 0;
     SELECT a, b, COUNT(*) AS c, COUNT(n) AS cn, SUM(n) AS s, MIN(n) AS mn, MAX(n) AS mx,
            AVG(n) AS av
       FROM t GROUP BY a, b ORDER BY a, b;
     SELECT @@warning_count, ROW_COUNT();
     SELECT name, label, COUNT(*) AS c, SUM(n) AS s
       FROM t GROUP BY name, label ORDER BY name, label;"
)
expect_value "integer null tuple" "NULL	NULL	1	1	1	1	1	1.0000" \
    "$(printf '%s\n' "$core" | sed -n '1p')"
expect_value "integer one one tuple" "1	1	2	2	30	10	20	15.0000" \
    "$(printf '%s\n' "$core" | sed -n '2p')"
expect_value "integer one two tuple" "1	2	1	1	30	30	30	30.0000" \
    "$(printf '%s\n' "$core" | sed -n '3p')"
expect_value "integer two one tuple" "2	1	2	1	40	40	40	40.0000" \
    "$(printf '%s\n' "$core" | sed -n '4p')"
expect_value "integer two two tuple" "2	2	2	2	110	50	60	55.0000" \
    "$(printf '%s\n' "$core" | sed -n '5p')"
expect_value "multi key status" "0	-1" "$(printf '%s\n' "$core" | sed -n '6p')"
expect_value "string null tuple" "NULL	NULL	1	1" "$(printf '%s\n' "$core" | sed -n '7p')"
expect_value "string alice a tuple" "alice	A	2	30" "$(printf '%s\n' "$core" | sed -n '8p')"
expect_value "string alice b tuple" "alice	B	1	30" "$(printf '%s\n' "$core" | sed -n '9p')"
expect_value "string bob b tuple" "bob	B	2	40" "$(printf '%s\n' "$core" | sed -n '10p')"
expect_value "string bob c tuple" "bob	C	1	50" "$(printf '%s\n' "$core" | sed -n '11p')"
expect_value "string carol c tuple" "carol	C	1	60" "$(printf '%s\n' "$core" | sed -n '12p')"

temporal=$(run_mysql \
    "USE ${DATABASE};
     SET SESSION sql_mode = '';
     SELECT YEAR(post_date) AS \`year\`, MONTH(post_date) AS \`month\`, COUNT(ID) AS posts
       FROM grouped_posts
      WHERE post_type = 'post' AND post_status = 'publish'
      GROUP BY YEAR(post_date), MONTH(post_date)
      ORDER BY post_date DESC;"
)
expect_value "temporal group first" "2024	2	1" "$(printf '%s\n' "$temporal" | sed -n '1p')"
expect_value "temporal group second" "2024	1	2" "$(printf '%s\n' "$temporal" | sed -n '2p')"
expect_value "temporal group third" "2023	12	1" "$(printf '%s\n' "$temporal" | sed -n '3p')"

temporal_day=$(run_mysql \
    "USE ${DATABASE};
     SET SESSION sql_mode = '';
     SELECT YEAR(post_date) AS \`year\`, MONTH(post_date) AS \`month\`,
            DAYOFMONTH(post_date) AS \`dayofmonth\`, COUNT(ID) AS posts
       FROM grouped_posts
      WHERE post_type = 'post' AND post_status = 'publish'
      GROUP BY YEAR(post_date), MONTH(post_date), DAYOFMONTH(post_date)
      ORDER BY post_date DESC;
     SELECT @@warning_count, ROW_COUNT();"
)
expect_value "temporal day group first" "2024	2	5	1" \
    "$(printf '%s\n' "$temporal_day" | sed -n '1p')"
expect_value "temporal day group second" "2024	1	20	1" \
    "$(printf '%s\n' "$temporal_day" | sed -n '2p')"
expect_value "temporal day group third" "2024	1	10	1" \
    "$(printf '%s\n' "$temporal_day" | sed -n '3p')"
expect_value "temporal day group fourth" "2023	12	31	1" \
    "$(printf '%s\n' "$temporal_day" | sed -n '4p')"
expect_value "temporal day group status" "0	-1" \
    "$(printf '%s\n' "$temporal_day" | sed -n '5p')"

temporal_week=$(run_mysql \
    "USE ${DATABASE};
     SET SESSION sql_mode = '';
     SELECT DISTINCT WEEK(post_date, 1) AS \`week\`, YEAR(post_date) AS \`yr\`,
            DATE_FORMAT(post_date, '%Y-%m-%d') AS \`yyyymmdd\`, COUNT(ID) AS posts
       FROM grouped_posts
      WHERE post_type = 'post' AND post_status = 'publish'
      GROUP BY WEEK(post_date, 1), YEAR(post_date)
      ORDER BY post_date DESC;
     SELECT @@warning_count, ROW_COUNT();"
)
expect_value "temporal week group first" "6	2024	2024-02-05	1" \
    "$(printf '%s\n' "$temporal_week" | sed -n '1p')"
expect_value "temporal week group second" "3	2024	2024-01-20	1" \
    "$(printf '%s\n' "$temporal_week" | sed -n '2p')"
expect_value "temporal week group third" "2	2024	2024-01-10	1" \
    "$(printf '%s\n' "$temporal_week" | sed -n '3p')"
expect_value "temporal week group fourth" "52	2023	2023-12-31	1" \
    "$(printf '%s\n' "$temporal_week" | sed -n '4p')"
expect_value "temporal week group status" "0	-1" \
    "$(printf '%s\n' "$temporal_week" | sed -n '5p')"

filters=$(run_mysql \
    "USE ${DATABASE};
     SELECT a, b, COUNT(*) AS c, SUM(n) AS s
       FROM t WHERE id >= 2 GROUP BY a, b HAVING c > 1 ORDER BY a, b;
     SELECT a, b, COUNT(*) AS c, SUM(n) AS s
       FROM t GROUP BY a, b HAVING b = 1 ORDER BY a, b;
     SELECT name, label, COUNT(*) AS c
       FROM t GROUP BY name, label HAVING label IS NOT NULL ORDER BY name DESC LIMIT 1;"
)
expect_value "where having first" "1	1	2	30" "$(printf '%s\n' "$filters" | sed -n '1p')"
expect_value "where having second" "2	1	2	40" "$(printf '%s\n' "$filters" | sed -n '2p')"
expect_value "where having third" "2	2	2	110" "$(printf '%s\n' "$filters" | sed -n '3p')"
expect_value "having group key first" "1	1	2	30" "$(printf '%s\n' "$filters" | sed -n '4p')"
expect_value "having group key second" "2	1	2	40" "$(printf '%s\n' "$filters" | sed -n '5p')"
expect_value "string having/order first" "carol	C	1" "$(printf '%s\n' "$filters" | sed -n '6p')"

ordering=$(run_mysql \
    "USE ${DATABASE};
     SELECT a AS x, b AS y, COUNT(*) AS c, SUM(n) AS s
       FROM t GROUP BY a, b ORDER BY s DESC LIMIT 2;
     SELECT a, b, AVG(n) AS av
       FROM t GROUP BY a, b ORDER BY av DESC LIMIT 2;
     SELECT q.a AS x, q.b AS y, COUNT(*) AS c
       FROM ${DATABASE}.t AS q WHERE q.b = 1 GROUP BY q.a, q.b ORDER BY x DESC;"
)
expect_value "aggregate order first" "2	2	2	110" "$(printf '%s\n' "$ordering" | sed -n '1p')"
expect_value "aggregate order second" "2	1	2	40" "$(printf '%s\n' "$ordering" | sed -n '2p')"
expect_value "avg aggregate order first" "2	2	55.0000" \
    "$(printf '%s\n' "$ordering" | sed -n '3p')"
expect_value "avg aggregate order second" "2	1	40.0000" \
    "$(printf '%s\n' "$ordering" | sed -n '4p')"
expect_value "qualified alias order first" "2	1	2" "$(printf '%s\n' "$ordering" | sed -n '5p')"
expect_value "qualified alias order second" "1	1	2" "$(printf '%s\n' "$ordering" | sed -n '6p')"

multi_order=$(run_mysql \
    "USE ${DATABASE};
     SELECT a AS x, b AS y, COUNT(*) AS c
       FROM t GROUP BY a, b ORDER BY c DESC, x DESC, y DESC LIMIT 3;"
)
expect_value "multi order first" "2	2	2" "$(printf '%s\n' "$multi_order" | sed -n '1p')"
expect_value "multi order second" "2	1	2" "$(printf '%s\n' "$multi_order" | sed -n '2p')"
expect_value "multi order third" "1	1	2" "$(printf '%s\n' "$multi_order" | sed -n '3p')"

count_distinct=$(run_mysql \
    "USE ${DATABASE};
     SELECT a, b, COUNT(DISTINCT n) AS cd FROM t GROUP BY a, b ORDER BY a, b;
     SELECT a, b, COUNT(DISTINCT n) AS cd FROM t GROUP BY a, b HAVING cd > 1 ORDER BY a, b;
     SELECT a, b, COUNT(DISTINCT n) AS cd FROM t GROUP BY a, b ORDER BY cd DESC, a, b LIMIT 2;"
)
expect_value "count distinct null tuple" "NULL	NULL	1" \
    "$(printf '%s\n' "$count_distinct" | sed -n '1p')"
expect_value "count distinct one one tuple" "1	1	2" \
    "$(printf '%s\n' "$count_distinct" | sed -n '2p')"
expect_value "count distinct one two tuple" "1	2	1" \
    "$(printf '%s\n' "$count_distinct" | sed -n '3p')"
expect_value "count distinct two one tuple" "2	1	1" \
    "$(printf '%s\n' "$count_distinct" | sed -n '4p')"
expect_value "count distinct two two tuple" "2	2	2" \
    "$(printf '%s\n' "$count_distinct" | sed -n '5p')"
expect_value "count distinct having first" "1	1	2" \
    "$(printf '%s\n' "$count_distinct" | sed -n '6p')"
expect_value "count distinct having second" "2	2	2" \
    "$(printf '%s\n' "$count_distinct" | sed -n '7p')"
expect_value "count distinct alias order first" "1	1	2" \
    "$(printf '%s\n' "$count_distinct" | sed -n '8p')"
expect_value "count distinct alias order second" "2	2	2" \
    "$(printf '%s\n' "$count_distinct" | sed -n '9p')"

run_mysql \
    "USE ${DATABASE};
     CREATE TABLE comments(
       id INT NOT NULL,
       post_id INT NULL,
       score INT NULL
     ) ENGINE=InnoDB;
     CREATE TABLE commentmeta(
       comment_id INT NOT NULL,
       meta_key VARCHAR(64) NOT NULL,
       meta_value TEXT
     ) ENGINE=InnoDB;
     INSERT INTO comments VALUES
       (101, 1, 5), (102, 1, NULL), (103, 2, 7), (104, NULL, 9), (105, 99, 11);
     INSERT INTO commentmeta VALUES
       (101, 'featured', 'a'), (102, 'featured', 'c'),
       (103, 'featured', 'b'), (105, 'featured', 'b'),
       (101, 'secondary', 'y'), (102, 'secondary', 'x'),
       (103, 'secondary', 'z'), (105, 'secondary', 'w'),
       (104, 'other', 'z');" >/dev/null

joined_order=$(run_mysql \
    "USE ${DATABASE};
     SET SESSION sql_mode = '';
     SELECT c.id
       FROM comments c INNER JOIN commentmeta cm ON c.id = cm.comment_id
      WHERE cm.meta_key = 'featured'
      GROUP BY c.id
      ORDER BY c.post_id ASC, c.id ASC;
     SELECT c.id
       FROM comments c INNER JOIN commentmeta cm ON c.id = cm.comment_id
      WHERE cm.meta_key = 'featured'
      GROUP BY c.id
      ORDER BY CAST(cm.meta_value AS CHAR) DESC, c.id DESC;
     SELECT c.id
       FROM comments c
       INNER JOIN commentmeta cm ON c.id = cm.comment_id
       INNER JOIN commentmeta mt1 ON c.id = mt1.comment_id
      WHERE cm.meta_key = 'featured' AND mt1.meta_key = 'secondary'
      GROUP BY c.id
      ORDER BY CAST(cm.meta_value AS CHAR) ASC, CAST(mt1.meta_value AS CHAR) DESC, c.id DESC;"
)
expect_value "joined descriptor multi order first" "101" "$(printf '%s\n' "$joined_order" | sed -n '1p')"
expect_value "joined descriptor multi order second" "102" "$(printf '%s\n' "$joined_order" | sed -n '2p')"
expect_value "joined descriptor multi order third" "103" "$(printf '%s\n' "$joined_order" | sed -n '3p')"
expect_value "joined descriptor multi order fourth" "105" "$(printf '%s\n' "$joined_order" | sed -n '4p')"
expect_value "joined cast multi order first" "102" "$(printf '%s\n' "$joined_order" | sed -n '5p')"
expect_value "joined cast multi order second" "105" "$(printf '%s\n' "$joined_order" | sed -n '6p')"
expect_value "joined cast multi order third" "103" "$(printf '%s\n' "$joined_order" | sed -n '7p')"
expect_value "joined cast multi order fourth" "101" "$(printf '%s\n' "$joined_order" | sed -n '8p')"
expect_value "joined two cast multi order first" "101" "$(printf '%s\n' "$joined_order" | sed -n '9p')"
expect_value "joined two cast multi order second" "103" "$(printf '%s\n' "$joined_order" | sed -n '10p')"
expect_value "joined two cast multi order third" "105" "$(printf '%s\n' "$joined_order" | sed -n '11p')"
expect_value "joined two cast multi order fourth" "102" "$(printf '%s\n' "$joined_order" | sed -n '12p')"

headers=$(run_mysql_with_headers \
    "USE ${DATABASE};
     SELECT a AS x, b AS y, COUNT(*) AS c FROM t GROUP BY a, b ORDER BY x, y LIMIT 1;"
)
expect_value "header labels" "x	y	c" "$(printf '%s\n' "$headers" | sed -n '1p')"
expect_value "header row" "NULL	NULL	1" "$(printf '%s\n' "$headers" | sed -n '2p')"

expect_error \
    "non grouped selected column" \
    1055 \
    "42000" \
    "Expression #3 of SELECT list is not in GROUP BY clause" \
    "SET SESSION sql_mode = 'ONLY_FULL_GROUP_BY,STRICT_TRANS_TABLES';
     USE ${DATABASE}; SELECT a, b, n, COUNT(*) FROM t GROUP BY a, b;"
expect_error \
    "missing selected group key" \
    1055 \
    "42000" \
    "Expression #2 of SELECT list is not in GROUP BY clause" \
    "SET SESSION sql_mode = 'ONLY_FULL_GROUP_BY,STRICT_TRANS_TABLES';
     USE ${DATABASE}; SELECT a, b, COUNT(*) FROM t GROUP BY a;"
expect_error \
    "unknown group key" \
    1054 \
    "42S22" \
    "Unknown column 'missing' in 'field list'" \
    "USE ${DATABASE}; SELECT a, missing, COUNT(*) FROM t GROUP BY a, missing;"

cleanup

printf '%s\n' "mysql_baseline_group_by_multiple_keys_expectations: ok"
