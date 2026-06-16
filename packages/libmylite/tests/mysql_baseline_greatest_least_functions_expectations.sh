#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
MYSQL_BIN="${MYLITE_MYSQL_BIN:-}"
MYSQL_SOCKET="${MYLITE_MYSQL_SOCKET:-}"
DATABASE="mylite_greatest_least_expectations_$$"

fail() {
    printf '%s\n' "mysql_baseline_greatest_least_functions_expectations: $1" >&2
    exit 1
}

run_mysql() {
    sql=$1
    shift
    if [ -n "$MYSQL_BIN" ]; then
        if [ -n "$MYSQL_SOCKET" ]; then
            printf '%s\n' "$sql" \
                | "$MYSQL_BIN" --protocol=SOCKET --socket="$MYSQL_SOCKET" -uroot \
                    --batch --raw --skip-column-names --default-character-set=utf8mb4 "$@"
        else
            printf '%s\n' "$sql" \
                | "$MYSQL_BIN" --protocol=TCP -h127.0.0.1 -uroot \
                    --batch --raw --skip-column-names --default-character-set=utf8mb4 "$@"
        fi
    else
        printf '%s\n' "$sql" \
            | docker exec -i "$MYSQL_CONTAINER" mysql --protocol=TCP -h127.0.0.1 -uroot \
                --batch --raw --skip-column-names --default-character-set=utf8mb4 "$@"
    fi
}

run_mysql_with_headers() {
    sql=$1
    shift
    if [ -n "$MYSQL_BIN" ]; then
        if [ -n "$MYSQL_SOCKET" ]; then
            printf '%s\n' "$sql" \
                | "$MYSQL_BIN" --protocol=SOCKET --socket="$MYSQL_SOCKET" -uroot \
                    --batch --raw --default-character-set=utf8mb4 "$@"
        else
            printf '%s\n' "$sql" \
                | "$MYSQL_BIN" --protocol=TCP -h127.0.0.1 -uroot \
                    --batch --raw --default-character-set=utf8mb4 "$@"
        fi
    else
        printf '%s\n' "$sql" \
            | docker exec -i "$MYSQL_CONTAINER" mysql --protocol=TCP -h127.0.0.1 -uroot \
                --batch --raw --default-character-set=utf8mb4 "$@"
    fi
}

expect_output() {
    label=$1
    expected=$2
    sql=$3
    shift 3

    output=$(run_mysql "$sql" "$@")
    if [ "$output" != "$expected" ]; then
        fail "$label: expected [$expected], got [$output]"
    fi
}

expect_output_with_headers() {
    label=$1
    expected=$2
    sql=$3
    shift 3

    output=$(run_mysql_with_headers "$sql" "$@")
    if [ "$output" != "$expected" ]; then
        fail "$label: expected [$expected], got [$output]"
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
        *) fail "$label: expected error $code/$state containing [$message], got [$output]" ;;
    esac
}

expect_upstream_accepts() {
    label=$1
    sql=$2
    shift 2

    set +e
    output=$(run_mysql "$sql" "$@" 2>&1)
    status_code=$?
    set -e

    if [ "$status_code" -ne 0 ]; then
        fail "$label: expected MySQL to accept deferred behavior, got [$output]"
    fi
}

cleanup() {
    run_mysql "DROP DATABASE IF EXISTS ${DATABASE};" >/dev/null 2>&1 || true
}

trap cleanup EXIT HUP INT TERM

version=$(run_mysql "SELECT VERSION();")
case "$version" in
    8.4.9*) ;;
    *) fail "expected MySQL 8.4.9 runtime, got [$version]" ;;
esac

cleanup
run_mysql "CREATE DATABASE ${DATABASE} DEFAULT CHARACTER SET utf8mb4 "\
"COLLATE utf8mb4_0900_ai_ci;" >/dev/null

