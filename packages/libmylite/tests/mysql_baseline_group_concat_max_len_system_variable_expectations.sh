#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_group_concat_max_len_$$"

fail() {
    printf '%s\n' "mysql_baseline_group_concat_max_len_system_variable_expectations: $1" >&2
    exit 1
}

run_mysql() {
    sql=$1
    shift
    printf '%s\n' "$sql" \
        | docker exec -i "$MYSQL_CONTAINER" mysql --protocol=TCP -h127.0.0.1 -uroot \
            --batch --raw --skip-column-names --default-character-set=utf8mb4 "$@"
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
run_mysql "CREATE DATABASE ${DATABASE};" >/dev/null

readback_expected=$(cat <<'EXPECTED'
default	1024	1024	1024
group_concat_max_len	1024
session8	8	1024	8	8	0
default_again	1024	0
local_plus7	7
direct9	9
direct_local10	10
user12	12
max	18446744073709551615
EXPECTED
)
expect_output \
    "readback and supported assignments" \
    "$readback_expected" \
    "SELECT 'default', @@group_concat_max_len, @@global.group_concat_max_len, "\
"@@session.group_concat_max_len; "\
"SHOW VARIABLES LIKE 'group_concat_max_len'; "\
"SET group_concat_max_len = 8; "\
"SELECT 'session8', @@group_concat_max_len, @@GLOBAL.group_concat_max_len, "\
"@@SESSION.group_concat_max_len, @@LOCAL.group_concat_max_len, @@warning_count; "\
"SET SESSION group_concat_max_len = DEFAULT; "\
"SELECT 'default_again', @@group_concat_max_len, @@warning_count; "\
"SET LOCAL group_concat_max_len = +7; "\
"SELECT 'local_plus7', @@group_concat_max_len; "\
"SET @@session.group_concat_max_len = 9; "\
"SELECT 'direct9', @@group_concat_max_len; "\
"SET @@local.group_concat_max_len = 10; "\
"SELECT 'direct_local10', @@group_concat_max_len; "\
"SET @gcm = 12; "\
"SET group_concat_max_len = @gcm; "\
"SELECT 'user12', @@group_concat_max_len; "\
"SET group_concat_max_len = 18446744073709551615; "\
"SELECT 'max', @@group_concat_max_len;" \
    "$DATABASE"

clamp_expected=$(cat <<'EXPECTED'
Warning	1292	Truncated incorrect group_concat_max_len value: '1'
clamp1	4
Warning	1292	Truncated incorrect group_concat_max_len value: '0'
clamp0	4
Warning	1292	Truncated incorrect group_concat_max_len value: '-1'
clamp_neg1	4
Warning	1292	Truncated incorrect group_concat_max_len value: '1'
clamp_true	4
Warning	1292	Truncated incorrect group_concat_max_len value: '0'
clamp_false	4
Warning	1292	Truncated incorrect group_concat_max_len value: '-2'
clamp_user_neg	4
EXPECTED
)
expect_output \
    "minimum clamp warnings" \
    "$clamp_expected" \
    "SET SESSION group_concat_max_len = 1; SHOW WARNINGS; "\
"SELECT 'clamp1', @@group_concat_max_len; "\
"SET SESSION group_concat_max_len = 0; SHOW WARNINGS; "\
"SELECT 'clamp0', @@group_concat_max_len; "\
"SET SESSION group_concat_max_len = -1; SHOW WARNINGS; "\
"SELECT 'clamp_neg1', @@group_concat_max_len; "\
"SET SESSION group_concat_max_len = TRUE; SHOW WARNINGS; "\
"SELECT 'clamp_true', @@group_concat_max_len; "\
"SET SESSION group_concat_max_len = FALSE; SHOW WARNINGS; "\
"SELECT 'clamp_false', @@group_concat_max_len; "\
"SET @gcm_neg = -2; "\
"SET SESSION group_concat_max_len = @gcm_neg; SHOW WARNINGS; "\
"SELECT 'clamp_user_neg', @@group_concat_max_len;" \
    "$DATABASE"

truncate_expected=$(cat <<'EXPECTED'
plain8	alpha|be
Warning	1260	Row 2 was cut by GROUP_CONCAT()
after_plain8	1
plain9	alpha|bet
Warning	1260	Row 2 was cut by GROUP_CONCAT()
no_trunc	alpha|beta|delta|echo
after_no_trunc	0
grouped8	1	alpha|be
grouped8	2	delta|ec
grouped8	3	<NULL>
Warning	1260	Row 2 was cut by GROUP_CONCAT()
Warning	1260	Row 4 was cut by GROUP_CONCAT()
after_grouped8	2
integer8	123456
sep_cut	a---
Warning	1260	Row 2 was cut by GROUP_CONCAT()
multibyte7	E282ACE282AC
Warning	1260	Row 1 was cut by GROUP_CONCAT()
EXPECTED
)
expect_output \
    "group concat truncation warnings" \
    "$truncate_expected" \
    "CREATE TABLE t(g INT, id INT, name VARCHAR(20), n INT) ENGINE=InnoDB; "\
"INSERT INTO t VALUES "\
"(1,1,'alpha',10),(1,2,'beta',20),(1,3,NULL,30), "\
"(2,4,'delta',40),(2,5,'echo',50),(3,6,NULL,60); "\
"CREATE TABLE short_values(id INT, s VARCHAR(20)) ENGINE=InnoDB; "\
"INSERT INTO short_values VALUES (1,'a'),(2,'b'); "\
"SET SESSION group_concat_max_len = 8; "\
"SELECT 'plain8', GROUP_CONCAT(name ORDER BY id SEPARATOR '|') FROM t; "\
"SHOW WARNINGS; "\
"SELECT 'after_plain8', @@warning_count; "\
"SET SESSION group_concat_max_len = 9; "\
"SELECT 'plain9', GROUP_CONCAT(name ORDER BY id SEPARATOR '|') FROM t; "\
"SHOW WARNINGS; "\
"SET SESSION group_concat_max_len = 1024; "\
"SELECT 'no_trunc', GROUP_CONCAT(name ORDER BY id SEPARATOR '|') FROM t; "\
"SELECT 'after_no_trunc', @@warning_count; "\
"SET SESSION group_concat_max_len = 8; "\
"SELECT 'grouped8', g, IFNULL(GROUP_CONCAT(name ORDER BY id SEPARATOR '|'), '<NULL>') "\
"FROM t GROUP BY g ORDER BY g; "\
"SHOW WARNINGS; "\
"SELECT 'after_grouped8', @@warning_count; "\
"SET SESSION group_concat_max_len = 8; "\
"SELECT 'integer8', GROUP_CONCAT(id ORDER BY id SEPARATOR '') FROM t; "\
"SHOW WARNINGS; "\
"SET SESSION group_concat_max_len = 4; "\
"SELECT 'sep_cut', GROUP_CONCAT(s ORDER BY id SEPARATOR '---') FROM short_values; "\
"SHOW WARNINGS; "\
"SET SESSION group_concat_max_len = 7; "\
"CREATE TABLE utf8_values(id INT, s VARCHAR(20)) ENGINE=InnoDB; "\
"INSERT INTO utf8_values VALUES "\
"(1, CONCAT(CONVERT(0xE282ACE282AC USING utf8mb4), CONVERT(0xE282AC USING utf8mb4))), "\
"(2, 'ab'); "\
"SELECT 'multibyte7', HEX(GROUP_CONCAT(s ORDER BY id SEPARATOR '')) FROM utf8_values; "\
"SHOW WARNINGS;" \
    "$DATABASE"

expect_error \
    "string literal assignment" \
    1232 \
    42000 \
    "Incorrect argument type to variable 'group_concat_max_len'" \
    "SET SESSION group_concat_max_len = '8';" \
    "$DATABASE"

expect_error \
    "decimal assignment" \
    1232 \
    42000 \
    "Incorrect argument type to variable 'group_concat_max_len'" \
    "SET SESSION group_concat_max_len = 1.5;" \
    "$DATABASE"

expect_error \
    "null assignment" \
    1232 \
    42000 \
    "Incorrect argument type to variable 'group_concat_max_len'" \
    "SET SESSION group_concat_max_len = NULL;" \
    "$DATABASE"

expect_error \
    "overflow assignment" \
    1232 \
    42000 \
    "Incorrect argument type to variable 'group_concat_max_len'" \
    "SET SESSION group_concat_max_len = 18446744073709551616;" \
    "$DATABASE"

expect_error \
    "string user variable assignment" \
    1232 \
    42000 \
    "Incorrect argument type to variable 'group_concat_max_len'" \
    "SET @gcm_string = '12'; SET SESSION group_concat_max_len = @gcm_string;" \
    "$DATABASE"

printf '%s\n' "mysql_baseline_group_concat_max_len_system_variable_expectations: ok"
