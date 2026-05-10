#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_create_table_select_$$"
OTHER_DATABASE="${DATABASE}_other"

fail() {
    printf '%s\n' "mysql_baseline_create_table_select_lifecycle_expectations: $1" >&2
    exit 1
}

run_mysql() {
    sql=$1
    shift
    printf '%s\n' "$sql" \
        | docker exec -i "$MYSQL_CONTAINER" mysql --protocol=TCP -h127.0.0.1 -uroot --batch --raw --skip-column-names "$@"
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
     CREATE TABLE src(
         id INT NOT NULL DEFAULT 7,
         n INTEGER NULL DEFAULT NULL,
         b BIGINT NOT NULL,
         iu INT UNSIGNED NOT NULL DEFAULT 6,
         inv INT NULL DEFAULT 9 INVISIBLE
     ) ENGINE=InnoDB;
     INSERT INTO src(id, n, b, iu, inv)
         VALUES (1, 10, 100, 1000, 70),
                (2, NULL, 200, 2000, 80),
                (3, 30, 300, 3000, 90);
     CREATE TABLE dst AS
         SELECT id, n, b, iu FROM src WHERE id >= 2 ORDER BY id DESC LIMIT 1;
     SELECT ROW_COUNT(), @@warning_count, @@error_count;
     SHOW COLUMNS FROM dst;
     SELECT id, n, b, iu FROM dst;
     CREATE TABLE dst_no_as SELECT id, n FROM src WHERE id = 1;
     SELECT ROW_COUNT(), @@warning_count, @@error_count;
     SELECT id, n FROM dst_no_as;" \
    >"/tmp/${DATABASE}_basic.out"

expect_value \
    "basic ctas status" \
    "1	0	0" \
    "$(sed -n '1p' "/tmp/${DATABASE}_basic.out")"
expected_basic_columns=$(
    printf '%s\t%s\t%s\t%s\t%s\t%s\n' \
        "id" "int" "NO" "" "7" "" \
        "n" "int" "YES" "" "NULL" "" \
        "b" "bigint" "NO" "" "NULL" "" \
        "iu" "int unsigned" "NO" "" "6" ""
)
expect_value \
    "basic inferred columns" \
    "$expected_basic_columns" \
    "$(sed -n '2,5p' "/tmp/${DATABASE}_basic.out")"
expect_value \
    "basic copied row" \
    "3	30	300	3000" \
    "$(sed -n '6p' "/tmp/${DATABASE}_basic.out")"
expect_value \
    "optional as status" \
    "1	0	0" \
    "$(sed -n '7p' "/tmp/${DATABASE}_basic.out")"
expect_value \
    "optional as copied row" \
    "1	10" \
    "$(sed -n '8p' "/tmp/${DATABASE}_basic.out")"

zero_status=$(
    run_mysql \
        "USE ${DATABASE};
         CREATE TABLE empty_dst AS SELECT id, n FROM src WHERE id = 999;
         SELECT ROW_COUNT(), @@warning_count, @@error_count;
         SELECT COUNT(*) FROM empty_dst;
         SHOW COLUMNS FROM empty_dst;"
)
expected_zero=$(
    printf '%s\n' \
        "0	0	0" \
        "0"
    printf '%s\t%s\t%s\t%s\t%s\t%s\n' \
        "id" "int" "NO" "" "7" "" \
        "n" "int" "YES" "" "NULL" ""
)
expect_value \
    "zero-row source creates empty target with columns" \
    "$expected_zero" \
    "$(printf '%s\n' "$zero_status" | tail -n 4)"

invisible_status=$(
    run_mysql \
        "USE ${DATABASE};
         CREATE TABLE visible_from_invisible AS SELECT inv FROM src WHERE id = 1;
         SELECT ROW_COUNT(), @@warning_count, @@error_count;
         SHOW COLUMNS FROM visible_from_invisible;
         SELECT inv FROM visible_from_invisible;"
)
expected_invisible=$(
    printf '%s\n' "1	0	0"
    printf '%s\t%s\t%s\t%s\t%s\t%s\n' \
        "inv" "int" "YES" "" "9" ""
    printf '%s\n' "70"
)
expect_value \
    "explicit invisible source becomes visible target column" \
    "$expected_invisible" \
    "$(printf '%s\n' "$invisible_status" | tail -n 3)"

alias_status=$(
    run_mysql \
        "USE ${DATABASE};
         CREATE TABLE alias_dst AS
             SELECT id AS alias_id, n nullable_alias FROM src WHERE id = 1;
         SELECT ROW_COUNT(), @@warning_count, @@error_count;
         SHOW COLUMNS FROM alias_dst;
         SELECT alias_id, nullable_alias FROM alias_dst;"
)
expected_alias=$(
    printf '%s\n' "1	0	0"
    printf '%s\t%s\t%s\t%s\t%s\t%s\n' \
        "alias_id" "int" "NO" "" "7" "" \
        "nullable_alias" "int" "YES" "" "NULL" ""
    printf '%s\n' "1	10"
)
expect_value \
    "selected aliases name target columns" \
    "$expected_alias" \
    "$(printf '%s\n' "$alias_status" | tail -n 4)"

qualified_status=$(
    run_mysql \
        "CREATE TABLE ${OTHER_DATABASE}.qualified_dst AS
             SELECT id, n FROM ${DATABASE}.src WHERE id = 2;
         SELECT ROW_COUNT(), @@warning_count, @@error_count;
         SELECT id, n FROM ${OTHER_DATABASE}.qualified_dst;"
)
expect_value \
    "qualified target and source without default database" \
    "1	0	0
2	NULL" \
    "$(printf '%s\n' "$qualified_status" | tail -n 2)"

if_not_exists_status=$(
    run_mysql \
        "USE ${DATABASE};
         CREATE TABLE existing(id INT);
         CREATE TABLE IF NOT EXISTS existing AS SELECT id FROM src;
         SELECT ROW_COUNT(), @@warning_count, @@error_count;
         SELECT COUNT(*) FROM existing;"
)
expect_value \
    "existing target if not exists status and no row copy" \
    "0	1	0
0" \
    "$(printf '%s\n' "$if_not_exists_status" | tail -n 2)"

expect_error \
    "missing default database for target" \
    1046 \
    3D000 \
    "No database selected" \
    "CREATE TABLE no_default_target AS SELECT id FROM ${DATABASE}.src;"

expect_error \
    "missing default database for source" \
    1046 \
    3D000 \
    "No database selected" \
    "CREATE TABLE ${DATABASE}.target_source_unqualified AS SELECT id FROM src;"

expect_error \
    "unknown target schema" \
    1049 \
    42000 \
    "Unknown database 'nosuch_schema_${DATABASE}'" \
    "CREATE TABLE nosuch_schema_${DATABASE}.dst AS SELECT id FROM ${DATABASE}.src;"

expect_error \
    "unknown source schema" \
    1049 \
    42000 \
    "Unknown database 'nosuch_schema_${DATABASE}'" \
    "CREATE TABLE ${DATABASE}.dst_unknown_source_schema AS SELECT id FROM nosuch_schema_${DATABASE}.src;"

expect_error \
    "source schema error before target schema error" \
    1049 \
    42000 \
    "Unknown database 'nosuch_source_schema_${DATABASE}'" \
    "CREATE TABLE nosuch_target_schema_${DATABASE}.dst AS SELECT id FROM nosuch_source_schema_${DATABASE}.src;"

expect_error \
    "source table error before target schema error" \
    1146 \
    42S02 \
    "Table '${DATABASE}.missing_source_precedence' doesn't exist" \
    "CREATE TABLE nosuch_target_schema_${DATABASE}.dst AS SELECT id FROM ${DATABASE}.missing_source_precedence;"

expect_error \
    "unknown source table" \
    1146 \
    42S02 \
    "Table '${DATABASE}.missing' doesn't exist" \
    "CREATE TABLE ${DATABASE}.dst_missing_source AS SELECT id FROM ${DATABASE}.missing;"

expect_error \
    "existing target without if not exists" \
    1050 \
    42S01 \
    "Table 'existing' already exists" \
    "USE ${DATABASE}; CREATE TABLE existing AS SELECT id FROM src;"

expect_error \
    "missing source before if not exists target noop" \
    1146 \
    42S02 \
    "Table '${DATABASE}.missing_source' doesn't exist" \
    "USE ${DATABASE}; CREATE TABLE IF NOT EXISTS existing AS SELECT id FROM missing_source;"

expect_error \
    "duplicate selected output name" \
    1060 \
    42S21 \
    "Duplicate column name 'id'" \
    "USE ${DATABASE}; CREATE TABLE duplicate_output AS SELECT id, id FROM src;"

expect_error \
    "unknown projection column" \
    1054 \
    42S22 \
    "Unknown column 'missing' in 'field list'" \
    "USE ${DATABASE}; CREATE TABLE bad_projection AS SELECT missing FROM src;"

expect_error \
    "unknown where column" \
    1054 \
    42S22 \
    "Unknown column 'missing' in 'where clause'" \
    "USE ${DATABASE}; CREATE TABLE bad_where AS SELECT id FROM src WHERE missing = 1;"

expect_error \
    "unknown order column" \
    1054 \
    42S22 \
    "Unknown column 'missing' in 'order clause'" \
    "USE ${DATABASE}; CREATE TABLE bad_order AS SELECT id FROM src ORDER BY missing;"

rm -f "/tmp/${DATABASE}_basic.out"
