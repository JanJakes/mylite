#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_count_aggregate_expectations_$$"

fail() {
    printf '%s\n' "mysql_baseline_count_aggregate_expectations: $1" >&2
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
     CREATE TABLE t(id INT NOT NULL, n INT NULL, nn INT NOT NULL, u INT UNSIGNED NULL, b BIGINT NULL, bu BIGINT UNSIGNED NULL);
     CREATE TABLE empty_t(id INT NOT NULL, n INT NULL, nn INT NOT NULL);
     INSERT INTO t VALUES
       (1, NULL, 10, 1, -1, 1),
       (2, 20, 20, 2, 0, 2),
       (3, 20, 30, 4294967295, 9223372036854775807, 9223372036854775807),
       (4, 30, 40, NULL, NULL, NULL);" >/dev/null

core=$(run_mysql \
    "USE ${DATABASE};
     DO 0; SELECT COUNT(*); SELECT @@warning_count, ROW_COUNT();
     DO 0; SELECT COUNT(*) FROM DUAL; SELECT @@warning_count, ROW_COUNT();
     DO 0; SELECT COUNT(*) FROM empty_t; SELECT @@warning_count, ROW_COUNT();
     DO 0; SELECT COUNT(*) FROM t; SELECT @@warning_count, ROW_COUNT();"
)
expect_value "count without source" "1" "$(printf '%s\n' "$core" | sed -n '1p')"
expect_value "count without source status" "0	-1" "$(printf '%s\n' "$core" | sed -n '2p')"
expect_value "count from dual" "1" "$(printf '%s\n' "$core" | sed -n '3p')"
expect_value "count from dual status" "0	-1" "$(printf '%s\n' "$core" | sed -n '4p')"
expect_value "empty table count" "0" "$(printf '%s\n' "$core" | sed -n '5p')"
expect_value "empty table count status" "0	-1" "$(printf '%s\n' "$core" | sed -n '6p')"
expect_value "nonempty table count" "4" "$(printf '%s\n' "$core" | sed -n '7p')"
expect_value "nonempty table count status" "0	-1" "$(printf '%s\n' "$core" | sed -n '8p')"

headers_output=$(run_mysql_with_headers \
    "USE ${DATABASE};
     SELECT COUNT(*), count(*), Count(*), COUNT( * ), COUNT(/*x*/*), (COUNT(*)) FROM t;"
)
headers=$(printf '%s\n' "$headers_output" | sed -n '1p')
values=$(printf '%s\n' "$headers_output" | sed -n '2p')
expect_value \
    "count labels" \
    "COUNT(*)	count(*)	Count(*)	COUNT( * )	COUNT(/*x*/ *)	(COUNT(*))" \
    "$headers"
expect_value "count label values" "4	4	4	4	4	4" "$values"

where_counts=$(run_mysql \
    "USE ${DATABASE};
     SELECT COUNT(*) FROM t WHERE id = 1;
     SELECT COUNT(*) FROM t WHERE id <> 1;
     SELECT COUNT(*) FROM t WHERE id != 1;
     SELECT COUNT(*) FROM t WHERE id < 3;
     SELECT COUNT(*) FROM t WHERE id <= 3;
     SELECT COUNT(*) FROM t WHERE id > 2;
     SELECT COUNT(*) FROM t WHERE id >= 2;
     SELECT COUNT(*) FROM t WHERE id <=> 2;
     SELECT COUNT(*) FROM t WHERE n = 20;
     SELECT COUNT(*) FROM t WHERE n <> 20;
     SELECT COUNT(*) FROM t WHERE n <=> 20;
     SELECT COUNT(*) FROM t WHERE n IS NULL;
     SELECT COUNT(*) FROM t WHERE n IS NOT NULL;
     SELECT COUNT(*) FROM t WHERE u = 4294967295;
     SELECT COUNT(*) FROM t WHERE b = -1;
     SELECT COUNT(*) FROM t WHERE bu = 9223372036854775807;"
)
expect_value "where equal" "1" "$(printf '%s\n' "$where_counts" | sed -n '1p')"
expect_value "where not equal <>" "3" "$(printf '%s\n' "$where_counts" | sed -n '2p')"
expect_value "where not equal !=" "3" "$(printf '%s\n' "$where_counts" | sed -n '3p')"
expect_value "where less" "2" "$(printf '%s\n' "$where_counts" | sed -n '4p')"
expect_value "where less equal" "3" "$(printf '%s\n' "$where_counts" | sed -n '5p')"
expect_value "where greater" "2" "$(printf '%s\n' "$where_counts" | sed -n '6p')"
expect_value "where greater equal" "3" "$(printf '%s\n' "$where_counts" | sed -n '7p')"
expect_value "where null safe equal" "1" "$(printf '%s\n' "$where_counts" | sed -n '8p')"
expect_value "where nullable equal" "2" "$(printf '%s\n' "$where_counts" | sed -n '9p')"
expect_value "where nullable not equal" "1" "$(printf '%s\n' "$where_counts" | sed -n '10p')"
expect_value "where nullable null-safe equal" "2" "$(printf '%s\n' "$where_counts" | sed -n '11p')"
expect_value "where is null" "1" "$(printf '%s\n' "$where_counts" | sed -n '12p')"
expect_value "where is not null" "3" "$(printf '%s\n' "$where_counts" | sed -n '13p')"
expect_value "where unsigned int boundary" "1" "$(printf '%s\n' "$where_counts" | sed -n '14p')"
expect_value "where bigint signed" "1" "$(printf '%s\n' "$where_counts" | sed -n '15p')"
expect_value "where bigint unsigned supported max" "1" "$(printf '%s\n' "$where_counts" | sed -n '16p')"

joined_counts=$(run_mysql \
    "USE ${DATABASE};
     CREATE TABLE posts (ID INT, post_status VARCHAR(20), post_type VARCHAR(20));
     CREATE TABLE term_relationships (object_id INT, term_taxonomy_id INT);
     INSERT INTO posts VALUES
       (1, 'publish', 'post'),
       (2, 'draft', 'post'),
       (3, 'publish', 'page'),
       (4, 'publish', 'post');
     INSERT INTO term_relationships VALUES
       (1, 1), (1, 2), (2, 1), (3, 1), (4, 1), (99, 1);
     SELECT COUNT(*) AS c
       FROM term_relationships, posts
      WHERE posts.ID = term_relationships.object_id
        AND post_status IN ('publish')
        AND post_type IN ('post')
        AND term_taxonomy_id = 1;
     SELECT COUNT(*)
       FROM posts AS p JOIN term_relationships AS tr
         ON p.ID = tr.object_id
      WHERE tr.term_taxonomy_id = 1;
     SELECT COUNT(*) AS c
       FROM posts AS p
       LEFT JOIN term_relationships AS tr ON p.ID = tr.object_id
      WHERE p.post_status = 'publish';
     SELECT @@warning_count, ROW_COUNT();"
)
expect_value "comma joined WordPress-style count star" "2" "$(printf '%s\n' "$joined_counts" | sed -n '1p')"
expect_value "explicit inner joined count star" "4" "$(printf '%s\n' "$joined_counts" | sed -n '2p')"
expect_value "left joined count star" "4" "$(printf '%s\n' "$joined_counts" | sed -n '3p')"
expect_value "joined count status" "0	-1" "$(printf '%s\n' "$joined_counts" | sed -n '4p')"

accepted_count_forms=$(run_mysql \
    "USE ${DATABASE};
     SELECT COUNT(1), COUNT(n), COUNT(NULL) FROM t;
     SELECT COUNT(*) FROM t ORDER BY id;
     SELECT COUNT(n) FROM t ORDER BY id;
     SELECT COUNT(1) FROM t ORDER BY id;
     SELECT COUNT(IFNULL(n, 0)), COUNT(NULLIF(n, 20)) FROM t;
     SELECT COUNT(DISTINCT n) FROM t ORDER BY id;
     SELECT COUNT(*) FROM t LIMIT 1;
     SELECT COUNT(n) FROM t LIMIT 1;
     SELECT COUNT(1) FROM t LIMIT 1;
     SELECT COUNT(DISTINCT n) FROM t LIMIT 1;
     SELECT COUNT(*) FROM t ORDER BY id LIMIT 0, 3;"
)
expect_value "count expr forms" "4	3	0" "$(printf '%s\n' "$accepted_count_forms" | sed -n '1p')"
expect_value "count order by aggregate" "4" "$(printf '%s\n' "$accepted_count_forms" | sed -n '2p')"
expect_value "count order by column" "3" "$(printf '%s\n' "$accepted_count_forms" | sed -n '3p')"
expect_value "count order by literal" "4" "$(printf '%s\n' "$accepted_count_forms" | sed -n '4p')"
expect_value "count row-scalar expressions" "4	1" "$(printf '%s\n' "$accepted_count_forms" | sed -n '5p')"
expect_value "count order by distinct" "2" "$(printf '%s\n' "$accepted_count_forms" | sed -n '6p')"
expect_value "count limit one returns row" "4" "$(printf '%s\n' "$accepted_count_forms" | sed -n '7p')"
expect_value "count limit column" "3" "$(printf '%s\n' "$accepted_count_forms" | sed -n '8p')"
expect_value "count limit literal" "4" "$(printf '%s\n' "$accepted_count_forms" | sed -n '9p')"
expect_value "count limit distinct" "2" "$(printf '%s\n' "$accepted_count_forms" | sed -n '10p')"
expect_value "count order by limit offset zero" "4" "$(printf '%s\n' "$accepted_count_forms" | sed -n '11p')"

limit_zero=$(run_mysql "USE ${DATABASE}; SELECT 'before'; SELECT COUNT(*) FROM t LIMIT 0; SELECT 'after';")
expect_value "count limit zero before marker" "before" "$(printf '%s\n' "$limit_zero" | sed -n '1p')"
expect_value "count limit zero after marker" "after" "$(printf '%s\n' "$limit_zero" | sed -n '2p')"
expect_value "count limit zero row count" "2" "$(printf '%s\n' "$limit_zero" | wc -l | tr -d ' ')"

limit_positive_offset=$(run_mysql "USE ${DATABASE}; SELECT 'before'; SELECT COUNT(*) FROM t LIMIT 1, 1; SELECT 'after';")
expect_value "count positive offset before marker" "before" "$(printf '%s\n' "$limit_positive_offset" | sed -n '1p')"
expect_value "count positive offset after marker" "after" "$(printf '%s\n' "$limit_positive_offset" | sed -n '2p')"
expect_value "count positive offset row count" "2" "$(printf '%s\n' "$limit_positive_offset" | wc -l | tr -d ' ')"

having_empty=$(run_mysql "USE ${DATABASE}; SELECT 'before'; SELECT COUNT(*) FROM t HAVING COUNT(*) > 99; SELECT 'after';")
expect_value "deferred empty having before marker" "before" "$(printf '%s\n' "$having_empty" | sed -n '1p')"
expect_value "deferred empty having after marker" "after" "$(printf '%s\n' "$having_empty" | sed -n '2p')"
expect_value "deferred empty having row count" "2" "$(printf '%s\n' "$having_empty" | wc -l | tr -d ' ')"

expect_error \
    "count table star is syntax error" \
    1064 \
    42000 \
    "near '*) FROM t' at line 1" \
    "USE ${DATABASE}; SELECT COUNT(t.*) FROM t;"

expect_error \
    "count whitespace before paren is syntax error" \
    1064 \
    42000 \
    "near '*) FROM DUAL' at line 1" \
    "SELECT COUNT (*) FROM DUAL;"

expect_error \
    "count comment before paren is syntax error" \
    1064 \
    42000 \
    "near '*) FROM DUAL' at line 1" \
    "SELECT COUNT/**/(*) FROM DUAL;"
