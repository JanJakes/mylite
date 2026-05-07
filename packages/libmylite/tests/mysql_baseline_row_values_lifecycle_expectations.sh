#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_row_values_expectations_$$"
OTHER_DATABASE="${DATABASE}_other"
MISSING_DATABASE="${DATABASE}_missing"

fail() {
    printf '%s\n' "mysql_baseline_row_values_lifecycle_expectations: $1" >&2
    exit 1
}

run_mysql() {
    sql=$1
    shift
    printf '%s\n' "$sql" \
        | docker exec -i "$MYSQL_CONTAINER" mysql -uroot --batch --raw --skip-column-names "$@"
}

run_mysql_with_headers() {
    sql=$1
    shift
    printf '%s\n' "$sql" \
        | docker exec -i "$MYSQL_CONTAINER" mysql -uroot --batch --raw "$@"
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
    status=$?
    set -e

    if [ "$status" -eq 0 ]; then
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
    run_mysql "DROP DATABASE IF EXISTS ${OTHER_DATABASE};" >/dev/null 2>&1 || true
    run_mysql "DROP DATABASE IF EXISTS ${MISSING_DATABASE};" >/dev/null 2>&1 || true
}

trap cleanup EXIT

version=$(run_mysql 'SELECT VERSION();')
case "$version" in
    8.4.9*) ;;
    *) fail "expected MySQL 8.4.9 runtime, got [$version]" ;;
esac

cleanup
run_mysql "CREATE DATABASE ${DATABASE}; CREATE DATABASE ${OTHER_DATABASE};" >/dev/null

expect_output \
    "default SQL mode includes strict trans tables" \
    "1" \
    "SELECT @@sql_mode LIKE '%STRICT_TRANS_TABLES%';"

expect_error \
    "insert without selected schema" \
    1046 \
    3D000 \
    "No database selected" \
    "INSERT INTO no_default_table VALUES (1);"

expect_error \
    "select without selected schema" \
    1046 \
    3D000 \
    "No database selected" \
    "SELECT * FROM no_default_table;"

expect_error \
    "qualified insert unknown schema" \
    1049 \
    42000 \
    "Unknown database '${MISSING_DATABASE}'" \
    "INSERT INTO ${MISSING_DATABASE}.missing_table VALUES (1);"

expect_error \
    "qualified select unknown schema" \
    1049 \
    42000 \
    "Unknown database '${MISSING_DATABASE}'" \
    "SELECT * FROM ${MISSING_DATABASE}.missing_table;"

run_mysql \
    "CREATE TABLE ${DATABASE}.numbers ("\
"i INT, iu INT UNSIGNED, b BIGINT, bu BIGINT UNSIGNED, n INT NULL, nn INT NOT NULL);" \
    >/dev/null

expect_output \
    "full-row insert status" \
    "1	0" \
    "INSERT INTO numbers VALUES (1, 2, 3, 4, NULL, 5); SELECT ROW_COUNT(), @@warning_count;" \
    "$DATABASE"

expected_full_row=$(cat <<'EOF'
i	iu	b	bu	n	nn
1	2	3	4	NULL	5
EOF
)
expect_output_with_headers \
    "select star after full-row insert" \
    "$expected_full_row" \
    "SELECT * FROM numbers;" \
    "$DATABASE"

expect_output \
    "explicit column order and unary plus" \
    "1	0" \
    "INSERT INTO numbers (nn, i) VALUES (6, +7); SELECT ROW_COUNT(), @@warning_count;" \
    "$DATABASE"

expected_after_explicit=$(cat <<'EOF'
1	2	3	4	NULL	5
7	NULL	NULL	NULL	NULL	6
EOF
)
expect_output \
    "omitted nullable columns become null" \
    "$expected_after_explicit" \
    "SELECT i, iu, b, bu, n, nn FROM numbers;" \
    "$DATABASE"

expect_output \
    "multi-row insert status" \
    "2	0" \
    "INSERT INTO numbers (i, nn) VALUES (8, 9), (10, 11); SELECT ROW_COUNT(), @@warning_count;" \
    "$DATABASE"

expected_projection=$(cat <<'EOF'
i	nn
1	5
7	6
8	9
10	11
EOF
)
expect_output_with_headers \
    "select projection labels and rows" \
    "$expected_projection" \
    "SELECT i, nn FROM numbers;" \
    "$DATABASE"

expect_output \
    "range boundaries insert status" \
    "4	0" \
    "INSERT INTO numbers (i, iu, b, bu, nn) VALUES "\
"(-2147483648, 0, -9223372036854775808, 0, 12), "\
"(2147483647, 4294967295, 9223372036854775807, 9223372036854775807, 13), "\
"(NULL, NULL, NULL, NULL, 14), "\
"(+1, +2, +3, +4, 15); "\
"SELECT ROW_COUNT(), @@warning_count;" \
    "$DATABASE"

expect_error \
    "too few values row one" \
    1136 \
    21S01 \
    "Column count doesn't match value count at row 1" \
    "INSERT INTO numbers VALUES (1);" \
    "$DATABASE"

