#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"

fail() {
    printf '%s\n' "mysql_baseline_show_plugins_metadata_expectations: $1" >&2
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

expect_value() {
    label=$1
    expected=$2
    actual=$3

    if [ "$actual" != "$expected" ]; then
        fail "$label: expected [$expected], got [$actual]"
    fi
}

expect_error() {
    label=$1
    code=$2
    state=$3
    message=$4
    sql=$5

    set +e
    output=$(run_mysql "$sql" 2>&1)
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

version=$(run_mysql 'SELECT VERSION();')
case "$version" in
    8.4.9*) ;;
    *) fail "expected MySQL 8.4.9 runtime, got [$version]" ;;
esac

show_plugins=$(run_mysql_with_headers 'SHOW PLUGINS;')
show_headers=$(printf '%s\n' "$show_plugins" | sed -n '1p')
innodb_show_row=$(printf '%s\n' "$show_plugins" | awk -F '\t' '$1 == "InnoDB" {print}')
expect_value "SHOW PLUGINS headers" "Name	Status	Type	Library	License" "$show_headers"
expect_value "SHOW PLUGINS InnoDB row" "InnoDB	ACTIVE	STORAGE ENGINE	NULL	GPL" "$innodb_show_row"

show_status=$(run_mysql 'SHOW PLUGINS; SELECT @@warning_count, ROW_COUNT();' | tail -n 1)
expect_value "SHOW PLUGINS diagnostics" "0	-1" "$show_status"

plugins_row=$(
    run_mysql \
        "SELECT PLUGIN_NAME,PLUGIN_VERSION,PLUGIN_STATUS,PLUGIN_TYPE,PLUGIN_TYPE_VERSION,PLUGIN_LIBRARY,"\
"PLUGIN_LIBRARY_VERSION,PLUGIN_AUTHOR,PLUGIN_DESCRIPTION,PLUGIN_LICENSE,LOAD_OPTION "\
"FROM INFORMATION_SCHEMA.PLUGINS WHERE PLUGIN_NAME = 'InnoDB';"
)
expect_value \
    "INFORMATION_SCHEMA.PLUGINS InnoDB row" \
    "InnoDB	8.4	ACTIVE	STORAGE ENGINE	80409.0	NULL	NULL	Oracle Corporation	Supports transactions, row-level locking, and foreign keys	GPL	FORCE" \
    "$plugins_row"

predicate_rows=$(
    run_mysql \
        "SELECT PLUGIN_NAME FROM INFORMATION_SCHEMA.PLUGINS "\
"WHERE PLUGIN_NAME = 'innodb' AND PLUGIN_STATUS = 'active' AND PLUGIN_TYPE = 'storage engine';"
)
expect_value "INFORMATION_SCHEMA.PLUGINS case-insensitive predicates" "InnoDB" "$predicate_rows"

count_rows=$(run_mysql "SELECT COUNT(*) FROM INFORMATION_SCHEMA.PLUGINS WHERE PLUGIN_NAME = 'missing';")
expect_value "INFORMATION_SCHEMA.PLUGINS count no match" "0" "$count_rows"

ordered_limited=$(
    run_mysql \
        "SELECT PLUGIN_NAME FROM INFORMATION_SCHEMA.PLUGINS "\
"WHERE PLUGIN_TYPE = 'STORAGE ENGINE' AND PLUGIN_NAME = 'InnoDB' ORDER BY PLUGIN_NAME DESC LIMIT 1;"
)
expect_value "INFORMATION_SCHEMA.PLUGINS order limit" "InnoDB" "$ordered_limited"

tables_row=$(
    run_mysql \
        "SELECT TABLE_SCHEMA,TABLE_NAME,TABLE_TYPE,ENGINE,VERSION,ROW_FORMAT,TABLE_ROWS,DATA_LENGTH,AUTO_INCREMENT "\
"FROM INFORMATION_SCHEMA.TABLES WHERE TABLE_SCHEMA = 'information_schema' AND TABLE_NAME = 'PLUGINS';"
)
expect_value \
    "INFORMATION_SCHEMA.TABLES PLUGINS row" \
    "information_schema	PLUGINS	SYSTEM VIEW	NULL	10	NULL	0	0	NULL" \
    "$tables_row"

