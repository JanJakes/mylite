#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_foreign_key_constraints_expectations_$$"

fail() {
    printf '%s\n' "mysql_baseline_foreign_key_constraints_expectations: $1" >&2
    exit 1
}

run_mysql() {
    sql=$1
    shift
    printf '%s\n' "$sql" \
        | docker exec -i "$MYSQL_CONTAINER" mysql --protocol=TCP -h127.0.0.1 -uroot \
              --batch --raw --skip-column-names "$@"
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
    run_mysql "DROP DATABASE IF EXISTS ${DATABASE};" >/dev/null 2>&1 || true
    run_mysql "DROP DATABASE IF EXISTS ${DATABASE}_bad1;" >/dev/null 2>&1 || true
    run_mysql "DROP DATABASE IF EXISTS ${DATABASE}_bad2;" >/dev/null 2>&1 || true
    run_mysql "DROP DATABASE IF EXISTS ${DATABASE}_bad3;" >/dev/null 2>&1 || true
    run_mysql "DROP DATABASE IF EXISTS ${DATABASE}_actions;" >/dev/null 2>&1 || true
    run_mysql "DROP DATABASE IF EXISTS ${DATABASE}_drop;" >/dev/null 2>&1 || true
}

trap cleanup EXIT

version=$(run_mysql 'SELECT VERSION();')
case "$version" in
    8.4.9*) ;;
    *) fail "expected MySQL 8.4.9 runtime, got [$version]" ;;
esac

cleanup

run_mysql \
    "CREATE DATABASE ${DATABASE}; USE ${DATABASE};
     CREATE TABLE parent(
       id INT NOT NULL,
       code INT NULL,
       PRIMARY KEY(id),
       UNIQUE KEY uq_parent_code(code)
     ) ENGINE=InnoDB;
     CREATE TABLE child(
       id INT NOT NULL,
       parent_id INT NULL,
       CONSTRAINT fk_child_parent FOREIGN KEY(parent_id) REFERENCES parent(id)
     ) ENGINE=InnoDB;
     CREATE TABLE child_existing_index(
       id INT,
       parent_id INT,
       KEY existing_parent(parent_id),
       CONSTRAINT fk_existing_parent FOREIGN KEY(parent_id) REFERENCES parent(id)
     ) ENGINE=InnoDB;
     CREATE TABLE child_unnamed(
       id INT,
       parent_id INT,
       FOREIGN KEY(parent_id) REFERENCES parent(id)
     ) ENGINE=InnoDB;
     CREATE TABLE child_unique_parent(
       id INT,
       code INT,
       CONSTRAINT fk_child_code FOREIGN KEY(code) REFERENCES parent(code)
     ) ENGINE=InnoDB;" >/dev/null

show_create=$(run_mysql "USE ${DATABASE}; SHOW CREATE TABLE child;")
case "$show_create" in
    *"KEY \`fk_child_parent\` (\`parent_id\`)"* ) ;;
    *) fail "SHOW CREATE child missing auto-created child index: [$show_create]" ;;
esac
case "$show_create" in
    *"CONSTRAINT \`fk_child_parent\` FOREIGN KEY (\`parent_id\`) REFERENCES \`parent\` (\`id\`)"* ) ;;
    *) fail "SHOW CREATE child missing FK definition: [$show_create]" ;;
esac

show_existing=$(run_mysql "USE ${DATABASE}; SHOW CREATE TABLE child_existing_index;")
case "$show_existing" in
    *"KEY \`existing_parent\` (\`parent_id\`)"* ) ;;
    *) fail "SHOW CREATE existing-index child missing existing key: [$show_existing]" ;;
esac
case "$show_existing" in
    *"KEY \`fk_existing_parent\`"* ) fail "existing child index should be reused: [$show_existing]" ;;
    *) ;;
esac

show_unnamed=$(run_mysql "USE ${DATABASE}; SHOW CREATE TABLE child_unnamed;")
case "$show_unnamed" in
    *"KEY \`parent_id\` (\`parent_id\`)"* ) ;;
    *) fail "SHOW CREATE unnamed child missing generated child key: [$show_unnamed]" ;;
esac
case "$show_unnamed" in
    *"CONSTRAINT \`child_unnamed_ibfk_1\` FOREIGN KEY"* ) ;;
    *) fail "SHOW CREATE unnamed child missing generated FK name: [$show_unnamed]" ;;
esac

