#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"

fail() {
    printf '%s\n' "mysql_baseline_information_schema_st_units_of_measure_expectations: $1" >&2
    exit 1
}

run_mysql() {
    sql=$1
    shift
    printf '%s\n' "$sql" \
        | docker exec -i "$MYSQL_CONTAINER" mysql --protocol=TCP -h127.0.0.1 -uroot --batch --raw --skip-column-names "$@"
}

expect_value() {
    label=$1
    expected=$2
    actual=$3

    if [ "$actual" != "$expected" ]; then
        fail "$label: expected [$expected], got [$actual]"
    fi
}

version=$(run_mysql 'SELECT VERSION();')
case "$version" in
    8.4.9*) ;;
    *) fail "expected MySQL 8.4.9 runtime, got [$version]" ;;
esac

system_table_row=$(run_mysql \
    "SELECT TABLE_SCHEMA,TABLE_NAME,TABLE_TYPE,ENGINE,VERSION,ROW_FORMAT,TABLE_ROWS,DATA_LENGTH,AUTO_INCREMENT "\
"FROM INFORMATION_SCHEMA.TABLES WHERE TABLE_SCHEMA = 'information_schema' "\
"AND TABLE_NAME = 'ST_UNITS_OF_MEASURE';")
expect_value "st units system table row" \
    "information_schema	ST_UNITS_OF_MEASURE	SYSTEM VIEW	NULL	10	NULL	0	0	NULL" \
    "$system_table_row"

expected_columns_metadata="ST_UNITS_OF_MEASURE	UNIT_NAME	1	NULL	YES	varchar	255	1020	NULL	NULL	NULL	utf8mb4	utf8mb4_0900_ai_ci	varchar(255)	select
ST_UNITS_OF_MEASURE	UNIT_TYPE	2	NULL	YES	varchar	7	28	NULL	NULL	NULL	utf8mb4	utf8mb4_0900_ai_ci	varchar(7)	select
ST_UNITS_OF_MEASURE	CONVERSION_FACTOR	3	NULL	YES	double	NULL	NULL	22	NULL	NULL	NULL	NULL	double	select
ST_UNITS_OF_MEASURE	DESCRIPTION	4	NULL	YES	varchar	255	1020	NULL	NULL	NULL	utf8mb4	utf8mb4_0900_ai_ci	varchar(255)	select"
columns_metadata=$(run_mysql \
    "SELECT TABLE_NAME,COLUMN_NAME,ORDINAL_POSITION,COLUMN_DEFAULT,IS_NULLABLE,DATA_TYPE, "\
"CHARACTER_MAXIMUM_LENGTH,CHARACTER_OCTET_LENGTH,NUMERIC_PRECISION,NUMERIC_SCALE, "\
"DATETIME_PRECISION,CHARACTER_SET_NAME,COLLATION_NAME,COLUMN_TYPE,PRIVILEGES "\
"FROM INFORMATION_SCHEMA.COLUMNS "\
"WHERE TABLE_SCHEMA = 'information_schema' AND TABLE_NAME = 'ST_UNITS_OF_MEASURE' "\
"ORDER BY ORDINAL_POSITION;")
expect_value "st units columns metadata" "$expected_columns_metadata" "$columns_metadata"

count=$(run_mysql "SELECT COUNT(*) FROM INFORMATION_SCHEMA.ST_UNITS_OF_MEASURE;")
expect_value "st units row count" "47" "$count"

linear_count=$(run_mysql \
    "SELECT COUNT(*) FROM INFORMATION_SCHEMA.ST_UNITS_OF_MEASURE WHERE UNIT_TYPE = 'LINEAR';")
expect_value "st units linear count" "47" "$linear_count"

empty_description_count=$(run_mysql \
    "SELECT COUNT(*) FROM INFORMATION_SCHEMA.ST_UNITS_OF_MEASURE WHERE DESCRIPTION = '';")
expect_value "st units empty description count" "47" "$empty_description_count"