columns_rows=$(
    run_mysql \
        "SELECT COLUMN_NAME,ORDINAL_POSITION,COLUMN_DEFAULT,IS_NULLABLE,DATA_TYPE,"\
"CHARACTER_MAXIMUM_LENGTH,CHARACTER_OCTET_LENGTH,NUMERIC_PRECISION,NUMERIC_SCALE,"\
"DATETIME_PRECISION,CHARACTER_SET_NAME,COLLATION_NAME,COLUMN_TYPE,COLUMN_KEY,EXTRA,"\
"PRIVILEGES,COLUMN_COMMENT,GENERATION_EXPRESSION,SRS_ID "\
"FROM INFORMATION_SCHEMA.COLUMNS WHERE TABLE_SCHEMA = 'information_schema' AND TABLE_NAME = 'PLUGINS' "\
"ORDER BY ORDINAL_POSITION;"
)
expected_columns=$(cat <<'EXPECTED'
PLUGIN_NAME	1		NO	varchar	21	64	NULL	NULL	NULL	utf8mb3	utf8mb3_general_ci	varchar(64)			select			NULL
PLUGIN_VERSION	2		NO	varchar	6	20	NULL	NULL	NULL	utf8mb3	utf8mb3_general_ci	varchar(20)			select			NULL
PLUGIN_STATUS	3		NO	varchar	3	10	NULL	NULL	NULL	utf8mb3	utf8mb3_general_ci	varchar(10)			select			NULL
PLUGIN_TYPE	4		NO	varchar	26	80	NULL	NULL	NULL	utf8mb3	utf8mb3_general_ci	varchar(80)			select			NULL
PLUGIN_TYPE_VERSION	5		NO	varchar	6	20	NULL	NULL	NULL	utf8mb3	utf8mb3_general_ci	varchar(20)			select			NULL
PLUGIN_LIBRARY	6		YES	varchar	21	64	NULL	NULL	NULL	utf8mb3	utf8mb3_general_ci	varchar(64)			select			NULL
PLUGIN_LIBRARY_VERSION	7		YES	varchar	6	20	NULL	NULL	NULL	utf8mb3	utf8mb3_general_ci	varchar(20)			select			NULL
PLUGIN_AUTHOR	8		YES	varchar	21	64	NULL	NULL	NULL	utf8mb3	utf8mb3_general_ci	varchar(64)			select			NULL
PLUGIN_DESCRIPTION	9		YES	varchar	21845	65535	NULL	NULL	NULL	utf8mb3	utf8mb3_general_ci	varchar(65535)			select			NULL
PLUGIN_LICENSE	10		YES	varchar	26	80	NULL	NULL	NULL	utf8mb3	utf8mb3_general_ci	varchar(80)			select			NULL
LOAD_OPTION	11		NO	varchar	21	64	NULL	NULL	NULL	utf8mb3	utf8mb3_general_ci	varchar(64)			select			NULL
EXPECTED
)
expect_value "INFORMATION_SCHEMA.COLUMNS PLUGINS rows" "$expected_columns" "$columns_rows"

expect_error \
    "SHOW PLUGINS LIKE syntax" \
    1064 \
    42000 \
    "near 'LIKE 'InnoDB''" \
    "SHOW PLUGINS LIKE 'InnoDB';"

expect_error \
    "SHOW PLUGINS WHERE syntax" \
    1064 \
    42000 \
    "near 'WHERE Name = 'InnoDB''" \
    "SHOW PLUGINS WHERE Name = 'InnoDB';"

expect_error \
    "SHOW FULL PLUGINS syntax" \
    1064 \
    42000 \
    "near 'PLUGINS'" \
    "SHOW FULL PLUGINS;"

expect_error \
    "SHOW PLUGINS FROM syntax" \
    1064 \
    42000 \
    "near 'FROM mysql'" \
    "SHOW PLUGINS FROM mysql;"

printf '%s\n' "mysql_baseline_show_plugins_metadata_expectations: ok"
