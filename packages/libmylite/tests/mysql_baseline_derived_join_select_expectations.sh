#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
MYSQL_BIN="${MYLITE_MYSQL_BIN:-}"
MYSQL_SOCKET="${MYLITE_MYSQL_SOCKET:-}"
DATABASE="mylite_derived_join_select_expectations_$$"

fail() {
    printf '%s\n' "mysql_baseline_derived_join_select_expectations: $1" >&2
    exit 1
}

run_mysql() {
    sql=$1
    shift
    if [ -n "$MYSQL_BIN" ]; then
        if [ -n "$MYSQL_SOCKET" ]; then
            printf '%s\n' "$sql" \
                | "$MYSQL_BIN" --protocol=SOCKET --socket="$MYSQL_SOCKET" -uroot \
                    --batch --raw --skip-column-names --default-character-set=utf8mb4 "$@"
        else
            printf '%s\n' "$sql" \
                | "$MYSQL_BIN" --protocol=TCP -h127.0.0.1 -uroot \
                    --batch --raw --skip-column-names --default-character-set=utf8mb4 "$@"
        fi
    else
        printf '%s\n' "$sql" \
            | docker exec -i "$MYSQL_CONTAINER" mysql --protocol=TCP -h127.0.0.1 -uroot \
                --batch --raw --skip-column-names --default-character-set=utf8mb4 "$@"
    fi
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
    "CREATE TABLE users (ID BIGINT NOT NULL, user_login VARCHAR(30) NOT NULL); "\
"CREATE TABLE posts (ID BIGINT NOT NULL, post_author BIGINT NOT NULL, "\
"post_type VARCHAR(20) NOT NULL, post_status VARCHAR(20) NOT NULL); "\
"INSERT INTO users VALUES (1,'alice'),(2,'bob'),(3,'carol'); "\
"INSERT INTO posts VALUES "\
"(10,1,'post','publish'),(11,1,'post','publish'),"\
"(12,2,'post','publish'),(13,2,'post','publish'),(14,2,'post','publish'),"\
"(15,3,'post','publish'),(16,3,'page','publish'),(17,3,'post','draft');" \
    "$DATABASE" >/dev/null

expect_output \
    "left join derived grouped source orders by aggregate alias" \
    "3
1
2" \
    "SELECT users.ID FROM users LEFT OUTER JOIN "\
"(SELECT post_author, COUNT(*) AS post_count FROM posts "\
"WHERE ((post_type = 'post' AND (post_status = 'publish'))) GROUP BY post_author) p "\
"ON (users.ID = p.post_author) WHERE 1=1 ORDER BY post_count ASC;" \
    "$DATABASE"

expect_output \
    "derived aggregate alias can be qualified" \
    "3	1
1	2
2	3" \
    "SELECT users.ID, p.post_count FROM users LEFT JOIN "\
"(SELECT post_author, COUNT(*) AS post_count FROM posts "\
"WHERE post_type = 'post' AND post_status = 'publish' GROUP BY post_author) AS p "\
"ON users.ID = p.post_author ORDER BY p.post_count ASC;" \
    "$DATABASE"

expect_error \
    "derived table requires alias" \
    1248 \
    42000 \
    "Every derived table must have its own alias" \
    "SELECT users.ID FROM users LEFT JOIN "\
"(SELECT post_author, COUNT(*) AS post_count FROM posts GROUP BY post_author) "\
"ON users.ID = post_author;" \
    "$DATABASE"

printf '%s\n' "mysql_baseline_derived_join_select_expectations: ok"
