#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
TAB=$(printf '\t')

fail() {
    printf '%s\n' "mysql_baseline_o_optimizer_system_variables_expectations: $1" >&2
    exit 1
}

run_mysql() {
    sql=$1
    shift
    printf '%s\n' "$sql" \
        | docker exec -i "$MYSQL_CONTAINER" mysql \
            --protocol=TCP \
            -h127.0.0.1 \
            -uroot \
            --batch \
            --raw \
            --skip-column-names \
            --default-character-set=utf8mb4 \
            "$@"
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

    set +e
    output=$(run_mysql "$sql" 2>&1)
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

normalize_tsv() {
    sed "s/${TAB}/|/g"
}

optimizer_switch_default() {
    cat <<'EOF'
index_merge=on,index_merge_union=on,index_merge_sort_union=on,index_merge_intersection=on,engine_condition_pushdown=on,index_condition_pushdown=on,mrr=on,mrr_cost_based=on,block_nested_loop=on,batched_key_access=off,materialization=on,semijoin=on,loosescan=on,firstmatch=on,duplicateweedout=on,subquery_materialization_cost_based=on,use_index_extensions=on,condition_fanout_filter=on,derived_merge=on,use_invisible_indexes=off,skip_scan=on,hash_join=on,subquery_to_derived=off,prefer_ordering_index=on,hypergraph_optimizer=off,derived_condition_pushdown=on,hash_set_operations=on
EOF
}

variables() {
    cat <<EOF
optimizer_prune_level|1
optimizer_search_depth|62
optimizer_switch|$(optimizer_switch_default)
optimizer_trace|enabled=off,one_line=off
optimizer_trace_features|greedy_search=on,range_optimizer=on,dynamic_range=on,repeated_subselect=on
optimizer_trace_limit|1
optimizer_trace_max_mem_size|1048576
optimizer_trace_offset|-1
parser_max_mem_size|18446744073709551615
partial_revokes|OFF
EOF
}

variable_names_in_clause() {
    variables | awk -F'|' '{printf sep "'\''" $1 "'\''"; sep=","}'
}

expected_show_rows() {
    variables | awk -F'|' '{print $1 "|" $2}'
}

reset_defaults() {
    run_mysql \
        "SET GLOBAL optimizer_prune_level = DEFAULT;
         SET GLOBAL optimizer_search_depth = DEFAULT;
         SET GLOBAL optimizer_switch = DEFAULT;
         SET GLOBAL optimizer_trace = DEFAULT;
         SET GLOBAL optimizer_trace_features = DEFAULT;
         SET GLOBAL optimizer_trace_limit = DEFAULT;
         SET GLOBAL optimizer_trace_max_mem_size = DEFAULT;
         SET GLOBAL optimizer_trace_offset = DEFAULT;
         SET GLOBAL parser_max_mem_size = DEFAULT;
         SET GLOBAL partial_revokes = OFF;
         SET SESSION optimizer_prune_level = DEFAULT;
         SET SESSION optimizer_search_depth = DEFAULT;
         SET SESSION optimizer_switch = DEFAULT;
         SET SESSION optimizer_trace = DEFAULT;
         SET SESSION optimizer_trace_features = DEFAULT;
         SET SESSION optimizer_trace_limit = DEFAULT;
         SET SESSION optimizer_trace_max_mem_size = DEFAULT;
         SET SESSION optimizer_trace_offset = DEFAULT;
         SET SESSION parser_max_mem_size = DEFAULT;" >/dev/null
}

version=$(run_mysql 'SELECT VERSION();')
case "$version" in
    8.4.9*) ;;
    *) fail "expected MySQL 8.4.9 runtime, got [$version]" ;;
esac

reset_defaults

scalar_values=$(
    run_mysql \
        "SELECT @@optimizer_prune_level, @@GLOBAL.optimizer_prune_level,
                @@optimizer_search_depth, @@SESSION.optimizer_search_depth,
                @@optimizer_switch, @@optimizer_trace, @@optimizer_trace_features,
                @@optimizer_trace_limit, @@optimizer_trace_max_mem_size,
                @@optimizer_trace_offset, @@parser_max_mem_size, @@GLOBAL.partial_revokes;" \
        | normalize_tsv
)
expect_value \
    "default scalar values" \
    "1|1|62|62|$(optimizer_switch_default)|enabled=off,one_line=off|greedy_search=on,range_optimizer=on,dynamic_range=on,repeated_subselect=on|1|1048576|-1|18446744073709551615|0" \
    "$scalar_values"

show_default=$(
    run_mysql "SHOW VARIABLES WHERE Variable_name IN ($(variable_names_in_clause));" \
        | normalize_tsv
)
expect_value "show default rows" "$(expected_show_rows)" "$show_default"

show_global=$(
    run_mysql "SHOW GLOBAL VARIABLES WHERE Variable_name IN ($(variable_names_in_clause));" \
        | normalize_tsv
)
expect_value "show global rows" "$(expected_show_rows)" "$show_global"

show_session=$(
    run_mysql "SHOW SESSION VARIABLES WHERE Variable_name IN ($(variable_names_in_clause));" \
        | normalize_tsv
)
expect_value "show session rows" "$(expected_show_rows)" "$show_session"

assignment_values=$(
    run_mysql \
        "SET SESSION optimizer_prune_level = 2;
         SHOW WARNINGS LIMIT 1;
         SET SESSION optimizer_search_depth = -1;
         SET SESSION optimizer_trace_limit = 18446744073709551615;
         SET SESSION optimizer_trace_max_mem_size = 18446744073709551615;
         SET SESSION optimizer_trace_offset = -9223372036854775808;
         SET SESSION parser_max_mem_size = 0;
         SELECT @@optimizer_prune_level, @@optimizer_search_depth,
                @@optimizer_trace_limit, @@optimizer_trace_max_mem_size,
                @@optimizer_trace_offset, @@parser_max_mem_size;" \
        | normalize_tsv
)
expect_value \
    "numeric assignment clamps" \
    "Warning|1292|Truncated incorrect optimizer_prune_level value: '2'
