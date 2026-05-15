#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_fk_symbol_actions_expectations_$$"

fail() {
    printf '%s\n' "mysql_baseline_foreign_key_symbol_actions_expectations: $1" >&2
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
        *) fail "$label: expected [$needle] in [$haystack]" ;;
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
    run_mysql "DROP DATABASE IF EXISTS ${DATABASE};" >/dev/null 2>&1 || true
}

trap cleanup EXIT

version=$(run_mysql "SELECT VERSION();")
case "$version" in
    8.4.9*) ;;
    *) fail "expected MySQL 8.4.9 runtime, got [$version]" ;;
esac

cleanup

run_mysql \
    "CREATE DATABASE ${DATABASE}; USE ${DATABASE};
     CREATE TABLE parent(id INT NOT NULL PRIMARY KEY) ENGINE=InnoDB;
     CREATE TABLE child_symbol(
       parent_id INT,
       FOREIGN KEY parent_idx(parent_id) REFERENCES parent(id)
     ) ENGINE=InnoDB;
     CREATE TABLE child_named_symbol(
       parent_id INT,
       CONSTRAINT fk_named FOREIGN KEY ignored_idx(parent_id) REFERENCES parent(id)
     ) ENGINE=InnoDB;
     CREATE TABLE child_existing_symbol(
       parent_id INT,
       KEY existing_parent(parent_id),
       FOREIGN KEY ignored_existing(parent_id) REFERENCES parent(id)
     ) ENGINE=InnoDB;
     CREATE TABLE child_cascade(
       parent_id INT,
       CONSTRAINT fk_cascade FOREIGN KEY(parent_id) REFERENCES parent(id)
         ON UPDATE CASCADE ON DELETE CASCADE
     ) ENGINE=InnoDB;
     CREATE TABLE child_restrict(
       parent_id INT,
       FOREIGN KEY(parent_id) REFERENCES parent(id)
         ON DELETE RESTRICT ON UPDATE RESTRICT
     ) ENGINE=InnoDB;
     CREATE TABLE child_noaction(
       parent_id INT,
       FOREIGN KEY(parent_id) REFERENCES parent(id)
         ON DELETE NO ACTION ON UPDATE NO ACTION
     ) ENGINE=InnoDB;
     CREATE TABLE alter_child(parent_id INT) ENGINE=InnoDB;
     ALTER TABLE alter_child
       ADD FOREIGN KEY alter_parent_idx(parent_id) REFERENCES parent(id)
       ON DELETE CASCADE;" >/dev/null

show_symbol=$(run_mysql "USE ${DATABASE}; SHOW CREATE TABLE child_symbol;")
expect_contains "FK index symbol names generated child index" \
    "$show_symbol" "KEY \`parent_idx\` (\`parent_id\`)"
expect_contains "FK index symbol keeps generated constraint name" \
    "$show_symbol" "CONSTRAINT \`child_symbol_ibfk_1\` FOREIGN KEY"

show_named_symbol=$(run_mysql "USE ${DATABASE}; SHOW CREATE TABLE child_named_symbol;")
expect_contains "explicit constraint names generated child index" \
    "$show_named_symbol" "KEY \`fk_named\` (\`parent_id\`)"
expect_contains "explicit constraint name renders constraint" \
    "$show_named_symbol" "CONSTRAINT \`fk_named\` FOREIGN KEY"

show_existing_symbol=$(run_mysql "USE ${DATABASE}; SHOW CREATE TABLE child_existing_symbol;")
expect_contains "existing child index is reused" \
    "$show_existing_symbol" "KEY \`existing_parent\` (\`parent_id\`)"
case "$show_existing_symbol" in
    *"KEY \`ignored_existing\`"*) fail "existing child index should hide FK index symbol" ;;
    *) ;;
esac

show_cascade=$(run_mysql "USE ${DATABASE}; SHOW CREATE TABLE child_cascade;")
expect_contains "cascade show create action order" \
    "$show_cascade" "ON DELETE CASCADE ON UPDATE CASCADE"

show_restrict=$(run_mysql "USE ${DATABASE}; SHOW CREATE TABLE child_restrict;")
expect_contains "restrict show create actions" \
    "$show_restrict" "ON DELETE RESTRICT ON UPDATE RESTRICT"

