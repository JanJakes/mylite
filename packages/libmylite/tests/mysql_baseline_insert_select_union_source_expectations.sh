#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_insert_select_union_$$"
OTHER_DATABASE="${DATABASE}_other"

fail() {
    printf '%s\n' "mysql_baseline_insert_select_union_source_expectations: $1" >&2
    exit 1
}

run_mysql() {
    sql=$1
    shift
    printf '%s\n' "$sql" \
        | docker exec -i "$MYSQL_CONTAINER" mysql --protocol=TCP -h127.0.0.1 -uroot \
            --batch --raw --skip-column-names "$@"
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
    run_mysql "DROP DATABASE IF EXISTS ${DATABASE}; DROP DATABASE IF EXISTS ${OTHER_DATABASE};" \
        >/dev/null 2>&1 || true
}

trap cleanup EXIT

version=$(run_mysql 'SELECT VERSION();')
case "$version" in
    8.4.9*) ;;
    *) fail "expected MySQL 8.4.9 runtime, got [$version]" ;;
esac

cleanup

basic_status=$(
    run_mysql \
        "CREATE DATABASE ${DATABASE};
         USE ${DATABASE};
         CREATE TABLE dst(id INT NOT NULL, n INT NULL);
         CREATE TABLE string_dst(id INT NOT NULL, v VARCHAR(10));
         INSERT INTO dst(id, n) SELECT 1, 10 UNION SELECT 1, 10 UNION SELECT 2, NULL;
         SELECT ROW_COUNT(), @@warning_count, @@error_count;
         SELECT id, n FROM dst ORDER BY id, n;
         TRUNCATE dst;
         INSERT INTO dst(id, n) SELECT 1, 10 UNION ALL SELECT 1, 10 UNION ALL SELECT 2, NULL;
         SELECT ROW_COUNT(), @@warning_count, @@error_count;
         SELECT id, n FROM dst ORDER BY id, n;
         INSERT INTO string_dst(id, v) SELECT 4, 'a' UNION SELECT 4, 'A';
         SELECT ROW_COUNT(), @@warning_count, @@error_count;
         SELECT id, v FROM string_dst ORDER BY id, BINARY v;
         TRUNCATE dst;
         INSERT INTO dst(id, n)
             SELECT 1, 10 UNION ALL SELECT 1, 10 UNION SELECT 1, 10 UNION ALL SELECT 1, 10;
         SELECT ROW_COUNT(), @@warning_count, @@error_count;
         SELECT id, n FROM dst;"
)
expect_value \
    "scalar compound source status and rows" \
    "2	0	0
1	10
2	NULL
3	0	0
1	10
1	10
2	NULL
1	0	0
4	a
2	0	0
1	10
1	10" \
    "$basic_status"

table_status=$(
    run_mysql \
        "USE ${DATABASE};
         CREATE TABLE src(id INT NOT NULL, n INT NULL);
         INSERT INTO src VALUES (1, 10), (2, NULL), (3, 30);
         TRUNCATE dst;
         INSERT INTO dst(id, n)
             SELECT id, n FROM src WHERE id <= 2 UNION SELECT id, n FROM src WHERE id >= 2;
         SELECT ROW_COUNT(), @@warning_count, @@error_count;
         SELECT id, n FROM dst ORDER BY id;
         TRUNCATE dst;
         INSERT INTO dst SELECT * FROM src UNION SELECT * FROM src;
         SELECT ROW_COUNT(), @@warning_count, @@error_count;
         SELECT id, n FROM dst ORDER BY id;
         CREATE TABLE case_src_a(id INT NOT NULL, v VARCHAR(10));
         CREATE TABLE case_src_b(id INT NOT NULL, v VARCHAR(10));
         CREATE TABLE case_dst(id INT NOT NULL, v VARCHAR(10));
         INSERT INTO case_src_a VALUES (4, 'a');
         INSERT INTO case_src_b VALUES (4, 'A');
         INSERT INTO case_dst SELECT * FROM case_src_a UNION SELECT * FROM case_src_b;
         SELECT ROW_COUNT(), @@warning_count, @@error_count;
         SELECT id, v FROM case_dst ORDER BY id, BINARY v;"
)
expect_value \
    "descriptor-backed compound source rows" \
    "3	0	0
1	10
2	NULL
3	30
3	0	0
1	10
2	NULL
3	30
1	0	0
4	a" \
    "$table_status"

qualified_status=$(
    run_mysql \
        "CREATE DATABASE ${OTHER_DATABASE};
         CREATE TABLE ${OTHER_DATABASE}.qualified_dst(id INT NOT NULL, n INT NULL);
         INSERT INTO ${OTHER_DATABASE}.qualified_dst(id, n)
             SELECT id, n FROM ${DATABASE}.src WHERE id = 1
             UNION SELECT id, n FROM ${DATABASE}.src WHERE id = 3;
         SELECT ROW_COUNT(), @@warning_count, @@error_count;
         SELECT id, n FROM ${OTHER_DATABASE}.qualified_dst ORDER BY id;"
)
expect_value \
    "qualified target and source branches without default database" \
    "2	0	0
