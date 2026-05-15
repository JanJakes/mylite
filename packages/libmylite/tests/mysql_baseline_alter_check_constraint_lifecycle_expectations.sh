#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_alter_check_constraint_expectations_$$"
OTHER_DATABASE="${DATABASE}_other"

fail() {
    printf '%s\n' "mysql_baseline_alter_check_constraint_lifecycle_expectations: $1" >&2
    exit 1
}

run_mysql() {
    sql=$1
    shift
    printf '%s\n' "$sql" \
        | docker exec -i "$MYSQL_CONTAINER" mysql -uroot --batch --raw --skip-column-names "$@"
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
    run_mysql "DROP DATABASE IF EXISTS ${OTHER_DATABASE};" >/dev/null 2>&1 || true
}

trap cleanup EXIT

version=$(run_mysql "SELECT VERSION();")
case "$version" in
    8.4.9*) ;;
    *) fail "expected MySQL 8.4.9 runtime, got [$version]" ;;
esac

cleanup
run_mysql "CREATE DATABASE ${DATABASE}; CREATE DATABASE ${OTHER_DATABASE};" >/dev/null

add_expected=$(cat <<EXPECTED
3	0
checked_chk_1	(\`a\` > 0)	YES
old_non	(\`a\` < 10)	NO
2
EXPECTED
)
expect_output \
    "add enforced CHECK metadata and row count" \
    "$add_expected" \
    "CREATE TABLE checked (
       id INT PRIMARY KEY,
       a INT,
       KEY a_key (a),
       CONSTRAINT old_non CHECK (a < 10) NOT ENFORCED
     );
     INSERT INTO checked VALUES (1,1),(2,2),(3,NULL);
     ALTER TABLE checked ADD CHECK (a > 0);
     SELECT ROW_COUNT(), @@warning_count;
     SELECT tc.CONSTRAINT_NAME, cc.CHECK_CLAUSE, tc.ENFORCED
       FROM INFORMATION_SCHEMA.TABLE_CONSTRAINTS tc
       JOIN INFORMATION_SCHEMA.CHECK_CONSTRAINTS cc
         ON tc.CONSTRAINT_SCHEMA = cc.CONSTRAINT_SCHEMA
        AND tc.CONSTRAINT_NAME = cc.CONSTRAINT_NAME
      WHERE tc.TABLE_SCHEMA = DATABASE() AND tc.TABLE_NAME = 'checked'
      ORDER BY tc.CONSTRAINT_NAME;
     SELECT COUNT(*) FROM INFORMATION_SCHEMA.STATISTICS
      WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = 'checked';" \
    "$DATABASE"

expect_error \
    "enforced CHECK rejects DML" \
    3819 \
    HY000 \
    "Check constraint 'checked_chk_1' is violated" \
    "INSERT INTO checked VALUES (4,-1);" \
    "$DATABASE"

toggle_expected=$(cat <<EXPECTED
0	0
1	0
3	0
0	0
EXPECTED
)
expect_output \
    "toggle enforcement row counts" \
    "$toggle_expected" \
    "ALTER TABLE checked ALTER CHECK checked_chk_1 NOT ENFORCED;
     SELECT ROW_COUNT(), @@warning_count;
     INSERT INTO checked VALUES (4,-1);
     SELECT ROW_COUNT(), @@warning_count;
     DELETE FROM checked WHERE id = 4;
     ALTER TABLE checked ALTER CHECK checked_chk_1 ENFORCED;
     SELECT ROW_COUNT(), @@warning_count;
     ALTER TABLE checked ALTER CHECK checked_chk_1 ENFORCED;
     SELECT ROW_COUNT(), @@warning_count;" \
    "$DATABASE"

expect_error \
    "failed enforce leaves descriptor unchanged" \
    3819 \
    HY000 \
    "Check constraint 'checked_chk_1' is violated" \
    "ALTER TABLE checked ALTER CHECK checked_chk_1 NOT ENFORCED;
     INSERT INTO checked VALUES (4,-1);
     ALTER TABLE checked ALTER CHECK checked_chk_1 ENFORCED;" \
    "$DATABASE"

drop_expected=$(cat <<EXPECTED
0	0
1	0
0
EXPECTED
)
expect_output \
    "drop CHECK row count and DML acceptance" \
    "$drop_expected" \
    "DELETE FROM checked WHERE id = 4;
     ALTER TABLE checked DROP CHECK checked_chk_1;
     SELECT ROW_COUNT(), @@warning_count;
     INSERT INTO checked VALUES (4,-1);
     SELECT ROW_COUNT(), @@warning_count;
     SELECT COUNT(*) FROM INFORMATION_SCHEMA.CHECK_CONSTRAINTS
      WHERE CONSTRAINT_SCHEMA = DATABASE() AND CONSTRAINT_NAME = 'checked_chk_1';" \
    "$DATABASE"

not_enforced_expected=$(cat <<EXPECTED
0	0
0	0
drop_owner_chk_1
EXPECTED
)
expect_output \
    "not enforced add/drop and generated ordinal reuse" \
    "$not_enforced_expected" \
    "CREATE TABLE drop_owner (a INT);
     ALTER TABLE drop_owner ADD CHECK (a > 0);
     ALTER TABLE drop_owner DROP CHECK drop_owner_chk_1;
     ALTER TABLE drop_owner ADD CHECK (a < 10) NOT ENFORCED;
     SELECT ROW_COUNT(), @@warning_count;
     ALTER TABLE drop_owner DROP CHECK drop_owner_chk_1;
     SELECT ROW_COUNT(), @@warning_count;
     ALTER TABLE drop_owner ADD CHECK (a IS NULL);
     SELECT CONSTRAINT_NAME FROM INFORMATION_SCHEMA.CHECK_CONSTRAINTS
      WHERE CONSTRAINT_SCHEMA = DATABASE() AND CONSTRAINT_NAME = 'drop_owner_chk_1';" \
    "$DATABASE"

expect_error \
    "generated check name duplicate in schema" \
    3822 \
    HY000 \
    "Duplicate check constraint name 'drop_owner_chk_2'" \
    "CREATE TABLE collision_owner(a INT, CONSTRAINT drop_owner_chk_2 CHECK (a > 0));
     ALTER TABLE drop_owner ADD CHECK (a <> 0);" \
    "$DATABASE"

run_mysql \
    "CREATE TABLE same_name(a INT, CONSTRAINT same_schema_name CHECK (a > 0));" \
    "$OTHER_DATABASE" >/dev/null

expect_error \
    "duplicate check name same schema" \
    3822 \
    HY000 \
    "Duplicate check constraint name 'old_non'" \
    "ALTER TABLE checked ADD CONSTRAINT old_non CHECK (a > 0);" \
    "$DATABASE"

expect_error \
    "unknown drop check" \
    3821 \
    HY000 \
    "Check constraint 'missing_check' is not found in the table" \
    "ALTER TABLE checked DROP CHECK missing_check;" \
    "$DATABASE"

expect_error \
    "unknown alter check" \
    3821 \
    HY000 \
    "Check constraint 'missing_check' is not found in the table" \
    "ALTER TABLE checked ALTER CHECK missing_check ENFORCED;" \
    "$DATABASE"

expect_error \
    "unknown alter add check column" \
    1054 \
    42S22 \
    "Unknown column 'missing' in 'check constraint bad expression'" \
    "ALTER TABLE checked ADD CONSTRAINT bad CHECK (missing > 0);" \
    "$DATABASE"

printf '%s\n' "mysql_baseline_alter_check_constraint_lifecycle_expectations: ok"