expected_rows="British chain (Benoit 1895 A)	LINEAR	20.1167824	[]
British chain (Benoit 1895 B)	LINEAR	20.116782494375872	[]
British chain (Sears 1922 truncated)	LINEAR	20.116756	[]
British chain (Sears 1922)	LINEAR	20.116765121552632	[]
British foot (1865)	LINEAR	0.30480083333333335	[]
British foot (1936)	LINEAR	0.3048007491	[]
British foot (Benoit 1895 A)	LINEAR	0.3047997333333333	[]
British foot (Benoit 1895 B)	LINEAR	0.30479973476327077	[]
British foot (Sears 1922 truncated)	LINEAR	0.30479933333333337	[]
British foot (Sears 1922)	LINEAR	0.3047994715386762	[]
British link (Benoit 1895 A)	LINEAR	0.201167824	[]
British link (Benoit 1895 B)	LINEAR	0.2011678249437587	[]
British link (Sears 1922 truncated)	LINEAR	0.20116756	[]
British link (Sears 1922)	LINEAR	0.2011676512155263	[]
British yard (Benoit 1895 A)	LINEAR	0.9143992	[]
British yard (Benoit 1895 B)	LINEAR	0.9143992042898124	[]
British yard (Sears 1922 truncated)	LINEAR	0.914398	[]
British yard (Sears 1922)	LINEAR	0.9143984146160288	[]
centimetre	LINEAR	0.01	[]
chain	LINEAR	20.1168	[]
Clarke's chain	LINEAR	20.1166195164	[]
Clarke's foot	LINEAR	0.3047972654	[]
Clarke's link	LINEAR	0.201166195164	[]
Clarke's yard	LINEAR	0.9143917962	[]
fathom	LINEAR	1.8288	[]
foot	LINEAR	0.3048	[]
German legal metre	LINEAR	1.0000135965	[]
Gold Coast foot	LINEAR	0.3047997101815088	[]
Indian foot	LINEAR	0.30479951024814694	[]
Indian foot (1937)	LINEAR	0.30479841	[]
Indian foot (1962)	LINEAR	0.3047996	[]
Indian foot (1975)	LINEAR	0.3047995	[]
Indian yard	LINEAR	0.9143985307444408	[]
Indian yard (1937)	LINEAR	0.91439523	[]
Indian yard (1962)	LINEAR	0.9143988	[]
Indian yard (1975)	LINEAR	0.9143985	[]
kilometre	LINEAR	1000	[]
link	LINEAR	0.201168	[]
metre	LINEAR	1	[]
millimetre	LINEAR	0.001	[]
nautical mile	LINEAR	1852	[]
Statute mile	LINEAR	1609.344	[]
US survey chain	LINEAR	20.11684023368047	[]
US survey foot	LINEAR	0.30480060960121924	[]
US survey link	LINEAR	0.2011684023368047	[]
US survey mile	LINEAR	1609.3472186944375	[]
yard	LINEAR	0.9144	[]"
rows=$(run_mysql \
    "SELECT UNIT_NAME,UNIT_TYPE,CONVERSION_FACTOR,CONCAT('[', DESCRIPTION, ']') "\
"FROM INFORMATION_SCHEMA.ST_UNITS_OF_MEASURE ORDER BY UNIT_NAME;")
expect_value "st units exact rows" "$expected_rows" "$rows"

alias_row=$(run_mysql \
    "SELECT u.CONVERSION_FACTOR FROM INFORMATION_SCHEMA.ST_UNITS_OF_MEASURE AS u "\
"WHERE u.UNIT_NAME = 'metre';")
expect_value "st units alias row" "1" "$alias_row"

status=$(run_mysql \
    "SELECT COUNT(*) FROM INFORMATION_SCHEMA.ST_UNITS_OF_MEASURE; "\
"SELECT @@warning_count, ROW_COUNT();" | tail -n 1)
expect_value "st units warning and row count status" "0	-1" "$status"

printf '%s\n' "mysql_baseline_information_schema_st_units_of_measure_expectations: ok"