1|0|9223372036854775807|18446744073709551615|-9223372036854775807|10000000" \
    "$assignment_values"

flag_values=$(
    run_mysql \
        "SET SESSION optimizer_switch = 'index_merge=off,batched_key_access=on';
         SET SESSION optimizer_trace = 'enabled=on,one_line=on';
         SET SESSION optimizer_trace_features = 'greedy_search=off,range_optimizer=off';
         SELECT @@optimizer_switch, @@optimizer_trace, @@optimizer_trace_features;
         SET SESSION optimizer_switch = 'default';
         SET SESSION optimizer_trace = DEFAULT;
         SET SESSION optimizer_trace_features = 'default';
         SELECT @@optimizer_switch, @@optimizer_trace, @@optimizer_trace_features;" \
        | normalize_tsv
)
expect_value \
    "flag assignment and defaults" \
    "index_merge=off,index_merge_union=on,index_merge_sort_union=on,index_merge_intersection=on,engine_condition_pushdown=on,index_condition_pushdown=on,mrr=on,mrr_cost_based=on,block_nested_loop=on,batched_key_access=on,materialization=on,semijoin=on,loosescan=on,firstmatch=on,duplicateweedout=on,subquery_materialization_cost_based=on,use_index_extensions=on,condition_fanout_filter=on,derived_merge=on,use_invisible_indexes=off,skip_scan=on,hash_join=on,subquery_to_derived=off,prefer_ordering_index=on,hypergraph_optimizer=off,derived_condition_pushdown=on,hash_set_operations=on|enabled=on,one_line=on|greedy_search=off,range_optimizer=off,dynamic_range=on,repeated_subselect=on
$(optimizer_switch_default)|enabled=off,one_line=off|greedy_search=on,range_optimizer=on,dynamic_range=on,repeated_subselect=on" \
    "$flag_values"

user_variable_values=$(
    run_mysql \
        "SET @opt_switch = 'index_merge=off';
         SET SESSION optimizer_switch = @opt_switch;
         SET @opt_int = 2;
         SET SESSION optimizer_search_depth = @opt_int;
         SET @opt_offset = -2147483649;
         SET SESSION optimizer_trace_offset = @opt_offset;
         SELECT @@optimizer_switch, @@optimizer_search_depth, @@optimizer_trace_offset;" \
        | normalize_tsv
)
expect_value \
    "user-variable assignments" \
    "index_merge=off,index_merge_union=on,index_merge_sort_union=on,index_merge_intersection=on,engine_condition_pushdown=on,index_condition_pushdown=on,mrr=on,mrr_cost_based=on,block_nested_loop=on,batched_key_access=off,materialization=on,semijoin=on,loosescan=on,firstmatch=on,duplicateweedout=on,subquery_materialization_cost_based=on,use_index_extensions=on,condition_fanout_filter=on,derived_merge=on,use_invisible_indexes=off,skip_scan=on,hash_join=on,subquery_to_derived=off,prefer_ordering_index=on,hypergraph_optimizer=off,derived_condition_pushdown=on,hash_set_operations=on|2|-2147483649" \
    "$user_variable_values"

global_defaults=$(
    run_mysql \
        "SET GLOBAL optimizer_prune_level = DEFAULT;
         SET GLOBAL optimizer_trace_offset = DEFAULT;
         SET GLOBAL optimizer_switch = DEFAULT;
         SET GLOBAL partial_revokes = OFF;
         SET GLOBAL partial_revokes = FALSE;
         SET GLOBAL partial_revokes = 0;
         SELECT @@GLOBAL.optimizer_prune_level, @@GLOBAL.optimizer_trace_offset,
                @@GLOBAL.optimizer_switch, @@GLOBAL.partial_revokes;" \
        | normalize_tsv
)
expect_value \
    "global defaults" \
    "1|-1|$(optimizer_switch_default)|0" \
    "$global_defaults"

expect_error \
    "partial_revokes session assignment" \
    1229 \
    HY000 \
    "Variable 'partial_revokes' is a GLOBAL variable and should be set with SET GLOBAL" \
    "SET SESSION partial_revokes = ON;"
expect_error \
    "partial_revokes session read" \
    1238 \
    HY000 \
    "Variable 'partial_revokes' is a GLOBAL variable" \
    "SELECT @@SESSION.partial_revokes;"
expect_error \
    "invalid optimizer flag" \
    1231 \
    42000 \
    "Variable 'optimizer_switch' can't be set to the value of 'unknown_flag=on'" \
    "SET SESSION optimizer_switch = 'unknown_flag=on';"
expect_error \
    "invalid optimizer flag value" \
    1231 \
    42000 \
    "Variable 'optimizer_switch' can't be set to the value of 'index_merge=maybe'" \
    "SET SESSION optimizer_switch = 'index_merge=maybe';"
expect_error \
    "incorrect integer argument" \
    1232 \
    42000 \
    "Incorrect argument type to variable 'optimizer_prune_level'" \
    "SET SESSION optimizer_prune_level = 'bogus';"
expect_error \
    "incorrect parser NULL argument" \
    1232 \
    42000 \
    "Incorrect argument type to variable 'parser_max_mem_size'" \
    "SET SESSION parser_max_mem_size = NULL;"

reset_defaults

printf '%s\n' "mysql_baseline_o_optimizer_system_variables_expectations: ok"
