#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_temp_like_$$"
OTHER_DATABASE="${DATABASE}_other"
MISSING_DATABASE="${DATABASE}_missing"

fail() {
    printf '%s\n' "mysql_baseline_temporary_table_like_expectations: $1" >&2
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

expect_not_contains() {
    label=$1
    needle=$2
    sql=$3
    shift 3

    output=$(run_mysql "$sql" "$@")
    case "$output" in
        *"$needle"*) fail "$label: expected output not to contain [$needle], got [$output]" ;;
        *) ;;
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
         name VARCHAR(10),
         KEY name_key(name(3))
     ) ENGINE=InnoDB;
     INSERT INTO src VALUES (1, 'aa'), (2, 'bb');" >/dev/null

expect_output \
    "temporary like from persistent source status and omission" \
    "0	0	0
0
0" \
    "USE ${DATABASE};
     CREATE TEMPORARY TABLE temp_clone LIKE src;
     SELECT ROW_COUNT(), @@warning_count, @@error_count;
     SELECT COUNT(*) FROM temp_clone;
     SELECT COUNT(*) FROM INFORMATION_SCHEMA.TABLES
      WHERE TABLE_SCHEMA = '${DATABASE}' AND TABLE_NAME = 'temp_clone';"

expect_output \
    "temporary like metadata from persistent source" \
    "id	int	NO		7	NULL
name	varchar(10)	YES	MUL	NULL	NULL
temp_clone	1	name_key	1	name	A	0	3	NULL	YES	BTREE			YES	NULL" \
    "USE ${DATABASE};
     CREATE TEMPORARY TABLE temp_clone LIKE src;
     SHOW COLUMNS FROM temp_clone;
     SHOW INDEX FROM temp_clone;"

expect_contains \
    "show create table renders temporary like target" \
    "CREATE TEMPORARY TABLE \`temp_clone\`" \
    "USE ${DATABASE};
     CREATE TEMPORARY TABLE temp_clone LIKE src;
     SHOW CREATE TABLE temp_clone;"

expect_output \
    "show table status omits temporary like target" \
    "" \
    "USE ${DATABASE};
     CREATE TEMPORARY TABLE temp_clone LIKE src;
     SHOW TABLE STATUS LIKE 'temp_clone';"

expect_output \
    "parenthesized temporary like is accepted" \
    "0	0	0" \
    "USE ${DATABASE};
     CREATE TEMPORARY TABLE paren_clone (LIKE src);
     SELECT ROW_COUNT(), @@warning_count, @@error_count;"

expect_output \
    "temporary source clone and persistent target from temporary source" \
    "0	0	0
0	0	0
0
0" \
    "USE ${DATABASE};
     CREATE TEMPORARY TABLE temp_src(
         id INT NOT NULL PRIMARY KEY,
         n INT DEFAULT 5,
         KEY n_key(n)
     );
     INSERT INTO temp_src VALUES (10, 20);
     CREATE TEMPORARY TABLE temp_from_temp LIKE temp_src;
     SELECT ROW_COUNT(), @@warning_count, @@error_count;
     CREATE TABLE persistent_from_temp LIKE temp_src;
     SELECT ROW_COUNT(), @@warning_count, @@error_count;
     SELECT COUNT(*) FROM temp_from_temp;
     SELECT COUNT(*) FROM persistent_from_temp;"

expect_output \
    "temporary source shadows persistent source" \
    "temp_col	varchar(8)	NO		x	NULL
temp_col	varchar(8)	NO		x	" \
    "USE ${DATABASE};
     CREATE TABLE shadow_src(persistent_col INT);
     CREATE TEMPORARY TABLE shadow_src(temp_col VARCHAR(8) NOT NULL DEFAULT 'x');
     CREATE TEMPORARY TABLE shadow_temp_clone LIKE shadow_src;
     SHOW COLUMNS FROM shadow_temp_clone;
     CREATE TABLE shadow_persistent_clone LIKE shadow_src;
     SHOW COLUMNS FROM shadow_persistent_clone;"

expect_output \
    "temporary target shadows persistent target without warning" \
    "0	0	1	0" \
    "USE ${DATABASE};
     CREATE TABLE existing_target(id INT);
     INSERT INTO existing_target VALUES (1);
     CREATE TEMPORARY TABLE existing_target LIKE src;
     SELECT ROW_COUNT(), @@warning_count,
            (SELECT COUNT(*) FROM INFORMATION_SCHEMA.TABLES
              WHERE TABLE_SCHEMA = '${DATABASE}' AND TABLE_NAME = 'existing_target'),
            (SELECT COUNT(*) FROM existing_target);"

