#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_schema_defaults_$$"
OTHER_DATABASE="${DATABASE}_other"
BAD_DATABASE="${DATABASE}_bad"
INFO_DATABASE="information_schema"

fail() {
    printf '%s\n' "mysql_baseline_schema_default_charset_collation_expectations: $1" >&2
    exit 1
}

run_mysql() {
    sql=$1
    shift
    printf '%s\n' "$sql" \
        | docker exec -i "$MYSQL_CONTAINER" mysql --protocol=TCP -h127.0.0.1 -uroot --batch --raw "$@"
}

run_mysql_no_headers() {
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

expect_contains() {
    label=$1
    needle=$2
    haystack=$3

    case "$haystack" in
        *"$needle"*) ;;
        *) fail "$label: expected output containing [$needle], got [$haystack]" ;;
    esac
}

expect_error() {
    label=$1
    code=$2
    state=$3
    message=$4
    sql=$5

    set +e
    output=$(run_mysql_no_headers "$sql" 2>&1)
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
    run_mysql_no_headers \
        "DROP DATABASE IF EXISTS \`${DATABASE}\`; DROP DATABASE IF EXISTS \`${OTHER_DATABASE}\`; DROP DATABASE IF EXISTS \`${BAD_DATABASE}\`;" \
        >/dev/null 2>&1 || true
}

trap cleanup EXIT

version=$(run_mysql_no_headers 'SELECT VERSION();')
case "$version" in
    8.4.9*) ;;
    *) fail "expected MySQL 8.4.9 runtime, got [$version]" ;;
esac

cleanup

run_mysql_no_headers \
    "CREATE DATABASE \`${DATABASE}\` CHARACTER SET utf8mb4; \
     CREATE DATABASE \`${OTHER_DATABASE}\` COLLATE utf8mb4_unicode_ci;" \
    >/dev/null

show_default=$(run_mysql_no_headers "SHOW CREATE DATABASE \`${DATABASE}\`;")
expect_value \
    "show create default charset" \
    "${DATABASE}	CREATE DATABASE \`${DATABASE}\` /*!40100 DEFAULT CHARACTER SET utf8mb4 COLLATE utf8mb4_0900_ai_ci */ /*!80016 DEFAULT ENCRYPTION='N' */" \
    "$show_default"

show_collation=$(run_mysql_no_headers "SHOW CREATE SCHEMA \`${OTHER_DATABASE}\`;")
expect_value \
    "show create inferred charset" \
    "${OTHER_DATABASE}	CREATE DATABASE \`${OTHER_DATABASE}\` /*!40100 DEFAULT CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci */ /*!80016 DEFAULT ENCRYPTION='N' */" \
    "$show_collation"

schemata_values=$(run_mysql_no_headers \
    "SELECT DEFAULT_CHARACTER_SET_NAME, DEFAULT_COLLATION_NAME, DEFAULT_ENCRYPTION \
     FROM INFORMATION_SCHEMA.SCHEMATA WHERE SCHEMA_NAME = '${OTHER_DATABASE}';")
expect_value "schemata defaults" "utf8mb4	utf8mb4_unicode_ci	NO" "$schemata_values"

variable_values=$(run_mysql_no_headers \
    "SELECT @@character_set_database, @@collation_database, \
            @@global.character_set_database, @@global.collation_database;")
expect_value \
    "database variables without selection" \
    "utf8mb4	utf8mb4_0900_ai_ci	utf8mb4	utf8mb4_0900_ai_ci" \
    "$variable_values"

variable_values=$(run_mysql_no_headers \
    "USE \`${OTHER_DATABASE}\`; \
     SELECT DATABASE(), @@character_set_database, @@collation_database, \
            @@session.character_set_database, @@local.collation_database, \
            @@global.character_set_database, @@global.collation_database;")