core_expected=$(cat <<EXPECTED
2	0	3	-2	2	0	NULL	NULL	9223372036854775807	-9223372036854775808	0
-1	0
EXPECTED
)
expect_output \
    "core integer and null values" \
    "$core_expected" \
"DO 0; SELECT GREATEST(2,0), LEAST(2,0), GREATEST(-2,+3,1), "\
"LEAST(-2,+3,1), GREATEST(TRUE,FALSE,2), LEAST(TRUE,FALSE,2), "\
"GREATEST(NULL,1), LEAST(1,NULL), "\
"GREATEST(-9223372036854775808, 9223372036854775807), "\
"LEAST(-9223372036854775808, 9223372036854775807), @@warning_count; "\
"SELECT ROW_COUNT(), @@warning_count;" \
    "$DATABASE"

string_expected=$(cat <<EXPECTED
C	A	c	A	45	11	a		A	a	B	b
EXPECTED
)
expect_output \
    "ASCII string comparison and ties" \
    "$string_expected" \
    "SELECT GREATEST('B','A','C'), LEAST('B','A','C'), "\
"GREATEST('b','A','c'), LEAST('b','A','c'), "\
"GREATEST('11','45','2'), LEAST('11','45','2'), "\
"GREATEST('', 'a'), LEAST('a', ''), "\
"GREATEST('a','A'), LEAST('a','A'), GREATEST('b','B','a'), LEAST('b','B','c');" \
    "$DATABASE"

labels_expected=$(cat <<EXPECTED
GREATEST (2,1)	least_alias
2	a
EXPECTED
)
expect_output_with_headers \
    "labels and whitespace" \
    "$labels_expected" \
    "SELECT GREATEST (2,1), LEAST('b','a') AS least_alias FROM DUAL;" \
    "$DATABASE"

table_expected=$(cat <<EXPECTED
1	3	2	m	alpha	m	first
2	5	3	m	Beta	zz	m
3	NULL	NULL	NULL	NULL	NULL	NULL
4	20	3	Other	m	m	m
EXPECTED
)
expect_output \
    "table rows" \
    "$table_expected" \
    "CREATE TABLE t(id INT, code VARCHAR(20), n INT, c CHAR(4), body TEXT); "\
"INSERT INTO t VALUES "\
"(1,'alpha',2,'Aa','first'), "\
"(2,'Beta',5,'zz','second'), "\
"(3,NULL,NULL,NULL,NULL), "\
"(4,'Other',20,'aa','third'); "\
"SELECT id, GREATEST(n,3), LEAST(n,3), GREATEST(code,'m'), LEAST(code,'m'), "\
"GREATEST(c,'m'), LEAST(body,'m') FROM t ORDER BY id;" \
    "$DATABASE"

ties_expected=$(cat <<EXPECTED
1	A	a
2	b	B
EXPECTED
)
expect_output \
    "table string ties" \
    "$ties_expected" \
    "CREATE TABLE ties(id INT, left_s VARCHAR(5), right_s VARCHAR(5)); "\
"INSERT INTO ties VALUES (1,'a','A'), (2,'B','b'); "\
"SELECT id, GREATEST(left_s,right_s), LEAST(left_s,right_s) FROM ties ORDER BY id;" \
    "$DATABASE"

integer_family_expected=$(cat <<EXPECTED
2	2	9223372036854775807	-9223372036854775808	4	9223372036854775807
EXPECTED
)
expect_output \
    "table integer families" \
    "$integer_family_expected" \
    "CREATE TABLE nums(i INT, j INTEGER, b BIGINT, b_min BIGINT, "\
"u INT UNSIGNED, ub BIGINT UNSIGNED); "\
"INSERT INTO nums VALUES "\
"(1,2,9223372036854775807,-9223372036854775808,4,9223372036854775807); "\
"SELECT GREATEST(i,2), LEAST(j,3), GREATEST(b,0), LEAST(b_min,0), "\
"LEAST(u,10), GREATEST(ub,0) FROM nums;" \
    "$DATABASE"

