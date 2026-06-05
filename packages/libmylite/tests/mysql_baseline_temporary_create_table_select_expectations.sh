#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_temp_ctas_$$"
OTHER_DATABASE="${DATABASE}_other"
MISSING_DATABASE="${DATABASE}_missing"

fail() {
    printf '%s\n' "mysql_baseline_temporary_create_table_select_expectations: $1" >&2
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
        *) fail "$label: expected error $code/$state containing [$message], got [$output]" ;;
    esac
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
run_mysql "CREATE DATABASE ${DATABASE}; CREATE DATABASE ${OTHER_DATABASE};" >/dev/null

run_mysql \
    "USE ${DATABASE};
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
                (3, 30, 300, 3000, 90);" >/dev/null

expect_output \
    "temporary ctas as select copies rows and reports status" \
    "1	0	0
3	30	300	3000" \
    "USE ${DATABASE};
     CREATE TEMPORARY TABLE tmp_as AS
         SELECT id, n, b, iu FROM src WHERE id >= 2 ORDER BY id DESC LIMIT 1;
     SELECT ROW_COUNT(), @@warning_count, @@error_count;
     SELECT id, n, b, iu FROM tmp_as;"

expect_output \
    "temporary ctas bare select copies rows" \
    "2	0	0
1	10
2	NULL" \
    "USE ${DATABASE};
     CREATE TEMPORARY TABLE tmp_no_as SELECT id, n FROM src ORDER BY id LIMIT 2;
     SELECT ROW_COUNT(), @@warning_count, @@error_count;
     SELECT id, n FROM tmp_no_as ORDER BY id;"

expect_output \
    "temporary ctas zero-row source creates empty table" \
    "0	0	0
0
id	int	NO		7	NULL" \
    "USE ${DATABASE};
     CREATE TEMPORARY TABLE tmp_zero AS SELECT id FROM src WHERE id = 999;
     SELECT ROW_COUNT(), @@warning_count, @@error_count;
     SELECT COUNT(*) FROM tmp_zero;
     SHOW COLUMNS FROM tmp_zero;"

expect_contains \
    "show create renders temporary target" \
    "CREATE TEMPORARY TABLE \`tmp_as\`" \
    "USE ${DATABASE};
     CREATE TEMPORARY TABLE tmp_as AS SELECT id, n FROM src;
     SHOW CREATE TABLE tmp_as;"

expect_output \
    "temporary target omitted from durable listings" \
    "0" \
    "USE ${DATABASE};
     CREATE TEMPORARY TABLE tmp_meta AS SELECT id FROM src;
     SELECT COUNT(*) FROM INFORMATION_SCHEMA.TABLES
      WHERE TABLE_SCHEMA = '${DATABASE}' AND TABLE_NAME = 'tmp_meta';
     SHOW TABLES LIKE 'tmp_meta';
     SHOW TABLE STATUS LIKE 'tmp_meta';"

expect_output \
    "aliases and explicit invisible source columns become visible" \
    "1	0	0
visible_inv	int	YES		9	NULL
nullable_alias	int	YES		NULL	NULL
70	10" \
    "USE ${DATABASE};
     CREATE TEMPORARY TABLE tmp_alias AS
         SELECT inv AS visible_inv, n nullable_alias FROM src WHERE id = 1;
     SELECT ROW_COUNT(), @@warning_count, @@error_count;
     SHOW COLUMNS FROM tmp_alias;
     SELECT visible_inv, nullable_alias FROM tmp_alias;"

expect_output \
    "temporary target shadows persistent target" \
    "1	0	0
1
99" \
    "USE ${DATABASE};
     CREATE TABLE shadow_target(id INT);
     INSERT INTO shadow_target VALUES (99);
     CREATE TEMPORARY TABLE shadow_target AS SELECT id FROM src WHERE id = 1;
     SELECT ROW_COUNT(), @@warning_count, @@error_count;
     SELECT id FROM shadow_target;
     DROP TEMPORARY TABLE shadow_target;
     SELECT id FROM shadow_target;"

expect_output \
    "temporary source shadows persistent source" \
    "4" \
    "USE ${DATABASE};
     CREATE TABLE shadow_src(id INT);
     INSERT INTO shadow_src VALUES (9);
     CREATE TEMPORARY TABLE shadow_src(id INT);
     INSERT INTO shadow_src VALUES (4);
     CREATE TEMPORARY TABLE shadow_copy AS SELECT id FROM shadow_src;
     SELECT id FROM shadow_copy;"

