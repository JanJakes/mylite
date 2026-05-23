#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_group_by_pk_projection_expectations_$$"

fail() {
    printf '%s\n' "mysql_baseline_group_by_primary_key_projection_expectations: $1" >&2
    exit 1
}

run_mysql() {
    sql=$1
    shift
    printf '%s\n' "$sql" \
        | docker exec -i "$MYSQL_CONTAINER" mysql --protocol=TCP -h127.0.0.1 -uroot --batch --raw --skip-column-names "$@"
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
     CREATE TABLE posts(
       id INT NOT NULL PRIMARY KEY,
       title VARCHAR(20) NULL,
       created DATETIME NULL,
       status VARCHAR(20) NULL
     ) ENGINE=InnoDB;
     CREATE TABLE comments(
       id INT NOT NULL PRIMARY KEY,
       post_id INT NULL,
       body VARCHAR(20) NULL,
       score INT NULL
     ) ENGINE=InnoDB;
     CREATE TABLE cpk(
       a INT NOT NULL,
       b INT NOT NULL,
       v VARCHAR(20) NULL,
       PRIMARY KEY(a, b)
     ) ENGINE=InnoDB;
     CREATE TABLE no_pk(
       id INT NOT NULL,
       title VARCHAR(20) NULL
     ) ENGINE=InnoDB;
     INSERT INTO posts VALUES
       (1, 'Alpha', '2024-01-01 00:00:00', 'publish'),
       (2, 'Beta', '2024-01-02 00:00:00', 'draft'),
       (3, 'Gamma', '2024-01-03 00:00:00', 'publish');
     INSERT INTO comments VALUES
       (10, 1, 'c1', 5),
       (11, 1, 'c2', 7),
       (12, 2, 'c3', NULL),
       (13, 99, 'orphan', 3);
     INSERT INTO cpk VALUES
       (1, 1, 'aa'),
       (1, 2, 'ab'),
       (2, 1, 'ba');
     INSERT INTO no_pk VALUES
       (1, 'x'),
       (1, 'y');" >/dev/null

core=$(run_mysql \
    "USE ${DATABASE};
     DO 0;
     SELECT id, title, created, status
       FROM posts GROUP BY id ORDER BY created DESC LIMIT 2;
     SELECT p.id, p.title, COUNT(c.id) AS c
       FROM posts AS p LEFT JOIN comments AS c ON p.id = c.post_id
       GROUP BY p.id ORDER BY p.title;
     SELECT p.*
       FROM posts AS p LEFT JOIN comments AS c ON p.id = c.post_id
       GROUP BY p.id ORDER BY p.created DESC LIMIT 2;
     SELECT a, b, v FROM cpk GROUP BY a, b ORDER BY a, b;
     SELECT title FROM posts GROUP BY id ORDER BY title;
     SELECT id, COUNT(*) AS c FROM posts GROUP BY id ORDER BY title;
     SELECT @@warning_count, ROW_COUNT();"
)
expect_value "single explicit first" "3	Gamma	2024-01-03 00:00:00	publish" \
    "$(printf '%s\n' "$core" | sed -n '1p')"
expect_value "single explicit second" "2	Beta	2024-01-02 00:00:00	draft" \
    "$(printf '%s\n' "$core" | sed -n '2p')"
expect_value "joined projection first" "1	Alpha	2" "$(printf '%s\n' "$core" | sed -n '3p')"
expect_value "joined projection second" "2	Beta	1" "$(printf '%s\n' "$core" | sed -n '4p')"
expect_value "joined projection third" "3	Gamma	0" "$(printf '%s\n' "$core" | sed -n '5p')"
expect_value "qualified wildcard first" "3	Gamma	2024-01-03 00:00:00	publish" \
    "$(printf '%s\n' "$core" | sed -n '6p')"
expect_value "qualified wildcard second" "2	Beta	2024-01-02 00:00:00	draft" \
    "$(printf '%s\n' "$core" | sed -n '7p')"
expect_value "composite first" "1	1	aa" "$(printf '%s\n' "$core" | sed -n '8p')"
expect_value "composite second" "1	2	ab" "$(printf '%s\n' "$core" | sed -n '9p')"
expect_value "composite third" "2	1	ba" "$(printf '%s\n' "$core" | sed -n '10p')"
expect_value "unselected group key order first" "Alpha" "$(printf '%s\n' "$core" | sed -n '11p')"
expect_value "unselected group key order second" "Beta" "$(printf '%s\n' "$core" | sed -n '12p')"
expect_value "unselected group key order third" "Gamma" "$(printf '%s\n' "$core" | sed -n '13p')"
expect_value "unselected fd order first" "1	1" "$(printf '%s\n' "$core" | sed -n '14p')"
expect_value "unselected fd order second" "2	1" "$(printf '%s\n' "$core" | sed -n '15p')"
expect_value "unselected fd order third" "3	1" "$(printf '%s\n' "$core" | sed -n '16p')"
expect_value "status" "0	-1" "$(printf '%s\n' "$core" | sed -n '17p')"

expect_error \
    "no primary key selected column" \
    1055 \
    "42000" \
    "Expression #1 of SELECT list is not in GROUP BY clause" \
    "USE ${DATABASE}; SELECT title FROM no_pk GROUP BY id;"
expect_error \
    "partial composite primary key" \
    1055 \
    "42000" \
    "Expression #2 of SELECT list is not in GROUP BY clause" \
    "USE ${DATABASE}; SELECT a, b, v FROM cpk GROUP BY a;"
expect_error \
    "right source selected column" \
    1055 \
    "42000" \
    "Expression #2 of SELECT list is not in GROUP BY clause" \
    "USE ${DATABASE};
     SELECT p.id, c.body, COUNT(c.id)
       FROM posts AS p LEFT JOIN comments AS c ON p.id = c.post_id
       GROUP BY p.id;"
expect_error \
    "joined unqualified wildcard" \
    1055 \
    "42000" \
    "Expression #5 of SELECT list is not in GROUP BY clause" \
    "USE ${DATABASE};
     SELECT *
       FROM posts AS p LEFT JOIN comments AS c ON p.id = c.post_id
       GROUP BY p.id;"
expect_error \
    "nondependent order by" \
    1055 \
    "42000" \
    "Expression #1 of ORDER BY clause is not in GROUP BY clause" \
    "USE ${DATABASE};
     SELECT p.id, COUNT(c.id) AS c
       FROM posts AS p LEFT JOIN comments AS c ON p.id = c.post_id
       GROUP BY p.id ORDER BY c.body;"

cleanup

printf '%s\n' "mysql_baseline_group_by_primary_key_projection_expectations: ok"
