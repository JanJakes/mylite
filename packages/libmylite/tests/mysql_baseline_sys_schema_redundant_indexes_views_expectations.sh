#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_schema_redundant_indexes_$$"

fail() {
    printf '%s\n' "mysql_baseline_sys_schema_redundant_indexes_views_expectations: $1" >&2
    exit 1
}

run_mysql() {
    sql=$1
    shift
    printf '%s\n' "$sql" \
        | docker exec -i "$MYSQL_CONTAINER" mysql --protocol=TCP -h127.0.0.1 -uroot \
            --batch --raw --skip-column-names "$@"
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

expect_contains() {
    label=$1
    needle=$2
    sql=$3
    shift 3

    output=$(run_mysql "$sql" "$@")
    case "$output" in
        *"$needle"*) ;;
        *) fail "$label: expected output to contain [$needle], got [$output]" ;;
    esac
}

cleanup() {
    run_mysql "DROP DATABASE IF EXISTS ${DATABASE};" >/dev/null 2>&1 || true
}

trap cleanup EXIT

version=$(run_mysql "SELECT VERSION();")
case "$version" in
    8.4.9*) ;;
    *) fail "expected MySQL 8.4.9 runtime, got [$version]" ;;
esac

cleanup
run_mysql "CREATE DATABASE ${DATABASE};" >/dev/null

redundant_show_columns_expected=$(
    printf '%b' \
'table_schema\tvarchar(64)\tNO\t\tNULL\t
table_name\tvarchar(64)\tNO\t\tNULL\t
redundant_index_name\tvarchar(64)\tYES\t\tNULL\t
redundant_index_columns\ttext\tYES\t\tNULL\t
redundant_index_non_unique\tint\tYES\t\tNULL\t
dominant_index_name\tvarchar(64)\tYES\t\tNULL\t
dominant_index_columns\ttext\tYES\t\tNULL\t
dominant_index_non_unique\tint\tYES\t\tNULL\t
subpart_exists\tint\tNO\t\t0\t
sql_drop_index\tvarchar(223)\tYES\t\tNULL\t'
)

flattened_show_columns_expected=$(
    printf '%b' \
'table_schema\tvarchar(64)\tNO\t\tNULL\t
table_name\tvarchar(64)\tNO\t\tNULL\t
index_name\tvarchar(64)\tYES\t\tNULL\t
non_unique\tint\tYES\t\tNULL\t
subpart_exists\tbigint\tYES\t\tNULL\t
index_columns\ttext\tYES\t\tNULL\t'
)

expect_output \
    "sys.schema_redundant_indexes SHOW COLUMNS" \
    "$redundant_show_columns_expected" \
    "SHOW COLUMNS FROM sys.schema_redundant_indexes;"

expect_output \
    "sys.x schema_flattened_keys SHOW COLUMNS" \
    "$flattened_show_columns_expected" \
    "SHOW COLUMNS FROM sys.\`x\$schema_flattened_keys\`;"