expect_output \
    "existing temporary if not exists noops with warning" \
    "0	1	0
1" \
    "USE ${DATABASE};
     CREATE TEMPORARY TABLE tmp_if AS SELECT id FROM src WHERE id = 1;
     CREATE TEMPORARY TABLE IF NOT EXISTS tmp_if AS SELECT id FROM src WHERE id = 2;
     SELECT ROW_COUNT(), @@warning_count, @@error_count;
     SELECT id FROM tmp_if;"

expect_output \
    "existing temporary if not exists noops before duplicate selected names" \
    "0	1	0
1" \
    "USE ${DATABASE};
     CREATE TEMPORARY TABLE tmp_if_duplicate AS SELECT id FROM src WHERE id = 1;
     CREATE TEMPORARY TABLE IF NOT EXISTS tmp_if_duplicate AS SELECT id, id FROM src;
     SELECT ROW_COUNT(), @@warning_count, @@error_count;
     SELECT id FROM tmp_if_duplicate;"

expect_output \
    "existing persistent target does not block temporary if not exists" \
    "1	0	0
3
0" \
    "USE ${DATABASE};
     CREATE TABLE persist_if(id INT);
     CREATE TEMPORARY TABLE IF NOT EXISTS persist_if AS SELECT id FROM src WHERE id = 3;
     SELECT ROW_COUNT(), @@warning_count, @@error_count;
     SELECT id FROM persist_if;
     DROP TEMPORARY TABLE persist_if;
     SELECT COUNT(*) FROM persist_if;"

expect_output \
    "qualified target and source without default schema" \
    "1	0	0
2" \
    "CREATE TEMPORARY TABLE ${OTHER_DATABASE}.qualified_tmp AS
         SELECT id FROM ${DATABASE}.src WHERE id = 2;
     SELECT ROW_COUNT(), @@warning_count, @@error_count;
     SELECT id FROM ${OTHER_DATABASE}.qualified_tmp;"

expect_error \
    "protected information schema target wins before source resolution" \
    1044 \
    42000 \
    "Access denied for user" \
    "USE ${DATABASE};
     CREATE TEMPORARY TABLE information_schema.tmp AS SELECT id FROM missing_source;"

expect_error \
    "missing default database for unqualified temporary target" \
    1046 \
    3D000 \
    "No database selected" \
    "CREATE TEMPORARY TABLE no_default AS SELECT id FROM ${DATABASE}.src;"

expect_error \
    "missing default database for unqualified source" \
    1046 \
    3D000 \
    "No database selected" \
    "CREATE TEMPORARY TABLE ${DATABASE}.target_source_unqualified AS SELECT id FROM src;"

expect_error \
    "source schema error before target schema error" \
    1049 \
    42000 \
    "Unknown database '${MISSING_DATABASE}'" \
    "CREATE TEMPORARY TABLE nosuch_target.dst AS SELECT id FROM ${MISSING_DATABASE}.src;"

expect_error \
    "source table error before target schema error" \
    1146 \
    42S02 \
    "Table '${DATABASE}.missing_source_precedence' doesn't exist" \
    "CREATE TEMPORARY TABLE nosuch_target.dst AS
         SELECT id FROM ${DATABASE}.missing_source_precedence;"

expect_error \
    "target schema error after valid source resolution" \
    1049 \
    42000 \
    "Unknown database 'nosuch_target'" \
    "CREATE TEMPORARY TABLE nosuch_target.dst AS SELECT id FROM ${DATABASE}.src;"

expect_error \
    "missing source before existing temporary if not exists noop" \
    1146 \
    42S02 \
    "Table '${DATABASE}.missing_source' doesn't exist" \
    "USE ${DATABASE};
     CREATE TEMPORARY TABLE existing_temp(id INT);
     CREATE TEMPORARY TABLE IF NOT EXISTS existing_temp AS SELECT id FROM missing_source;"

expect_error \
    "duplicate selected output names fail when creating target" \
    1060 \
    42S21 \
    "Duplicate column name 'id'" \
    "USE ${DATABASE};
     CREATE TEMPORARY TABLE duplicate_columns AS SELECT id, id FROM src;"

expect_output \
    "temporary ctas survives rollback upstream" \
    "0" \
    "USE ${DATABASE};
     START TRANSACTION;
     CREATE TEMPORARY TABLE tx_tmp AS SELECT id FROM src;
     ROLLBACK;
     SELECT COUNT(*) FROM tx_tmp;"
