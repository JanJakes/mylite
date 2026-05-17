#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_fk_set_null_actions_expectations_$$"

fail() {
    printf '%s\n' "mysql_baseline_foreign_key_set_null_actions_expectations: $1" >&2
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
     CREATE TABLE pdel(
       id INT NOT NULL PRIMARY KEY,
       b INT NOT NULL,
       UNIQUE KEY ub(id,b)
     ) ENGINE=InnoDB;
     CREATE TABLE pup(
       id INT NOT NULL PRIMARY KEY,
       b INT NOT NULL,
       UNIQUE KEY ub(id,b)
     ) ENGINE=InnoDB;
     CREATE TABLE cdel(
       id INT NOT NULL PRIMARY KEY,
       pid INT,
       pb INT,
       CONSTRAINT fk_del FOREIGN KEY(pid,pb) REFERENCES pdel(id,b)
         ON DELETE SET NULL
     ) ENGINE=InnoDB;
     CREATE TABLE cup(
       id INT NOT NULL PRIMARY KEY,
       pid INT,
       pb INT,
       CONSTRAINT fk_up FOREIGN KEY(pid,pb) REFERENCES pup(id,b)
         ON UPDATE SET NULL
     ) ENGINE=InnoDB;
     CREATE TABLE alter_child(pid INT) ENGINE=InnoDB;
     ALTER TABLE alter_child
       ADD FOREIGN KEY fk_alter(pid) REFERENCES pdel(id)
       ON DELETE SET NULL;" >/dev/null

show_cdel=$(run_mysql "USE ${DATABASE}; SHOW CREATE TABLE cdel;")
expect_contains "delete set null show create" \
    "$show_cdel" "CONSTRAINT \`fk_del\` FOREIGN KEY (\`pid\`, \`pb\`) REFERENCES \`pdel\` (\`id\`, \`b\`) ON DELETE SET NULL"

show_cup=$(run_mysql "USE ${DATABASE}; SHOW CREATE TABLE cup;")
expect_contains "update set null show create" \
    "$show_cup" "CONSTRAINT \`fk_up\` FOREIGN KEY (\`pid\`, \`pb\`) REFERENCES \`pup\` (\`id\`, \`b\`) ON UPDATE SET NULL"

referential=$(run_mysql \
    "USE ${DATABASE};
     SELECT TABLE_NAME, UPDATE_RULE, DELETE_RULE
       FROM INFORMATION_SCHEMA.REFERENTIAL_CONSTRAINTS
      WHERE CONSTRAINT_SCHEMA = '${DATABASE}'
      ORDER BY TABLE_NAME;")
expect_value "referential set null rules" \
    "alter_child	NO ACTION	SET NULL
cdel	NO ACTION	SET NULL
cup	SET NULL	NO ACTION" \
    "$referential"

delete_rows=$(run_mysql \
    "USE ${DATABASE};
     INSERT INTO pdel VALUES (1,10),(2,20),(3,30);
     INSERT INTO cdel VALUES (1,1,10),(2,2,20),(3,NULL,30),(4,3,NULL);
     DELETE FROM pdel WHERE id = 1;
     SELECT ROW_COUNT(), @@warning_count;
     SELECT id, pid, pb FROM cdel ORDER BY id;")
expect_value "delete set null rows" \
    "1	0
1	NULL	NULL
2	2	20
3	NULL	30
4	3	NULL" \
    "$delete_rows"

update_rows=$(run_mysql \
    "USE ${DATABASE};
     INSERT INTO pup VALUES (1,10),(2,20),(3,30);
     INSERT INTO cup VALUES (1,1,10),(2,2,20),(3,NULL,30),(4,3,NULL);
     UPDATE pup SET id = 4 WHERE id = 2;
     SELECT ROW_COUNT(), @@warning_count;
     SELECT id, pid, pb FROM cup ORDER BY id;")
expect_value "update set null rows" \
    "1	0
1	1	10
2	NULL	NULL
3	NULL	30
4	3	NULL" \
    "$update_rows"

expect_error \
    "create set null rejects not null child column" \
    1830 \
    "HY000" \
    "Column 'pid' cannot be NOT NULL: needed in a foreign key constraint 'create_not_null_ibfk_1' SET NULL" \
    "USE ${DATABASE};
     CREATE TABLE create_not_null(
       pid INT NOT NULL,
       FOREIGN KEY(pid) REFERENCES pdel(id) ON DELETE SET NULL
     ) ENGINE=InnoDB;"

expect_error \
    "alter set null rejects not null child column" \
    1830 \
    "HY000" \
    "Column 'pid' cannot be NOT NULL: needed in a foreign key constraint 'alter_not_null_ibfk_1' SET NULL" \
    "USE ${DATABASE};
     CREATE TABLE alter_not_null(pid INT NOT NULL) ENGINE=InnoDB;
     ALTER TABLE alter_not_null
       ADD FOREIGN KEY(pid) REFERENCES pdel(id) ON UPDATE SET NULL;"

set_default_show=$(run_mysql \
    "USE ${DATABASE};
     CREATE TABLE default_child(
       pid INT DEFAULT 2,
       FOREIGN KEY(pid) REFERENCES pdel(id) ON DELETE SET DEFAULT ON UPDATE SET DEFAULT
     ) ENGINE=InnoDB;
     SHOW CREATE TABLE default_child;")
expect_contains "mysql stores set default metadata even though mylite defers it" \
    "$set_default_show" "ON DELETE SET DEFAULT ON UPDATE SET DEFAULT"

expect_error \
    "set default delete remains restrictive in tested innodb behavior" \
    1451 \
    "23000" \
    "Cannot delete or update a parent row" \
    "USE ${DATABASE};
     INSERT INTO default_child VALUES (2);
     DELETE FROM pdel WHERE id = 2;"

printf '%s\n' "mysql_baseline_foreign_key_set_null_actions_expectations: ok"
