#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"

fail() {
    printf '%s\n' "mysql_baseline_performance_schema_instance_tables_expectations: $1" >&2
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

version=$(run_mysql "SELECT VERSION();")
case "$version" in
    8.4.9*) ;;
    *) fail "expected MySQL 8.4.9 runtime, got [$version]" ;;
esac

tables="'cond_instances', 'mutex_instances', 'rwlock_instances', 'file_instances', 'socket_instances'"

columns_expected=$(
    printf '%b' \
        'cond_instances\tNAME\t1\tNO\tvarchar(128)\tMUL\tutf8mb4_0900_ai_ci\n' \
        'cond_instances\tOBJECT_INSTANCE_BEGIN\t2\tNO\tbigint unsigned\tPRI\tNULL\n' \
        'mutex_instances\tNAME\t1\tNO\tvarchar(128)\tMUL\tutf8mb4_0900_ai_ci\n' \
        'mutex_instances\tOBJECT_INSTANCE_BEGIN\t2\tNO\tbigint unsigned\tPRI\tNULL\n' \
        'mutex_instances\tLOCKED_BY_THREAD_ID\t3\tYES\tbigint unsigned\tMUL\tNULL\n' \
        'rwlock_instances\tNAME\t1\tNO\tvarchar(128)\tMUL\tutf8mb4_0900_ai_ci\n' \
        'rwlock_instances\tOBJECT_INSTANCE_BEGIN\t2\tNO\tbigint unsigned\tPRI\tNULL\n' \
        'rwlock_instances\tWRITE_LOCKED_BY_THREAD_ID\t3\tYES\tbigint unsigned\tMUL\tNULL\n' \
        'rwlock_instances\tREAD_LOCKED_BY_COUNT\t4\tNO\tint unsigned\t\tNULL\n' \
        'file_instances\tFILE_NAME\t1\tNO\tvarchar(512)\tPRI\tutf8mb4_0900_ai_ci\n' \
        'file_instances\tEVENT_NAME\t2\tNO\tvarchar(128)\tMUL\tutf8mb4_0900_ai_ci\n' \
        'file_instances\tOPEN_COUNT\t3\tNO\tint unsigned\t\tNULL\n' \
        'socket_instances\tEVENT_NAME\t1\tNO\tvarchar(128)\t\tutf8mb4_0900_ai_ci\n' \
        'socket_instances\tOBJECT_INSTANCE_BEGIN\t2\tNO\tbigint unsigned\tPRI\tNULL\n' \
        'socket_instances\tTHREAD_ID\t3\tYES\tbigint unsigned\tMUL\tNULL\n' \
        'socket_instances\tSOCKET_ID\t4\tNO\tint\tMUL\tNULL\n' \
        'socket_instances\tIP\t5\tNO\tvarchar(64)\tMUL\tutf8mb4_0900_ai_ci\n' \
        'socket_instances\tPORT\t6\tNO\tint\t\tNULL\n' \
        "socket_instances\tSTATE\t7\tNO\tenum('IDLE','ACTIVE')\t\tutf8mb4_0900_ai_ci"
)
expect_output \
    "Performance Schema instance columns" \
    "$columns_expected" \
    "SELECT TABLE_NAME, COLUMN_NAME, ORDINAL_POSITION, IS_NULLABLE, COLUMN_TYPE,
            COLUMN_KEY, COALESCE(COLLATION_NAME, 'NULL')
       FROM information_schema.columns
      WHERE TABLE_SCHEMA = 'performance_schema'
        AND TABLE_NAME IN ($tables)
      ORDER BY FIELD(TABLE_NAME, 'cond_instances', 'mutex_instances', 'rwlock_instances',
                     'file_instances', 'socket_instances'),
               ORDINAL_POSITION;"

