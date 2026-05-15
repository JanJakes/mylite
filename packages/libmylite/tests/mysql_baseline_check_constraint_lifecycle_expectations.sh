#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_check_constraint_expectations_$$"

fail() {
    printf '%s\n' "mysql_baseline_check_constraint_lifecycle_expectations: $1" >&2
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
    run_mysql "DROP DATABASE IF EXISTS ${DATABASE}_other;" >/dev/null 2>&1 || true
}

trap cleanup EXIT

version=$(run_mysql "SELECT VERSION();")
case "$version" in
    8.4.9*) ;;
    *) fail "expected MySQL 8.4.9 runtime, got [$version]" ;;
esac

cleanup
run_mysql "CREATE DATABASE ${DATABASE}; CREATE DATABASE ${DATABASE}_other;" >/dev/null

run_mysql \
    "USE ${DATABASE};
     CREATE TABLE names_base (
       a INT CHECK (a > 0),
       b INT,
       CONSTRAINT explicit_b_positive CHECK (b > 0),
       CHECK (a < b)
     ) ENGINE=InnoDB;" >/dev/null

show_names=$(run_mysql "USE ${DATABASE}; SHOW CREATE TABLE names_base;")
case "$show_names" in
    *"CONSTRAINT \`explicit_b_positive\` CHECK ((\`b\` > 0))"* ) ;;
    *) fail "SHOW CREATE missing explicit check: [$show_names]" ;;
esac
case "$show_names" in
    *"CONSTRAINT \`names_base_chk_1\` CHECK ((\`a\` > 0))"* ) ;;
    *) fail "SHOW CREATE missing first generated check: [$show_names]" ;;
esac
case "$show_names" in
    *"CONSTRAINT \`names_base_chk_2\` CHECK ((\`a\` < \`b\`))"* ) ;;
    *) fail "SHOW CREATE missing second generated check: [$show_names]" ;;
esac