columns_expected=$(
    printf '%b' \
'schema_redundant_indexes\ttable_schema\t1\tNULL\tNO\tvarchar\t64\t192\tNULL\tNULL\tutf8mb3\tutf8mb3_bin\tvarchar(64)\t\t\tselect,insert,update,references\t
schema_redundant_indexes\ttable_name\t2\tNULL\tNO\tvarchar\t64\t192\tNULL\tNULL\tutf8mb3\tutf8mb3_bin\tvarchar(64)\t\t\tselect,insert,update,references\t
schema_redundant_indexes\tredundant_index_name\t3\tNULL\tYES\tvarchar\t64\t192\tNULL\tNULL\tutf8mb3\tutf8mb3_tolower_ci\tvarchar(64)\t\t\tselect,insert,update,references\t
schema_redundant_indexes\tredundant_index_columns\t4\tNULL\tYES\ttext\t65535\t65535\tNULL\tNULL\tutf8mb3\tutf8mb3_tolower_ci\ttext\t\t\tselect,insert,update,references\t
schema_redundant_indexes\tredundant_index_non_unique\t5\tNULL\tYES\tint\tNULL\tNULL\t10\t0\tNULL\tNULL\tint\t\t\tselect,insert,update,references\t
schema_redundant_indexes\tdominant_index_name\t6\tNULL\tYES\tvarchar\t64\t192\tNULL\tNULL\tutf8mb3\tutf8mb3_tolower_ci\tvarchar(64)\t\t\tselect,insert,update,references\t
schema_redundant_indexes\tdominant_index_columns\t7\tNULL\tYES\ttext\t65535\t65535\tNULL\tNULL\tutf8mb3\tutf8mb3_tolower_ci\ttext\t\t\tselect,insert,update,references\t
schema_redundant_indexes\tdominant_index_non_unique\t8\tNULL\tYES\tint\tNULL\tNULL\t10\t0\tNULL\tNULL\tint\t\t\tselect,insert,update,references\t
schema_redundant_indexes\tsubpart_exists\t9\t0\tNO\tint\tNULL\tNULL\t10\t0\tNULL\tNULL\tint\t\t\tselect,insert,update,references\t
schema_redundant_indexes\tsql_drop_index\t10\tNULL\tYES\tvarchar\t223\t669\tNULL\tNULL\tutf8mb3\tutf8mb3_tolower_ci\tvarchar(223)\t\t\tselect,insert,update,references\t
x$schema_flattened_keys\ttable_schema\t1\tNULL\tNO\tvarchar\t64\t192\tNULL\tNULL\tutf8mb3\tutf8mb3_bin\tvarchar(64)\t\t\tselect,insert,update,references\t
x$schema_flattened_keys\ttable_name\t2\tNULL\tNO\tvarchar\t64\t192\tNULL\tNULL\tutf8mb3\tutf8mb3_bin\tvarchar(64)\t\t\tselect,insert,update,references\t
x$schema_flattened_keys\tindex_name\t3\tNULL\tYES\tvarchar\t64\t192\tNULL\tNULL\tutf8mb3\tutf8mb3_tolower_ci\tvarchar(64)\t\t\tselect,insert,update,references\t
x$schema_flattened_keys\tnon_unique\t4\tNULL\tYES\tint\tNULL\tNULL\t10\t0\tNULL\tNULL\tint\t\t\tselect,insert,update,references\t
x$schema_flattened_keys\tsubpart_exists\t5\tNULL\tYES\tbigint\tNULL\tNULL\t19\t0\tNULL\tNULL\tbigint\t\t\tselect,insert,update,references\t
x$schema_flattened_keys\tindex_columns\t6\tNULL\tYES\ttext\t65535\t65535\tNULL\tNULL\tutf8mb3\tutf8mb3_tolower_ci\ttext\t\t\tselect,insert,update,references\t'
)

expect_output \
    "sys redundant indexes INFORMATION_SCHEMA.COLUMNS" \
    "$columns_expected" \
    "SELECT TABLE_NAME, COLUMN_NAME, ORDINAL_POSITION, COLUMN_DEFAULT, IS_NULLABLE, DATA_TYPE,
            CHARACTER_MAXIMUM_LENGTH, CHARACTER_OCTET_LENGTH, NUMERIC_PRECISION,
            NUMERIC_SCALE, CHARACTER_SET_NAME, COLLATION_NAME, COLUMN_TYPE,
            COLUMN_KEY, EXTRA, PRIVILEGES, COLUMN_COMMENT
       FROM INFORMATION_SCHEMA.COLUMNS
      WHERE TABLE_SCHEMA = 'sys'
        AND TABLE_NAME IN ('schema_redundant_indexes', 'x\$schema_flattened_keys')
      ORDER BY TABLE_NAME, ORDINAL_POSITION;"

