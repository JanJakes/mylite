#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_substring_functions_expectations_$$"

fail() {
    printf '%s\n' "mysql_baseline_substring_functions_expectations: $1" >&2
    exit 1
}

run_mysql() {
    sql=$1
    shift
    printf '%s\n' "$sql" \
        | docker exec -i "$MYSQL_CONTAINER" mysql --protocol=TCP -h127.0.0.1 -uroot \
            --batch --raw --skip-column-names --default-character-set=utf8mb4 "$@"
}

run_mysql_with_headers() {
    sql=$1
    shift
    printf '%s\n' "$sql" \
        | docker exec -i "$MYSQL_CONTAINER" mysql --protocol=TCP -h127.0.0.1 -uroot \
            --batch --raw --default-character-set=utf8mb4 "$@"
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

cleanup() {
    run_mysql "DROP DATABASE IF EXISTS ${DATABASE};" >/dev/null 2>&1 || true
}

trap cleanup EXIT

version=$(run_mysql "SELECT VERSION();")
case "$version" in
    8.4.9*) ;;
    *) fail "expected MySQL 8.4.9 runtime, got [$version]" ;;
esac

cleanup
run_mysql "CREATE DATABASE ${DATABASE} CHARACTER SET utf8mb4 COLLATE utf8mb4_0900_ai_ci; USE ${DATABASE}; SET NAMES utf8mb4; SET SESSION sql_mode = 'NO_ENGINE_SUBSTITUTION';" >/dev/null

scalar_expected=$(cat <<EXPECTED
bcdef	bcdef	bcdef	bcd	bcd	bcd	cd	cd	cd	def					é🙂	🙂a	6	2	234	12345	1	0	mylite	SUBSTITUTION	NULL	NULL	NULL
-1	0
EXPECTED
)
expect_output \
    "scalar substring values" \
    "$scalar_expected" \
    "DO 0; SELECT SUBSTRING('abcdef', 2), SUBSTR('abcdef' FROM 2), "\
"MID('abcdef', 2), SUBSTRING('abcdef', 2, 3), SUBSTR('abcdef' FROM 2 FOR 3), "\
"MID('abcdef', 2, 3), SUBSTRING('abcdef', -4, 2), "\
"SUBSTR('abcdef' FROM -4 FOR 2), MID('abcdef' FROM -4 FOR 2), "\
"SUBSTRING('abcdef', -3), SUBSTRING('abc', -5), SUBSTRING('abc', 0), "\
"SUBSTRING('abc', 1, 0), SUBSTRING('abc', 1, -1), "\
"SUBSTRING('é🙂abc', 1, 2), SUBSTRING('é🙂abc', -4, 2), "\
"LENGTH(SUBSTRING('é🙂abc', 1, 2)), CHAR_LENGTH(SUBSTRING('é🙂abc', 1, 2)), "\
"SUBSTRING(12345, 2, 3), SUBSTRING(-12345, 2), SUBSTRING(TRUE, 1), "\
"SUBSTRING(FALSE, 1), SUBSTRING(DATABASE(), 1, 6), SUBSTRING(@@sql_mode, -12), "\
"SUBSTRING(NULL, 1), SUBSTRING('abc', NULL), SUBSTRING('abc', 1, NULL); "\
"SELECT ROW_COUNT(), @@warning_count;" \
    "$DATABASE"

expect_output \
    "from dual values" \
    "bcd	bcd" \
    "SELECT SUBSTRING('abcdef', 2, 3), MID('abcdef' FROM 2 FOR 3) FROM DUAL;" \
    "$DATABASE"

expect_error \
    "substring rejects default-mode whitespace before parenthesis" \
    1630 \
    42000 \
    "FUNCTION ${DATABASE}.SUBSTRING does not exist" \
    "SELECT SUBSTRING ('abcdef', 2, 3);" \
    "$DATABASE"

expect_output \
    "do row count" \
    "0	0" \
    "DO SUBSTRING('abc', 1), SUBSTR(NULL, 1), MID('abc', 1, 1); "\
"SELECT ROW_COUNT(), @@warning_count;" \
    "$DATABASE"

run_mysql \
    "CREATE TABLE t ("\
"id INT, v VARCHAR(20), c CHAR(5), txt TEXT, i INT, d DECIMAL(6,2), y YEAR, dt DATETIME, "\
"b VARBINARY(4), f DOUBLE"\
"); "\
"INSERT INTO t VALUES "\
"(1, 'abcdef', 'a  ', 'hello', 12345, 12.30, 2024, '2024-01-02 13:29:17', X'4142', 1.5), "\
"(2, 'é🙂abc', 'é', '', -7, -4.50, 70, NULL, X'c389', -2.5), "\
"(3, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL);" \
    "$DATABASE" >/dev/null

