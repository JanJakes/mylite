#!/usr/bin/env sh
set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
MYSQL_ARGS="--protocol=TCP -h127.0.0.1 -uroot --batch --raw --skip-column-names"
DATABASE="mylite_baseline_system_function_residuals"

run_mysql() {
    sql="$1"
    printf '%s\n' "$sql" | docker exec -i "$MYSQL_CONTAINER" mysql $MYSQL_ARGS
}

expect_mysql_error() {
    sql="$1"
    expected="$2"
    output_file="$(mktemp)"
    if printf '%s\n' "$sql" | docker exec -i "$MYSQL_CONTAINER" mysql $MYSQL_ARGS >"$output_file" 2>&1; then
        echo "expected MySQL error for: $sql" >&2
        cat "$output_file" >&2
        rm -f "$output_file"
        exit 1
    fi
    if ! grep -F "$expected" "$output_file" >/dev/null; then
        echo "unexpected MySQL error for: $sql" >&2
        cat "$output_file" >&2
        rm -f "$output_file"
        exit 1
    fi
    rm -f "$output_file"
}

cleanup() {
    run_mysql "DROP DATABASE IF EXISTS ${DATABASE};" >/dev/null 2>&1 || true
}

trap cleanup EXIT
cleanup

version=$(run_mysql "SELECT VERSION();")
case "$version" in
    8.4.9*) ;;
    *)
        echo "expected MySQL 8.4.9, got: $version" >&2
        exit 1
        ;;
esac

run_mysql "CREATE DATABASE ${DATABASE}; USE ${DATABASE}; CREATE TABLE xml_rows(id INT PRIMARY KEY, xml_root TEXT, xml_child TEXT); INSERT INTO xml_rows VALUES (1, '<a>one</a>', '<a><b>one</b><b>two</b></a>'), (2, '<a>two</a>', '<a><b>three</b><b>four</b></a>');" >/dev/null

sleep_values=$(run_mysql "SELECT SLEEP(0), SLEEP(0.001), SLEEP(' 0 '), SLEEP('abc'); SHOW WARNINGS;")
expected_sleep_values="0	0	0	0
Warning	1292	Truncated incorrect DOUBLE value: 'abc'"
if [ "$sleep_values" != "$expected_sleep_values" ]; then
    echo "unexpected SLEEP() values:" >&2
    printf '%s\n' "$sleep_values" >&2
    exit 1
fi

expect_mysql_error "SELECT SLEEP(NULL);" "ERROR 1210 (HY000)"
expect_mysql_error "SELECT SLEEP(-1);" "ERROR 1210 (HY000)"

nonstrict_sleep=$(run_mysql "SET sql_mode=''; SELECT SLEEP(NULL), SLEEP(-1); SHOW WARNINGS;")
expected_nonstrict_sleep="0	0
Warning	1210	Incorrect arguments to sleep.
Warning	1210	Incorrect arguments to sleep."
if [ "$nonstrict_sleep" != "$expected_nonstrict_sleep" ]; then
    echo "unexpected non-strict SLEEP() values:" >&2
    printf '%s\n' "$nonstrict_sleep" >&2
    exit 1
fi

name_values=$(run_mysql "SELECT NAME_CONST('answer', 42), NAME_CONST('txt', 'abc'), NAME_CONST('nil', NULL), NAME_CONST('neg', -1), NAME_CONST('dec', 1.25), NAME_CONST('hex', X'41');")
expected_name_values="42	abc	NULL	-1	1.25	A"
if [ "$name_values" != "$expected_name_values" ]; then
    echo "unexpected NAME_CONST() values:" >&2
    printf '%s\n' "$name_values" >&2
    exit 1
fi

expect_mysql_error "SELECT NAME_CONST('expr', 1 + 2);" "ERROR 1210 (HY000)"
expect_mysql_error "SELECT NAME_CONST(CONCAT('a','b'), 1);" "ERROR 1210 (HY000)"
expect_mysql_error "SELECT NAME_CONST('truth', TRUE);" "ERROR 1210 (HY000)"

load_file_values=$(run_mysql "SELECT LOAD_FILE(NULL), LOAD_FILE('/definitely/no/such/file');")
expected_load_file_values="NULL	NULL"
if [ "$load_file_values" != "$expected_load_file_values" ]; then
    echo "unexpected LOAD_FILE() values:" >&2
    printf '%s\n' "$load_file_values" >&2
    exit 1
fi

xml_values=$(run_mysql "SELECT ExtractValue('<a>one</a>', '/a'), ExtractValue('<a><b>one</b><b>two</b></a>', '/a/b'), ExtractValue('<a>one</a>', '/z'), ExtractValue(NULL, '/a'), ExtractValue('<a>one</a>', NULL), UpdateXML('<a>one</a>', '/a', '<b>two</b>'), UpdateXML('<a><b>one</b><b>two</b></a>', '/a/b', '<c>x</c>'), UpdateXML('<a>one</a>', '/z', '<b>two</b>'), UpdateXML(NULL, '/a', '<x/>'), UpdateXML('<a/>', NULL, '<x/>'), UpdateXML('<a/>', '/a', NULL);")
expected_xml_values="one	one two		NULL	NULL	<b>two</b>	<a><b>one</b><b>two</b></a>	<a>one</a>	NULL	NULL	NULL"
if [ "$xml_values" != "$expected_xml_values" ]; then
    echo "unexpected XML function values:" >&2
    printf '%s\n' "$xml_values" >&2
    exit 1
fi

xml_warning=$(run_mysql "SELECT ExtractValue('<a>', '/a'); SHOW WARNINGS;")
case "$xml_warning" in
    "NULL
Warning	1525	Incorrect XML value: "*)
        ;;
    *)
        echo "unexpected XML warning:" >&2
        printf '%s\n' "$xml_warning" >&2
        exit 1
        ;;
esac

expect_mysql_error "SELECT ExtractValue('<a>one</a>', '@@bad');" "ERROR 1105 (HY000)"

row_values=$(run_mysql "USE ${DATABASE}; SELECT id, SLEEP(0), LOAD_FILE(xml_root), ExtractValue(xml_root, '/a'), ExtractValue(xml_child, '/a/b'), UpdateXML(xml_root, '/a', '<c>x</c>'), UpdateXML(xml_child, '/a/b', '<c>x</c>'), NAME_CONST('constant', 7) FROM xml_rows ORDER BY id;")
expected_row_values="1	0	NULL	one	one two	<c>x</c>	<a><b>one</b><b>two</b></a>	7
2	0	NULL	two	three four	<c>x</c>	<a><b>three</b><b>four</b></a>	7"
if [ "$row_values" != "$expected_row_values" ]; then
    echo "unexpected row-scalar residual system function values:" >&2
    printf '%s\n' "$row_values" >&2
    exit 1
fi

expect_mysql_error "USE ${DATABASE}; SELECT ExtractValue(xml_root, xml_root) FROM xml_rows;" "Only constant XPATH queries are supported"
expect_mysql_error "USE ${DATABASE}; SELECT UpdateXML(xml_root, xml_root, '<x/>') FROM xml_rows;" "Only constant XPATH queries are supported"
expect_mysql_error "USE ${DATABASE}; SELECT ExtractValue(xml_root, '@@bad') FROM xml_rows;" "XPATH syntax error"
expect_mysql_error "USE ${DATABASE}; SELECT UpdateXML(xml_root, '@@bad', '<x/>') FROM xml_rows;" "XPATH syntax error"

echo "mysql_baseline_system_function_residuals_expectations: ok"
