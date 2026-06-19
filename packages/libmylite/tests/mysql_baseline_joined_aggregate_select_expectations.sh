#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_joined_aggregate_select_expectations_$$"

fail() {
    printf '%s\n' "mysql_baseline_joined_aggregate_select_expectations: $1" >&2
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
     CREATE TABLE posts(id INT NOT NULL, category INT NULL, title VARCHAR(20));
     CREATE TABLE comments(id INT NOT NULL, post_id INT NULL, score INT NULL, body VARCHAR(20));
     INSERT INTO posts VALUES
       (1,10,'alpha'),(2,10,'beta'),(3,20,'gamma'),(4,NULL,'delta');
     INSERT INTO comments VALUES
       (101,1,5,'good'),(102,1,NULL,'none'),(103,2,7,'ok'),
       (104,NULL,9,'orphan'),(105,99,11,'missing');" >/dev/null

core=$(run_mysql_with_headers \
    "USE ${DATABASE};
     DO 0;
     SELECT p.id, COUNT(*) AS joined_rows, COUNT(c.id) AS matched_comments,
            MIN(c.score) AS score_min, MAX(c.score) AS score_max,
            SUM(c.score) AS score_sum, AVG(c.score) AS score_avg
       FROM posts AS p LEFT JOIN comments AS c ON p.id = c.post_id
       GROUP BY p.id ORDER BY id;
     SELECT @@warning_count, ROW_COUNT();
     SELECT c.post_id, COUNT(*)
       FROM posts AS p JOIN comments AS c ON p.id = c.post_id
       GROUP BY c.post_id ORDER BY c.post_id;
     SELECT p.category, COUNT(c.id)
       FROM posts p LEFT JOIN comments c ON p.id = c.post_id
       WHERE p.id >= 2 GROUP BY p.category ORDER BY p.category;
     SELECT p.id AS post_id, COUNT(c.id) AS c
       FROM posts p LEFT JOIN comments c ON p.id = c.post_id
       GROUP BY p.id HAVING c > 0 ORDER BY post_id DESC LIMIT 2;"
)
expect_value "left join aggregate headers" "id	joined_rows	matched_comments	score_min	score_max	score_sum	score_avg" \
    "$(printf '%s\n' "$core" | sed -n '1p')"
expect_value "post one counts both joined rows" "1	2	2	5	5	5	5.0000" \
    "$(printf '%s\n' "$core" | sed -n '2p')"
expect_value "post two one match" "2	1	1	7	7	7	7.0000" \
    "$(printf '%s\n' "$core" | sed -n '3p')"
expect_value "post three null-extended count star" "3	1	0	NULL	NULL	NULL	NULL" \
    "$(printf '%s\n' "$core" | sed -n '4p')"
expect_value "post four null category null-extended" "4	1	0	NULL	NULL	NULL	NULL" \
    "$(printf '%s\n' "$core" | sed -n '5p')"
expect_value "joined aggregate status" "0	-1" "$(printf '%s\n' "$core" | sed -n '7p')"
expect_value "inner join group first" "1	2" "$(printf '%s\n' "$core" | sed -n '9p')"
expect_value "inner join group second" "2	1" "$(printf '%s\n' "$core" | sed -n '10p')"
expect_value "where null category group" "NULL	0" "$(printf '%s\n' "$core" | sed -n '12p')"
expect_value "where category ten group" "10	1" "$(printf '%s\n' "$core" | sed -n '13p')"
expect_value "where category twenty group" "20	0" "$(printf '%s\n' "$core" | sed -n '14p')"
expect_value "having desc first" "2	1" "$(printf '%s\n' "$core" | sed -n '16p')"
expect_value "having desc second" "1	2" "$(printf '%s\n' "$core" | sed -n '17p')"

limits=$(run_mysql \
    "USE ${DATABASE};
     SELECT p.id, COUNT(c.id)
       FROM posts p LEFT JOIN comments c ON p.id = c.post_id
       GROUP BY p.id ORDER BY p.id LIMIT 0;
     SELECT p.id, COUNT(c.id)
       FROM posts p LEFT JOIN comments c ON p.id = c.post_id
       GROUP BY p.id ORDER BY p.id LIMIT 2 OFFSET 1;
     SELECT p.id, COUNT(c.id)
       FROM posts p LEFT JOIN comments c ON p.id = c.post_id
       GROUP BY p.id ORDER BY p.id LIMIT 1,2;"
)
expect_value "limit offset first row" "2	1" "$(printf '%s\n' "$limits" | sed -n '1p')"
expect_value "limit offset second row" "3	0" "$(printf '%s\n' "$limits" | sed -n '2p')"
expect_value "limit comma first row" "2	1" "$(printf '%s\n' "$limits" | sed -n '3p')"
expect_value "limit comma second row" "3	0" "$(printf '%s\n' "$limits" | sed -n '4p')"

count_star=$(run_mysql \
    "USE ${DATABASE};
     SELECT DISTINCT COUNT(*)
       FROM posts p JOIN comments c ON p.id = c.post_id;
     SELECT COUNT(*)
       FROM posts p JOIN comments c ON p.id = c.post_id LIMIT 2000;"
)
expect_value "joined distinct count star" "3" "$(printf '%s\n' "$count_star" | sed -n '1p')"
expect_value "joined count star limit" "3" "$(printf '%s\n' "$count_star" | sed -n '2p')"

count_star_zero=$(run_mysql \
    "USE ${DATABASE};
     SELECT COUNT(*)
       FROM posts p JOIN comments c ON p.id = c.post_id LIMIT 0;"
)
expect_value "joined count star zero limit" "" "$count_star_zero"

bitwise=$(run_mysql \
    "USE ${DATABASE};
     SELECT p.category, MIN(c.score), MAX(c.score), BIT_AND(c.score),
            BIT_OR(c.score), BIT_XOR(c.score)
       FROM posts p LEFT JOIN comments c ON p.id = c.post_id
       GROUP BY p.category ORDER BY p.category;"
)
expect_value "bitwise null category" "NULL	NULL	NULL	18446744073709551615	0	0" \
    "$(printf '%s\n' "$bitwise" | sed -n '1p')"
expect_value "bitwise category ten" "10	5	7	5	7	2" \
    "$(printf '%s\n' "$bitwise" | sed -n '2p')"
expect_value "bitwise category twenty" "20	NULL	NULL	18446744073709551615	0	0" \
    "$(printf '%s\n' "$bitwise" | sed -n '3p')"

accepted_but_deferred=$(run_mysql \
    "USE ${DATABASE};
     SELECT p.id, COUNT(c.id) FROM posts p LEFT JOIN comments c USING (id) GROUP BY p.id;
     SELECT p.id, COUNT(c.id) FROM posts p LEFT JOIN comments c ON p.id = c.post_id GROUP BY 1;
     SELECT p.id, COUNT(DISTINCT c.id) FROM posts p LEFT JOIN comments c ON p.id = c.post_id GROUP BY p.id;"
)
expect_value "using accepted by mysql" "1	0" "$(printf '%s\n' "$accepted_but_deferred" | sed -n '1p')"
expect_value "group ordinal accepted by mysql" "1	2" "$(printf '%s\n' "$accepted_but_deferred" | sed -n '5p')"
expect_value "grouped distinct accepted by mysql" "1	2" \
    "$(printf '%s\n' "$accepted_but_deferred" | sed -n '9p')"
expect_value "grouped distinct second post" "2	1" \
    "$(printf '%s\n' "$accepted_but_deferred" | sed -n '10p')"
expect_value "grouped distinct no comment post" "3	0" \
    "$(printf '%s\n' "$accepted_but_deferred" | sed -n '11p')"
expect_value "grouped distinct null category post" "4	0" \
    "$(printf '%s\n' "$accepted_but_deferred" | sed -n '12p')"

expect_error \
    "ambiguous selected group column" \
    1052 \
    "23000" \
    "Column 'id' in field list is ambiguous" \
    "USE ${DATABASE}; SELECT id, COUNT(*) FROM posts p JOIN comments c ON p.id = c.post_id GROUP BY id;"
expect_error \
    "ambiguous group column" \
    1052 \
    "23000" \
    "Column 'id' in group statement is ambiguous" \
    "USE ${DATABASE}; SELECT p.id AS selected_id, COUNT(*) FROM posts p JOIN comments c ON p.id = c.post_id GROUP BY id;"
expect_error \
    "unknown aggregate column" \
    1054 \
    "42S22" \
    "Unknown column 'c.missing' in 'field list'" \
    "USE ${DATABASE}; SELECT p.id, COUNT(c.missing) FROM posts p LEFT JOIN comments c ON p.id = c.post_id GROUP BY p.id;"
expect_error \
    "unknown having column" \
    1054 \
    "42S22" \
    "Unknown column 'missing' in 'having clause'" \
    "USE ${DATABASE}; SELECT p.id, COUNT(c.id) FROM posts p LEFT JOIN comments c ON p.id = c.post_id GROUP BY p.id HAVING missing > 0;"
expect_error \
    "unknown order column" \
    1054 \
    "42S22" \
    "Unknown column 'missing' in 'order clause'" \
    "USE ${DATABASE}; SELECT p.id, COUNT(c.id) FROM posts p LEFT JOIN comments c ON p.id = c.post_id GROUP BY p.id ORDER BY missing;"
expect_error \
    "left join without condition is syntax before table lookup" \
    1064 \
    "42000" \
    "You have an error in your SQL syntax" \
    "USE ${DATABASE}; SELECT p.id, COUNT(*) FROM missing p LEFT JOIN also_missing c GROUP BY p.id;"

cleanup

printf '%s\n' "mysql_baseline_joined_aggregate_select_expectations: ok"
