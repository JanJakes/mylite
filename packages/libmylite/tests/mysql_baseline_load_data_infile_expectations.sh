#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
MYSQL_SOCKET="${MYLITE_MYSQL_SOCKET:-}"
DATABASE="mylite_load_data_infile_$$"

fail() {
    printf '%s\n' "mysql_baseline_load_data_infile_expectations: $1" >&2
    exit 1
}

run_mysql() {
    sql=$1
    shift
    if [ -n "$MYSQL_SOCKET" ]; then
        printf '%s\n' "$sql" \
            | mysql --protocol=SOCKET --socket="$MYSQL_SOCKET" -uroot --batch --raw \
                --skip-column-names "$@"
    else
        printf '%s\n' "$sql" \
            | docker exec -i "$MYSQL_CONTAINER" mysql -uroot --batch --raw \
                --skip-column-names "$@"
    fi
}

write_server_file() {
    path=$1
    contents=$2

    if [ -n "$MYSQL_SOCKET" ]; then
        printf '%s' "$contents" >"$path"
    else
        printf '%s' "$contents" | docker exec -i "$MYSQL_CONTAINER" sh -c "cat > '$path'"
    fi
}

write_server_file_escaped() {
    path=$1
    contents_format=$2

    if [ -n "$MYSQL_SOCKET" ]; then
        printf '%b' "$contents_format" >"$path"
    else
        printf '%b' "$contents_format" | docker exec -i "$MYSQL_CONTAINER" sh -c "cat > '$path'"
    fi
}

remove_server_file() {
    path=$1

    if [ -n "$MYSQL_SOCKET" ]; then
        rm -f "$path"
    else
        docker exec "$MYSQL_CONTAINER" rm -f "$path" >/dev/null 2>&1 || true
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

cleanup() {
    run_mysql "DROP DATABASE IF EXISTS ${DATABASE};" >/dev/null 2>&1 || true
    for file in $SERVER_FILES; do
        remove_server_file "$file"
    done
}

version=$(run_mysql "SELECT VERSION();")
case "$version" in
    8.4.9*) ;;
    *) fail "expected MySQL 8.4.9 runtime, got [$version]" ;;
esac

secure_file_priv=$(run_mysql "SELECT IFNULL(@@secure_file_priv, '');")
case "$secure_file_priv" in
    "") load_dir="/tmp/" ;;
    */) load_dir=$secure_file_priv ;;
    *) load_dir="${secure_file_priv}/" ;;
esac

local_infile=$(run_mysql "SELECT @@local_infile;")
case "$local_infile" in
    0) ;;
    *) fail "expected @@local_infile=0 for LOCAL disabled expectation, got [$local_infile]" ;;
esac

basic_file="${load_dir}mylite_load_data_basic_$$.tsv"
columns_file="${load_dir}mylite_load_data_columns_$$.tsv"
header_file="${load_dir}mylite_load_data_header_$$.tsv"
too_few_file="${load_dir}mylite_load_data_too_few_$$.tsv"
too_many_file="${load_dir}mylite_load_data_too_many_$$.tsv"
bad_int_file="${load_dir}mylite_load_data_bad_int_$$.tsv"
null_not_null_file="${load_dir}mylite_load_data_null_not_null_$$.tsv"
empty_int_file="${load_dir}mylite_load_data_empty_int_$$.tsv"
missing_defaults_file="${load_dir}mylite_load_data_missing_defaults_$$.tsv"
empty_temporal_file="${load_dir}mylite_load_data_empty_temporal_$$.tsv"
missing_file="${load_dir}mylite_load_data_missing_$$.tsv"
SERVER_FILES="$basic_file $columns_file $header_file $too_few_file $too_many_file $bad_int_file $null_not_null_file $empty_int_file $missing_defaults_file $empty_temporal_file"

trap cleanup EXIT

cleanup
write_server_file "$basic_file" "1	alpha	\\N
2		20
3	gamma	30
"
write_server_file "$columns_file" "one	1
two	2
"
write_server_file "$header_file" "id	label
4	delta
"
write_server_file "$too_few_file" "1
"
write_server_file "$too_many_file" "1	a	extra
"
write_server_file "$bad_int_file" "1	a
x	b
"
write_server_file "$null_not_null_file" "\\N	a
"
write_server_file "$empty_int_file" "	
"
write_server_file "$missing_defaults_file" "12
"
write_server_file_escaped "$empty_temporal_file" '\t\t\t\t\n'

run_mysql "CREATE DATABASE ${DATABASE};" >/dev/null

expect_output \
    "default file shape and null handling" \
    "basic	3	0
basic_row	1	[alpha]	NULL
basic_row	2	[]	20
basic_row	3	[gamma]	30" \
    "SET sql_mode='STRICT_TRANS_TABLES,NO_ZERO_IN_DATE,NO_ZERO_DATE,ERROR_FOR_DIVISION_BY_ZERO,NO_ENGINE_SUBSTITUTION'; "\
"CREATE TABLE basic_t(id INT NOT NULL, label VARCHAR(10) NOT NULL, n INT NULL); "\
"LOAD DATA INFILE '${basic_file}' INTO TABLE basic_t; "\
"SELECT 'basic', ROW_COUNT(), @@warning_count; "\
"SELECT 'basic_row', id, CONCAT('[', label, ']'), IF(n IS NULL, 'NULL', n) "\
"FROM basic_t ORDER BY id;" \
    "$DATABASE"