expect_value \
    "database variables selected schema" \
    "${OTHER_DATABASE}	utf8mb4	utf8mb4_unicode_ci	utf8mb4	utf8mb4_unicode_ci	utf8mb4	utf8mb4_0900_ai_ci" \
    "$variable_values"
run_mysql_no_headers "CREATE TABLE \`${OTHER_DATABASE}\`.pre_alter_src (v VARCHAR(10));" >/dev/null

info_variable_values=$(run_mysql_no_headers \
    "USE ${INFO_DATABASE}; SELECT DATABASE(), @@character_set_database, @@collation_database;")
expect_value \
    "information_schema database variables" \
    "information_schema	utf8mb3	utf8mb3_general_ci" \
    "$info_variable_values"

alter_status=$(run_mysql_no_headers \
    "ALTER DATABASE \`${OTHER_DATABASE}\` DEFAULT COLLATE utf8mb4_0900_bin; \
     SELECT ROW_COUNT(), @@warning_count; \
     SHOW CREATE DATABASE \`${OTHER_DATABASE}\`;")
expect_contains "alter database status" "1	0" "$alter_status"
expect_contains \
    "alter database show create" \
    "CREATE DATABASE \`${OTHER_DATABASE}\` /*!40100 DEFAULT CHARACTER SET utf8mb4 COLLATE utf8mb4_0900_bin */ /*!80016 DEFAULT ENCRYPTION='N' */" \
    "$alter_status"

current_alter_values=$(run_mysql_no_headers \
    "USE \`${OTHER_DATABASE}\`; \
     ALTER DATABASE DEFAULT COLLATE utf8mb4_unicode_520_ci; \
     SELECT DATABASE(), @@character_set_database, @@collation_database;")
expect_value \
    "alter selected database" \
    "${OTHER_DATABASE}	utf8mb4	utf8mb4_unicode_520_ci" \
    "$current_alter_values"

run_mysql_no_headers \
    "CREATE TABLE \`${OTHER_DATABASE}\`.ctas_after_alter AS \
     SELECT v FROM \`${OTHER_DATABASE}\`.pre_alter_src;" \
    >/dev/null
ctas_create=$(run_mysql_no_headers "SHOW CREATE TABLE \`${OTHER_DATABASE}\`.ctas_after_alter;")
expect_contains \
    "create table select keeps expression collation" \
    "\`v\` varchar(10) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci DEFAULT NULL" \
    "$ctas_create"
expect_contains \
    "create table select inherits target schema default" \
    "ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_520_ci" \
    "$ctas_create"

run_mysql_no_headers "CREATE TABLE \`${OTHER_DATABASE}\`.inherited (v VARCHAR(10));" >/dev/null
table_create=$(run_mysql_no_headers "SHOW CREATE TABLE \`${OTHER_DATABASE}\`.inherited;")
expect_contains \
    "create table inherits schema default" \
    "ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_520_ci" \
    "$table_create"
expect_contains \
    "column inherits schema collation" \
    "\`v\` varchar(10) COLLATE utf8mb4_unicode_520_ci DEFAULT NULL" \
    "$table_create"

run_mysql_no_headers \
    "CREATE TABLE \`${OTHER_DATABASE}\`.override_table (v VARCHAR(10)) \
     DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_bin;" \
    >/dev/null
override_create=$(run_mysql_no_headers "SHOW CREATE TABLE \`${OTHER_DATABASE}\`.override_table;")
expect_contains \
    "explicit table default overrides schema" \
    "ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_bin" \
    "$override_create"

run_mysql_no_headers "DROP DATABASE IF EXISTS \`${DATABASE}\`; CREATE DATABASE \`${DATABASE}\` DEFAULT CHARSET=binary;" >/dev/null
binary_show=$(run_mysql_no_headers "SHOW CREATE DATABASE \`${DATABASE}\`;")
expect_value \
    "binary schema show create" \
    "${DATABASE}	CREATE DATABASE \`${DATABASE}\` /*!40100 DEFAULT CHARACTER SET binary */ /*!80016 DEFAULT ENCRYPTION='N' */" \
    "$binary_show"