expect_output \
    "sys redundant indexes TABLES rows" \
    "$(printf '%b' 'sys\tschema_redundant_indexes\tVIEW\tNULL\tNULL\tNULL\tVIEW\nsys\tx$schema_flattened_keys\tVIEW\tNULL\tNULL\tNULL\tVIEW')" \
    "SELECT TABLE_SCHEMA, TABLE_NAME, TABLE_TYPE, ENGINE, TABLE_ROWS, DATA_LENGTH,
            TABLE_COMMENT
       FROM INFORMATION_SCHEMA.TABLES
      WHERE TABLE_SCHEMA = 'sys'
        AND TABLE_NAME IN ('schema_redundant_indexes', 'x\$schema_flattened_keys')
      ORDER BY TABLE_NAME;"

expect_output \
    "sys redundant indexes VIEWS rows" \
    "$(printf '%b' 'sys\tschema_redundant_indexes\tNONE\tNO\tmysql.sys@localhost\tINVOKER\nsys\tx$schema_flattened_keys\tNONE\tNO\tmysql.sys@localhost\tINVOKER')" \
    "SELECT TABLE_SCHEMA, TABLE_NAME, CHECK_OPTION, IS_UPDATABLE, DEFINER, SECURITY_TYPE
       FROM INFORMATION_SCHEMA.VIEWS
      WHERE TABLE_SCHEMA = 'sys'
        AND TABLE_NAME IN ('schema_redundant_indexes', 'x\$schema_flattened_keys')
      ORDER BY TABLE_NAME;"

expect_output \
    "sys redundant indexes dependency metadata" \
    "$(printf '%b' 'sys\tschema_redundant_indexes\tsys\tx$schema_flattened_keys\nsys\tx$schema_flattened_keys\tinformation_schema\tSTATISTICS\n0')" \
    "SELECT VIEW_SCHEMA, VIEW_NAME, TABLE_SCHEMA, TABLE_NAME
       FROM INFORMATION_SCHEMA.VIEW_TABLE_USAGE
      WHERE VIEW_SCHEMA = 'sys'
        AND VIEW_NAME IN ('schema_redundant_indexes', 'x\$schema_flattened_keys')
      ORDER BY VIEW_NAME;
     SELECT COUNT(*)
       FROM INFORMATION_SCHEMA.VIEW_ROUTINE_USAGE
      WHERE TABLE_SCHEMA = 'sys'
        AND TABLE_NAME IN ('schema_redundant_indexes', 'x\$schema_flattened_keys');"

expect_output \
    "sys redundant indexes empty index and constraints" \
    "$(printf '%b' '0\t0\t0\t0')" \
    "SELECT
        (SELECT COUNT(*) FROM INFORMATION_SCHEMA.STATISTICS
          WHERE TABLE_SCHEMA = 'sys'
            AND TABLE_NAME IN ('schema_redundant_indexes', 'x\$schema_flattened_keys')),
        (SELECT COUNT(*) FROM INFORMATION_SCHEMA.TABLE_CONSTRAINTS
          WHERE TABLE_SCHEMA = 'sys'
            AND TABLE_NAME IN ('schema_redundant_indexes', 'x\$schema_flattened_keys')),
        (SELECT COUNT(*) FROM INFORMATION_SCHEMA.KEY_COLUMN_USAGE
          WHERE TABLE_SCHEMA = 'sys'
            AND TABLE_NAME IN ('schema_redundant_indexes', 'x\$schema_flattened_keys')),
        (SELECT COUNT(*) FROM INFORMATION_SCHEMA.TABLE_CONSTRAINTS_EXTENSIONS
          WHERE CONSTRAINT_SCHEMA = 'sys'
            AND TABLE_NAME IN ('schema_redundant_indexes', 'x\$schema_flattened_keys'));"

run_mysql "CREATE TABLE base_one (
               id INT NOT NULL,
               a INT,
               b INT,
               c VARCHAR(20),
               PRIMARY KEY (id),
               KEY idx_a (a),
               KEY idx_ab (a,b),
               UNIQUE KEY uq_b (b),
               KEY idx_b (b),
               KEY idx_c_prefix (c(5)),
               KEY idx_c_full (c),
               FULLTEXT KEY ft_c (c)
           );" "$DATABASE" >/dev/null