expect_output \
    "column list and ignored header" \
    "columns	2	0
header	1	0
c_row	1	[one]	NULL
c_row	2	[two]	NULL
c_row	4	[delta]	NULL" \
    "CREATE TABLE c(id INT NOT NULL DEFAULT 9, label VARCHAR(10) NOT NULL DEFAULT 'd', n INT NULL); "\
"LOAD DATA INFILE '${columns_file}' INTO TABLE c (label, id); "\
"SELECT 'columns', ROW_COUNT(), @@warning_count; "\
"LOAD DATA INFILE '${header_file}' INTO TABLE c IGNORE 1 LINES (id, label); "\
"SELECT 'header', ROW_COUNT(), @@warning_count; "\
"SELECT 'c_row', id, CONCAT('[', label, ']'), IF(n IS NULL, 'NULL', n) FROM c ORDER BY id;" \
    "$DATABASE"

expect_error \
    "strict too few fields" \
    1261 \
    01000 \
    "Row 1 doesn't contain data for all columns" \
    "DROP TABLE IF EXISTS shape_t; CREATE TABLE shape_t(id INT NOT NULL, label VARCHAR(10) NOT NULL); "\
"SET sql_mode='STRICT_TRANS_TABLES'; "\
"LOAD DATA INFILE '${too_few_file}' INTO TABLE shape_t;" \
    "$DATABASE"

expect_output \
    "nonstrict too few fields" \
    "too_few	1	1
too_few_row	1	[]" \
    "DROP TABLE IF EXISTS shape_t; CREATE TABLE shape_t(id INT NOT NULL, label VARCHAR(10) NOT NULL); "\
"SET sql_mode=''; "\
"LOAD DATA INFILE '${too_few_file}' INTO TABLE shape_t; "\
"SELECT 'too_few', ROW_COUNT(), @@warning_count; "\
"SELECT 'too_few_row', id, CONCAT('[', label, ']') FROM shape_t;" \
    "$DATABASE"

expect_output \
    "nonstrict too few warning" \
    "Warning	1261	Row 1 doesn't contain data for all columns" \
    "DROP TABLE IF EXISTS shape_t; CREATE TABLE shape_t(id INT NOT NULL, label VARCHAR(10) NOT NULL); "\
"SET sql_mode=''; "\
"LOAD DATA INFILE '${too_few_file}' INTO TABLE shape_t; "\
"SHOW WARNINGS;" \
    "$DATABASE"

expect_output \
    "nonstrict missing fields use load implicit values" \
    "missing_defaults	1	4
missing_defaults_row	12	NULL	0	0	[]" \
    "DROP TABLE IF EXISTS missing_defaults; "\
"CREATE TABLE missing_defaults("\
"id INT, no_default INT NULL, explicit_default INT NULL DEFAULT 9, "\
"not_null_default INT NOT NULL DEFAULT 7, body VARCHAR(10) NOT NULL DEFAULT 'd'); "\
"SET sql_mode=''; "\
"LOAD DATA INFILE '${missing_defaults_file}' INTO TABLE missing_defaults; "\
"SELECT 'missing_defaults', ROW_COUNT(), @@warning_count; "\
"SELECT 'missing_defaults_row', id, IF(no_default IS NULL, 'NULL', no_default), "\
"explicit_default, not_null_default, CONCAT('[', body, ']') FROM missing_defaults;" \
    "$DATABASE"

expect_output \
    "nonstrict empty temporal fields" \
    "empty_temporal	1	4
empty_temporal_row	0000	0000-00-00	00:00:00	0000-00-00 00:00:00	0000-00-00 00:00:00" \
    "DROP TABLE IF EXISTS empty_temporal; "\
"CREATE TABLE empty_temporal(y YEAR, d DATE, tm TIME, dt DATETIME, ts TIMESTAMP NULL); "\
"SET sql_mode=''; "\
"LOAD DATA INFILE '${empty_temporal_file}' INTO TABLE empty_temporal; "\
"SELECT 'empty_temporal', ROW_COUNT(), @@warning_count; "\
"SELECT 'empty_temporal_row', y, d, tm, dt, ts FROM empty_temporal;" \
    "$DATABASE"

expect_error \
    "strict too many fields" \
    1262 \
    01000 \
    "Row 1 was truncated; it contained more data than there were input columns" \
    "DROP TABLE IF EXISTS shape_t; CREATE TABLE shape_t(id INT NOT NULL, label VARCHAR(10) NOT NULL); "\
"SET sql_mode='STRICT_TRANS_TABLES'; "\
"LOAD DATA INFILE '${too_many_file}' INTO TABLE shape_t;" \
    "$DATABASE"