expect_output \
    "temporary if not exists existing temporary target warning" \
    "0	1	0" \
    "USE ${DATABASE};
     CREATE TEMPORARY TABLE existing_temp(id INT);
     CREATE TEMPORARY TABLE IF NOT EXISTS existing_temp LIKE src;
     SELECT ROW_COUNT(), @@warning_count, @@error_count;"

expect_error \
    "missing source before existing temporary if not exists noop" \
    1146 \
    42S02 \
    "Table '${DATABASE}.missing_source' doesn't exist" \
    "USE ${DATABASE};
     CREATE TEMPORARY TABLE existing_temp(id INT);
     CREATE TEMPORARY TABLE IF NOT EXISTS existing_temp LIKE missing_source;"

expect_error \
    "missing default database for unqualified temporary target" \
    1046 \
    3D000 \
    "No database selected" \
    "CREATE TEMPORARY TABLE no_default LIKE ${DATABASE}.src;"

expect_error \
    "missing default database for unqualified source" \
    1046 \
    3D000 \
    "No database selected" \
    "CREATE TEMPORARY TABLE ${DATABASE}.target_source_unqualified LIKE src;"

expect_error \
    "source schema error before target schema error" \
    1049 \
    42000 \
    "Unknown database '${MISSING_DATABASE}'" \
    "CREATE TEMPORARY TABLE nosuch_target.dst LIKE ${MISSING_DATABASE}.src;"

expect_error \
    "source table error before target schema error" \
    1146 \
    42S02 \
    "Table '${DATABASE}.missing_source_precedence' doesn't exist" \
    "CREATE TEMPORARY TABLE nosuch_target.dst LIKE ${DATABASE}.missing_source_precedence;"

expect_error \
    "information schema target denied before source resolution" \
    1044 \
    42000 \
    "Access denied for user 'root'@'%' to database 'information_schema'" \
    "CREATE TEMPORARY TABLE information_schema.copy LIKE ${MISSING_DATABASE}.src;"

expect_error \
    "unknown target schema after valid source" \
    1049 \
    42000 \
    "Unknown database 'nosuch_target'" \
    "CREATE TEMPORARY TABLE nosuch_target.dst LIKE ${DATABASE}.src;"

expect_error \
    "source view is not base table" \
    1347 \
    HY000 \
    "'${DATABASE}.src_view' is not BASE TABLE" \
    "USE ${DATABASE};
     CREATE VIEW src_view AS SELECT id FROM src;
     CREATE TEMPORARY TABLE view_clone LIKE src_view;"

expect_contains \
    "mysql clones auto increment into temporary like target" \
    "AUTO_INCREMENT" \
    "USE ${DATABASE};
     CREATE TABLE ai(id INT NOT NULL AUTO_INCREMENT PRIMARY KEY, value INT) ENGINE=InnoDB;
     CREATE TEMPORARY TABLE ai_tmp LIKE ai;
     SHOW CREATE TABLE ai_tmp;"

run_mysql \
    "USE ${DATABASE};
     CREATE TABLE parent(id INT PRIMARY KEY) ENGINE=InnoDB;
     CREATE TABLE child(
         id INT PRIMARY KEY,
         parent_id INT,
         CONSTRAINT fk_child_parent FOREIGN KEY(parent_id) REFERENCES parent(id)
     ) ENGINE=InnoDB;" >/dev/null

expect_contains \
    "mysql temporary like clones foreign-key supporting key" \
    "KEY \`fk_child_parent\`" \
    "USE ${DATABASE};
     CREATE TEMPORARY TABLE child_tmp LIKE child;
     SHOW CREATE TABLE child_tmp;"

expect_not_contains \
    "mysql temporary like skips foreign key constraint" \
    "FOREIGN KEY" \
    "USE ${DATABASE};
     CREATE TEMPORARY TABLE child_tmp LIKE child;
     SHOW CREATE TABLE child_tmp;"

expect_contains \
    "mysql clones check constraints into temporary like target" \
    "CHECK" \
    "USE ${DATABASE};
     CREATE TABLE checked_source(value INT CHECK (value > 0)) ENGINE=InnoDB;
     CREATE TEMPORARY TABLE checked_tmp LIKE checked_source;
     SHOW CREATE TABLE checked_tmp;"

expect_output \
    "temporary like survives rollback upstream" \
    "0" \
    "USE ${DATABASE};
     START TRANSACTION;
     CREATE TEMPORARY TABLE tx_like LIKE src;
     INSERT INTO tx_like VALUES (4, 'cc');
     ROLLBACK;
     SELECT COUNT(*) FROM tx_like;"