expect_output \
    "Performance Schema instance statistics" \
    "$(printf '%b' 'cond_instances\tNAME\t1\t1\tNAME\t1\t1\tHASH\tYES\n' \
        'cond_instances\tPRIMARY\t0\t1\tOBJECT_INSTANCE_BEGIN\t1\t1\tHASH\tYES\n' \
        'mutex_instances\tLOCKED_BY_THREAD_ID\t1\t1\tLOCKED_BY_THREAD_ID\t1\t1\tHASH\tYES\n' \
        'mutex_instances\tNAME\t1\t1\tNAME\t1\t1\tHASH\tYES\n' \
        'mutex_instances\tPRIMARY\t0\t1\tOBJECT_INSTANCE_BEGIN\t1\t1\tHASH\tYES\n' \
        'rwlock_instances\tNAME\t1\t1\tNAME\t1\t1\tHASH\tYES\n' \
        'rwlock_instances\tPRIMARY\t0\t1\tOBJECT_INSTANCE_BEGIN\t1\t1\tHASH\tYES\n' \
        'rwlock_instances\tWRITE_LOCKED_BY_THREAD_ID\t1\t1\tWRITE_LOCKED_BY_THREAD_ID\t1\t1\tHASH\tYES\n' \
        'file_instances\tEVENT_NAME\t1\t1\tEVENT_NAME\t1\t1\tHASH\tYES\n' \
        'file_instances\tPRIMARY\t0\t1\tFILE_NAME\t1\t1\tHASH\tYES\n' \
        'socket_instances\tIP\t1\t1\tIP\t1\t1\tHASH\tYES\n' \
        'socket_instances\tIP\t1\t2\tPORT\t1\t1\tHASH\tYES\n' \
        'socket_instances\tPRIMARY\t0\t1\tOBJECT_INSTANCE_BEGIN\t1\t1\tHASH\tYES\n' \
        'socket_instances\tSOCKET_ID\t1\t1\tSOCKET_ID\t1\t1\tHASH\tYES\n' \
        'socket_instances\tTHREAD_ID\t1\t1\tTHREAD_ID\t1\t1\tHASH\tYES')" \
    "SELECT TABLE_NAME, INDEX_NAME, NON_UNIQUE, SEQ_IN_INDEX, COLUMN_NAME,
            COLLATION IS NULL, CARDINALITY IS NULL, INDEX_TYPE, IS_VISIBLE
       FROM information_schema.statistics
      WHERE TABLE_SCHEMA = 'performance_schema'
        AND TABLE_NAME IN ($tables)
      ORDER BY FIELD(TABLE_NAME, 'cond_instances', 'mutex_instances', 'rwlock_instances',
                     'file_instances', 'socket_instances'),
               INDEX_NAME, SEQ_IN_INDEX;"

expect_output \
    "Performance Schema instance constraints" \
    "$(printf '%b' 'cond_instances\tPRIMARY\tPRIMARY KEY\tYES\n' \
        'mutex_instances\tPRIMARY\tPRIMARY KEY\tYES\n' \
        'rwlock_instances\tPRIMARY\tPRIMARY KEY\tYES\n' \
        'file_instances\tPRIMARY\tPRIMARY KEY\tYES\n' \
        'socket_instances\tPRIMARY\tPRIMARY KEY\tYES')" \
    "SELECT TABLE_NAME, CONSTRAINT_NAME, CONSTRAINT_TYPE, ENFORCED
       FROM information_schema.table_constraints
      WHERE TABLE_SCHEMA = 'performance_schema'
        AND TABLE_NAME IN ($tables)
      ORDER BY FIELD(TABLE_NAME, 'cond_instances', 'mutex_instances', 'rwlock_instances',
                     'file_instances', 'socket_instances'),
               CONSTRAINT_NAME;"

expect_output \
    "Performance Schema instance table rows" \
    "$(printf '%b' 'cond_instances\tPERFORMANCE_SCHEMA\tDynamic\tNULL\tutf8mb4_0900_ai_ci\n' \
        'mutex_instances\tPERFORMANCE_SCHEMA\tDynamic\tNULL\tutf8mb4_0900_ai_ci\n' \
        'rwlock_instances\tPERFORMANCE_SCHEMA\tDynamic\tNULL\tutf8mb4_0900_ai_ci\n' \
        'file_instances\tPERFORMANCE_SCHEMA\tDynamic\tNULL\tutf8mb4_0900_ai_ci\n' \
        'socket_instances\tPERFORMANCE_SCHEMA\tDynamic\tNULL\tutf8mb4_0900_ai_ci')" \
    "SELECT TABLE_NAME, ENGINE, ROW_FORMAT, COALESCE(AUTO_INCREMENT, 'NULL'), TABLE_COLLATION
       FROM information_schema.tables
      WHERE TABLE_SCHEMA = 'performance_schema'
        AND TABLE_NAME IN ($tables)
      ORDER BY FIELD(TABLE_NAME, 'cond_instances', 'mutex_instances', 'rwlock_instances',
                     'file_instances', 'socket_instances');"

printf '%s\n' "mysql_baseline_performance_schema_instance_tables_expectations: ok"
