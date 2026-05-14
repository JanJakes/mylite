#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_composite_fk_expectations_$$"
MISSING_DATABASE="${DATABASE}_missing"
BAD_DATABASE="${DATABASE}_bad"
ACTIONS_DATABASE="${DATABASE}_actions"

fail() {
    printf '%s\n' "mysql_baseline_composite_foreign_key_constraints_expectations: $1" >&2
    exit 1
}

run_mysql() {
    sql=$1
    shift
    printf '%s\n' "$sql" \
        | docker exec -i "$MYSQL_CONTAINER" mysql --protocol=TCP -h127.0.0.1 -uroot \
              --batch --raw --skip-column-names "$@"
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
    haystack=$2
    needle=$3

    case "$haystack" in
        *"$needle"*) ;;
        *) fail "$label: expected output to contain [$needle], got [$haystack]" ;;
    esac
}

expect_not_contains() {
    label=$1
    haystack=$2
    needle=$3

    case "$haystack" in
        *"$needle"*) fail "$label: output should not contain [$needle], got [$haystack]" ;;
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

expect_accept() {
    label=$1
    sql=$2
    shift 2

    set +e
    output=$(run_mysql "$sql" "$@" 2>&1)
    status_code=$?
    set -e

    if [ "$status_code" -ne 0 ]; then
        fail "$label: expected MySQL to accept deferred behavior, got [$output]"
    fi
}

cleanup() {
    run_mysql "DROP DATABASE IF EXISTS ${DATABASE};" >/dev/null 2>&1 || true
    run_mysql "DROP DATABASE IF EXISTS ${MISSING_DATABASE};" >/dev/null 2>&1 || true
    run_mysql "DROP DATABASE IF EXISTS ${BAD_DATABASE};" >/dev/null 2>&1 || true
    run_mysql "DROP DATABASE IF EXISTS ${ACTIONS_DATABASE};" >/dev/null 2>&1 || true
}

trap cleanup EXIT

version=$(run_mysql "SELECT VERSION();")
case "$version" in
    8.4.9*) ;;
    *) fail "expected MySQL 8.4.9 runtime, got [$version]" ;;
esac

cleanup
run_mysql "CREATE DATABASE ${DATABASE};" >/dev/null

run_mysql \
    "USE ${DATABASE};
     CREATE TABLE parent_pk(
       a INT NOT NULL,
       b INT NOT NULL,
       c INT,
       PRIMARY KEY(a,b),
       UNIQUE KEY u_ba(b,a)
     ) ENGINE=InnoDB;
     CREATE TABLE child_fk(
       id INT NOT NULL,
       a INT NULL,
       b INT NULL,
       PRIMARY KEY(id),
       CONSTRAINT fk_child_parent FOREIGN KEY(a,b) REFERENCES parent_pk(a,b)
     ) ENGINE=InnoDB;
     CREATE TABLE child_existing(
       id INT,
       a INT,
       b INT,
       c INT,
       KEY existing_ab(a,b,c),
       CONSTRAINT fk_existing FOREIGN KEY(a,b) REFERENCES parent_pk(a,b)
     ) ENGINE=InnoDB;
     CREATE TABLE child_unnamed(
       id INT,
       a INT,
       b INT,
       FOREIGN KEY(a,b) REFERENCES parent_pk(a,b)
     ) ENGINE=InnoDB;
     CREATE TABLE child_ref_ba(
       id INT,
       a INT,
       b INT,
       CONSTRAINT fk_ba FOREIGN KEY(b,a) REFERENCES parent_pk(b,a)
     ) ENGINE=InnoDB;
     CREATE TABLE parent_unique_nullable(
       a INT NULL,
       b INT NULL,
       UNIQUE KEY u_ab(a,b)
     ) ENGINE=InnoDB;
     CREATE TABLE child_unique_nullable(
       id INT,
       a INT,
       b INT,
       CONSTRAINT fk_unique_nullable FOREIGN KEY(a,b)
       REFERENCES parent_unique_nullable(a,b)
     ) ENGINE=InnoDB;" >/dev/null

show_create=$(run_mysql "USE ${DATABASE}; SHOW CREATE TABLE child_fk;")
expect_contains \
    "SHOW CREATE child auto index" \
    "$show_create" \
    "KEY \`fk_child_parent\` (\`a\`,\`b\`)"
expect_contains \
    "SHOW CREATE child composite FK" \
    "$show_create" \
    "CONSTRAINT \`fk_child_parent\` FOREIGN KEY (\`a\`, \`b\`) REFERENCES \`parent_pk\` (\`a\`, \`b\`)"

show_existing=$(run_mysql "USE ${DATABASE}; SHOW CREATE TABLE child_existing;")
expect_contains \
    "existing child index reused" \
    "$show_existing" \
    "KEY \`existing_ab\` (\`a\`,\`b\`,\`c\`)"
