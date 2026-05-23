#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_view_lifecycle_$$"
OTHER_DATABASE="${DATABASE}_other"
TAB=$(printf '\t')

fail() {
    printf '%s\n' "mysql_baseline_view_lifecycle_expectations: $1" >&2
    exit 1
}

run_mysql() {
    sql=$1
    shift
    printf '%s\n' "$sql" \
        | docker exec -i "$MYSQL_CONTAINER" mysql \
            --protocol=TCP \
            -h127.0.0.1 \
            -uroot \
            --batch \
            --raw \
            --skip-column-names \
            "$@"
}

run_mysql_with_headers() {
    sql=$1
    shift
    printf '%s\n' "$sql" \
        | docker exec -i "$MYSQL_CONTAINER" mysql \
            --protocol=TCP \
            -h127.0.0.1 \
            -uroot \
            --batch \
            --raw \
            "$@"
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
        *) fail "$label: expected error $code/$state containing [$message], got [$output]" ;;
    esac
}

normalize_tsv() {
    sed "s/${TAB}/|/g"
}

cleanup() {
    run_mysql "DROP DATABASE IF EXISTS ${DATABASE}; DROP DATABASE IF EXISTS ${OTHER_DATABASE};" \
        >/dev/null 2>&1 || true
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
     USE ${DATABASE};
     SET NAMES utf8mb4 COLLATE utf8mb4_0900_ai_ci;
     CREATE TABLE t(id INT NOT NULL, name VARCHAR(20) NULL);
     CREATE VIEW v AS SELECT id, name AS label FROM t;" >/dev/null

show_tables=$(run_mysql_with_headers "USE ${DATABASE}; SHOW FULL TABLES;" | normalize_tsv)
expect_value \
    "show full tables includes view" \
    "Tables_in_${DATABASE}|Table_type
t|BASE TABLE
v|VIEW" \
    "$show_tables"

show_create_header=$(
    run_mysql_with_headers "USE ${DATABASE}; SHOW CREATE VIEW v;" \
        | sed -n '1p' \
        | normalize_tsv
)
expect_value \
    "show create view header" \
    "View|Create View|character_set_client|collation_connection" \
    "$show_create_header"

show_create_row=$(run_mysql "USE ${DATABASE}; SHOW CREATE VIEW v;" | normalize_tsv)
expect_value \
    "show create view row" \
    "v|CREATE ALGORITHM=UNDEFINED DEFINER=\`root\`@\`%\` SQL SECURITY DEFINER VIEW \`v\` AS select \`t\`.\`id\` AS \`id\`,\`t\`.\`name\` AS \`label\` from \`t\`|utf8mb4|utf8mb4_0900_ai_ci" \
    "$show_create_row"

show_create_table_header=$(
    run_mysql_with_headers "USE ${DATABASE}; SHOW CREATE TABLE v;" \
        | sed -n '1p' \
        | normalize_tsv
)
expect_value \
    "show create table on view header" \
    "View|Create View|character_set_client|collation_connection" \
    "$show_create_table_header"

views_row=$(run_mysql \
    "SELECT TABLE_SCHEMA,TABLE_NAME,VIEW_DEFINITION,CHECK_OPTION,IS_UPDATABLE,DEFINER,SECURITY_TYPE,"\
"CHARACTER_SET_CLIENT,COLLATION_CONNECTION "\
"FROM INFORMATION_SCHEMA.VIEWS WHERE TABLE_SCHEMA = '${DATABASE}' AND TABLE_NAME = 'v';" \
    | normalize_tsv)
expect_value \
    "information_schema views row" \
    "${DATABASE}|v|select \`${DATABASE}\`.\`t\`.\`id\` AS \`id\`,\`${DATABASE}\`.\`t\`.\`name\` AS \`label\` from \`${DATABASE}\`.\`t\`|NONE|YES|root@%|DEFINER|utf8mb4|utf8mb4_0900_ai_ci" \
    "$views_row"

usage_row=$(run_mysql \
    "SELECT VIEW_SCHEMA,VIEW_NAME,TABLE_SCHEMA,TABLE_NAME "\
"FROM INFORMATION_SCHEMA.VIEW_TABLE_USAGE WHERE VIEW_SCHEMA = '${DATABASE}' AND VIEW_NAME = 'v';" \
    | normalize_tsv)
expect_value "view table usage row" "${DATABASE}|v|${DATABASE}|t" "$usage_row"

tables_rows=$(run_mysql \
    "SELECT TABLE_NAME,TABLE_TYPE,ENGINE,VERSION,ROW_FORMAT,TABLE_ROWS,DATA_LENGTH,"\
"AUTO_INCREMENT,TABLE_COLLATION,TABLE_COMMENT "\
"FROM INFORMATION_SCHEMA.TABLES WHERE TABLE_SCHEMA = '${DATABASE}' ORDER BY TABLE_NAME;" \
    | normalize_tsv)
expect_value \
    "information_schema tables rows" \
    "t|BASE TABLE|InnoDB|10|Dynamic|0|16384|NULL|utf8mb4_0900_ai_ci|
v|VIEW|NULL|NULL|NULL|NULL|NULL|NULL|NULL|VIEW" \
    "$tables_rows"

columns_rows=$(run_mysql \
    "SELECT TABLE_NAME,COLUMN_NAME,ORDINAL_POSITION,DATA_TYPE,COLUMN_TYPE,IS_NULLABLE,"\
"COLUMN_DEFAULT,COLUMN_KEY,EXTRA FROM INFORMATION_SCHEMA.COLUMNS "\
"WHERE TABLE_SCHEMA = '${DATABASE}' ORDER BY TABLE_NAME, ORDINAL_POSITION;" \
    | normalize_tsv)
expect_value \
    "information_schema columns rows" \
    "t|id|1|int|int|NO|NULL||
t|name|2|varchar|varchar(20)|YES|NULL||
v|id|1|int|int|NO|NULL||
v|label|2|varchar|varchar(20)|YES|NULL||" \
    "$columns_rows"

show_columns=$(run_mysql "USE ${DATABASE}; SHOW COLUMNS FROM v;" | normalize_tsv)
expect_value \
    "show columns from view" \
    "id|int|NO||NULL|
label|varchar(20)|YES||NULL|" \
    "$show_columns"

shadow_show_create=$(run_mysql \
    "USE ${DATABASE}; CREATE TEMPORARY TABLE v(id INT); SHOW CREATE VIEW v; DROP TEMPORARY TABLE v;" \
    | normalize_tsv)
expect_value \
    "show create view ignores temporary table shadow" \
    "v|CREATE ALGORITHM=UNDEFINED DEFINER=\`root\`@\`%\` SQL SECURITY DEFINER VIEW \`v\` AS select \`t\`.\`id\` AS \`id\`,\`t\`.\`name\` AS \`label\` from \`t\`|utf8mb4|utf8mb4_0900_ai_ci" \
    "$shadow_show_create"

status_row=$(run_mysql \
    "USE ${DATABASE}; SHOW TABLE STATUS LIKE 'v';" \
    | awk -F '\t' '{print $1 "|" $2 "|" $3 "|" $4 "|" $5 "|" $18}' )
expect_value "show table status view row" "v|NULL|NULL|NULL|NULL|VIEW" "$status_row"

expect_error \
    "show create view on base table" \
    1347 \
    HY000 \
    "'${DATABASE}.t' is not VIEW" \
    "USE ${DATABASE}; SHOW CREATE VIEW t;"

expect_error \
    "drop view on base table" \
    1347 \
    HY000 \
    "'${DATABASE}.t' is not VIEW" \
    "USE ${DATABASE}; DROP VIEW t;"

expect_error \
    "drop table on view" \
    1051 \
    42S02 \
    "Unknown table '${DATABASE}.v'" \
    "USE ${DATABASE}; DROP TABLE v;"

expect_error \
    "missing default schema create view" \
    1046 \
    3D000 \
    "No database selected" \
    "CREATE VIEW no_default_v AS SELECT 1 AS x;"

expect_error \
    "unknown schema create view" \
    1049 \
    42000 \
    "Unknown database 'missing_view_schema'" \
    "CREATE VIEW missing_view_schema.v AS SELECT 1 AS x;"

expect_error \
    "missing source table create view" \
    1146 \
    42S02 \
    "Table '${DATABASE}.missing_t' doesn't exist" \
    "USE ${DATABASE}; CREATE VIEW missing_source AS SELECT id FROM missing_t;"

expect_error \
    "unknown source column create view" \
    1054 \
    42S22 \
    "Unknown column 'missing_col' in 'field list'" \
    "USE ${DATABASE}; CREATE VIEW missing_column AS SELECT missing_col FROM t;"

expect_error \
    "duplicate output column create view" \
    1060 \
    42S21 \
    "Duplicate column name 'id'" \
    "USE ${DATABASE}; CREATE VIEW duplicate_column AS SELECT id, id FROM t;"

expect_error \
    "duplicate target create view" \
    1050 \
    42S01 \
    "Table 'v' already exists" \
    "USE ${DATABASE}; CREATE VIEW v AS SELECT id FROM t;"

expect_error \
    "drop missing view" \
    1051 \
    42S02 \
    "Unknown table '${DATABASE}.missing_v'" \
    "USE ${DATABASE}; DROP VIEW missing_v;"

if_exists_note=$(
    run_mysql "USE ${DATABASE}; DROP VIEW IF EXISTS missing_v; SHOW WARNINGS;" \
        | normalize_tsv
)
expect_value \
    "drop view if exists note" \
    "Note|1051|Unknown table '${DATABASE}.missing_v'" \
    "$if_exists_note"

run_mysql \
    "USE ${DATABASE}; CREATE VIEW v2 AS SELECT id FROM t; CREATE VIEW v3 AS SELECT id FROM t;" \
    >/dev/null
multi_if_exists_notes=$(run_mysql \
    "USE ${DATABASE}; DROP VIEW IF EXISTS v2, missing_v2, v3, missing_v3; SHOW WARNINGS;" \
    | normalize_tsv)
expect_value \
    "multi drop view if exists notes" \
    "Note|1051|Unknown table '${DATABASE}.missing_v2'
Note|1051|Unknown table '${DATABASE}.missing_v3'" \
    "$multi_if_exists_notes"

expect_error \
    "drop missing view is atomic" \
    1051 \
    42S02 \
    "Unknown table '${DATABASE}.missing_atomic'" \
    "USE ${DATABASE}; CREATE VIEW keep_v AS SELECT id FROM t; DROP VIEW keep_v, missing_atomic;"

keep_count=$(run_mysql \
    "SELECT COUNT(*) FROM INFORMATION_SCHEMA.VIEWS WHERE TABLE_SCHEMA = '${DATABASE}' AND TABLE_NAME = 'keep_v';")
expect_value "drop view missing atomic keeps existing view" "1" "$keep_count"

run_mysql \
    "USE ${DATABASE};
     CREATE TABLE dep_base(id INT);
     CREATE VIEW dep_v AS SELECT id FROM dep_base;
     RENAME TABLE dep_base TO dep_base_renamed;" >/dev/null
usage_after_rename=$(run_mysql \
    "SELECT VIEW_SCHEMA,VIEW_NAME,TABLE_SCHEMA,TABLE_NAME "\
"FROM INFORMATION_SCHEMA.VIEW_TABLE_USAGE WHERE VIEW_SCHEMA = '${DATABASE}' AND VIEW_NAME = 'dep_v';" \
    | normalize_tsv)
expect_value \
    "view table usage keeps source name after rename" \
    "${DATABASE}|dep_v|${DATABASE}|dep_base" \
    "$usage_after_rename"
run_mysql "USE ${DATABASE}; DROP TABLE dep_base_renamed;" >/dev/null
usage_after_drop=$(run_mysql \
    "SELECT VIEW_SCHEMA,VIEW_NAME,TABLE_SCHEMA,TABLE_NAME "\
"FROM INFORMATION_SCHEMA.VIEW_TABLE_USAGE WHERE VIEW_SCHEMA = '${DATABASE}' AND VIEW_NAME = 'dep_v';" \
    | normalize_tsv)
expect_value \
    "view table usage keeps source name after drop" \
    "${DATABASE}|dep_v|${DATABASE}|dep_base" \
    "$usage_after_drop"
run_mysql "USE ${DATABASE}; DROP VIEW dep_v;" >/dev/null

run_mysql \
    "USE ${DATABASE};
     SET NAMES utf8mb4 COLLATE utf8mb4_0900_ai_ci;
     CREATE TABLE tx_t(id INT, name VARCHAR(20));
     START TRANSACTION;
     INSERT INTO tx_t VALUES (1, 'a');
     CREATE VIEW tx_v AS SELECT id FROM tx_t;
     ROLLBACK;" >/dev/null
tx_count_after_create=$(run_mysql "USE ${DATABASE}; SELECT COUNT(*) FROM tx_t;")
expect_value "create view implicitly commits active transaction" "1" "$tx_count_after_create"
tx_show_after_create=$(run_mysql "USE ${DATABASE}; SHOW CREATE VIEW tx_v;" | normalize_tsv)
expect_value \
    "transaction-created view persists" \
    "tx_v|CREATE ALGORITHM=UNDEFINED DEFINER=\`root\`@\`%\` SQL SECURITY DEFINER VIEW \`tx_v\` AS select \`tx_t\`.\`id\` AS \`id\` from \`tx_t\`|utf8mb4|utf8mb4_0900_ai_ci" \
    "$tx_show_after_create"
run_mysql \
    "USE ${DATABASE};
     START TRANSACTION;
     INSERT INTO tx_t VALUES (2, 'b');
     DROP VIEW tx_v;
     ROLLBACK;" >/dev/null
tx_count_after_drop=$(run_mysql "USE ${DATABASE}; SELECT COUNT(*) FROM tx_t;")
expect_value "drop view implicitly commits active transaction" "2" "$tx_count_after_drop"
expect_error \
    "transaction-dropped view stays dropped" \
    1146 \
    42S02 \
    "Table '${DATABASE}.tx_v' doesn't exist" \
    "USE ${DATABASE}; SHOW CREATE VIEW tx_v;"

run_mysql "USE ${DATABASE}; DROP VIEW v; DROP VIEW keep_v;" >/dev/null
post_drop=$(run_mysql "USE ${DATABASE}; SHOW FULL TABLES;" | normalize_tsv)
expect_value \
    "post drop view table list" \
    "t|BASE TABLE
tx_t|BASE TABLE" \
    "$post_drop"

printf '%s\n' "baseline-view-lifecycle MySQL 8.4.9 expectations verified"
