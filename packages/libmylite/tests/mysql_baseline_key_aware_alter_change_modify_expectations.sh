#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_key_aware_alter_change_modify_expectations_$$"

fail() {
    printf '%s\n' "mysql_baseline_key_aware_alter_change_modify_expectations: $1" >&2
    exit 1
}

run_mysql() {
    sql=$1
    shift
    printf '%s\n' "$sql" \
        | docker exec -i "$MYSQL_CONTAINER" mysql -uroot --default-character-set=utf8mb4 \
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
run_mysql "CREATE DATABASE ${DATABASE};" >/dev/null

expect_output \
    "modify non-key column on keyed table" \
    "2	0
keyed	CREATE TABLE \`keyed\` (
  \`id\` int NOT NULL,
  \`k\` varchar(20) NOT NULL,
  \`v\` bigint DEFAULT NULL,
  PRIMARY KEY (\`id\`),
  KEY \`k_idx\` (\`k\`(10)),
  KEY \`v_idx\` (\`v\`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
1	aa	10
2	bb	20" \
    "USE ${DATABASE};
     CREATE TABLE keyed (
         id INT NOT NULL PRIMARY KEY,
         k VARCHAR(20) NOT NULL,
         v INT,
         KEY k_idx (k(10)),
         KEY v_idx (v)
     );
     INSERT INTO keyed VALUES (1, 'aa', 10), (2, 'bb', 20);
     ALTER TABLE keyed MODIFY COLUMN v BIGINT;
     SELECT ROW_COUNT(), @@warning_count;
     SHOW CREATE TABLE keyed;
     SELECT id, k, v FROM keyed ORDER BY id;"

expect_output \
    "change indexed column name preserves key metadata" \
    "0	0
keyed	CREATE TABLE \`keyed\` (
  \`id\` int NOT NULL,
  \`kk\` varchar(20) NOT NULL,
  \`v\` bigint DEFAULT NULL,
  PRIMARY KEY (\`id\`),
  KEY \`k_idx\` (\`kk\`(10)),
  KEY \`v_idx\` (\`v\`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
1	aa	10
2	bb	20" \
    "USE ${DATABASE};
     ALTER TABLE keyed CHANGE COLUMN k kk VARCHAR(20) NOT NULL;
     SELECT ROW_COUNT(), @@warning_count;
     SHOW CREATE TABLE keyed;
     SELECT id, kk, v FROM keyed ORDER BY id;"

expect_output \
    "modify primary key keeps omitted nullability not null" \
    "2	0
pkprobe	CREATE TABLE \`pkprobe\` (
  \`id\` bigint NOT NULL,
  \`v\` int DEFAULT NULL,
  PRIMARY KEY (\`id\`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci" \
    "USE ${DATABASE};
     CREATE TABLE pkprobe (id INT PRIMARY KEY, v INT);
     INSERT INTO pkprobe VALUES (1, 10), (2, 20);
     ALTER TABLE pkprobe MODIFY COLUMN id BIGINT;
     SELECT ROW_COUNT(), @@warning_count;
     SHOW CREATE TABLE pkprobe;"

expect_error \
    "explicit null primary key replacement" \
    1171 \
    "42000" \
    "All parts of a PRIMARY KEY must be NOT NULL" \
    "USE ${DATABASE}; ALTER TABLE pkprobe MODIFY COLUMN id BIGINT NULL;"

expect_output \
    "change primary key keeps omitted nullability not null" \
    "2	0
pkchange	CREATE TABLE \`pkchange\` (
  \`id\` bigint NOT NULL,
  \`v\` int DEFAULT NULL,
  PRIMARY KEY (\`id\`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci" \
    "USE ${DATABASE};
     CREATE TABLE pkchange (id INT PRIMARY KEY, v INT);
     INSERT INTO pkchange VALUES (1, 10), (2, 20);
     ALTER TABLE pkchange CHANGE COLUMN id id BIGINT;
     SELECT ROW_COUNT(), @@warning_count;
     SHOW CREATE TABLE pkchange;"

expect_error \
    "explicit null primary key change replacement" \
    1171 \
    "42000" \
    "All parts of a PRIMARY KEY must be NOT NULL" \
    "USE ${DATABASE}; ALTER TABLE pkchange CHANGE COLUMN id id BIGINT NULL;"

expect_output \
    "mysql rewrites overlong prefix on shrink" \
    "prefix_shrink	CREATE TABLE \`prefix_shrink\` (
  \`k\` varchar(8) DEFAULT NULL,
  KEY \`k_idx\` (\`k\`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci" \
    "USE ${DATABASE};
     CREATE TABLE prefix_shrink (k VARCHAR(20), KEY k_idx(k(10)));
     ALTER TABLE prefix_shrink MODIFY COLUMN k VARCHAR(8);
     SHOW CREATE TABLE prefix_shrink;"

expect_output \
    "metadata fulltext survives compatible replacement" \
    "fulltext_probe	CREATE TABLE \`fulltext_probe\` (
  \`body\` varchar(120) DEFAULT NULL,
  FULLTEXT KEY \`ft_body\` (\`body\`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci" \
    "USE ${DATABASE};
     CREATE TABLE fulltext_probe (body VARCHAR(100), FULLTEXT KEY ft_body(body));
     ALTER TABLE fulltext_probe MODIFY COLUMN body VARCHAR(120);
     SHOW CREATE TABLE fulltext_probe;"

expect_output \
    "metadata fulltext follows changed column" \
    "fulltext_change	CREATE TABLE \`fulltext_change\` (
  \`content\` varchar(120) DEFAULT NULL,
  FULLTEXT KEY \`ft_body\` (\`content\`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci" \
    "USE ${DATABASE};
     CREATE TABLE fulltext_change (body VARCHAR(100), FULLTEXT KEY ft_body(body));
     ALTER TABLE fulltext_change CHANGE COLUMN body content VARCHAR(120);
     SHOW CREATE TABLE fulltext_change;"

cleanup