show_noaction=$(run_mysql "USE ${DATABASE}; SHOW CREATE TABLE child_noaction;")
case "$show_noaction" in
    *"NO ACTION"*) fail "explicit NO ACTION should be omitted from SHOW CREATE" ;;
    *) ;;
esac

referential=$(run_mysql \
    "USE ${DATABASE};
     SELECT TABLE_NAME, UPDATE_RULE, DELETE_RULE
       FROM INFORMATION_SCHEMA.REFERENTIAL_CONSTRAINTS
      WHERE CONSTRAINT_SCHEMA = '${DATABASE}'
      ORDER BY TABLE_NAME;")
expect_value "referential rule rows" \
    "alter_child	NO ACTION	CASCADE
child_cascade	CASCADE	CASCADE
child_existing_symbol	NO ACTION	NO ACTION
child_named_symbol	NO ACTION	NO ACTION
child_noaction	NO ACTION	NO ACTION
child_restrict	RESTRICT	RESTRICT
child_symbol	NO ACTION	NO ACTION" \
    "$referential"

statistics=$(run_mysql \
    "USE ${DATABASE};
     SELECT TABLE_NAME, INDEX_NAME, COLUMN_NAME
       FROM INFORMATION_SCHEMA.STATISTICS
      WHERE TABLE_SCHEMA = '${DATABASE}' AND TABLE_NAME LIKE 'child_%'
      ORDER BY TABLE_NAME, INDEX_NAME, SEQ_IN_INDEX;")
expect_value "foreign key child index metadata" \
    "child_cascade	fk_cascade	parent_id
child_existing_symbol	existing_parent	parent_id
child_named_symbol	fk_named	parent_id
child_noaction	parent_id	parent_id
child_restrict	parent_id	parent_id
child_symbol	parent_idx	parent_id" \
    "$statistics"

cascade_rows=$(run_mysql \
    "USE ${DATABASE};
     INSERT INTO parent VALUES (1),(2),(3),(4);
     INSERT INTO child_cascade VALUES (1),(1),(2),(NULL);
     UPDATE parent SET id = id + 10 WHERE id = 1;
     SELECT ROW_COUNT(), @@warning_count;
     SELECT parent_id FROM child_cascade ORDER BY parent_id IS NULL, parent_id;
     DELETE FROM parent WHERE id = 11;
     SELECT ROW_COUNT(), @@warning_count;
     SELECT parent_id FROM child_cascade ORDER BY parent_id IS NULL, parent_id;")
expect_value "direct cascade update and delete rows" \
    "1	0
2
11
11
NULL
1	0
2
NULL" \
    "$cascade_rows"

composite_rows=$(run_mysql \
    "USE ${DATABASE};
     CREATE TABLE parent_pair(a INT NOT NULL, b INT NOT NULL, PRIMARY KEY(a,b)) ENGINE=InnoDB;
     CREATE TABLE child_pair(
       a INT,
       b INT,
       FOREIGN KEY pair_idx(a,b) REFERENCES parent_pair(a,b) ON DELETE CASCADE
     ) ENGINE=InnoDB;
     INSERT INTO parent_pair VALUES (1,1),(1,2),(2,1);
     INSERT INTO child_pair VALUES (1,1),(1,2),(2,1),(NULL,2);
     DELETE FROM parent_pair WHERE a = 1;
     SELECT ROW_COUNT(), @@warning_count;
     SELECT a, b FROM child_pair ORDER BY a IS NULL, a, b;")
expect_value "direct composite delete cascade rows" \
    "2	0
2	1
NULL	2" \
    "$composite_rows"

run_mysql "USE ${DATABASE}; INSERT INTO child_restrict VALUES (3);" >/dev/null
expect_error \
    "restrict still rejects parent delete" \
    1451 \
    "23000" \
    "Cannot delete or update a parent row" \
    "USE ${DATABASE}; DELETE FROM parent WHERE id = 3;"

cleanup

printf '%s\n' "mysql_baseline_foreign_key_symbol_actions_expectations: ok"
