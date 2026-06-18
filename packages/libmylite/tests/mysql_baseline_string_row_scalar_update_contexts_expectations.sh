#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_string_update_contexts_expectations_$$"

fail() {
    printf '%s\n' "mysql_baseline_string_row_scalar_update_contexts_expectations: $1" >&2
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
run_mysql "CREATE DATABASE ${DATABASE} DEFAULT CHARACTER SET utf8mb4 "\
"COLLATE utf8mb4_0900_ai_ci;" >/dev/null

setup_sql="CREATE TABLE t("\
"id INT, s VARCHAR(64), needle VARCHAR(32), csv VARCHAR(64), b64 VARCHAR(64), n INT, "\
"out_concat VARCHAR(128), out_concat_ws VARCHAR(128), out_field INT, "\
"out_hex VARCHAR(128), out_to_base64 VARCHAR(128), out_from_base64 VARCHAR(128), "\
"out_left VARCHAR(128), out_right VARCHAR(128), out_substring VARCHAR(128), "\
"out_lpad VARCHAR(128), out_rpad VARCHAR(128), out_repeat VARCHAR(128), "\
"out_space VARCHAR(128), out_locate INT, out_instr INT, out_replace VARCHAR(128), "\
"out_insert VARCHAR(128), out_substring_index VARCHAR(128), out_find_in_set INT, "\
"out_strcmp INT, out_regexp_like INT, out_regexp_instr INT, "\
"out_regexp_substr VARCHAR(128), out_regexp_replace VARCHAR(128), "\
"out_export_set VARCHAR(128), out_make_set VARCHAR(128)); "\
"INSERT INTO t(id, s, needle, csv, b64, n) VALUES "\
"(1, 'Alpha Beta', 'Beta', 'Alpha,Beta,Gamma', 'QWxwaGE=', 2), "\
"(2, 'One,Two,Three', 'Two', 'One,Two,Three', 'VHdv', 3), "\
"(3, NULL, NULL, NULL, NULL, NULL);"
run_mysql "$setup_sql" "$DATABASE" >/dev/null

expected=$(cat <<EXPECTED
concat	2
concat_ws	3
field	3
hex	2
to_base64	2
from_base64	2
left	2
right	2
substring	2
lpad	2
rpad	2
repeat	2
space	2
locate	2
instr	2
replace	2
insert_func	2
substring_index	2
find_in_set	2
strcmp	2
regexp_like	2
regexp_instr	2
regexp_substr	2
regexp_replace	2
export_set	2
make_set	2
1	Alpha Beta-2	Alpha Beta:Beta:2	1	416C7068612042657461	QWxwaGEgQmV0YQ==	Alpha	Al	ta	lp	..Beta	Beta..	BetaBeta	[  ]	7	7	Alpha X	AZha Beta	Alpha Beta	2	-1	0	1	Alpha	_lph_ B_t_	N,Y,N,N	b
2	One,Two,Three-3	One,Two,Three:Two:3	2	4F6E652C54776F2C5468726565	T25lLFR3byxUaHJlZQ==	Two	One	ree	ne,	...Two	Two...	TwoTwoTwo	[   ]	5	5	One,X,Three	OZTwo,Three	One,Two	2	-1	1	1	One	_n_,Tw_,Thr__	Y,Y,N,N	a,b
3	NULL		0	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL
-1	0
EXPECTED
)
expect_output \
    "string row-scalar UPDATE assignments" \
    "$expected" \
    "UPDATE t SET out_concat = CONCAT(s, '-', n); SELECT 'concat', ROW_COUNT(); "\
"UPDATE t SET out_concat_ws = CONCAT_WS(':', s, needle, n); SELECT 'concat_ws', ROW_COUNT(); "\
"UPDATE t SET out_field = FIELD(needle, 'Beta', 'Two', 'Nope'); SELECT 'field', ROW_COUNT(); "\
"UPDATE t SET out_hex = HEX(s); SELECT 'hex', ROW_COUNT(); "\
"UPDATE t SET out_to_base64 = TO_BASE64(s); SELECT 'to_base64', ROW_COUNT(); "\
"UPDATE t SET out_from_base64 = FROM_BASE64(b64); SELECT 'from_base64', ROW_COUNT(); "\
"UPDATE t SET out_left = LEFT(s, n); SELECT 'left', ROW_COUNT(); "\
"UPDATE t SET out_right = RIGHT(s, n); SELECT 'right', ROW_COUNT(); "\
"UPDATE t SET out_substring = SUBSTRING(s, 2, n); SELECT 'substring', ROW_COUNT(); "\
"UPDATE t SET out_lpad = LPAD(needle, 6, '.'); SELECT 'lpad', ROW_COUNT(); "\
"UPDATE t SET out_rpad = RPAD(needle, 6, '.'); SELECT 'rpad', ROW_COUNT(); "\
"UPDATE t SET out_repeat = REPEAT(needle, n); SELECT 'repeat', ROW_COUNT(); "\
"UPDATE t SET out_space = SPACE(n); SELECT 'space', ROW_COUNT(); "\
"UPDATE t SET out_locate = LOCATE(needle, s); SELECT 'locate', ROW_COUNT(); "\
"UPDATE t SET out_instr = INSTR(s, needle); SELECT 'instr', ROW_COUNT(); "\
"UPDATE t SET out_replace = REPLACE(s, needle, 'X'); SELECT 'replace', ROW_COUNT(); "\
"UPDATE t SET out_insert = INSERT(s, 2, n, 'Z'); SELECT 'insert_func', ROW_COUNT(); "\
"UPDATE t SET out_substring_index = SUBSTRING_INDEX(s, ',', 2); "\
"SELECT 'substring_index', ROW_COUNT(); "\
"UPDATE t SET out_find_in_set = FIND_IN_SET(needle, csv); "\
"SELECT 'find_in_set', ROW_COUNT(); "\
"UPDATE t SET out_strcmp = STRCMP(s, needle); SELECT 'strcmp', ROW_COUNT(); "\
"UPDATE t SET out_regexp_like = REGEXP_LIKE(s, '^One'); SELECT 'regexp_like', ROW_COUNT(); "\
"UPDATE t SET out_regexp_instr = REGEXP_INSTR(s, '[A-Z][a-z]+'); "\
"SELECT 'regexp_instr', ROW_COUNT(); "\
"UPDATE t SET out_regexp_substr = REGEXP_SUBSTR(s, '[A-Z][a-z]+'); "\
"SELECT 'regexp_substr', ROW_COUNT(); "\
"UPDATE t SET out_regexp_replace = REGEXP_REPLACE(s, '[aeiou]', '_'); "\
"SELECT 'regexp_replace', ROW_COUNT(); "\
"UPDATE t SET out_export_set = EXPORT_SET(n, 'Y', 'N', ',', 4); "\
"SELECT 'export_set', ROW_COUNT(); "\
"UPDATE t SET out_make_set = MAKE_SET(n, 'a', 'b', 'c'); SELECT 'make_set', ROW_COUNT(); "\
"SELECT id, out_concat, out_concat_ws, out_field, out_hex, out_to_base64, "\
"out_from_base64, out_left, out_right, out_substring, out_lpad, out_rpad, "\
"out_repeat, CONCAT('[', out_space, ']'), out_locate, out_instr, out_replace, "\
"out_insert, out_substring_index, out_find_in_set, out_strcmp, out_regexp_like, "\
"out_regexp_instr, out_regexp_substr, out_regexp_replace, out_export_set, "\
"out_make_set FROM t ORDER BY id; SELECT ROW_COUNT(), @@warning_count;" \
    "$DATABASE"

printf '%s\n' "mysql_baseline_string_row_scalar_update_contexts_expectations: ok"