table_expected=$(cat <<\EXPECTED
1	bcd	cd	a	ello	23	30	2024	2024-01-02
2	🙂ab	🙂a	é		7	50	1970	NULL
3	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL
EXPECTED
)
expect_output \
    "table substring values" \
    "$table_expected" \
    "SELECT id, SUBSTRING(v, 2, 3), SUBSTR(v FROM -4 FOR 2), MID(c, 1, 2), "\
"SUBSTRING(txt, 2), SUBSTRING(i, 2, 2), SUBSTR(d, -2), MID(y, 1, 4), "\
"SUBSTRING(dt, 1, 10) FROM t ORDER BY id;" \
    "$DATABASE"

expect_output \
    "table row envelope" \
    "3	NULL
2	🙂a" \
    "SELECT id, SUBSTR(v FROM -4 FOR 2) AS s FROM t WHERE id >= 1 ORDER BY id DESC LIMIT 2;" \
    "$DATABASE"

expect_output \
    "table null and empty branches" \
    "1				NULL	NULL
3	NULL	NULL	NULL	NULL	NULL" \
    "SELECT id, SUBSTRING(v, 0), SUBSTRING(v, 1, 0), SUBSTRING(v, 1, -1), "\
"SUBSTRING(v, NULL), SUBSTRING(v, 1, NULL) FROM t WHERE id IN (1, 3) ORDER BY id;" \
    "$DATABASE"

labels_expected=$(cat <<\EXPECTED
SUBSTRING(v, 2, 3)	s
bcd	cd
EXPECTED
)
expect_output_with_headers \
    "labels and aliases" \
    "$labels_expected" \
    "SELECT SUBSTRING(v, 2, 3), SUBSTR(v FROM -4 FOR 2) AS s FROM t WHERE id = 1;" \
    "$DATABASE"

expect_output \
    "mysql accepted deferred binary and converted positions" \
    "4142	4100	bcdef	bcdef	ab" \
    "SELECT HEX(SUBSTRING(CAST('ABC' AS BINARY), 1, 2)), "\
"HEX(SUBSTRING(X'410042', 1, 2)), SUBSTRING('abcdef', 1.5), "\
"SUBSTRING('abcdef', '2'), SUBSTRING('abcdef', 1, 1.5);" \
    "$DATABASE"

expect_output \
    "substring predicate count" \
    "1" \
    "SELECT COUNT(*) FROM t WHERE SUBSTRING(v, 1, 1) = 'a';" \
    "$DATABASE"

substring_predicate_expected=$(cat <<\EXPECTED
1,2
2,4
1,2,3
3
EXPECTED
)
expect_output \
    "substring predicates" \
    "$substring_predicate_expected" \
    "CREATE TABLE predicates(id INT, v VARCHAR(20), n VARCHAR(20)); "\
"INSERT INTO predicates VALUES (1, 'a', 'x'), (2, 'AB', NULL), (3, 'é', ''), "\
"(4, NULL, NULL); "\
"SELECT GROUP_CONCAT(id ORDER BY id) FROM predicates WHERE SUBSTRING(v, 1, 1) = 'a'; "\
"SELECT GROUP_CONCAT(id ORDER BY id) FROM predicates WHERE SUBSTR(n FROM 1 FOR 1) <=> NULL; "\
"SELECT GROUP_CONCAT(id ORDER BY id) FROM predicates WHERE MID(v, 1, 1) IS NOT NULL; "\
"SELECT GROUP_CONCAT(id ORDER BY id) FROM predicates "\
"WHERE SUBSTRING(v, 1, 1) <> 'a' AND id < 4;" \
    "$DATABASE"

expect_error \
    "substring rejects zero arguments as syntax" \
    1064 \
    42000 \
    "near ')' at line 1" \
    "SELECT SUBSTRING();" \
    "$DATABASE"

expect_error \
    "substring rejects one argument as syntax" \
    1064 \
    42000 \
    "near ')' at line 1" \
    "SELECT SUBSTRING('a');" \
    "$DATABASE"

expect_error \
    "mid rejects too many arguments as syntax" \
    1064 \
    42000 \
    "near ', 4)' at line 1" \
    "SELECT MID('abcdef', 1, 2, 4);" \
    "$DATABASE"

printf '%s\n' "mysql_baseline_substring_functions_expectations: ok"