1	10
3	30" \
    "$qualified_status"

zero_status=$(
    run_mysql \
        "USE ${DATABASE};
         CREATE TABLE required_target(id INT NOT NULL, must INT NOT NULL);
         INSERT INTO required_target(id)
             SELECT id FROM src WHERE id > 9 UNION SELECT id FROM src WHERE id < 0;
         SELECT ROW_COUNT(), @@warning_count, @@error_count, COUNT(*) FROM required_target;"
)
expect_value \
    "zero-row compound source skips omitted required column" \
    "0	0	0	0" \
    "$zero_status"

same_table_status=$(
    run_mysql \
        "USE ${DATABASE};
         TRUNCATE src;
         INSERT INTO src VALUES (1, 10), (2, NULL), (3, 30);
         INSERT INTO src
             SELECT * FROM src WHERE id = 1 UNION SELECT * FROM src WHERE id = 4;
         SELECT ROW_COUNT(), @@warning_count, @@error_count;
         SELECT id, n FROM src ORDER BY id, n;"
)
expect_value \
    "same table source target materializes before insert" \
    "1	0	0
1	10
1	10
2	NULL
3	30" \
    "$same_table_status"

expect_error \
    "target source column count mismatch too few" \
    1136 \
    21S01 \
    "Column count doesn't match value count at row 1" \
    "USE ${DATABASE}; INSERT INTO dst(id, n) SELECT 1 UNION SELECT 2;"

expect_error \
    "target source column count mismatch too many" \
    1136 \
    21S01 \
    "Column count doesn't match value count at row 1" \
    "USE ${DATABASE}; INSERT INTO dst(id) SELECT 1, 2 UNION SELECT 3, 4;"

expect_error \
    "branch column count mismatch" \
    1222 \
    21000 \
    "The used SELECT statements have a different number of columns" \
    "USE ${DATABASE}; INSERT INTO dst(id, n) SELECT 1, 2 UNION SELECT 3;"

expect_error \
    "branch column count mismatch before target count mismatch" \
    1222 \
    21000 \
    "The used SELECT statements have a different number of columns" \
    "USE ${DATABASE}; INSERT INTO dst(id, n) SELECT 1 UNION SELECT 2, 3;"

expect_error \
    "omitted not-null no-default with matching source" \
    1364 \
    HY000 \
    "Field 'must' doesn't have a default value" \
    "USE ${DATABASE}; INSERT INTO required_target(id) SELECT 1 UNION SELECT 2;"

expect_error \
    "selected null into not null" \
    1048 \
    23000 \
    "Column 'must' cannot be null" \
    "USE ${DATABASE}; INSERT INTO required_target(id, must) SELECT 1, NULL UNION SELECT 2, 2;"

run_mysql \
    "USE ${DATABASE};
     CREATE TABLE pk_target(id INT PRIMARY KEY, n INT NOT NULL);
     INSERT INTO pk_target VALUES (9, 90);" >/dev/null
expect_error \
    "duplicate key rolls back statement" \
    1062 \
    23000 \
    "Duplicate entry '1' for key 'pk_target.PRIMARY'" \
    "USE ${DATABASE}; INSERT INTO pk_target SELECT 1, 10 UNION ALL SELECT 1, 20;"
rollback_status=$(run_mysql "USE ${DATABASE}; SELECT COUNT(*), MIN(id), MAX(id) FROM pk_target;")
expect_value "duplicate key rollback leaves original rows" "1	9	9" "$rollback_status"

global_order_status=$(
    run_mysql \
        "USE ${DATABASE};
         TRUNCATE dst;
         INSERT INTO dst SELECT 2, 20 UNION SELECT 1, 10 ORDER BY 1;
         SELECT ROW_COUNT(), @@warning_count, @@error_count;
         SELECT id, n FROM dst ORDER BY id;"
)
expect_value \
    "global order by is broader MySQL surface" \
    "2	0	0
1	10
2	20" \
    "$global_order_status"

global_limit_status=$(
    run_mysql \
        "USE ${DATABASE};
         TRUNCATE dst;
         INSERT INTO dst SELECT 1, 10 UNION ALL SELECT 2, 20 LIMIT 1;
         SELECT ROW_COUNT(), @@warning_count, @@error_count;
         SELECT id, n FROM dst;"
)
expect_value \
    "global limit is broader MySQL surface" \
    "1	0	0
1	10" \
    "$global_limit_status"
