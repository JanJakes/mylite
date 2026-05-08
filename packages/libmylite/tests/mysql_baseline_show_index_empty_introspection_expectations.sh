#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_show_index_expectations_$$"
OTHER_DATABASE="${DATABASE}_other"

fail() {
    printf '%s\n' "mysql_baseline_show_index_empty_introspection_expectations: $1" >&2
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
    run_mysql "DROP DATABASE IF EXISTS ${DATABASE}; DROP DATABASE IF EXISTS ${OTHER_DATABASE};" >/dev/null 2>&1 || true
}

trap cleanup EXIT

version=$(run_mysql 'SELECT VERSION();')
case "$version" in
    8.4.9*) ;;
    *) fail "expected MySQL 8.4.9 runtime, got [$version]" ;;
esac

cleanup

run_mysql \
    "CREATE DATABASE ${DATABASE};
     CREATE DATABASE ${OTHER_DATABASE};
     CREATE TABLE ${DATABASE}.no_keys(id INT, value BIGINT NULL);
     CREATE TABLE ${OTHER_DATABASE}.no_keys(other_id BIGINT NULL);
     CREATE TABLE ${OTHER_DATABASE}.indexed(other_id BIGINT NULL, KEY idx_other(other_id));" \
    >/dev/null

expected_headers="Table	Non_unique	Key_name	Seq_in_index	Column_name	Collation	Cardinality	Sub_part	Packed	Null	Index_type	Comment	Index_comment	Visible	Expression"

indexed_output=$(run_mysql_with_headers "SHOW INDEX FROM ${OTHER_DATABASE}.indexed;")
expect_value "show index header shape" "$expected_headers" "$(printf '%s\n' "$indexed_output" | sed -n '1p')"

expect_empty_show_index() {
    label=$1
    sql=$2
    shift 2

    output=$(run_mysql "$sql" "$@")
    expect_value "$label rows" "" "$output"
}

expect_empty_show_index "show index from table" "USE ${DATABASE}; SHOW INDEX FROM no_keys;"
status=$(run_mysql "USE ${DATABASE}; SHOW INDEX FROM no_keys; SELECT @@warning_count, ROW_COUNT();" | tail -n 1)
expect_value "show index status" "0	-1" "$status"

expect_empty_show_index "show index in table" "USE ${DATABASE}; SHOW INDEX IN no_keys;"
expect_empty_show_index "show indexes from table" "USE ${DATABASE}; SHOW INDEXES FROM no_keys;"
expect_empty_show_index "show indexes in table" "USE ${DATABASE}; SHOW INDEXES IN no_keys;"
expect_empty_show_index "show keys from table" "USE ${DATABASE}; SHOW KEYS FROM no_keys;"
expect_empty_show_index "show keys in table" "USE ${DATABASE}; SHOW KEYS IN no_keys;"
expect_empty_show_index "schema-qualified show index" "SHOW INDEX FROM ${DATABASE}.no_keys;"
expect_empty_show_index "show index from table from schema" "SHOW INDEX FROM no_keys FROM ${DATABASE};"
expect_empty_show_index "show index from table in schema" "SHOW INDEX FROM no_keys IN ${DATABASE};"
expect_empty_show_index "show index in table from schema" "SHOW INDEX IN no_keys FROM ${DATABASE};"
expect_empty_show_index "show index in table in schema" "SHOW INDEX IN no_keys IN ${DATABASE};"
expect_empty_show_index "show indexes from table from schema" "SHOW INDEXES FROM no_keys FROM ${DATABASE};"
expect_empty_show_index "show keys in table in schema" "SHOW KEYS IN no_keys IN ${DATABASE};"

trailing_schema_output=$(run_mysql_with_headers "SHOW INDEX FROM ${DATABASE}.indexed FROM ${OTHER_DATABASE};")
expect_value \
    "trailing schema wins for show index" \
    "indexed	1	idx_other	1	other_id	A	0	NULL	NULL	YES	BTREE			YES	NULL" \
    "$(printf '%s\n' "$trailing_schema_output" | sed -n '2p')"

accepted_but_deferred=$(run_mysql_with_headers \
    "SHOW EXTENDED INDEX FROM ${OTHER_DATABASE}.indexed;
     SHOW INDEX FROM ${OTHER_DATABASE}.indexed WHERE Key_name = 'idx_other';"
)
expect_value \
    "extended show index accepted upstream" \
    "$expected_headers" \
    "$(printf '%s\n' "$accepted_but_deferred" | sed -n '1p')"
expect_value \
    "where show index accepted upstream" \
    "indexed	1	idx_other	1	other_id	A	0	NULL	NULL	YES	BTREE			YES	NULL" \
    "$(printf '%s\n' "$accepted_but_deferred" | tail -n 1)"

expect_error \
    "missing default schema show index" \
    1046 \
    3D000 \
    "No database selected" \
    "SHOW INDEX FROM no_keys;"

expect_error \
    "unknown schema qualified show index" \
    1049 \
    42000 \
    "Unknown database 'missing_schema'" \
    "SHOW INDEX FROM missing_schema.no_keys;"

expect_error \
    "unknown schema explicit show index" \
    1049 \
    42000 \
    "Unknown database 'missing_schema'" \
    "SHOW INDEX FROM no_keys FROM missing_schema;"

expect_error \
    "unknown table show index" \
    1146 \
    42S02 \
    "Table '${DATABASE}.missing_table' doesn't exist" \
    "USE ${DATABASE}; SHOW INDEX FROM missing_table;"

printf '%s\n' "baseline-show-index-empty-introspection MySQL 8.4.9 expectations verified"