expect_error \
    "too many values row one" \
    1136 \
    21S01 \
    "Column count doesn't match value count at row 1" \
    "INSERT INTO numbers (i, nn) VALUES (1, 2, 3);" \
    "$DATABASE"

expect_error \
    "multi-row value mismatch row two" \
    1136 \
    21S01 \
    "Column count doesn't match value count at row 2" \
    "INSERT INTO numbers (i, nn) VALUES (16, 17), (18);" \
    "$DATABASE"

expect_output \
    "failed multi-row value mismatch is atomic" \
    "0" \
    "SELECT COUNT(*) FROM numbers WHERE i IN (16, 18);" \
    "$DATABASE"

expect_error \
    "unknown insert column" \
    1054 \
    42S22 \
    "Unknown column 'missing' in 'field list'" \
    "INSERT INTO numbers (missing) VALUES (1);" \
    "$DATABASE"

expect_error \
    "duplicate insert column" \
    1110 \
    42000 \
    "Column 'i' specified twice" \
    "INSERT INTO numbers (i, i) VALUES (1, 2);" \
    "$DATABASE"

expect_error \
    "null into not null" \
    1048 \
    23000 \
    "Column 'nn' cannot be null" \
    "INSERT INTO numbers (nn) VALUES (NULL);" \
    "$DATABASE"

expect_error \
    "omitted not null without default" \
    1364 \
    HY000 \
    "Field 'nn' doesn't have a default value" \
    "INSERT INTO numbers (i) VALUES (20);" \
    "$DATABASE"

expect_error \
    "signed int high out of range" \
    1264 \
    22003 \
    "Out of range value for column 'i' at row 1" \
    "INSERT INTO numbers (i, nn) VALUES (2147483648, 21);" \
    "$DATABASE"

expect_error \
    "signed int low out of range" \
    1264 \
    22003 \
    "Out of range value for column 'i' at row 1" \
    "INSERT INTO numbers (i, nn) VALUES (-2147483649, 22);" \
    "$DATABASE"

expect_error \
    "unsigned int high out of range" \
    1264 \
    22003 \
    "Out of range value for column 'iu' at row 1" \
    "INSERT INTO numbers (iu, nn) VALUES (4294967296, 23);" \
    "$DATABASE"

expect_error \
    "unsigned int negative out of range" \
    1264 \
    22003 \
    "Out of range value for column 'iu' at row 1" \
    "INSERT INTO numbers (iu, nn) VALUES (-1, 24);" \
    "$DATABASE"

expect_error \
    "signed bigint high out of range" \
    1264 \
    22003 \
    "Out of range value for column 'b' at row 1" \
    "INSERT INTO numbers (b, nn) VALUES (9223372036854775808, 25);" \
    "$DATABASE"

expect_error \
    "unsigned bigint high out of range upstream" \
    1264 \
    22003 \
    "Out of range value for column 'bu' at row 1" \
    "INSERT INTO numbers (bu, nn) VALUES (18446744073709551616, 26);" \
    "$DATABASE"

expect_error \
    "unknown select column" \
    1054 \
    42S22 \
    "Unknown column 'missing' in 'field list'" \
    "SELECT missing FROM numbers;" \
    "$DATABASE"

expect_error \
    "unknown insert table" \
    1146 \
    42S02 \
    "Table '${DATABASE}.missing_table' doesn't exist" \
    "INSERT INTO missing_table VALUES (1);" \
    "$DATABASE"

expect_error \
    "unknown select table" \
    1146 \
    42S02 \
    "Table '${DATABASE}.missing_table' doesn't exist" \
    "SELECT * FROM missing_table;" \
    "$DATABASE"

expect_output \
    "schema-qualified insert status" \
    "1	0" \
    "INSERT INTO ${DATABASE}.numbers (i, nn) VALUES (27, 28); SELECT ROW_COUNT(), @@warning_count;"

run_mysql "CREATE TABLE ${DATABASE}.rename_source (id INT NOT NULL); INSERT INTO ${DATABASE}.rename_source VALUES (31);" >/dev/null
expect_output \
    "rows survive rename" \
    "31" \
    "RENAME TABLE ${DATABASE}.rename_source TO ${DATABASE}.rename_target; SELECT id FROM ${DATABASE}.rename_target;"

expect_output \
    "drop removes rows and descriptor" \
    "0" \
    "DROP TABLE ${DATABASE}.rename_target; SELECT COUNT(*) FROM INFORMATION_SCHEMA.TABLES WHERE TABLE_SCHEMA = '${DATABASE}' AND TABLE_NAME = 'rename_target';"

expect_output \
    "mysql accepts unsupported expression insert upstream" \
    "1	0	3" \
    "CREATE TABLE ${OTHER_DATABASE}.expr_source (id INT NOT NULL); "\
"INSERT INTO ${OTHER_DATABASE}.expr_source VALUES (1 + 2); "\
"SELECT ROW_COUNT(), @@warning_count, id FROM ${OTHER_DATABASE}.expr_source;"

expect_output \
    "mysql accepts table-qualified select column upstream" \
    "1" \
    "SELECT numbers.i FROM ${DATABASE}.numbers LIMIT 1;"

printf '%s\n' "baseline-row-values-lifecycle MySQL 8.4.9 expectations verified"
