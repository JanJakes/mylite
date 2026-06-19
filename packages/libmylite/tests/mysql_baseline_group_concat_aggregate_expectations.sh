#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_group_concat_aggregate_$$"

fail() {
    printf '%s\n' "mysql_baseline_group_concat_aggregate_expectations: $1" >&2
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
run_mysql "CREATE DATABASE ${DATABASE};" >/dev/null

supported_expected=$(cat <<'EXPECTED'
ids_default	1,2,3,4,5,6
ids_asc	1,2,3,4,5,6
ids_desc	6,5,4,3,2,1
names_pipe	alpha|beta|delta|echo
names_double	alpha/beta/delta/echo
names_empty	alphabetadeltaecho
ifnull_values	alpha:beta::delta:echo:
concat_values	alphaA|betaB|deltaD|echoE
multi_expr	alpha1,beta2,delta4,echo5
multi_expr_separator	alpha:1|beta:2|delta:4|echo:5
sort_asc	delta:beta:alpha:echo
sort_desc	echo:alpha:beta:delta
where_filtered	alpha|beta
no_rows	<NULL>
all_null	<NULL>
grouped	1	alpha:beta
grouped	2	delta:echo
grouped	3	<NULL>
grouped_having	2	delta:echo
status	-1	0
space_ignore	alpha,beta,delta,echo
EXPECTED
)
expect_output \
    "supported group concat subset" \
    "$supported_expected" \
    "CREATE TABLE t(g INT, id INT, name VARCHAR(20), notes TEXT, sort_n INT NOT NULL) "\
"ENGINE=InnoDB; "\
"INSERT INTO t VALUES "\
"(1,2,'beta','B',0), "\
"(1,1,'alpha','A',5), "\
"(1,3,NULL,NULL,4), "\
"(2,4,'delta','D',-1), "\
"(2,5,'echo','E',7), "\
"(3,6,NULL,NULL,8); "\
"SELECT 'ids_default', GROUP_CONCAT(id ORDER BY id) FROM t; "\
"SELECT 'ids_asc', GROUP_CONCAT(id ORDER BY id ASC) FROM t; "\
"SELECT 'ids_desc', GROUP_CONCAT(id ORDER BY id DESC) FROM t; "\
"SELECT 'names_pipe', GROUP_CONCAT(name ORDER BY id SEPARATOR '|') FROM t; "\
"SELECT 'names_double', GROUP_CONCAT(name ORDER BY id SEPARATOR \"/\") FROM t; "\
"SELECT 'names_empty', GROUP_CONCAT(name ORDER BY id SEPARATOR '') FROM t; "\
"SELECT 'ifnull_values', GROUP_CONCAT(IFNULL(name, '') ORDER BY id SEPARATOR ':') FROM t; "\
"SELECT 'concat_values', GROUP_CONCAT(CONCAT(name, notes) ORDER BY id SEPARATOR '|') FROM t; "\
"SELECT 'multi_expr', GROUP_CONCAT(name, id ORDER BY id) FROM t; "\
"SELECT 'multi_expr_separator', GROUP_CONCAT(name, ':', id ORDER BY id SEPARATOR '|') FROM t; "\
"SELECT 'sort_asc', GROUP_CONCAT(name ORDER BY sort_n ASC SEPARATOR ':') FROM t; "\
"SELECT 'sort_desc', GROUP_CONCAT(name ORDER BY sort_n DESC SEPARATOR ':') FROM t; "\
"SELECT 'where_filtered', GROUP_CONCAT(name ORDER BY id SEPARATOR '|') FROM t WHERE g = 1; "\
"SELECT 'no_rows', IFNULL(GROUP_CONCAT(name ORDER BY id), '<NULL>') FROM t WHERE g = 99; "\
"SELECT 'all_null', IFNULL(GROUP_CONCAT(name ORDER BY id), '<NULL>') FROM t WHERE g = 3; "\
"SELECT 'grouped', g, IFNULL(GROUP_CONCAT(name ORDER BY id SEPARATOR ':'), '<NULL>') "\
"FROM t GROUP BY g ORDER BY g; "\
"SELECT 'grouped_having', g, GROUP_CONCAT(name ORDER BY id SEPARATOR ':') "\
"FROM t GROUP BY g HAVING g = 2; "\
"SELECT 'status', ROW_COUNT(), @@warning_count; "\
"SET SESSION sql_mode = 'IGNORE_SPACE'; "\
"SELECT 'space_ignore', GROUP_CONCAT (name ORDER BY id) FROM t;" \
    "$DATABASE"

wider_mysql_expected=$(cat <<'EXPECTED'
distinct	alpha,beta,delta,echo
ordinal_order	echo,delta,beta,alpha
expression_order	alpha,beta,delta,echo
multi_order	delta,beta,alpha,echo
nullable_order	low,null,zero,high
EXPECTED
)
expect_output \
    "wider mysql forms intentionally deferred by mylite" \
    "$wider_mysql_expected" \
"SET SESSION sql_mode = ''; "\
"SELECT 'distinct', GROUP_CONCAT(DISTINCT name ORDER BY name) FROM t; "\
"SELECT 'ordinal_order', GROUP_CONCAT(name ORDER BY 1 DESC) FROM t; "\
"SELECT 'expression_order', GROUP_CONCAT(name ORDER BY id + 1) FROM t; "\
"SELECT 'multi_order', GROUP_CONCAT(name ORDER BY sort_n, id DESC) FROM t; "\
"CREATE TABLE nullable_order(name VARCHAR(20), sort_n INT) ENGINE=InnoDB; "\
"INSERT INTO nullable_order VALUES ('low', -1), ('zero', 0), ('null', NULL), ('high', 5); "\
"SELECT 'nullable_order', GROUP_CONCAT(name ORDER BY sort_n) FROM nullable_order;" \
    "$DATABASE"

expect_error \
    "space without ignore_space" \
    1064 \
    42000 \
    "You have an error in your SQL syntax" \
    "SET SESSION sql_mode = ''; SELECT GROUP_CONCAT (name ORDER BY id) FROM t;" \
    "$DATABASE"

expect_error \
    "separator null syntax" \
    1064 \
    42000 \
    "You have an error in your SQL syntax" \
    "SELECT GROUP_CONCAT(name ORDER BY id SEPARATOR NULL) FROM t;" \
    "$DATABASE"

expect_error \
    "separator numeric syntax" \
    1064 \
    42000 \
    "You have an error in your SQL syntax" \
    "SELECT GROUP_CONCAT(name ORDER BY id SEPARATOR 1) FROM t;" \
    "$DATABASE"

printf '%s\n' "mysql_baseline_group_concat_aggregate_expectations: ok"