where_order_limit_expected=$(cat <<EXPECTED
4	Other	4
2	m	4
EXPECTED
)
expect_output \
    "row envelope" \
    "$where_order_limit_expected" \
    "SELECT id, GREATEST(code,'m'), LEAST(n,4) FROM t WHERE id <> 3 ORDER BY id DESC LIMIT 2;" \
    "$DATABASE"

control_flow_expected=$(cat <<EXPECTED
1	3	alpha	NULL	nz	yes
2	5	Beta	5	nz	yes
3	99	fallback	NULL	z	no
4	20	m	20	nz	yes
EXPECTED
)
expect_output \
    "control-flow nested greatest least" \
    "$control_flow_expected" \
    "SELECT id, IFNULL(GREATEST(n,3),99), "\
"COALESCE(LEAST(code,'m'),'fallback'), NULLIF(GREATEST(n,3),3), "\
"IF(GREATEST(n,0),'nz','z'), "\
"CASE WHEN LEAST(n,1) THEN 'yes' ELSE 'no' END FROM t ORDER BY id;" \
    "$DATABASE"

do_expected=$(cat <<EXPECTED
0	0
EXPECTED
)
expect_output \
    "DO status" \
    "$do_expected" \
    "DO GREATEST('b','a'), LEAST(3,1), GREATEST(NULL,1); "\
"SELECT ROW_COUNT(), @@warning_count;" \
    "$DATABASE"

expect_error \
    "GREATEST zero arguments" \
    1582 \
    "42000" \
    "Incorrect parameter count" \
    "SELECT GREATEST();" \
    "$DATABASE"

expect_error \
    "GREATEST one argument" \
    1582 \
    "42000" \
    "Incorrect parameter count" \
    "SELECT GREATEST('x');" \
    "$DATABASE"

expect_error \
    "LEAST zero arguments" \
    1582 \
    "42000" \
    "Incorrect parameter count" \
    "SELECT LEAST();" \
    "$DATABASE"

expect_error \
    "LEAST one argument" \
    1582 \
    "42000" \
    "Incorrect parameter count" \
    "SELECT LEAST('x');" \
    "$DATABASE"

expect_upstream_accepts \
    "mixed domain accepted by MySQL but deferred by MyLite" \
    "SELECT GREATEST(2, '10'), LEAST(2, '10'); SHOW WARNINGS;" \
    "$DATABASE"

expect_upstream_accepts \
    "decimal domain accepted by MySQL but deferred by MyLite" \
    "SELECT GREATEST(1.5, 1.25), LEAST(1.5, 1.25);" \
    "$DATABASE"

expect_upstream_accepts \
    "binary domain accepted by MySQL but deferred by MyLite" \
    "SELECT GREATEST(_binary 'a', _binary 'A'), LEAST(_binary 'a', _binary 'A');" \
    "$DATABASE"

expect_upstream_accepts \
    "concat nested row-scalar usage accepted by MySQL but deferred by MyLite" \
    "SELECT CONCAT(GREATEST(n,2), 'x') FROM t;" \
    "$DATABASE"

expect_upstream_accepts \
    "predicate and order usage accepted by MySQL but deferred by MyLite" \
    "SELECT n FROM t WHERE GREATEST(n,2) = 2 ORDER BY GREATEST(n,2);" \
    "$DATABASE"

expect_upstream_accepts \
    "update assignment usage accepted by MySQL but deferred by MyLite" \
    "UPDATE t SET n = GREATEST(n,2);" \
    "$DATABASE"

expect_upstream_accepts \
    "accent-insensitive collation accepted by MySQL but deferred by MyLite" \
    "SELECT GREATEST('é','z'), LEAST('é','z');" \
    "$DATABASE"

printf '%s\n' "mysql_baseline_greatest_least_functions_expectations: ok"