run_mysql_no_headers "CREATE TABLE \`${DATABASE}\`.binary_inherited (v VARCHAR(10));" >/dev/null
binary_table=$(run_mysql_no_headers "SHOW CREATE TABLE \`${DATABASE}\`.binary_inherited;")
expect_contains "binary schema table inheritance" "\`v\` varbinary(10) DEFAULT NULL" "$binary_table"
expect_contains "binary schema table default" "ENGINE=InnoDB DEFAULT CHARSET=binary" "$binary_table"

run_mysql_no_headers \
    "DROP DATABASE IF EXISTS \`${DATABASE}\`; \
     CREATE DATABASE \`${DATABASE}\` DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_bin; \
     CREATE DATABASE IF NOT EXISTS \`${DATABASE}\` DEFAULT CHARSET=binary; \
     SHOW WARNINGS; \
     SHOW CREATE DATABASE \`${DATABASE}\`;" \
    >"/tmp/${DATABASE}_if_not_exists.out"
if_not_exists_output=$(cat "/tmp/${DATABASE}_if_not_exists.out")
rm -f "/tmp/${DATABASE}_if_not_exists.out"
expect_contains "if not exists note" "Note	1007	Can't create database '${DATABASE}'; database exists" "$if_not_exists_output"
expect_contains \
    "if not exists preserves defaults" \
    "CREATE DATABASE \`${DATABASE}\` /*!40100 DEFAULT CHARACTER SET utf8mb4 COLLATE utf8mb4_bin */ /*!80016 DEFAULT ENCRYPTION='N' */" \
    "$if_not_exists_output"

expect_error \
    "alter database without selection" \
    1046 \
    3D000 \
    "No database selected" \
    "ALTER DATABASE DEFAULT CHARACTER SET utf8mb4;"

expect_error \
    "alter database unknown schema" \
    3503 \
    42Y07 \
    "Database 'missing_${DATABASE}' doesn't exist" \
    "ALTER DATABASE \`missing_${DATABASE}\` DEFAULT CHARACTER SET utf8mb4;"

expect_error \
    "alter information schema access denied" \
    1044 \
    42000 \
    "Access denied" \
    "ALTER DATABASE information_schema DEFAULT COLLATE utf8mb4_bin;"

expect_error \
    "unknown charset" \
    1115 \
    42000 \
    "Unknown character set: 'nosuch_charset'" \
    "CREATE DATABASE \`${BAD_DATABASE}\` CHARACTER SET nosuch_charset;"

expect_error \
    "unknown collation" \
    1273 \
    HY000 \
    "Unknown collation: 'nosuch_collation'" \
    "CREATE DATABASE \`${BAD_DATABASE}\` COLLATE nosuch_collation;"

expect_error \
    "mismatched collation" \
    1253 \
    42000 \
    "COLLATION 'latin1_swedish_ci' is not valid for CHARACTER SET 'utf8mb4'" \
    "CREATE DATABASE \`${BAD_DATABASE}\` CHARACTER SET utf8mb4 COLLATE latin1_swedish_ci;"

expect_error \
    "conflicting charsets" \
    1302 \
    HY000 \
    "Conflicting declarations" \
    "CREATE DATABASE \`${BAD_DATABASE}\` CHARACTER SET utf8mb4 CHARACTER SET binary;"

expect_error \
    "default charset default syntax" \
    1064 \
    42000 \
    "near 'DEFAULT'" \
    "CREATE DATABASE \`${BAD_DATABASE}\` DEFAULT CHARACTER SET DEFAULT;"

expect_error \
    "default collation default syntax" \
    1064 \
    42000 \
    "near 'DEFAULT'" \
    "CREATE DATABASE \`${BAD_DATABASE}\` DEFAULT COLLATE DEFAULT;"