check_rows_expected=$(cat <<EXPECTED
explicit_b_positive	(\`b\` > 0)
names_base_chk_1	(\`a\` > 0)
names_base_chk_2	(\`a\` < \`b\`)
EXPECTED
)
expect_output \
    "check constraints metadata" \
    "$check_rows_expected" \
    "SELECT CONSTRAINT_NAME, CHECK_CLAUSE
       FROM INFORMATION_SCHEMA.CHECK_CONSTRAINTS
      WHERE CONSTRAINT_SCHEMA = '${DATABASE}'
      ORDER BY CONSTRAINT_NAME;" \
    "$DATABASE"

table_constraints_expected=$(cat <<EXPECTED
explicit_b_positive	CHECK	names_base	YES
names_base_chk_1	CHECK	names_base	YES
names_base_chk_2	CHECK	names_base	YES
EXPECTED
)
expect_output \
    "table constraints metadata" \
    "$table_constraints_expected" \
    "SELECT CONSTRAINT_NAME, CONSTRAINT_TYPE, TABLE_NAME, ENFORCED
       FROM INFORMATION_SCHEMA.TABLE_CONSTRAINTS
      WHERE TABLE_SCHEMA = '${DATABASE}' AND TABLE_NAME = 'names_base'
      ORDER BY CONSTRAINT_NAME;" \
    "$DATABASE"

expect_error \
    "duplicate check name same schema" \
    3822 \
    HY000 \
    "Duplicate check constraint name 'explicit_b_positive'" \
    "USE ${DATABASE};
     CREATE TABLE dup_same(a INT, CONSTRAINT explicit_b_positive CHECK (a > 0));"

run_mysql \
    "USE ${DATABASE};
     CREATE TABLE same_as_unique(a INT, UNIQUE KEY same_kind(a), CONSTRAINT same_kind CHECK (a > 0));
     CREATE TABLE parent_fk(a INT PRIMARY KEY);
     CREATE TABLE same_as_fk(a INT, CONSTRAINT fk_same CHECK (a > 0),
       CONSTRAINT fk_same FOREIGN KEY(a) REFERENCES parent_fk(a));
     USE ${DATABASE}_other;
     CREATE TABLE dup_other(a INT, CONSTRAINT explicit_b_positive CHECK (a > 0));" >/dev/null

expect_error \
    "column check references other column" \
    3813 \
    HY000 \
    "Column check constraint 'column_ref_other_chk_1' references other column" \
    "USE ${DATABASE}; CREATE TABLE column_ref_other(a INT CHECK (b > 0), b INT);"

run_mysql \
    "USE ${DATABASE};
     CREATE TABLE later_column(a INT, b INT, CHECK (a < b));" >/dev/null
later_check=$(run_mysql \
    "USE ${DATABASE};
     SELECT CHECK_CLAUSE FROM INFORMATION_SCHEMA.CHECK_CONSTRAINTS
      WHERE CONSTRAINT_SCHEMA = '${DATABASE}' AND CONSTRAINT_NAME = 'later_column_chk_1';")
expect_value "later column check accepted" "(\`a\` < \`b\`)" "$later_check"

expect_error \
    "auto increment check reference" \
    3818 \
    HY000 \
    "Check constraint 'ai_check_chk_1' cannot refer to an auto-increment column" \
    "USE ${DATABASE};
     CREATE TABLE ai_check(a INT AUTO_INCREMENT PRIMARY KEY, CHECK (a > 0));"

run_mysql \
    "USE ${DATABASE};
     CREATE TABLE enforced_t(id INT, v INT, CONSTRAINT v_positive CHECK (v > 0));
     INSERT INTO enforced_t VALUES (1, 1), (2, NULL);
     UPDATE enforced_t SET v = 2 WHERE id = 1;
     REPLACE INTO enforced_t VALUES (3, 3);" >/dev/null
expect_output \
    "valid dml and unknown passes" \
    "1:2,2:N,3:3" \
    "SELECT GROUP_CONCAT(CONCAT(id, ':', IF(v IS NULL, 'N', CAST(v AS CHAR))) ORDER BY id)
       FROM enforced_t;" \
    "$DATABASE"

expect_error \
    "insert check violation" \
    3819 \
    HY000 \
    "Check constraint 'v_positive' is violated" \
    "USE ${DATABASE}; INSERT INTO enforced_t VALUES (4, 0);"

expect_error \
    "update check violation" \
    3819 \
    HY000 \
    "Check constraint 'v_positive' is violated" \
    "USE ${DATABASE}; UPDATE enforced_t SET v = -5 WHERE id = 1;"

expect_error \
    "replace check violation" \
    3819 \
    HY000 \
    "Check constraint 'v_positive' is violated" \
    "USE ${DATABASE}; REPLACE INTO enforced_t VALUES (4, -4);"

ignore_status=$(run_mysql \
    "USE ${DATABASE};
     INSERT IGNORE INTO enforced_t VALUES (4, 4), (5, 0), (6, -1);
     SELECT ROW_COUNT(), @@warning_count;
     SELECT GROUP_CONCAT(id ORDER BY id) FROM enforced_t;")
expect_value "insert ignore check status" "1	2
1,2,3,4" "$ignore_status"

ignore_warnings=$(run_mysql \
    "USE ${DATABASE};
     INSERT IGNORE INTO enforced_t VALUES (7, 0), (8, -8);
     SHOW WARNINGS;")
case "$ignore_warnings" in
    *"Warning	3819	Check constraint 'v_positive' is violated."* ) ;;
    *) fail "INSERT IGNORE warning mismatch: [$ignore_warnings]" ;;
esac

run_mysql \
    "USE ${DATABASE};
     CREATE TABLE not_enforced_t(a INT, CONSTRAINT c_ne CHECK (a > 0) NOT ENFORCED);
     INSERT INTO not_enforced_t VALUES (-1), (0), (1), (NULL);" >/dev/null
not_enforced=$(run_mysql \
    "USE ${DATABASE};
     SHOW CREATE TABLE not_enforced_t;
     SELECT GROUP_CONCAT(IF(a IS NULL, 'N', CAST(a AS CHAR)) ORDER BY a IS NULL, a), ENFORCED
       FROM not_enforced_t
       JOIN INFORMATION_SCHEMA.TABLE_CONSTRAINTS
         ON CONSTRAINT_SCHEMA = '${DATABASE}'
        AND CONSTRAINT_NAME = 'c_ne'
      GROUP BY ENFORCED;")
case "$not_enforced" in
    *"CONSTRAINT \`c_ne\` CHECK ((\`a\` > 0)) /*!80016 NOT ENFORCED */"*"-1,0,1,N	NO"* ) ;;
    *) fail "NOT ENFORCED behavior mismatch: [$not_enforced]" ;;
esac

expect_error \
    "check null non boolean" \
    3812 \
    HY000 \
    "An expression of non-boolean type specified to a check constraint 'check_null_chk_1'" \
    "USE ${DATABASE}; CREATE TABLE check_null(a INT, CHECK (NULL));"

run_mysql \
    "USE ${DATABASE};
     CREATE TABLE check_true(a INT, CHECK (TRUE));
     CREATE TABLE check_false(a INT, CHECK (FALSE));
     CREATE TABLE check_false_ne(a INT, CHECK (FALSE) NOT ENFORCED);
     INSERT INTO check_true VALUES (1);
     INSERT INTO check_false_ne VALUES (1);" >/dev/null

expect_error \
    "check false violation" \
    3819 \
    HY000 \
    "Check constraint 'check_false_chk_1' is violated" \
    "USE ${DATABASE}; INSERT INTO check_false VALUES (1);"

run_mysql \
    "USE ${DATABASE};
     CREATE TABLE expr_t(a INT, b INT,
       CHECK (a + b > 10),
       CHECK (a <=> b),
       CHECK (a IS NOT NULL)
     ) ENGINE=InnoDB;" >/dev/null
expr_clauses=$(run_mysql \
    "USE ${DATABASE};
     SELECT CHECK_CLAUSE
       FROM INFORMATION_SCHEMA.CHECK_CONSTRAINTS
      WHERE CONSTRAINT_SCHEMA = '${DATABASE}'
        AND CONSTRAINT_NAME IN ('expr_t_chk_1', 'expr_t_chk_2', 'expr_t_chk_3')
      ORDER BY CONSTRAINT_NAME;")
case "$expr_clauses" in
    *"((\`a\` + \`b\`) > 10)"* ) ;;
    *) fail "arithmetic check clause mismatch: [$expr_clauses]" ;;
esac
case "$expr_clauses" in
    *"(\`a\` <=> \`b\`)"* ) ;;
    *) fail "null-safe equality check clause mismatch: [$expr_clauses]" ;;
esac
case "$expr_clauses" in
    *"(\`a\` is not null)"* ) ;;
    *) fail "IS NOT NULL check clause mismatch: [$expr_clauses]" ;;
esac

run_mysql \
    "USE ${DATABASE};
     CREATE TABLE like_src(a INT, b INT, CHECK (b > a), CONSTRAINT ck_explicit CHECK (a > 0));
     CREATE TABLE like_clone LIKE like_src;
     CREATE TABLE ctas_clone AS SELECT * FROM like_src;
     RENAME TABLE like_src TO renamed_src;" >/dev/null

propagation=$(run_mysql \
    "USE ${DATABASE};
     SELECT tc.TABLE_NAME, cc.CONSTRAINT_NAME, cc.CHECK_CLAUSE
       FROM INFORMATION_SCHEMA.CHECK_CONSTRAINTS cc
       JOIN INFORMATION_SCHEMA.TABLE_CONSTRAINTS tc
         ON tc.CONSTRAINT_SCHEMA = cc.CONSTRAINT_SCHEMA
        AND tc.CONSTRAINT_NAME = cc.CONSTRAINT_NAME
      WHERE cc.CONSTRAINT_SCHEMA = '${DATABASE}'
        AND tc.TABLE_NAME IN ('like_clone', 'ctas_clone', 'renamed_src')
      ORDER BY tc.TABLE_NAME, cc.CONSTRAINT_NAME;")
case "$propagation" in
    *"like_clone	like_clone_chk_1	(\`a\` > 0)"* ) ;;
    *) fail "LIKE clone missing first generated check: [$propagation]" ;;
esac
case "$propagation" in
    *"like_clone	like_clone_chk_2	(\`b\` > \`a\`)"* ) ;;
    *) fail "LIKE clone missing second generated check: [$propagation]" ;;
esac
case "$propagation" in
    *"renamed_src	ck_explicit	(\`a\` > 0)"* ) ;;
    *) fail "rename missing explicit check: [$propagation]" ;;
esac
case "$propagation" in
    *"renamed_src	renamed_src_chk_1	(\`b\` > \`a\`)"* ) ;;
    *) fail "rename missing generated check rename: [$propagation]" ;;
esac
case "$propagation" in
    *"ctas_clone"* ) fail "CTAS should omit checks: [$propagation]" ;;
    *) ;;
esac

drop_counts=$(run_mysql \
    "USE ${DATABASE};
     SELECT COUNT(*) FROM INFORMATION_SCHEMA.CHECK_CONSTRAINTS
      WHERE CONSTRAINT_SCHEMA = '${DATABASE}' AND CONSTRAINT_NAME = 'renamed_src_chk_1';
     DROP TABLE renamed_src;
     SELECT COUNT(*) FROM INFORMATION_SCHEMA.CHECK_CONSTRAINTS
      WHERE CONSTRAINT_SCHEMA = '${DATABASE}' AND CONSTRAINT_NAME = 'renamed_src_chk_1';")
expect_value "drop removes check metadata" "1
0" "$drop_counts"

expect_error \
    "unknown column in check" \
    3820 \
    HY000 \
    "Check constraint 'bad_unknown_chk_1' refers to non-existing column 'missing'" \
    "USE ${DATABASE}; CREATE TABLE bad_unknown(a INT, CHECK (missing > 0));"

expect_error \
    "subquery in check" \
    3815 \
    HY000 \
    "An expression of a check constraint 'bad_subquery_chk_1' contains disallowed function" \
    "USE ${DATABASE}; CREATE TABLE bad_subquery(a INT, CHECK (a > (SELECT 1)));"

expect_error \
    "variable in check" \
    3816 \
    HY000 \
    "An expression of a check constraint 'bad_user_var_chk_1' cannot refer to a user or system variable" \
    "USE ${DATABASE}; CREATE TABLE bad_user_var(a INT, CHECK (a > @@version));"

expect_error \
    "function in check" \
    3814 \
    HY000 \
    "An expression of a check constraint 'bad_rand_chk_1' contains disallowed function: rand" \
    "USE ${DATABASE}; CREATE TABLE bad_rand(a INT, CHECK (a > RAND()));"

printf '%s\n' "mysql_baseline_check_constraint_lifecycle_expectations: ok"