flattened_expected=$(cat <<EXPECTED
${DATABASE}	base_one	idx_a	1	0	a
${DATABASE}	base_one	idx_ab	1	0	a,b
${DATABASE}	base_one	idx_b	1	0	b
${DATABASE}	base_one	idx_c_full	1	0	c
${DATABASE}	base_one	idx_c_prefix	1	1	c
${DATABASE}	base_one	PRIMARY	0	0	id
${DATABASE}	base_one	uq_b	0	0	b
EXPECTED
)

expect_output \
    "sys.x schema_flattened_keys row values" \
    "$flattened_expected" \
    "SELECT table_schema, table_name, index_name, non_unique, subpart_exists, index_columns
       FROM sys.\`x\$schema_flattened_keys\`
      WHERE table_schema = '${DATABASE}'
      ORDER BY table_name, index_name;"

redundant_expected=$(cat <<EXPECTED
${DATABASE}	base_one	idx_a	a	1	idx_ab	a,b	1	0	ALTER TABLE \`${DATABASE}\`.\`base_one\` DROP INDEX \`idx_a\`
${DATABASE}	base_one	idx_b	b	1	uq_b	b	0	0	ALTER TABLE \`${DATABASE}\`.\`base_one\` DROP INDEX \`idx_b\`
${DATABASE}	base_one	idx_c_prefix	c	1	idx_c_full	c	1	1	ALTER TABLE \`${DATABASE}\`.\`base_one\` DROP INDEX \`idx_c_prefix\`
EXPECTED
)

expect_output \
    "sys.schema_redundant_indexes row values" \
    "$redundant_expected" \
    "SELECT table_schema, table_name, redundant_index_name, redundant_index_columns,
            redundant_index_non_unique, dominant_index_name, dominant_index_columns,
            dominant_index_non_unique, subpart_exists, sql_drop_index
       FROM sys.schema_redundant_indexes
      WHERE table_schema = '${DATABASE}'
      ORDER BY redundant_index_name, dominant_index_name;"

expect_output \
    "selected sys flattened helper read" \
    "$(printf '%b' "${DATABASE}\tbase_one\tidx_ab\ta,b")" \
    "USE sys;
     SELECT table_schema, table_name, index_name, index_columns
       FROM \`x\$schema_flattened_keys\`
      WHERE table_schema = '${DATABASE}' AND index_name = 'idx_ab';"

expect_output \
    "selected sys redundant indexes read" \
    "$(printf '%b' "${DATABASE}\tbase_one\tidx_b\tuq_b")" \
    "USE sys;
     SELECT table_schema, table_name, redundant_index_name, dominant_index_name
       FROM schema_redundant_indexes
      WHERE table_schema = '${DATABASE}' AND redundant_index_name = 'idx_b';"

expect_contains \
    "sys.schema_redundant_indexes SHOW CREATE VIEW" \
    "CREATE ALGORITHM=TEMPTABLE DEFINER=\`mysql.sys\`@\`localhost\` SQL SECURITY INVOKER VIEW \`sys\`.\`schema_redundant_indexes\`" \
    "SHOW CREATE VIEW sys.schema_redundant_indexes;"

expect_contains \
    "sys.x schema_flattened_keys SHOW CREATE TABLE" \
    "CREATE ALGORITHM=TEMPTABLE DEFINER=\`mysql.sys\`@\`localhost\` SQL SECURITY INVOKER VIEW \`x\$schema_flattened_keys\`" \
    "USE sys; SHOW CREATE TABLE \`x\$schema_flattened_keys\`;"

status=$(run_mysql "SELECT COUNT(*) FROM sys.schema_redundant_indexes WHERE table_schema = '${DATABASE}'; SELECT @@warning_count, ROW_COUNT();")
status=$(printf '%s\n' "$status" | tail -n 1)
if [ "$status" != "0	-1" ]; then
    fail "sys.schema_redundant_indexes SELECT status: expected [0	-1], got [$status]"
fi

printf '%s\n' "mysql_baseline_sys_schema_redundant_indexes_views_expectations: ok"
