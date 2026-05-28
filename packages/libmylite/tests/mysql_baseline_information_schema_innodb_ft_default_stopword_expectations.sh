#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"

fail() {
    printf '%s\n' "mysql_baseline_information_schema_innodb_ft_default_stopword_expectations: $1" >&2
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

count=$(run_mysql "SELECT COUNT(*) FROM INFORMATION_SCHEMA.INNODB_FT_DEFAULT_STOPWORD;")
expect_value "default innodb ft default stopword count" "36" "$count"

case_count=$(run_mysql "SELECT COUNT(*) FROM INFORMATION_SCHEMA.innodb_ft_default_stopword;")
expect_value "case-insensitive table name count" "36" "$case_count"

status=$(run_mysql \
    "SELECT * FROM INFORMATION_SCHEMA.INNODB_FT_DEFAULT_STOPWORD; "\
"SELECT @@warning_count, ROW_COUNT();" | tail -n 1)
expect_value "innodb ft default stopword status" "0	-1" "$status"

expected_stopwords="a
about
an
are
as
at
be
by
com
de
en
for
from
how
i
in
is
it
la
of
on
or
that
the
this
to
was
what
when
where
who
will
with
und
the
www"
stopwords=$(run_mysql "SELECT value FROM INFORMATION_SCHEMA.INNODB_FT_DEFAULT_STOPWORD;")
expect_value "ordered innodb ft default stopwords" "$expected_stopwords" "$stopwords"

alias_rows=$(run_mysql \
    "SELECT s.value FROM INFORMATION_SCHEMA.INNODB_FT_DEFAULT_STOPWORD AS s "\
"WHERE s.value IN ('a', 'www') ORDER BY s.value;")
expect_value "innodb ft default stopword alias observation" "a
www" "$alias_rows"

duplicate_count=$(run_mysql \
    "SELECT COUNT(*) FROM INFORMATION_SCHEMA.INNODB_FT_DEFAULT_STOPWORD WHERE value = 'the';")
expect_value "duplicate the stopword count" "2" "$duplicate_count"

system_table_row=$(run_mysql \
    "SELECT TABLE_SCHEMA,TABLE_NAME,TABLE_TYPE,ENGINE,VERSION,ROW_FORMAT,TABLE_ROWS,DATA_LENGTH,AUTO_INCREMENT "\
"FROM INFORMATION_SCHEMA.TABLES WHERE TABLE_SCHEMA = 'information_schema' "\
"AND TABLE_NAME = 'INNODB_FT_DEFAULT_STOPWORD';")
expect_value "innodb ft default stopword system table row" \
    "information_schema	INNODB_FT_DEFAULT_STOPWORD	SYSTEM VIEW	NULL	10	NULL	0	0	NULL" \
    "$system_table_row"

expected_columns_metadata="INNODB_FT_DEFAULT_STOPWORD	value	1		NO	varchar	6	18	NULL	NULL	NULL	utf8mb3	utf8mb3_general_ci	varchar(18)	select"
columns_metadata=$(run_mysql \
    "SELECT TABLE_NAME,COLUMN_NAME,ORDINAL_POSITION,COLUMN_DEFAULT,IS_NULLABLE,DATA_TYPE, "\
"CHARACTER_MAXIMUM_LENGTH,CHARACTER_OCTET_LENGTH,NUMERIC_PRECISION,NUMERIC_SCALE, "\
"DATETIME_PRECISION,CHARACTER_SET_NAME,COLLATION_NAME,COLUMN_TYPE,PRIVILEGES "\
"FROM INFORMATION_SCHEMA.COLUMNS "\
"WHERE TABLE_SCHEMA = 'information_schema' AND TABLE_NAME = 'INNODB_FT_DEFAULT_STOPWORD' "\
"ORDER BY ORDINAL_POSITION;")
expect_value "innodb ft default stopword columns metadata" "$expected_columns_metadata" "$columns_metadata"

printf '%s\n' "mysql_baseline_information_schema_innodb_ft_default_stopword_expectations: ok"