expect_not_contains \
    "existing child index does not create duplicate FK index" \
    "$show_existing" \
    "KEY \`fk_existing\`"

show_unnamed=$(run_mysql "USE ${DATABASE}; SHOW CREATE TABLE child_unnamed;")
expect_contains "unnamed child index name" "$show_unnamed" "KEY \`a\` (\`a\`,\`b\`)"
expect_contains \
    "unnamed FK generated name" \
    "$show_unnamed" \
    "CONSTRAINT \`child_unnamed_ibfk_1\` FOREIGN KEY"

statistics=$(run_mysql \
    "USE ${DATABASE};
     SELECT INDEX_NAME, NON_UNIQUE, SEQ_IN_INDEX, COLUMN_NAME
       FROM INFORMATION_SCHEMA.STATISTICS
      WHERE TABLE_SCHEMA = '${DATABASE}'
        AND TABLE_NAME = 'child_fk'
        AND INDEX_NAME = 'fk_child_parent'
      ORDER BY SEQ_IN_INDEX;")
expect_value "child composite index part 1" "fk_child_parent	1	1	a" \
    "$(printf '%s\n' "$statistics" | sed -n '1p')"
expect_value "child composite index part 2" "fk_child_parent	1	2	b" \
    "$(printf '%s\n' "$statistics" | sed -n '2p')"

table_constraints=$(run_mysql \
    "USE ${DATABASE};
     SELECT CONSTRAINT_NAME, TABLE_NAME, CONSTRAINT_TYPE, ENFORCED
       FROM INFORMATION_SCHEMA.TABLE_CONSTRAINTS
      WHERE TABLE_SCHEMA = '${DATABASE}' AND TABLE_NAME = 'child_fk'
      ORDER BY CONSTRAINT_NAME;")
expect_value "child FK table constraint" "fk_child_parent	child_fk	FOREIGN KEY	YES" \
    "$(printf '%s\n' "$table_constraints" | sed -n '1p')"
expect_value "child primary table constraint" "PRIMARY	child_fk	PRIMARY KEY	YES" \
    "$(printf '%s\n' "$table_constraints" | sed -n '2p')"

key_usage=$(run_mysql \
    "USE ${DATABASE};
     SELECT CONSTRAINT_NAME, TABLE_NAME, COLUMN_NAME, ORDINAL_POSITION,
            POSITION_IN_UNIQUE_CONSTRAINT, REFERENCED_TABLE_NAME,
            REFERENCED_COLUMN_NAME
       FROM INFORMATION_SCHEMA.KEY_COLUMN_USAGE
      WHERE TABLE_SCHEMA = '${DATABASE}' AND TABLE_NAME = 'child_fk'
        AND CONSTRAINT_NAME = 'fk_child_parent'
      ORDER BY ORDINAL_POSITION;")
expect_value "child FK key usage part 1" "fk_child_parent	child_fk	a	1	1	parent_pk	a" \
    "$(printf '%s\n' "$key_usage" | sed -n '1p')"
expect_value "child FK key usage part 2" "fk_child_parent	child_fk	b	2	2	parent_pk	b" \
    "$(printf '%s\n' "$key_usage" | sed -n '2p')"

referential=$(run_mysql \
    "USE ${DATABASE};
     SELECT CONSTRAINT_NAME, UNIQUE_CONSTRAINT_NAME, MATCH_OPTION,
            UPDATE_RULE, DELETE_RULE, TABLE_NAME, REFERENCED_TABLE_NAME
       FROM INFORMATION_SCHEMA.REFERENTIAL_CONSTRAINTS
      WHERE CONSTRAINT_SCHEMA = '${DATABASE}'
        AND CONSTRAINT_NAME = 'fk_child_parent';")
expect_value \
    "child FK referential metadata" \
    "fk_child_parent	PRIMARY	NONE	NO ACTION	NO ACTION	child_fk	parent_pk" \
    "$referential"

run_mysql \
    "USE ${DATABASE};
     INSERT INTO parent_pk VALUES (1,2,10),(3,4,20);
     INSERT INTO child_fk VALUES (1,1,2),(2,NULL,2),(3,1,NULL),(4,NULL,NULL);
     INSERT INTO child_fk SET id = 5, a = 3, b = 4;
     UPDATE child_fk SET a = 1, b = 2 WHERE id = 5;" >/dev/null

child_rows=$(run_mysql "USE ${DATABASE}; SELECT id, a, b FROM child_fk ORDER BY id;")
expect_value "child FK valid tuple" "1	1	2" "$(printf '%s\n' "$child_rows" | sed -n '1p')"
expect_value "child FK NULL first part" "2	NULL	2" "$(printf '%s\n' "$child_rows" | sed -n '2p')"
expect_value "child FK NULL second part" "3	1	NULL" "$(printf '%s\n' "$child_rows" | sed -n '3p')"
expect_value "child FK all NULL tuple" "4	NULL	NULL" "$(printf '%s\n' "$child_rows" | sed -n '4p')"
expect_value "child FK valid update tuple" "5	1	2" "$(printf '%s\n' "$child_rows" | sed -n '5p')"

expect_error \
    "missing composite parent insert" \
    1452 \
    "23000" \
    "Cannot add or update a child row" \
    "USE ${DATABASE}; INSERT INTO child_fk VALUES (6,1,99);"

ignore_status=$(run_mysql \
    "USE ${DATABASE};
     INSERT IGNORE INTO child_fk VALUES (6,1,99);
     SELECT ROW_COUNT(), @@warning_count;")
expect_value "missing composite parent INSERT IGNORE status" "0	1" \
    "$(printf '%s\n' "$ignore_status" | sed -n '1p')"
ignore_warning=$(run_mysql \
    "USE ${DATABASE};
     INSERT IGNORE INTO child_fk VALUES (6,1,99);
     SHOW WARNINGS;")
case "$(printf '%s\n' "$ignore_warning" | sed -n '1p')" in
    Warning*1452*"Cannot add or update a child row"*) ;;
    *) fail "missing composite parent INSERT IGNORE warning mismatch: [$ignore_warning]" ;;
esac

expect_error \
    "missing composite parent update" \
    1452 \
    "23000" \
    "Cannot add or update a child row" \
    "USE ${DATABASE}; UPDATE child_fk SET b = 99 WHERE id = 1;"
expect_error \
    "referenced composite parent delete" \
    1451 \
    "23000" \
    "Cannot delete or update a parent row" \
    "USE ${DATABASE}; DELETE FROM parent_pk WHERE a = 1 AND b = 2;"
expect_error \
    "referenced composite parent update" \
    1451 \
    "23000" \
    "Cannot delete or update a parent row" \
    "USE ${DATABASE}; UPDATE parent_pk SET a = 9 WHERE a = 1 AND b = 2;"

run_mysql \
    "USE ${DATABASE};
     INSERT INTO parent_unique_nullable VALUES (7,8),(NULL,8),(7,NULL),(NULL,NULL);
     INSERT INTO child_unique_nullable VALUES (1,7,8),(2,NULL,8),(3,7,NULL),(4,NULL,NULL);" \
    >/dev/null
nullable_rows=$(run_mysql \
    "USE ${DATABASE}; SELECT id, a, b FROM child_unique_nullable ORDER BY id;")
expect_value "nullable unique parent full tuple" "1	7	8" \
    "$(printf '%s\n' "$nullable_rows" | sed -n '1p')"
expect_value "nullable unique parent NULL first child tuple" "2	NULL	8" \
    "$(printf '%s\n' "$nullable_rows" | sed -n '2p')"
expect_value "nullable unique parent NULL second child tuple" "3	7	NULL" \
    "$(printf '%s\n' "$nullable_rows" | sed -n '3p')"
expect_value "nullable unique parent NULL tuple" "4	NULL	NULL" \
    "$(printf '%s\n' "$nullable_rows" | sed -n '4p')"
expect_error \
    "missing nullable unique parent full tuple" \
    1452 \
    "23000" \
    "Cannot add or update a child row" \
    "USE ${DATABASE}; INSERT INTO child_unique_nullable VALUES (5,9,9);"

run_mysql \
    "USE ${DATABASE};
     CREATE TABLE alter_child(id INT, a INT, b INT) ENGINE=InnoDB;
     INSERT INTO alter_child VALUES (1,1,2),(2,NULL,2);
     ALTER TABLE alter_child ADD CONSTRAINT fk_alter
       FOREIGN KEY(a,b) REFERENCES parent_pk(a,b);" >/dev/null
alter_metadata=$(run_mysql \
    "USE ${DATABASE};
     SELECT CONSTRAINT_NAME, UNIQUE_CONSTRAINT_NAME, TABLE_NAME
       FROM INFORMATION_SCHEMA.REFERENTIAL_CONSTRAINTS
      WHERE CONSTRAINT_SCHEMA = '${DATABASE}' AND CONSTRAINT_NAME = 'fk_alter';")
expect_value "alter add composite FK metadata" "fk_alter	PRIMARY	alter_child" "$alter_metadata"

run_mysql "CREATE DATABASE ${BAD_DATABASE};" >/dev/null
expect_error \
    "composite FK count mismatch child longer" \
    1239 \
    "42000" \
    "Key reference and table reference don't match" \
    "USE ${BAD_DATABASE};
     CREATE TABLE p(a INT NOT NULL, b INT NOT NULL, PRIMARY KEY(a,b)) ENGINE=InnoDB;
     CREATE TABLE c(a INT, b INT,
       CONSTRAINT fk_bad_count FOREIGN KEY(a,b) REFERENCES p(a)) ENGINE=InnoDB;"
expect_error \
    "composite FK count mismatch parent longer" \
    1239 \
    "42000" \
    "Key reference and table reference don't match" \
    "USE ${BAD_DATABASE};
     DROP TABLE IF EXISTS c;
     CREATE TABLE c(a INT,
       CONSTRAINT fk_bad_parent_count FOREIGN KEY(a) REFERENCES p(a,b)) ENGINE=InnoDB;"
expect_error \
    "composite FK missing parent unique" \
    6125 \
    "HY000" \
    "Missing unique key" \
    "USE ${BAD_DATABASE};
     DROP TABLE IF EXISTS c;
     CREATE TABLE p_no_unique(a INT, b INT, KEY k_ab(a,b)) ENGINE=InnoDB;
     CREATE TABLE c(a INT, b INT,
       CONSTRAINT fk_missing_unique FOREIGN KEY(a,b) REFERENCES p_no_unique(a,b))
       ENGINE=InnoDB;"

expect_error \
    "composite FK incompatible descriptor" \
    3780 \
    "HY000" \
    "incompatible" \
    "USE ${BAD_DATABASE};
     CREATE TABLE p_big(a BIGINT NOT NULL, b INT NOT NULL, PRIMARY KEY(a,b)) ENGINE=InnoDB;
     CREATE TABLE c(a INT, b INT,
       CONSTRAINT fk_bad_type FOREIGN KEY(a,b) REFERENCES p_big(a,b)) ENGINE=InnoDB;"
expect_error \
    "duplicate composite FK name create" \
    1826 \
    "HY000" \
    "Duplicate foreign key constraint name" \
    "USE ${BAD_DATABASE};
     DROP TABLE IF EXISTS c;
     CREATE TABLE c(a INT, b INT,
       CONSTRAINT fk_dup FOREIGN KEY(a,b) REFERENCES p(a,b),
       CONSTRAINT fk_dup FOREIGN KEY(a,b) REFERENCES p(a,b)) ENGINE=InnoDB;"
expect_error \
    "duplicate composite FK child part create" \
    1060 \
    "42S21" \
    "Duplicate column name 'a'" \
    "USE ${BAD_DATABASE};
     DROP TABLE IF EXISTS c;
     CREATE TABLE c(a INT, b INT,
       CONSTRAINT fk_dup_child_part FOREIGN KEY(a,a) REFERENCES p(a,b)) ENGINE=InnoDB;"
expect_error \
    "duplicate composite FK parent part create" \
    6125 \
    "HY000" \
    "Missing unique key" \
    "USE ${BAD_DATABASE};
     DROP TABLE IF EXISTS c;
     CREATE TABLE c(a INT, b INT,
       CONSTRAINT fk_dup_parent_part FOREIGN KEY(a,b) REFERENCES p(a,a)) ENGINE=InnoDB;"
expect_error \
    "duplicate composite FK child part alter" \
    1060 \
    "42S21" \
    "Duplicate column name 'a'" \
    "USE ${BAD_DATABASE};
     DROP TABLE IF EXISTS c;
     CREATE TABLE c(a INT, b INT) ENGINE=InnoDB;
     ALTER TABLE c ADD CONSTRAINT fk_alter_dup_child FOREIGN KEY(a,a) REFERENCES p(a,b);"
expect_error \
    "missing composite parent table" \
    1824 \
    "HY000" \
    "Failed to open the referenced table" \
    "USE ${BAD_DATABASE};
     DROP TABLE IF EXISTS c;
     CREATE TABLE c(a INT, b INT,
       CONSTRAINT fk_missing_parent FOREIGN KEY(a,b) REFERENCES missing_parent(a,b))
       ENGINE=InnoDB;"

expect_accept \
    "mysql accepts composite cascade deferred by MyLite" \
    "CREATE DATABASE ${ACTIONS_DATABASE}; USE ${ACTIONS_DATABASE};
     CREATE TABLE p(a INT NOT NULL, b INT NOT NULL, PRIMARY KEY(a,b)) ENGINE=InnoDB;
     CREATE TABLE c(a INT, b INT,
       CONSTRAINT fk_cascade FOREIGN KEY(a,b) REFERENCES p(a,b) ON DELETE CASCADE)
       ENGINE=InnoDB;"

printf '%s\n' "baseline composite foreign key MySQL 8.4.9 expectations verified"