expect_output \
    "nonstrict too many fields" \
    "too_many	1	1
too_many_row	1	[a]" \
    "DROP TABLE IF EXISTS shape_t; CREATE TABLE shape_t(id INT NOT NULL, label VARCHAR(10) NOT NULL); "\
"SET sql_mode=''; "\
"LOAD DATA INFILE '${too_many_file}' INTO TABLE shape_t; "\
"SELECT 'too_many', ROW_COUNT(), @@warning_count; "\
"SELECT 'too_many_row', id, CONCAT('[', label, ']') FROM shape_t;" \
    "$DATABASE"

expect_output \
    "nonstrict too many warning" \
    "Warning	1262	Row 1 was truncated; it contained more data than there were input columns" \
    "DROP TABLE IF EXISTS shape_t; CREATE TABLE shape_t(id INT NOT NULL, label VARCHAR(10) NOT NULL); "\
"SET sql_mode=''; "\
"LOAD DATA INFILE '${too_many_file}' INTO TABLE shape_t; "\
"SHOW WARNINGS;" \
    "$DATABASE"

expect_error \
    "strict invalid integer" \
    1366 \
    HY000 \
    "Incorrect integer value: 'x' for column 'id' at row 2" \
    "DROP TABLE IF EXISTS int_t; CREATE TABLE int_t(id INT NOT NULL, label VARCHAR(10) NOT NULL); "\
"SET sql_mode='STRICT_TRANS_TABLES'; "\
"LOAD DATA INFILE '${bad_int_file}' INTO TABLE int_t;" \
    "$DATABASE"

expect_output \
    "nonstrict empty integer" \
    "empty_int	1	1
empty_int_row	0	[]	NULL" \
    "DROP TABLE IF EXISTS int_t; CREATE TABLE int_t(id INT NOT NULL, label VARCHAR(10) NOT NULL, n INT NULL); "\
"SET sql_mode=''; "\
"LOAD DATA INFILE '${empty_int_file}' INTO TABLE int_t (id, label); "\
"SELECT 'empty_int', ROW_COUNT(), @@warning_count; "\
"SELECT 'empty_int_row', id, CONCAT('[', label, ']'), IF(n IS NULL, 'NULL', n) FROM int_t;" \
    "$DATABASE"

expect_output \
    "nonstrict empty integer warning" \
    "Warning	1366	Incorrect integer value: '' for column 'id' at row 1" \
    "DROP TABLE IF EXISTS int_t; CREATE TABLE int_t(id INT NOT NULL, label VARCHAR(10) NOT NULL, n INT NULL); "\
"SET sql_mode=''; "\
"LOAD DATA INFILE '${empty_int_file}' INTO TABLE int_t (id, label); "\
"SHOW WARNINGS;" \
    "$DATABASE"

expect_error \
    "strict null into not null" \
    1263 \
    22004 \
    "Column set to default value; NULL supplied to NOT NULL column 'id' at row 1" \
    "DROP TABLE IF EXISTS null_t; CREATE TABLE null_t(id INT NOT NULL, label VARCHAR(10) NOT NULL); "\
"SET sql_mode='STRICT_TRANS_TABLES'; "\
"LOAD DATA INFILE '${null_not_null_file}' INTO TABLE null_t;" \
    "$DATABASE"

expect_error \
    "missing file" \
    13 \
    HY000 \
    "Can't get stat of '${missing_file}'" \
    "DROP TABLE IF EXISTS diag_t; CREATE TABLE diag_t(id INT NOT NULL); "\
"LOAD DATA INFILE '${missing_file}' INTO TABLE diag_t;" \
    "$DATABASE"

expect_error \
    "unknown table" \
    1146 \
    42S02 \
    "Table '${DATABASE}.missing_t' doesn't exist" \
    "LOAD DATA INFILE '${basic_file}' INTO TABLE missing_t;" \
    "$DATABASE"

expect_error \
    "unknown schema" \
    1049 \
    42000 \
    "Unknown database 'missing_schema'" \
    "LOAD DATA INFILE '${basic_file}' INTO TABLE missing_schema.missing_t;"

expect_error \
    "unknown column" \
    1054 \
    42S22 \
    "Unknown column 'missing' in 'field list'" \
    "DROP TABLE IF EXISTS diag_t; CREATE TABLE diag_t(id INT NOT NULL); "\
"LOAD DATA INFILE '${basic_file}' INTO TABLE diag_t (missing);" \
    "$DATABASE"

expect_error \
    "no selected database" \
    1046 \
    3D000 \
    "No database selected" \
    "LOAD DATA INFILE '${basic_file}' INTO TABLE no_db_t;"

expect_error \
    "local disabled" \
    3948 \
    42000 \
    "Loading local data is disabled" \
    "DROP TABLE IF EXISTS diag_t; CREATE TABLE diag_t(id INT NOT NULL); "\
"LOAD DATA LOCAL INFILE '${basic_file}' INTO TABLE diag_t;" \
    "$DATABASE"

printf '%s\n' "mysql_baseline_load_data_infile_expectations: ok"