constraints=$(run_mysql \
    "USE ${DATABASE};
     SELECT CONSTRAINT_NAME, TABLE_NAME, CONSTRAINT_TYPE, ENFORCED
       FROM INFORMATION_SCHEMA.TABLE_CONSTRAINTS
      WHERE TABLE_SCHEMA = '${DATABASE}'
      ORDER BY TABLE_NAME, CONSTRAINT_NAME;")
expect_value "table constraints child fk" "fk_child_parent	child	FOREIGN KEY	YES" \
    "$(printf '%s\n' "$constraints" | sed -n '1p')"
expect_value "table constraints existing fk" "fk_existing_parent	child_existing_index	FOREIGN KEY	YES" \
    "$(printf '%s\n' "$constraints" | sed -n '2p')"
expect_value "table constraints unique-parent fk" "fk_child_code	child_unique_parent	FOREIGN KEY	YES" \
    "$(printf '%s\n' "$constraints" | sed -n '3p')"
expect_value "table constraints unnamed fk" "child_unnamed_ibfk_1	child_unnamed	FOREIGN KEY	YES" \
    "$(printf '%s\n' "$constraints" | sed -n '4p')"
expect_value "table constraints parent primary" "PRIMARY	parent	PRIMARY KEY	YES" \
    "$(printf '%s\n' "$constraints" | sed -n '5p')"
expect_value "table constraints parent unique" "uq_parent_code	parent	UNIQUE	YES" \
    "$(printf '%s\n' "$constraints" | sed -n '6p')"

key_usage=$(run_mysql \
    "USE ${DATABASE};
     SELECT CONSTRAINT_NAME, TABLE_NAME, COLUMN_NAME, ORDINAL_POSITION,
            POSITION_IN_UNIQUE_CONSTRAINT, REFERENCED_TABLE_SCHEMA,
            REFERENCED_TABLE_NAME, REFERENCED_COLUMN_NAME
       FROM INFORMATION_SCHEMA.KEY_COLUMN_USAGE
      WHERE TABLE_SCHEMA = '${DATABASE}'
      ORDER BY TABLE_NAME, CONSTRAINT_NAME, ORDINAL_POSITION;")
expect_value "key usage child fk" \
    "fk_child_parent	child	parent_id	1	1	${DATABASE}	parent	id" \
    "$(printf '%s\n' "$key_usage" | sed -n '1p')"
expect_value "key usage existing fk" \
    "fk_existing_parent	child_existing_index	parent_id	1	1	${DATABASE}	parent	id" \
    "$(printf '%s\n' "$key_usage" | sed -n '2p')"
expect_value "key usage unique-parent fk" \
    "fk_child_code	child_unique_parent	code	1	1	${DATABASE}	parent	code" \
    "$(printf '%s\n' "$key_usage" | sed -n '3p')"
expect_value "key usage unnamed fk" \
    "child_unnamed_ibfk_1	child_unnamed	parent_id	1	1	${DATABASE}	parent	id" \
    "$(printf '%s\n' "$key_usage" | sed -n '4p')"

referential=$(run_mysql \
    "USE ${DATABASE};
     SELECT CONSTRAINT_NAME, UNIQUE_CONSTRAINT_NAME, MATCH_OPTION,
            UPDATE_RULE, DELETE_RULE, TABLE_NAME, REFERENCED_TABLE_NAME
       FROM INFORMATION_SCHEMA.REFERENTIAL_CONSTRAINTS
      WHERE CONSTRAINT_SCHEMA = '${DATABASE}'
      ORDER BY CONSTRAINT_NAME;")
expect_value "referential child fk" \
    "child_unnamed_ibfk_1	PRIMARY	NONE	NO ACTION	NO ACTION	child_unnamed	parent" \
    "$(printf '%s\n' "$referential" | sed -n '1p')"
expect_value "referential unique parent fk" \
    "fk_child_code	uq_parent_code	NONE	NO ACTION	NO ACTION	child_unique_parent	parent" \
    "$(printf '%s\n' "$referential" | sed -n '2p')"
expect_value "referential named fk" \
    "fk_child_parent	PRIMARY	NONE	NO ACTION	NO ACTION	child	parent" \
    "$(printf '%s\n' "$referential" | sed -n '3p')"

run_mysql \
    "USE ${DATABASE};
     CREATE TABLE self_ref(
       id INT PRIMARY KEY,
       parent_id INT,
       CONSTRAINT parent_id_fk FOREIGN KEY(parent_id) REFERENCES self_ref(id)
     ) ENGINE=InnoDB;" >/dev/null

self_show=$(run_mysql "USE ${DATABASE}; SHOW CREATE TABLE self_ref;")
case "$self_show" in
    *"KEY \`parent_id_fk\` (\`parent_id\`)"*) ;;
    *) fail "SHOW CREATE self-ref missing auto-created child key: [$self_show]" ;;
esac
case "$self_show" in
    *"CONSTRAINT \`parent_id_fk\` FOREIGN KEY (\`parent_id\`) REFERENCES \`self_ref\` (\`id\`)"*) ;;
    *) fail "SHOW CREATE self-ref missing FK definition: [$self_show]" ;;
esac

self_key_usage=$(run_mysql \
    "USE ${DATABASE};
     SELECT CONSTRAINT_NAME, TABLE_NAME, COLUMN_NAME, ORDINAL_POSITION,
            POSITION_IN_UNIQUE_CONSTRAINT, REFERENCED_TABLE_NAME,
            REFERENCED_COLUMN_NAME
       FROM INFORMATION_SCHEMA.KEY_COLUMN_USAGE
      WHERE TABLE_SCHEMA = '${DATABASE}' AND TABLE_NAME = 'self_ref'
        AND CONSTRAINT_NAME = 'parent_id_fk';")
expect_value "self-ref key usage" \
    "parent_id_fk	self_ref	parent_id	1	1	self_ref	id" \
    "$self_key_usage"

self_rows=$(run_mysql \
    "USE ${DATABASE};
     INSERT INTO self_ref VALUES (1,NULL),(2,1),(3,3);
     SELECT id, parent_id FROM self_ref ORDER BY id;")
expect_value "self-ref root row" "1	NULL" "$(printf '%s\n' "$self_rows" | sed -n '1p')"
expect_value "self-ref child row" "2	1" "$(printf '%s\n' "$self_rows" | sed -n '2p')"
expect_value "self-ref same-row reference" "3	3" "$(printf '%s\n' "$self_rows" | sed -n '3p')"

expect_error \
    "self-ref missing parent insert" \
    1452 \
    "23000" \
    "Cannot add or update a child row" \
    "USE ${DATABASE}; INSERT INTO self_ref VALUES (4,99);"
expect_error \
    "self-ref referenced parent delete" \
    1451 \
    "23000" \
    "Cannot delete or update a parent row" \
    "USE ${DATABASE}; DELETE FROM self_ref WHERE id = 1;"

run_mysql \
    "USE ${DATABASE};
     CREATE TABLE self_unique(
       slug INT UNIQUE,
       parent_slug INT,
       CONSTRAINT unique_parent_fk FOREIGN KEY(parent_slug) REFERENCES self_unique(slug)
     ) ENGINE=InnoDB;
     INSERT INTO self_unique VALUES (1,NULL),(2,1);" >/dev/null
expect_error \
    "self-ref unique missing parent insert" \
    1452 \
    "23000" \
    "Cannot add or update a child row" \
    "USE ${DATABASE}; INSERT INTO self_unique VALUES (3,99);"

run_mysql \
    "USE ${DATABASE};
     INSERT INTO parent VALUES (1,10),(2,20);
     INSERT INTO child VALUES (1,1),(2,NULL);
     INSERT INTO child SET id = 3, parent_id = 2;
     UPDATE child SET parent_id = 1 WHERE id = 3;" >/dev/null

rows=$(run_mysql "USE ${DATABASE}; SELECT id, parent_id FROM child ORDER BY id; SELECT ROW_COUNT(), @@warning_count;")
expect_value "child rows after valid writes" "1	1" "$(printf '%s\n' "$rows" | sed -n '1p')"
expect_value "child null row after valid writes" "2	NULL" "$(printf '%s\n' "$rows" | sed -n '2p')"
expect_value "child updated row after valid writes" "3	1" "$(printf '%s\n' "$rows" | sed -n '3p')"
expect_value "select status after valid writes" "-1	0" "$(printf '%s\n' "$rows" | sed -n '4p')"

expect_error \
    "missing parent insert" \
    1452 \
    "23000" \
    "Cannot add or update a child row" \
    "USE ${DATABASE}; INSERT INTO child VALUES (4,99);"
ignore_rows=$(run_mysql \
    "USE ${DATABASE};
     INSERT IGNORE INTO child VALUES (4,99);
     SELECT ROW_COUNT(), @@warning_count;")
expect_value "missing parent INSERT IGNORE status" "0	1" \
    "$(printf '%s\n' "$ignore_rows" | sed -n '1p')"
ignore_warning=$(run_mysql \
    "USE ${DATABASE};
     INSERT IGNORE INTO child VALUES (4,99);
     SHOW WARNINGS;")
case "$(printf '%s\n' "$ignore_warning" | sed -n '1p')" in
    Warning*1452*"Cannot add or update a child row"*) ;;
    *) fail "missing parent INSERT IGNORE warning mismatch: [$ignore_warning]" ;;
esac
ignore_count=$(run_mysql "USE ${DATABASE}; SELECT COUNT(*) FROM child WHERE id = 4;")
expect_value "missing parent INSERT IGNORE skipped row" "0" \
    "$ignore_count"
expect_error \
    "missing parent update" \
    1452 \
    "23000" \
    "Cannot add or update a child row" \
    "USE ${DATABASE}; UPDATE child SET parent_id = 99 WHERE id = 1;"
expect_error \
    "referenced parent delete" \
    1451 \
    "23000" \
    "Cannot delete or update a parent row" \
    "USE ${DATABASE}; DELETE FROM parent WHERE id = 1;"
expect_error \
    "referenced parent update" \
    1451 \
    "23000" \
    "Cannot delete or update a parent row" \
    "USE ${DATABASE}; UPDATE parent SET id = 9 WHERE id = 1;"

run_mysql \
    "USE ${DATABASE};
     CREATE TABLE alter_child(id INT, parent_id INT) ENGINE=InnoDB;
     ALTER TABLE alter_child ADD CONSTRAINT fk_alter_parent
       FOREIGN KEY(parent_id) REFERENCES parent(id);
     INSERT INTO alter_child VALUES (1, 1), (2, NULL);" >/dev/null
alter_metadata=$(run_mysql \
    "USE ${DATABASE};
     SELECT CONSTRAINT_NAME, TABLE_NAME, REFERENCED_TABLE_NAME
       FROM INFORMATION_SCHEMA.REFERENTIAL_CONSTRAINTS
      WHERE CONSTRAINT_SCHEMA = '${DATABASE}' AND CONSTRAINT_NAME = 'fk_alter_parent';")
expect_value "alter add fk metadata" "fk_alter_parent	alter_child	parent" "$alter_metadata"

drop_database_cleanup=$(run_mysql \
    "DROP DATABASE IF EXISTS ${DATABASE}_drop; CREATE DATABASE ${DATABASE}_drop;
     USE ${DATABASE}_drop;
     CREATE TABLE p(id INT PRIMARY KEY) ENGINE=InnoDB;
     CREATE TABLE c(id INT PRIMARY KEY, pid INT,
       CONSTRAINT fk_drop_parent FOREIGN KEY(pid) REFERENCES p(id)) ENGINE=InnoDB;
     DROP DATABASE ${DATABASE}_drop;
     CREATE DATABASE ${DATABASE}_drop; USE ${DATABASE}_drop;
     CREATE TABLE p(id INT PRIMARY KEY) ENGINE=InnoDB;
     CREATE TABLE c(id INT PRIMARY KEY, pid INT) ENGINE=InnoDB;
     INSERT INTO c VALUES (1, 99);
     SELECT COUNT(*) FROM INFORMATION_SCHEMA.REFERENTIAL_CONSTRAINTS
      WHERE CONSTRAINT_SCHEMA = '${DATABASE}_drop';")
expect_value "drop database removes foreign key metadata" "0" "$drop_database_cleanup"

expect_error \
    "missing parent unique key" \
    6125 \
    "HY000" \
    "Missing unique key" \
    "DROP DATABASE IF EXISTS ${DATABASE}_bad1; CREATE DATABASE ${DATABASE}_bad1;
     USE ${DATABASE}_bad1;
     CREATE TABLE p(id INT, KEY k(id)) ENGINE=InnoDB;
     CREATE TABLE c(pid INT, FOREIGN KEY(pid) REFERENCES p(id)) ENGINE=InnoDB;"
expect_error \
    "incompatible referenced column" \
    3780 \
    "HY000" \
    "are incompatible" \
    "DROP DATABASE IF EXISTS ${DATABASE}_bad2; CREATE DATABASE ${DATABASE}_bad2;
     USE ${DATABASE}_bad2;
     CREATE TABLE p(id INT NOT NULL PRIMARY KEY) ENGINE=InnoDB;
     CREATE TABLE c(pid BIGINT, FOREIGN KEY(pid) REFERENCES p(id)) ENGINE=InnoDB;"
expect_error \
    "missing parent table" \
    1824 \
    "HY000" \
    "Failed to open the referenced table" \
    "DROP DATABASE IF EXISTS ${DATABASE}_bad3; CREATE DATABASE ${DATABASE}_bad3;
     USE ${DATABASE}_bad3;
     CREATE TABLE c(pid INT, FOREIGN KEY(pid) REFERENCES p(id)) ENGINE=InnoDB;"

cascade=$(run_mysql \
    "DROP DATABASE IF EXISTS ${DATABASE}_actions; CREATE DATABASE ${DATABASE}_actions;
     USE ${DATABASE}_actions;
     CREATE TABLE p(id INT NOT NULL PRIMARY KEY) ENGINE=InnoDB;
     CREATE TABLE c(pid INT, FOREIGN KEY(pid) REFERENCES p(id) ON DELETE CASCADE) ENGINE=InnoDB;
     SELECT CONSTRAINT_NAME, UPDATE_RULE, DELETE_RULE
       FROM INFORMATION_SCHEMA.REFERENTIAL_CONSTRAINTS
      WHERE CONSTRAINT_SCHEMA = '${DATABASE}_actions';")
expect_value "mysql accepts cascade but mylite defers it" "c_ibfk_1	NO ACTION	CASCADE" "$cascade"

cleanup

printf '%s\n' "mysql_baseline_foreign_key_constraints_expectations: ok"
