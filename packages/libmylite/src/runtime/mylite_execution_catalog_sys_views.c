#include "mylite_execution_catalog.h"

#include <stddef.h>
#include <string.h>

static const char sys_version_view_definition[] =
    "select '2.1.3' AS `sys_version`,version() AS `mysql_version`";

static const char sys_version_show_create_view_sql[] =
    "CREATE ALGORITHM=UNDEFINED DEFINER=`mysql.sys`@`localhost` SQL SECURITY INVOKER VIEW "
    "`version` (`sys_version`,`mysql_version`) AS select '2.1.3' AS "
    "`sys_version`,version() AS `mysql_version`";

static const char sys_version_show_create_qualified_view_sql[] =
    "CREATE ALGORITHM=UNDEFINED DEFINER=`mysql.sys`@`localhost` SQL SECURITY INVOKER VIEW "
    "`sys`.`version` (`sys_version`,`mysql_version`) AS select '2.1.3' AS "
    "`sys_version`,version() AS `mysql_version`";

#define SYS_HOST_SUMMARY_VIEW_COLUMNS                                                              \
    "(`host`,`statements`,`statement_latency`,`statement_avg_latency`,`table_scans`,`file_ios`,"   \
    "`file_io_latency`,`current_connections`,`total_connections`,`unique_users`,`current_memory`," \
    "`total_memory_allocated`)"

#define SYS_HOST_SUMMARY_QUALIFIED_SELECT_SUFFIX                                                   \
    "sum(`performance_schema`.`accounts`.`CURRENT_CONNECTIONS`) AS `current_connections`,"         \
    "sum(`performance_schema`.`accounts`.`TOTAL_CONNECTIONS`) AS `total_connections`,count("       \
    "distinct `performance_schema`.`accounts`.`USER`) AS `unique_users`,"

#define SYS_HOST_SUMMARY_QUALIFIED_FROM                                                            \
    " from (((`performance_schema`.`accounts` join `sys`."                                         \
    "`x$host_summary_by_statement_latency` `stmt` on((`performance_schema`.`accounts`.`HOST` "     \
    "= `sys`.`stmt`.`host`))) join `sys`.`x$host_summary_by_file_io` `io` on(("                    \
    "`performance_schema`.`accounts`.`HOST` = `sys`.`io`.`host`))) join `sys`."                    \
    "`x$memory_by_host_by_current_bytes` `mem` on((`performance_schema`.`accounts`.`HOST` = "      \
    "`sys`.`mem`.`host`))) group by if((`performance_schema`.`accounts`.`HOST` is null),"          \
    "'background',`performance_schema`.`accounts`.`HOST`)"

#define SYS_HOST_SUMMARY_UNQUALIFIED_SELECT_SUFFIX                                                 \
    "sum(`performance_schema`.`accounts`.`CURRENT_CONNECTIONS`) AS `current_connections`,"         \
    "sum(`performance_schema`.`accounts`.`TOTAL_CONNECTIONS`) AS `total_connections`,count("       \
    "distinct `performance_schema`.`accounts`.`USER`) AS `unique_users`,"

#define SYS_HOST_SUMMARY_UNQUALIFIED_FROM                                                          \
    " from (((`performance_schema`.`accounts` join `x$host_summary_by_statement_latency` "         \
    "`stmt` on((`performance_schema`.`accounts`.`HOST` = `stmt`.`host`))) join "                   \
    "`x$host_summary_by_file_io` `io` on((`performance_schema`.`accounts`.`HOST` = "               \
    "`io`.`host`))) join `x$memory_by_host_by_current_bytes` `mem` on(("                           \
    "`performance_schema`.`accounts`.`HOST` = `mem`.`host`))) group by if(("                       \
    "`performance_schema`.`accounts`.`HOST` is null),'background',"                                \
    "`performance_schema`.`accounts`.`HOST`)"

#define SYS_HOST_SUMMARY_QUALIFIED_VIEW_DEFINITION                                                 \
    "select if((`performance_schema`.`accounts`.`HOST` is null),'background',"                     \
    "`performance_schema`.`accounts`.`HOST`) AS `host`,sum(`sys`.`stmt`.`total`) AS "              \
    "`statements`,format_pico_time(sum(`sys`.`stmt`.`total_latency`)) AS "                         \
    "`statement_latency`,format_pico_time(ifnull((sum(`sys`.`stmt`.`total_latency`) / "            \
    "nullif(sum(`sys`.`stmt`.`total`),0)),0)) AS `statement_avg_latency`,sum("                     \
    "`sys`.`stmt`.`full_scans`) AS `table_scans`,sum(`sys`.`io`.`ios`) AS "                        \
    "`file_ios`,format_pico_time(sum(`sys`.`io`.`io_latency`)) AS "                                \
    "`file_io_latency`," SYS_HOST_SUMMARY_QUALIFIED_SELECT_SUFFIX                                  \
    "format_bytes(sum(`sys`.`mem`.`current_allocated`)) AS `current_memory`,format_bytes("         \
    "sum(`sys`.`mem`.`total_allocated`)) AS "                                                      \
    "`total_memory_allocated`" SYS_HOST_SUMMARY_QUALIFIED_FROM

#define SYS_X_HOST_SUMMARY_QUALIFIED_VIEW_DEFINITION                                               \
    "select if((`performance_schema`.`accounts`.`HOST` is null),'background',"                     \
    "`performance_schema`.`accounts`.`HOST`) AS `host`,sum(`sys`.`stmt`.`total`) AS "              \
    "`statements`,sum(`sys`.`stmt`.`total_latency`) AS `statement_latency`,("                      \
    "sum(`sys`.`stmt`.`total_latency`) / sum(`sys`.`stmt`.`total`)) AS "                           \
    "`statement_avg_latency`,sum(`sys`.`stmt`.`full_scans`) AS `table_scans`,sum("                 \
    "`sys`.`io`.`ios`) AS `file_ios`,sum(`sys`.`io`.`io_latency`) AS "                             \
    "`file_io_latency`," SYS_HOST_SUMMARY_QUALIFIED_SELECT_SUFFIX                                  \
    "sum(`sys`.`mem`.`current_allocated`) AS `current_memory`,sum("                                \
    "`sys`.`mem`.`total_allocated`) AS `total_memory_allocated`" SYS_HOST_SUMMARY_QUALIFIED_FROM

#define SYS_HOST_SUMMARY_UNQUALIFIED_VIEW_DEFINITION                                               \
    "select if((`performance_schema`.`accounts`.`HOST` is null),'background',"                     \
    "`performance_schema`.`accounts`.`HOST`) AS `host`,sum(`stmt`.`total`) AS "                    \
    "`statements`,format_pico_time(sum(`stmt`.`total_latency`)) AS "                               \
    "`statement_latency`,format_pico_time(ifnull((sum(`stmt`.`total_latency`) / nullif(sum("       \
    "`stmt`.`total`),0)),0)) AS `statement_avg_latency`,sum(`stmt`.`full_scans`) AS "              \
    "`table_scans`,sum(`io`.`ios`) AS `file_ios`,format_pico_time(sum(`io`."                       \
    "`io_latency`)) AS `file_io_latency`," SYS_HOST_SUMMARY_UNQUALIFIED_SELECT_SUFFIX              \
    "format_bytes(sum(`mem`.`current_allocated`)) AS `current_memory`,format_bytes(sum("           \
    "`mem`.`total_allocated`)) AS `total_memory_allocated`" SYS_HOST_SUMMARY_UNQUALIFIED_FROM

#define SYS_X_HOST_SUMMARY_UNQUALIFIED_VIEW_DEFINITION                                             \
    "select if((`performance_schema`.`accounts`.`HOST` is null),'background',"                     \
    "`performance_schema`.`accounts`.`HOST`) AS `host`,sum(`stmt`.`total`) AS "                    \
    "`statements`,sum(`stmt`.`total_latency`) AS `statement_latency`,(sum("                        \
    "`stmt`.`total_latency`) / sum(`stmt`.`total`)) AS `statement_avg_latency`,sum("               \
    "`stmt`.`full_scans`) AS `table_scans`,sum(`io`.`ios`) AS `file_ios`,sum("                     \
    "`io`.`io_latency`) AS `file_io_latency`," SYS_HOST_SUMMARY_UNQUALIFIED_SELECT_SUFFIX          \
    "sum(`mem`.`current_allocated`) AS `current_memory`,sum(`mem`.`total_allocated`) AS "          \
    "`total_memory_allocated`" SYS_HOST_SUMMARY_UNQUALIFIED_FROM

static const char sys_host_summary_view_definition[] = SYS_HOST_SUMMARY_QUALIFIED_VIEW_DEFINITION;

static const char sys_host_summary_show_create_view_sql[] =
    "CREATE ALGORITHM=TEMPTABLE DEFINER=`mysql.sys`@`localhost` SQL SECURITY INVOKER VIEW "
    "`host_summary` " SYS_HOST_SUMMARY_VIEW_COLUMNS
    " AS " SYS_HOST_SUMMARY_UNQUALIFIED_VIEW_DEFINITION;

static const char sys_host_summary_show_create_qualified_view_sql[] =
    "CREATE ALGORITHM=TEMPTABLE DEFINER=`mysql.sys`@`localhost` SQL SECURITY INVOKER VIEW "
    "`sys`.`host_summary` " SYS_HOST_SUMMARY_VIEW_COLUMNS
    " AS " SYS_HOST_SUMMARY_QUALIFIED_VIEW_DEFINITION;

static const char sys_x_host_summary_view_definition[] =
    SYS_X_HOST_SUMMARY_QUALIFIED_VIEW_DEFINITION;

static const char sys_x_host_summary_show_create_view_sql[] =
    "CREATE ALGORITHM=TEMPTABLE DEFINER=`mysql.sys`@`localhost` SQL SECURITY INVOKER VIEW "
    "`x$host_summary` " SYS_HOST_SUMMARY_VIEW_COLUMNS
    " AS " SYS_X_HOST_SUMMARY_UNQUALIFIED_VIEW_DEFINITION;

static const char sys_x_host_summary_show_create_qualified_view_sql[] =
    "CREATE ALGORITHM=TEMPTABLE DEFINER=`mysql.sys`@`localhost` SQL SECURITY INVOKER VIEW "
    "`sys`.`x$host_summary` " SYS_HOST_SUMMARY_VIEW_COLUMNS
    " AS " SYS_X_HOST_SUMMARY_QUALIFIED_VIEW_DEFINITION;

#undef SYS_HOST_SUMMARY_VIEW_COLUMNS
#undef SYS_HOST_SUMMARY_QUALIFIED_SELECT_SUFFIX
#undef SYS_HOST_SUMMARY_QUALIFIED_FROM
#undef SYS_HOST_SUMMARY_UNQUALIFIED_SELECT_SUFFIX
#undef SYS_HOST_SUMMARY_UNQUALIFIED_FROM
#undef SYS_HOST_SUMMARY_QUALIFIED_VIEW_DEFINITION
#undef SYS_X_HOST_SUMMARY_QUALIFIED_VIEW_DEFINITION
#undef SYS_HOST_SUMMARY_UNQUALIFIED_VIEW_DEFINITION
#undef SYS_X_HOST_SUMMARY_UNQUALIFIED_VIEW_DEFINITION

#define SYS_HOST_SUMMARY_BY_FILE_IO_VIEW_COLUMNS "(`host`,`ios`,`io_latency`)"

#define SYS_HOST_SUMMARY_BY_FILE_IO_SELECT_PREFIX                                                  \
    "select if((`performance_schema`.`events_waits_summary_by_host_by_event_name`.`HOST` is "      \
    "null),'background',`performance_schema`.`events_waits_summary_by_host_by_event_name`."        \
    "`HOST`) AS `host`,sum(`performance_schema`.`events_waits_summary_by_host_by_event_name`."     \
    "`COUNT_STAR`) AS `ios`,"

#define SYS_HOST_SUMMARY_BY_FILE_IO_SELECT_SUFFIX                                                  \
    " from `performance_schema`.`events_waits_summary_by_host_by_event_name` where ("              \
    "`performance_schema`.`events_waits_summary_by_host_by_event_name`.`EVENT_NAME` like "         \
    "'wait/io/file/%') group by if((`performance_schema`."                                         \
    "`events_waits_summary_by_host_by_event_name`.`HOST` is null),'background',"                   \
    "`performance_schema`.`events_waits_summary_by_host_by_event_name`.`HOST`) order by sum("      \
    "`performance_schema`.`events_waits_summary_by_host_by_event_name`.`SUM_TIMER_WAIT`) desc"

#define SYS_HOST_SUMMARY_BY_FILE_IO_VIEW_DEFINITION                                                \
    SYS_HOST_SUMMARY_BY_FILE_IO_SELECT_PREFIX                                                      \
    "format_pico_time(sum(`performance_schema`.`events_waits_summary_by_host_by_event_name`."      \
    "`SUM_TIMER_WAIT`)) AS `io_latency`" SYS_HOST_SUMMARY_BY_FILE_IO_SELECT_SUFFIX

#define SYS_X_HOST_SUMMARY_BY_FILE_IO_VIEW_DEFINITION                                              \
    SYS_HOST_SUMMARY_BY_FILE_IO_SELECT_PREFIX                                                      \
    "sum(`performance_schema`.`events_waits_summary_by_host_by_event_name`.`SUM_TIMER_WAIT`) AS "  \
    "`io_latency`" SYS_HOST_SUMMARY_BY_FILE_IO_SELECT_SUFFIX

static const char sys_host_summary_by_file_io_view_definition[] =
    SYS_HOST_SUMMARY_BY_FILE_IO_VIEW_DEFINITION;

static const char sys_host_summary_by_file_io_show_create_view_sql[] =
    "CREATE ALGORITHM=TEMPTABLE DEFINER=`mysql.sys`@`localhost` SQL SECURITY INVOKER VIEW "
    "`host_summary_by_file_io` " SYS_HOST_SUMMARY_BY_FILE_IO_VIEW_COLUMNS
    " AS " SYS_HOST_SUMMARY_BY_FILE_IO_VIEW_DEFINITION;

static const char sys_host_summary_by_file_io_show_create_qualified_view_sql[] =
    "CREATE ALGORITHM=TEMPTABLE DEFINER=`mysql.sys`@`localhost` SQL SECURITY INVOKER VIEW "
    "`sys`.`host_summary_by_file_io` " SYS_HOST_SUMMARY_BY_FILE_IO_VIEW_COLUMNS
    " AS " SYS_HOST_SUMMARY_BY_FILE_IO_VIEW_DEFINITION;

static const char sys_x_host_summary_by_file_io_view_definition[] =
    SYS_X_HOST_SUMMARY_BY_FILE_IO_VIEW_DEFINITION;

static const char sys_x_host_summary_by_file_io_show_create_view_sql[] =
    "CREATE ALGORITHM=TEMPTABLE DEFINER=`mysql.sys`@`localhost` SQL SECURITY INVOKER VIEW "
    "`x$host_summary_by_file_io` " SYS_HOST_SUMMARY_BY_FILE_IO_VIEW_COLUMNS
    " AS " SYS_X_HOST_SUMMARY_BY_FILE_IO_VIEW_DEFINITION;

static const char sys_x_host_summary_by_file_io_show_create_qualified_view_sql[] =
    "CREATE ALGORITHM=TEMPTABLE DEFINER=`mysql.sys`@`localhost` SQL SECURITY INVOKER VIEW "
    "`sys`.`x$host_summary_by_file_io` " SYS_HOST_SUMMARY_BY_FILE_IO_VIEW_COLUMNS
    " AS " SYS_X_HOST_SUMMARY_BY_FILE_IO_VIEW_DEFINITION;

#undef SYS_HOST_SUMMARY_BY_FILE_IO_VIEW_COLUMNS
#undef SYS_HOST_SUMMARY_BY_FILE_IO_SELECT_PREFIX
#undef SYS_HOST_SUMMARY_BY_FILE_IO_SELECT_SUFFIX
#undef SYS_HOST_SUMMARY_BY_FILE_IO_VIEW_DEFINITION
#undef SYS_X_HOST_SUMMARY_BY_FILE_IO_VIEW_DEFINITION

#define SYS_HOST_SUMMARY_BY_FILE_IO_TYPE_VIEW_COLUMNS                                              \
    "(`host`,`event_name`,`total`,`total_latency`,`max_latency`)"

#define SYS_HOST_SUMMARY_BY_FILE_IO_TYPE_SELECT_PREFIX                                             \
    "select if((`performance_schema`.`events_waits_summary_by_host_by_event_name`.`HOST` is "      \
    "null),'background',`performance_schema`.`events_waits_summary_by_host_by_event_name`."        \
    "`HOST`) AS `host`,`performance_schema`.`events_waits_summary_by_host_by_event_name`."         \
    "`EVENT_NAME` AS `event_name`,`performance_schema`."                                           \
    "`events_waits_summary_by_host_by_event_name`.`COUNT_STAR` AS `total`,"

#define SYS_HOST_SUMMARY_BY_FILE_IO_TYPE_SELECT_SUFFIX                                             \
    " from `performance_schema`.`events_waits_summary_by_host_by_event_name` where (("             \
    "`performance_schema`.`events_waits_summary_by_host_by_event_name`.`EVENT_NAME` like "         \
    "'wait/io/file%') and (`performance_schema`."                                                  \
    "`events_waits_summary_by_host_by_event_name`.`COUNT_STAR` > 0)) order by if(("                \
    "`performance_schema`.`events_waits_summary_by_host_by_event_name`.`HOST` is null),"           \
    "'background',`performance_schema`.`events_waits_summary_by_host_by_event_name`.`HOST`),"      \
    "`performance_schema`.`events_waits_summary_by_host_by_event_name`.`SUM_TIMER_WAIT` desc"

#define SYS_HOST_SUMMARY_BY_FILE_IO_TYPE_VIEW_DEFINITION                                           \
    SYS_HOST_SUMMARY_BY_FILE_IO_TYPE_SELECT_PREFIX                                                 \
    "format_pico_time(`performance_schema`.`events_waits_summary_by_host_by_event_name`."          \
    "`SUM_TIMER_WAIT`) AS `total_latency`,format_pico_time(`performance_schema`."                  \
    "`events_waits_summary_by_host_by_event_name`.`MAX_TIMER_WAIT`) AS "                           \
    "`max_latency`" SYS_HOST_SUMMARY_BY_FILE_IO_TYPE_SELECT_SUFFIX

#define SYS_X_HOST_SUMMARY_BY_FILE_IO_TYPE_VIEW_DEFINITION                                         \
    SYS_HOST_SUMMARY_BY_FILE_IO_TYPE_SELECT_PREFIX                                                 \
    "`performance_schema`.`events_waits_summary_by_host_by_event_name`.`SUM_TIMER_WAIT` AS "       \
    "`total_latency`,`performance_schema`.`events_waits_summary_by_host_by_event_name`."           \
    "`MAX_TIMER_WAIT` AS `max_latency`" SYS_HOST_SUMMARY_BY_FILE_IO_TYPE_SELECT_SUFFIX

static const char sys_host_summary_by_file_io_type_view_definition[] =
    SYS_HOST_SUMMARY_BY_FILE_IO_TYPE_VIEW_DEFINITION;

static const char sys_host_summary_by_file_io_type_show_create_view_sql[] =
    "CREATE ALGORITHM=MERGE DEFINER=`mysql.sys`@`localhost` SQL SECURITY INVOKER VIEW "
    "`host_summary_by_file_io_type` " SYS_HOST_SUMMARY_BY_FILE_IO_TYPE_VIEW_COLUMNS
    " AS " SYS_HOST_SUMMARY_BY_FILE_IO_TYPE_VIEW_DEFINITION;

static const char sys_host_summary_by_file_io_type_show_create_qualified_view_sql[] =
    "CREATE ALGORITHM=MERGE DEFINER=`mysql.sys`@`localhost` SQL SECURITY INVOKER VIEW "
    "`sys`.`host_summary_by_file_io_type` " SYS_HOST_SUMMARY_BY_FILE_IO_TYPE_VIEW_COLUMNS
    " AS " SYS_HOST_SUMMARY_BY_FILE_IO_TYPE_VIEW_DEFINITION;

static const char sys_x_host_summary_by_file_io_type_view_definition[] =
    SYS_X_HOST_SUMMARY_BY_FILE_IO_TYPE_VIEW_DEFINITION;

static const char sys_x_host_summary_by_file_io_type_show_create_view_sql[] =
    "CREATE ALGORITHM=MERGE DEFINER=`mysql.sys`@`localhost` SQL SECURITY INVOKER VIEW "
    "`x$host_summary_by_file_io_type` " SYS_HOST_SUMMARY_BY_FILE_IO_TYPE_VIEW_COLUMNS
    " AS " SYS_X_HOST_SUMMARY_BY_FILE_IO_TYPE_VIEW_DEFINITION;

static const char sys_x_host_summary_by_file_io_type_show_create_qualified_view_sql[] =
    "CREATE ALGORITHM=MERGE DEFINER=`mysql.sys`@`localhost` SQL SECURITY INVOKER VIEW "
    "`sys`.`x$host_summary_by_file_io_type` " SYS_HOST_SUMMARY_BY_FILE_IO_TYPE_VIEW_COLUMNS
    " AS " SYS_X_HOST_SUMMARY_BY_FILE_IO_TYPE_VIEW_DEFINITION;

#undef SYS_HOST_SUMMARY_BY_FILE_IO_TYPE_VIEW_COLUMNS
#undef SYS_HOST_SUMMARY_BY_FILE_IO_TYPE_SELECT_PREFIX
#undef SYS_HOST_SUMMARY_BY_FILE_IO_TYPE_SELECT_SUFFIX
#undef SYS_HOST_SUMMARY_BY_FILE_IO_TYPE_VIEW_DEFINITION
#undef SYS_X_HOST_SUMMARY_BY_FILE_IO_TYPE_VIEW_DEFINITION

#define SYS_HOST_SUMMARY_BY_STAGES_VIEW_COLUMNS                                                    \
    "(`host`,`event_name`,`total`,`total_latency`,`avg_latency`)"

#define SYS_HOST_SUMMARY_BY_STAGES_SELECT_PREFIX                                                   \
    "select if((`performance_schema`.`events_stages_summary_by_host_by_event_name`.`HOST` is "     \
    "null),'background',`performance_schema`.`events_stages_summary_by_host_by_event_name`."       \
    "`HOST`) AS `host`,`performance_schema`.`events_stages_summary_by_host_by_event_name`."        \
    "`EVENT_NAME` AS `event_name`,`performance_schema`."                                           \
    "`events_stages_summary_by_host_by_event_name`.`COUNT_STAR` AS `total`,"

#define SYS_HOST_SUMMARY_BY_STAGES_SELECT_SUFFIX                                                   \
    " from `performance_schema`.`events_stages_summary_by_host_by_event_name` where ("             \
    "`performance_schema`.`events_stages_summary_by_host_by_event_name`.`SUM_TIMER_WAIT` <> "      \
    "0) order by if((`performance_schema`.`events_stages_summary_by_host_by_event_name`."          \
    "`HOST` is null),'background',`performance_schema`."                                           \
    "`events_stages_summary_by_host_by_event_name`.`HOST`),`performance_schema`."                  \
    "`events_stages_summary_by_host_by_event_name`.`SUM_TIMER_WAIT` desc"

#define SYS_HOST_SUMMARY_BY_STAGES_VIEW_DEFINITION                                                 \
    SYS_HOST_SUMMARY_BY_STAGES_SELECT_PREFIX                                                       \
    "format_pico_time(`performance_schema`.`events_stages_summary_by_host_by_event_name`."         \
    "`SUM_TIMER_WAIT`) AS `total_latency`,format_pico_time(`performance_schema`."                  \
    "`events_stages_summary_by_host_by_event_name`.`AVG_TIMER_WAIT`) AS "                          \
    "`avg_latency`" SYS_HOST_SUMMARY_BY_STAGES_SELECT_SUFFIX

#define SYS_X_HOST_SUMMARY_BY_STAGES_VIEW_DEFINITION                                               \
    SYS_HOST_SUMMARY_BY_STAGES_SELECT_PREFIX                                                       \
    "`performance_schema`.`events_stages_summary_by_host_by_event_name`.`SUM_TIMER_WAIT` AS "      \
    "`total_latency`,`performance_schema`.`events_stages_summary_by_host_by_event_name`."          \
    "`AVG_TIMER_WAIT` AS `avg_latency`" SYS_HOST_SUMMARY_BY_STAGES_SELECT_SUFFIX

static const char sys_host_summary_by_stages_view_definition[] =
    SYS_HOST_SUMMARY_BY_STAGES_VIEW_DEFINITION;

static const char sys_host_summary_by_stages_show_create_view_sql[] =
    "CREATE ALGORITHM=MERGE DEFINER=`mysql.sys`@`localhost` SQL SECURITY INVOKER VIEW "
    "`host_summary_by_stages` " SYS_HOST_SUMMARY_BY_STAGES_VIEW_COLUMNS
    " AS " SYS_HOST_SUMMARY_BY_STAGES_VIEW_DEFINITION;

static const char sys_host_summary_by_stages_show_create_qualified_view_sql[] =
    "CREATE ALGORITHM=MERGE DEFINER=`mysql.sys`@`localhost` SQL SECURITY INVOKER VIEW "
    "`sys`.`host_summary_by_stages` " SYS_HOST_SUMMARY_BY_STAGES_VIEW_COLUMNS
    " AS " SYS_HOST_SUMMARY_BY_STAGES_VIEW_DEFINITION;

static const char sys_x_host_summary_by_stages_view_definition[] =
    SYS_X_HOST_SUMMARY_BY_STAGES_VIEW_DEFINITION;

static const char sys_x_host_summary_by_stages_show_create_view_sql[] =
    "CREATE ALGORITHM=MERGE DEFINER=`mysql.sys`@`localhost` SQL SECURITY INVOKER VIEW "
    "`x$host_summary_by_stages` " SYS_HOST_SUMMARY_BY_STAGES_VIEW_COLUMNS
    " AS " SYS_X_HOST_SUMMARY_BY_STAGES_VIEW_DEFINITION;

static const char sys_x_host_summary_by_stages_show_create_qualified_view_sql[] =
    "CREATE ALGORITHM=MERGE DEFINER=`mysql.sys`@`localhost` SQL SECURITY INVOKER VIEW "
    "`sys`.`x$host_summary_by_stages` " SYS_HOST_SUMMARY_BY_STAGES_VIEW_COLUMNS
    " AS " SYS_X_HOST_SUMMARY_BY_STAGES_VIEW_DEFINITION;

#undef SYS_HOST_SUMMARY_BY_STAGES_VIEW_COLUMNS
#undef SYS_HOST_SUMMARY_BY_STAGES_SELECT_PREFIX
#undef SYS_HOST_SUMMARY_BY_STAGES_SELECT_SUFFIX
#undef SYS_HOST_SUMMARY_BY_STAGES_VIEW_DEFINITION
#undef SYS_X_HOST_SUMMARY_BY_STAGES_VIEW_DEFINITION

#define SYS_HOST_SUMMARY_BY_STATEMENT_LATENCY_VIEW_COLUMNS                                         \
    "(`host`,`total`,`total_latency`,`max_latency`,`lock_latency`,`cpu_latency`,`rows_sent`,"      \
    "`rows_examined`,`rows_affected`,`full_scans`)"

#define SYS_HOST_SUMMARY_BY_STATEMENT_LATENCY_SOURCE                                               \
    "`performance_schema`.`events_statements_summary_by_host_by_event_name`"

#define SYS_HOST_SUMMARY_BY_STATEMENT_LATENCY_HOST_EXPR                                            \
    "if((" SYS_HOST_SUMMARY_BY_STATEMENT_LATENCY_SOURCE                                            \
    ".`HOST` is null),'background'," SYS_HOST_SUMMARY_BY_STATEMENT_LATENCY_SOURCE ".`HOST`)"

#define SYS_HOST_SUMMARY_BY_STATEMENT_LATENCY_SELECT_PREFIX                                        \
    "select " SYS_HOST_SUMMARY_BY_STATEMENT_LATENCY_HOST_EXPR                                      \
    " AS `host`,sum(" SYS_HOST_SUMMARY_BY_STATEMENT_LATENCY_SOURCE ".`COUNT_STAR`) AS `total`,"

#define SYS_HOST_SUMMARY_BY_STATEMENT_LATENCY_SELECT_SUFFIX                                        \
    "sum(" SYS_HOST_SUMMARY_BY_STATEMENT_LATENCY_SOURCE ".`SUM_ROWS_SENT`) AS `rows_sent`,"        \
    "sum(" SYS_HOST_SUMMARY_BY_STATEMENT_LATENCY_SOURCE ".`SUM_ROWS_EXAMINED`) AS "                \
    "`rows_examined`,sum(" SYS_HOST_SUMMARY_BY_STATEMENT_LATENCY_SOURCE                            \
    ".`SUM_ROWS_AFFECTED`) AS `rows_affected`,(sum(" SYS_HOST_SUMMARY_BY_STATEMENT_LATENCY_SOURCE  \
    ".`SUM_NO_INDEX_USED`) + sum(" SYS_HOST_SUMMARY_BY_STATEMENT_LATENCY_SOURCE                    \
    ".`SUM_NO_GOOD_INDEX_USED`)) AS `full_scans` "                                                 \
    "from " SYS_HOST_SUMMARY_BY_STATEMENT_LATENCY_SOURCE                                           \
    " group by " SYS_HOST_SUMMARY_BY_STATEMENT_LATENCY_HOST_EXPR                                   \
    " order by sum(" SYS_HOST_SUMMARY_BY_STATEMENT_LATENCY_SOURCE ".`SUM_TIMER_WAIT`) desc"

#define SYS_HOST_SUMMARY_BY_STATEMENT_LATENCY_VIEW_DEFINITION                                      \
    SYS_HOST_SUMMARY_BY_STATEMENT_LATENCY_SELECT_PREFIX                                            \
    "format_pico_time(sum(" SYS_HOST_SUMMARY_BY_STATEMENT_LATENCY_SOURCE ".`SUM_TIMER_WAIT`)) AS " \
    "`total_latency`,format_pico_time(max(" SYS_HOST_SUMMARY_BY_STATEMENT_LATENCY_SOURCE           \
    ".`MAX_TIMER_WAIT`)) AS "                                                                      \
    "`max_latency`,format_pico_time(sum(" SYS_HOST_SUMMARY_BY_STATEMENT_LATENCY_SOURCE             \
    ".`SUM_LOCK_TIME`)) AS "                                                                       \
    "`lock_latency`,format_pico_time(sum(" SYS_HOST_SUMMARY_BY_STATEMENT_LATENCY_SOURCE            \
    ".`SUM_CPU_TIME`)) AS `cpu_latency`," SYS_HOST_SUMMARY_BY_STATEMENT_LATENCY_SELECT_SUFFIX

#define SYS_X_HOST_SUMMARY_BY_STATEMENT_LATENCY_VIEW_DEFINITION                                    \
    SYS_HOST_SUMMARY_BY_STATEMENT_LATENCY_SELECT_PREFIX                                            \
    "sum(" SYS_HOST_SUMMARY_BY_STATEMENT_LATENCY_SOURCE                                            \
    ".`SUM_TIMER_WAIT`) AS `total_latency`,max(" SYS_HOST_SUMMARY_BY_STATEMENT_LATENCY_SOURCE      \
    ".`MAX_TIMER_WAIT`) AS `max_latency`,sum(" SYS_HOST_SUMMARY_BY_STATEMENT_LATENCY_SOURCE        \
    ".`SUM_LOCK_TIME`) AS `lock_latency`,sum(" SYS_HOST_SUMMARY_BY_STATEMENT_LATENCY_SOURCE        \
    ".`SUM_CPU_TIME`) AS `cpu_latency`," SYS_HOST_SUMMARY_BY_STATEMENT_LATENCY_SELECT_SUFFIX

static const char sys_host_summary_by_statement_latency_view_definition[] =
    SYS_HOST_SUMMARY_BY_STATEMENT_LATENCY_VIEW_DEFINITION;

static const char sys_host_summary_by_statement_latency_show_create_view_sql[] =
    "CREATE ALGORITHM=TEMPTABLE DEFINER=`mysql.sys`@`localhost` SQL SECURITY INVOKER VIEW "
    "`host_summary_by_statement_latency` " SYS_HOST_SUMMARY_BY_STATEMENT_LATENCY_VIEW_COLUMNS
    " AS " SYS_HOST_SUMMARY_BY_STATEMENT_LATENCY_VIEW_DEFINITION;

static const char sys_host_summary_by_statement_latency_show_create_qualified_view_sql[] =
    "CREATE ALGORITHM=TEMPTABLE DEFINER=`mysql.sys`@`localhost` SQL SECURITY INVOKER VIEW "
    "`sys`.`host_summary_by_statement_latency` " SYS_HOST_SUMMARY_BY_STATEMENT_LATENCY_VIEW_COLUMNS
    " AS " SYS_HOST_SUMMARY_BY_STATEMENT_LATENCY_VIEW_DEFINITION;

static const char sys_x_host_summary_by_statement_latency_view_definition[] =
    SYS_X_HOST_SUMMARY_BY_STATEMENT_LATENCY_VIEW_DEFINITION;

static const char sys_x_host_summary_by_statement_latency_show_create_view_sql[] =
    "CREATE ALGORITHM=TEMPTABLE DEFINER=`mysql.sys`@`localhost` SQL SECURITY INVOKER VIEW "
    "`x$host_summary_by_statement_latency` " SYS_HOST_SUMMARY_BY_STATEMENT_LATENCY_VIEW_COLUMNS
    " AS " SYS_X_HOST_SUMMARY_BY_STATEMENT_LATENCY_VIEW_DEFINITION;

static const char sys_x_host_summary_by_statement_latency_show_create_qualified_view_sql[] =
    "CREATE ALGORITHM=TEMPTABLE DEFINER=`mysql.sys`@`localhost` SQL SECURITY INVOKER VIEW "
    "`sys`.`x$host_summary_by_statement_latency`"
    " " SYS_HOST_SUMMARY_BY_STATEMENT_LATENCY_VIEW_COLUMNS
    " AS " SYS_X_HOST_SUMMARY_BY_STATEMENT_LATENCY_VIEW_DEFINITION;

#undef SYS_HOST_SUMMARY_BY_STATEMENT_LATENCY_VIEW_COLUMNS
#undef SYS_HOST_SUMMARY_BY_STATEMENT_LATENCY_SOURCE
#undef SYS_HOST_SUMMARY_BY_STATEMENT_LATENCY_HOST_EXPR
#undef SYS_HOST_SUMMARY_BY_STATEMENT_LATENCY_SELECT_PREFIX
#undef SYS_HOST_SUMMARY_BY_STATEMENT_LATENCY_SELECT_SUFFIX
#undef SYS_HOST_SUMMARY_BY_STATEMENT_LATENCY_VIEW_DEFINITION
#undef SYS_X_HOST_SUMMARY_BY_STATEMENT_LATENCY_VIEW_DEFINITION

#define SYS_HOST_SUMMARY_BY_STATEMENT_TYPE_VIEW_COLUMNS                                            \
    "(`host`,`statement`,`total`,`total_latency`,`max_latency`,`lock_latency`,`cpu_latency`,"      \
    "`rows_sent`,`rows_examined`,`rows_affected`,`full_scans`)"

#define SYS_HOST_SUMMARY_BY_STATEMENT_TYPE_SOURCE                                                  \
    "`performance_schema`.`events_statements_summary_by_host_by_event_name`"

#define SYS_HOST_SUMMARY_BY_STATEMENT_TYPE_HOST_EXPR                                               \
    "if((" SYS_HOST_SUMMARY_BY_STATEMENT_TYPE_SOURCE                                               \
    ".`HOST` is null),'background'," SYS_HOST_SUMMARY_BY_STATEMENT_TYPE_SOURCE ".`HOST`)"

#define SYS_HOST_SUMMARY_BY_STATEMENT_TYPE_STATEMENT_EXPR                                          \
    "substring_index(" SYS_HOST_SUMMARY_BY_STATEMENT_TYPE_SOURCE ".`EVENT_NAME`,'/',-(1))"

#define SYS_HOST_SUMMARY_BY_STATEMENT_TYPE_SELECT_PREFIX                                           \
    "select " SYS_HOST_SUMMARY_BY_STATEMENT_TYPE_HOST_EXPR                                         \
    " AS `host`," SYS_HOST_SUMMARY_BY_STATEMENT_TYPE_STATEMENT_EXPR                                \
    " AS `statement`," SYS_HOST_SUMMARY_BY_STATEMENT_TYPE_SOURCE ".`COUNT_STAR` AS `total`,"

#define SYS_HOST_SUMMARY_BY_STATEMENT_TYPE_SELECT_SUFFIX                                           \
    SYS_HOST_SUMMARY_BY_STATEMENT_TYPE_SOURCE                                                      \
    ".`SUM_ROWS_SENT` AS `rows_sent`," SYS_HOST_SUMMARY_BY_STATEMENT_TYPE_SOURCE                   \
    ".`SUM_ROWS_EXAMINED` AS `rows_examined`," SYS_HOST_SUMMARY_BY_STATEMENT_TYPE_SOURCE           \
    ".`SUM_ROWS_AFFECTED` AS `rows_affected`,(" SYS_HOST_SUMMARY_BY_STATEMENT_TYPE_SOURCE          \
    ".`SUM_NO_INDEX_USED` + " SYS_HOST_SUMMARY_BY_STATEMENT_TYPE_SOURCE                            \
    ".`SUM_NO_GOOD_INDEX_USED`) AS `full_scans` "                                                  \
    "from " SYS_HOST_SUMMARY_BY_STATEMENT_TYPE_SOURCE                                              \
    " where (" SYS_HOST_SUMMARY_BY_STATEMENT_TYPE_SOURCE                                           \
    ".`SUM_TIMER_WAIT` <> 0) order by " SYS_HOST_SUMMARY_BY_STATEMENT_TYPE_HOST_EXPR               \
    "," SYS_HOST_SUMMARY_BY_STATEMENT_TYPE_SOURCE ".`SUM_TIMER_WAIT` desc"

#define SYS_HOST_SUMMARY_BY_STATEMENT_TYPE_VIEW_DEFINITION                                         \
    SYS_HOST_SUMMARY_BY_STATEMENT_TYPE_SELECT_PREFIX                                               \
    "format_pico_time(" SYS_HOST_SUMMARY_BY_STATEMENT_TYPE_SOURCE ".`SUM_TIMER_WAIT`) AS "         \
    "`total_latency`,format_pico_time(" SYS_HOST_SUMMARY_BY_STATEMENT_TYPE_SOURCE                  \
    ".`MAX_TIMER_WAIT`) AS "                                                                       \
    "`max_latency`,format_pico_time(" SYS_HOST_SUMMARY_BY_STATEMENT_TYPE_SOURCE                    \
    ".`SUM_LOCK_TIME`) AS `lock_latency`,"                                                         \
    "format_pico_time(" SYS_HOST_SUMMARY_BY_STATEMENT_TYPE_SOURCE                                  \
    ".`SUM_CPU_TIME`) AS `cpu_latency`," SYS_HOST_SUMMARY_BY_STATEMENT_TYPE_SELECT_SUFFIX

#define SYS_X_HOST_SUMMARY_BY_STATEMENT_TYPE_VIEW_DEFINITION                                       \
    SYS_HOST_SUMMARY_BY_STATEMENT_TYPE_SELECT_PREFIX                                               \
    SYS_HOST_SUMMARY_BY_STATEMENT_TYPE_SOURCE                                                      \
    ".`SUM_TIMER_WAIT` AS `total_latency`," SYS_HOST_SUMMARY_BY_STATEMENT_TYPE_SOURCE              \
    ".`MAX_TIMER_WAIT` AS `max_latency`," SYS_HOST_SUMMARY_BY_STATEMENT_TYPE_SOURCE                \
    ".`SUM_LOCK_TIME` AS `lock_latency`," SYS_HOST_SUMMARY_BY_STATEMENT_TYPE_SOURCE                \
    ".`SUM_CPU_TIME` AS `cpu_latency`," SYS_HOST_SUMMARY_BY_STATEMENT_TYPE_SELECT_SUFFIX

static const char sys_host_summary_by_statement_type_view_definition[] =
    SYS_HOST_SUMMARY_BY_STATEMENT_TYPE_VIEW_DEFINITION;

static const char sys_host_summary_by_statement_type_show_create_view_sql[] =
    "CREATE ALGORITHM=MERGE DEFINER=`mysql.sys`@`localhost` SQL SECURITY INVOKER VIEW "
    "`host_summary_by_statement_type` " SYS_HOST_SUMMARY_BY_STATEMENT_TYPE_VIEW_COLUMNS
    " AS " SYS_HOST_SUMMARY_BY_STATEMENT_TYPE_VIEW_DEFINITION;

static const char sys_host_summary_by_statement_type_show_create_qualified_view_sql[] =
    "CREATE ALGORITHM=MERGE DEFINER=`mysql.sys`@`localhost` SQL SECURITY INVOKER VIEW "
    "`sys`.`host_summary_by_statement_type` " SYS_HOST_SUMMARY_BY_STATEMENT_TYPE_VIEW_COLUMNS
    " AS " SYS_HOST_SUMMARY_BY_STATEMENT_TYPE_VIEW_DEFINITION;

static const char sys_x_host_summary_by_statement_type_view_definition[] =
    SYS_X_HOST_SUMMARY_BY_STATEMENT_TYPE_VIEW_DEFINITION;

static const char sys_x_host_summary_by_statement_type_show_create_view_sql[] =
    "CREATE ALGORITHM=MERGE DEFINER=`mysql.sys`@`localhost` SQL SECURITY INVOKER VIEW "
    "`x$host_summary_by_statement_type` " SYS_HOST_SUMMARY_BY_STATEMENT_TYPE_VIEW_COLUMNS
    " AS " SYS_X_HOST_SUMMARY_BY_STATEMENT_TYPE_VIEW_DEFINITION;

static const char sys_x_host_summary_by_statement_type_show_create_qualified_view_sql[] =
    "CREATE ALGORITHM=MERGE DEFINER=`mysql.sys`@`localhost` SQL SECURITY INVOKER VIEW "
    "`sys`.`x$host_summary_by_statement_type` " SYS_HOST_SUMMARY_BY_STATEMENT_TYPE_VIEW_COLUMNS
    " AS " SYS_X_HOST_SUMMARY_BY_STATEMENT_TYPE_VIEW_DEFINITION;

#undef SYS_HOST_SUMMARY_BY_STATEMENT_TYPE_VIEW_COLUMNS
#undef SYS_HOST_SUMMARY_BY_STATEMENT_TYPE_SOURCE
#undef SYS_HOST_SUMMARY_BY_STATEMENT_TYPE_HOST_EXPR
#undef SYS_HOST_SUMMARY_BY_STATEMENT_TYPE_STATEMENT_EXPR
#undef SYS_HOST_SUMMARY_BY_STATEMENT_TYPE_SELECT_PREFIX
#undef SYS_HOST_SUMMARY_BY_STATEMENT_TYPE_SELECT_SUFFIX
#undef SYS_HOST_SUMMARY_BY_STATEMENT_TYPE_VIEW_DEFINITION
#undef SYS_X_HOST_SUMMARY_BY_STATEMENT_TYPE_VIEW_DEFINITION

#define SYS_MEMORY_BY_HOST_BY_CURRENT_BYTES_VIEW_COLUMNS                                           \
    "(`host`,`current_count_used`,`current_allocated`,`current_avg_alloc`,`current_max_alloc`,"    \
    "`total_allocated`)"

#define SYS_MEMORY_BY_HOST_BY_CURRENT_BYTES_SOURCE                                                 \
    "`performance_schema`.`memory_summary_by_host_by_event_name`"

#define SYS_MEMORY_BY_HOST_BY_CURRENT_BYTES_HOST_EXPR                                              \
    "if((" SYS_MEMORY_BY_HOST_BY_CURRENT_BYTES_SOURCE                                              \
    ".`HOST` is null),'background'," SYS_MEMORY_BY_HOST_BY_CURRENT_BYTES_SOURCE ".`HOST`)"

#define SYS_MEMORY_BY_HOST_BY_CURRENT_BYTES_CURRENT_BYTES_EXPR                                     \
    "sum(" SYS_MEMORY_BY_HOST_BY_CURRENT_BYTES_SOURCE ".`CURRENT_NUMBER_OF_BYTES_USED`)"

#define SYS_MEMORY_BY_HOST_BY_CURRENT_BYTES_COUNT_EXPR                                             \
    "sum(" SYS_MEMORY_BY_HOST_BY_CURRENT_BYTES_SOURCE ".`CURRENT_COUNT_USED`)"

#define SYS_MEMORY_BY_HOST_BY_CURRENT_BYTES_SELECT_PREFIX                                          \
    "select " SYS_MEMORY_BY_HOST_BY_CURRENT_BYTES_HOST_EXPR                                        \
    " AS `host`," SYS_MEMORY_BY_HOST_BY_CURRENT_BYTES_COUNT_EXPR " AS `current_count_used`,"

#define SYS_MEMORY_BY_HOST_BY_CURRENT_BYTES_AVG_EXPR                                               \
    "ifnull((" SYS_MEMORY_BY_HOST_BY_CURRENT_BYTES_CURRENT_BYTES_EXPR                              \
    " / nullif(" SYS_MEMORY_BY_HOST_BY_CURRENT_BYTES_COUNT_EXPR ",0)),0)"

#define SYS_MEMORY_BY_HOST_BY_CURRENT_BYTES_SELECT_SUFFIX                                          \
    "max(" SYS_MEMORY_BY_HOST_BY_CURRENT_BYTES_SOURCE ".`CURRENT_NUMBER_OF_BYTES_USED`) AS "       \
    "`current_max_alloc`,sum(" SYS_MEMORY_BY_HOST_BY_CURRENT_BYTES_SOURCE                          \
    ".`SUM_NUMBER_OF_BYTES_ALLOC`) AS `total_allocated` "                                          \
    "from " SYS_MEMORY_BY_HOST_BY_CURRENT_BYTES_SOURCE                                             \
    " group by " SYS_MEMORY_BY_HOST_BY_CURRENT_BYTES_HOST_EXPR                                     \
    " order by " SYS_MEMORY_BY_HOST_BY_CURRENT_BYTES_CURRENT_BYTES_EXPR " desc"

#define SYS_MEMORY_BY_HOST_BY_CURRENT_BYTES_VIEW_DEFINITION                                        \
    SYS_MEMORY_BY_HOST_BY_CURRENT_BYTES_SELECT_PREFIX                                              \
    "format_bytes(" SYS_MEMORY_BY_HOST_BY_CURRENT_BYTES_CURRENT_BYTES_EXPR                         \
    ") AS `current_allocated`,format_bytes(" SYS_MEMORY_BY_HOST_BY_CURRENT_BYTES_AVG_EXPR          \
    ") AS `current_avg_alloc`,format_bytes(max(" SYS_MEMORY_BY_HOST_BY_CURRENT_BYTES_SOURCE        \
    ".`CURRENT_NUMBER_OF_BYTES_USED`)) AS "                                                        \
    "`current_max_alloc`,format_bytes(sum(" SYS_MEMORY_BY_HOST_BY_CURRENT_BYTES_SOURCE             \
    ".`SUM_NUMBER_OF_BYTES_ALLOC`)) AS `total_allocated` "                                         \
    "from " SYS_MEMORY_BY_HOST_BY_CURRENT_BYTES_SOURCE                                             \
    " group by " SYS_MEMORY_BY_HOST_BY_CURRENT_BYTES_HOST_EXPR                                     \
    " order by " SYS_MEMORY_BY_HOST_BY_CURRENT_BYTES_CURRENT_BYTES_EXPR " desc"

#define SYS_X_MEMORY_BY_HOST_BY_CURRENT_BYTES_VIEW_DEFINITION                                      \
    SYS_MEMORY_BY_HOST_BY_CURRENT_BYTES_SELECT_PREFIX                                              \
    SYS_MEMORY_BY_HOST_BY_CURRENT_BYTES_CURRENT_BYTES_EXPR                                         \
    " AS `current_allocated`," SYS_MEMORY_BY_HOST_BY_CURRENT_BYTES_AVG_EXPR                        \
    " AS `current_avg_alloc`," SYS_MEMORY_BY_HOST_BY_CURRENT_BYTES_SELECT_SUFFIX

static const char sys_memory_by_host_by_current_bytes_view_definition[] =
    SYS_MEMORY_BY_HOST_BY_CURRENT_BYTES_VIEW_DEFINITION;

static const char sys_memory_by_host_by_current_bytes_show_create_view_sql[] =
    "CREATE ALGORITHM=TEMPTABLE DEFINER=`mysql.sys`@`localhost` SQL SECURITY INVOKER VIEW "
    "`memory_by_host_by_current_bytes` " SYS_MEMORY_BY_HOST_BY_CURRENT_BYTES_VIEW_COLUMNS
    " AS " SYS_MEMORY_BY_HOST_BY_CURRENT_BYTES_VIEW_DEFINITION;

static const char sys_memory_by_host_by_current_bytes_show_create_qualified_view_sql[] =
    "CREATE ALGORITHM=TEMPTABLE DEFINER=`mysql.sys`@`localhost` SQL SECURITY INVOKER VIEW "
    "`sys`.`memory_by_host_by_current_bytes` " SYS_MEMORY_BY_HOST_BY_CURRENT_BYTES_VIEW_COLUMNS
    " AS " SYS_MEMORY_BY_HOST_BY_CURRENT_BYTES_VIEW_DEFINITION;

static const char sys_x_memory_by_host_by_current_bytes_view_definition[] =
    SYS_X_MEMORY_BY_HOST_BY_CURRENT_BYTES_VIEW_DEFINITION;

static const char sys_x_memory_by_host_by_current_bytes_show_create_view_sql[] =
    "CREATE ALGORITHM=TEMPTABLE DEFINER=`mysql.sys`@`localhost` SQL SECURITY INVOKER VIEW "
    "`x$memory_by_host_by_current_bytes` " SYS_MEMORY_BY_HOST_BY_CURRENT_BYTES_VIEW_COLUMNS
    " AS " SYS_X_MEMORY_BY_HOST_BY_CURRENT_BYTES_VIEW_DEFINITION;

static const char sys_x_memory_by_host_by_current_bytes_show_create_qualified_view_sql[] =
    "CREATE ALGORITHM=TEMPTABLE DEFINER=`mysql.sys`@`localhost` SQL SECURITY INVOKER VIEW "
    "`sys`.`x$memory_by_host_by_current_bytes` " SYS_MEMORY_BY_HOST_BY_CURRENT_BYTES_VIEW_COLUMNS
    " AS " SYS_X_MEMORY_BY_HOST_BY_CURRENT_BYTES_VIEW_DEFINITION;

#undef SYS_MEMORY_BY_HOST_BY_CURRENT_BYTES_VIEW_COLUMNS
#undef SYS_MEMORY_BY_HOST_BY_CURRENT_BYTES_SOURCE
#undef SYS_MEMORY_BY_HOST_BY_CURRENT_BYTES_HOST_EXPR
#undef SYS_MEMORY_BY_HOST_BY_CURRENT_BYTES_CURRENT_BYTES_EXPR
#undef SYS_MEMORY_BY_HOST_BY_CURRENT_BYTES_COUNT_EXPR
#undef SYS_MEMORY_BY_HOST_BY_CURRENT_BYTES_SELECT_PREFIX
#undef SYS_MEMORY_BY_HOST_BY_CURRENT_BYTES_AVG_EXPR
#undef SYS_MEMORY_BY_HOST_BY_CURRENT_BYTES_SELECT_SUFFIX
#undef SYS_MEMORY_BY_HOST_BY_CURRENT_BYTES_VIEW_DEFINITION
#undef SYS_X_MEMORY_BY_HOST_BY_CURRENT_BYTES_VIEW_DEFINITION

#define SYS_MEMORY_BY_THREAD_BY_CURRENT_BYTES_VIEW_COLUMNS                                         \
    "(`thread_id`,`user`,`current_count_used`,`current_allocated`,`current_avg_alloc`,"            \
    "`current_max_alloc`,`total_allocated`)"

#define SYS_MEMORY_BY_THREAD_BY_CURRENT_BYTES_MEMORY_SOURCE                                        \
    "`performance_schema`.`memory_summary_by_thread_by_event_name` `mt`"

#define SYS_MEMORY_BY_THREAD_BY_CURRENT_BYTES_THREAD_SOURCE "`performance_schema`.`threads` `t`"

#define SYS_MEMORY_BY_THREAD_BY_CURRENT_BYTES_FROM_EXPR                                            \
    "(" SYS_MEMORY_BY_THREAD_BY_CURRENT_BYTES_MEMORY_SOURCE                                        \
    " join " SYS_MEMORY_BY_THREAD_BY_CURRENT_BYTES_THREAD_SOURCE                                   \
    " on((`mt`.`THREAD_ID` = `t`.`THREAD_ID`)))"

#define SYS_MEMORY_BY_THREAD_BY_CURRENT_BYTES_USER_EXPR                                            \
    "if((`t`.`NAME` = 'thread/sql/one_connection'),concat(`t`.`PROCESSLIST_USER`,'@',"             \
    "convert(`t`.`PROCESSLIST_HOST` using utf8mb4)),replace(`t`.`NAME`,'thread/',''))"

#define SYS_MEMORY_BY_THREAD_BY_CURRENT_BYTES_CURRENT_BYTES_EXPR                                   \
    "sum(`mt`.`CURRENT_NUMBER_OF_BYTES_USED`)"

#define SYS_MEMORY_BY_THREAD_BY_CURRENT_BYTES_COUNT_EXPR "sum(`mt`.`CURRENT_COUNT_USED`)"

#define SYS_MEMORY_BY_THREAD_BY_CURRENT_BYTES_AVG_EXPR                                             \
    "ifnull((" SYS_MEMORY_BY_THREAD_BY_CURRENT_BYTES_CURRENT_BYTES_EXPR                            \
    " / nullif(" SYS_MEMORY_BY_THREAD_BY_CURRENT_BYTES_COUNT_EXPR ",0)),0)"

#define SYS_MEMORY_BY_THREAD_BY_CURRENT_BYTES_SELECT_SUFFIX                                        \
    "max(`mt`.`CURRENT_NUMBER_OF_BYTES_USED`) AS `current_max_alloc`,"                             \
    "sum(`mt`.`SUM_NUMBER_OF_BYTES_ALLOC`) AS `total_allocated` "                                  \
    "from " SYS_MEMORY_BY_THREAD_BY_CURRENT_BYTES_FROM_EXPR

#define SYS_MEMORY_BY_THREAD_BY_CURRENT_BYTES_VIEW_DEFINITION                                      \
    "select `mt`.`THREAD_ID` AS `thread_id`," SYS_MEMORY_BY_THREAD_BY_CURRENT_BYTES_USER_EXPR      \
    " AS `user`," SYS_MEMORY_BY_THREAD_BY_CURRENT_BYTES_COUNT_EXPR " AS `current_count_used`,"     \
    "format_bytes(" SYS_MEMORY_BY_THREAD_BY_CURRENT_BYTES_CURRENT_BYTES_EXPR                       \
    ") AS `current_allocated`,format_bytes(" SYS_MEMORY_BY_THREAD_BY_CURRENT_BYTES_AVG_EXPR        \
    ") AS `current_avg_alloc`,format_bytes(max(`mt`.`CURRENT_NUMBER_OF_BYTES_USED`)) AS "          \
    "`current_max_alloc`,format_bytes(sum(`mt`.`SUM_NUMBER_OF_BYTES_ALLOC`)) AS "                  \
    "`total_allocated` from " SYS_MEMORY_BY_THREAD_BY_CURRENT_BYTES_FROM_EXPR                      \
    " group by `mt`.`THREAD_ID`," SYS_MEMORY_BY_THREAD_BY_CURRENT_BYTES_USER_EXPR                  \
    " order by " SYS_MEMORY_BY_THREAD_BY_CURRENT_BYTES_CURRENT_BYTES_EXPR " desc"

#define SYS_X_MEMORY_BY_THREAD_BY_CURRENT_BYTES_VIEW_DEFINITION                                    \
    "select `t`.`THREAD_ID` AS `thread_id`," SYS_MEMORY_BY_THREAD_BY_CURRENT_BYTES_USER_EXPR       \
    " AS `user`," SYS_MEMORY_BY_THREAD_BY_CURRENT_BYTES_COUNT_EXPR                                 \
    " AS `current_count_used`," SYS_MEMORY_BY_THREAD_BY_CURRENT_BYTES_CURRENT_BYTES_EXPR           \
    " AS `current_allocated`," SYS_MEMORY_BY_THREAD_BY_CURRENT_BYTES_AVG_EXPR                      \
    " AS `current_avg_alloc`," SYS_MEMORY_BY_THREAD_BY_CURRENT_BYTES_SELECT_SUFFIX                 \
    " group by `t`.`THREAD_ID`," SYS_MEMORY_BY_THREAD_BY_CURRENT_BYTES_USER_EXPR                   \
    " order by " SYS_MEMORY_BY_THREAD_BY_CURRENT_BYTES_CURRENT_BYTES_EXPR " desc"

static const char sys_memory_by_thread_by_current_bytes_view_definition[] =
    SYS_MEMORY_BY_THREAD_BY_CURRENT_BYTES_VIEW_DEFINITION;

static const char sys_memory_by_thread_by_current_bytes_show_create_view_sql[] =
    "CREATE ALGORITHM=TEMPTABLE DEFINER=`mysql.sys`@`localhost` SQL SECURITY INVOKER VIEW "
    "`memory_by_thread_by_current_bytes` " SYS_MEMORY_BY_THREAD_BY_CURRENT_BYTES_VIEW_COLUMNS
    " AS " SYS_MEMORY_BY_THREAD_BY_CURRENT_BYTES_VIEW_DEFINITION;

static const char sys_memory_by_thread_by_current_bytes_show_create_qualified_view_sql[] =
    "CREATE ALGORITHM=TEMPTABLE DEFINER=`mysql.sys`@`localhost` SQL SECURITY INVOKER VIEW "
    "`sys`.`memory_by_thread_by_current_bytes` " SYS_MEMORY_BY_THREAD_BY_CURRENT_BYTES_VIEW_COLUMNS
    " AS " SYS_MEMORY_BY_THREAD_BY_CURRENT_BYTES_VIEW_DEFINITION;

static const char sys_x_memory_by_thread_by_current_bytes_view_definition[] =
    SYS_X_MEMORY_BY_THREAD_BY_CURRENT_BYTES_VIEW_DEFINITION;

static const char sys_x_memory_by_thread_by_current_bytes_show_create_view_sql[] =
    "CREATE ALGORITHM=TEMPTABLE DEFINER=`mysql.sys`@`localhost` SQL SECURITY INVOKER VIEW "
    "`x$memory_by_thread_by_current_bytes` " SYS_MEMORY_BY_THREAD_BY_CURRENT_BYTES_VIEW_COLUMNS
    " AS " SYS_X_MEMORY_BY_THREAD_BY_CURRENT_BYTES_VIEW_DEFINITION;

static const char sys_x_memory_by_thread_by_current_bytes_show_create_qualified_view_sql[] =
    "CREATE ALGORITHM=TEMPTABLE DEFINER=`mysql.sys`@`localhost` SQL SECURITY INVOKER VIEW "
    "`sys`.`x$memory_by_thread_by_current_bytes`"
    " " SYS_MEMORY_BY_THREAD_BY_CURRENT_BYTES_VIEW_COLUMNS
    " AS " SYS_X_MEMORY_BY_THREAD_BY_CURRENT_BYTES_VIEW_DEFINITION;

#undef SYS_MEMORY_BY_THREAD_BY_CURRENT_BYTES_VIEW_COLUMNS
#undef SYS_MEMORY_BY_THREAD_BY_CURRENT_BYTES_MEMORY_SOURCE
#undef SYS_MEMORY_BY_THREAD_BY_CURRENT_BYTES_THREAD_SOURCE
#undef SYS_MEMORY_BY_THREAD_BY_CURRENT_BYTES_FROM_EXPR
#undef SYS_MEMORY_BY_THREAD_BY_CURRENT_BYTES_USER_EXPR
#undef SYS_MEMORY_BY_THREAD_BY_CURRENT_BYTES_CURRENT_BYTES_EXPR
#undef SYS_MEMORY_BY_THREAD_BY_CURRENT_BYTES_COUNT_EXPR
#undef SYS_MEMORY_BY_THREAD_BY_CURRENT_BYTES_AVG_EXPR
#undef SYS_MEMORY_BY_THREAD_BY_CURRENT_BYTES_SELECT_SUFFIX
#undef SYS_MEMORY_BY_THREAD_BY_CURRENT_BYTES_VIEW_DEFINITION
#undef SYS_X_MEMORY_BY_THREAD_BY_CURRENT_BYTES_VIEW_DEFINITION

#define SYS_MEMORY_BY_USER_BY_CURRENT_BYTES_VIEW_COLUMNS                                           \
    "(`user`,`current_count_used`,`current_allocated`,`current_avg_alloc`,`current_max_alloc`,"    \
    "`total_allocated`)"

#define SYS_MEMORY_BY_USER_BY_CURRENT_BYTES_SOURCE                                                 \
    "`performance_schema`.`memory_summary_by_user_by_event_name`"

#define SYS_MEMORY_BY_USER_BY_CURRENT_BYTES_USER_EXPR                                              \
    "if((" SYS_MEMORY_BY_USER_BY_CURRENT_BYTES_SOURCE                                              \
    ".`USER` is null),'background'," SYS_MEMORY_BY_USER_BY_CURRENT_BYTES_SOURCE ".`USER`)"

#define SYS_MEMORY_BY_USER_BY_CURRENT_BYTES_CURRENT_BYTES_EXPR                                     \
    "sum(" SYS_MEMORY_BY_USER_BY_CURRENT_BYTES_SOURCE ".`CURRENT_NUMBER_OF_BYTES_USED`)"

#define SYS_MEMORY_BY_USER_BY_CURRENT_BYTES_COUNT_EXPR                                             \
    "sum(" SYS_MEMORY_BY_USER_BY_CURRENT_BYTES_SOURCE ".`CURRENT_COUNT_USED`)"

#define SYS_MEMORY_BY_USER_BY_CURRENT_BYTES_SELECT_PREFIX                                          \
    "select " SYS_MEMORY_BY_USER_BY_CURRENT_BYTES_USER_EXPR                                        \
    " AS `user`," SYS_MEMORY_BY_USER_BY_CURRENT_BYTES_COUNT_EXPR " AS `current_count_used`,"

#define SYS_MEMORY_BY_USER_BY_CURRENT_BYTES_AVG_EXPR                                               \
    "ifnull((" SYS_MEMORY_BY_USER_BY_CURRENT_BYTES_CURRENT_BYTES_EXPR                              \
    " / nullif(" SYS_MEMORY_BY_USER_BY_CURRENT_BYTES_COUNT_EXPR ",0)),0)"

#define SYS_MEMORY_BY_USER_BY_CURRENT_BYTES_SELECT_SUFFIX                                          \
    "max(" SYS_MEMORY_BY_USER_BY_CURRENT_BYTES_SOURCE ".`CURRENT_NUMBER_OF_BYTES_USED`) AS "       \
    "`current_max_alloc`,sum(" SYS_MEMORY_BY_USER_BY_CURRENT_BYTES_SOURCE                          \
    ".`SUM_NUMBER_OF_BYTES_ALLOC`) AS `total_allocated` "                                          \
    "from " SYS_MEMORY_BY_USER_BY_CURRENT_BYTES_SOURCE                                             \
    " group by " SYS_MEMORY_BY_USER_BY_CURRENT_BYTES_USER_EXPR                                     \
    " order by " SYS_MEMORY_BY_USER_BY_CURRENT_BYTES_CURRENT_BYTES_EXPR " desc"

#define SYS_MEMORY_BY_USER_BY_CURRENT_BYTES_VIEW_DEFINITION                                        \
    SYS_MEMORY_BY_USER_BY_CURRENT_BYTES_SELECT_PREFIX                                              \
    "format_bytes(" SYS_MEMORY_BY_USER_BY_CURRENT_BYTES_CURRENT_BYTES_EXPR                         \
    ") AS `current_allocated`,format_bytes(" SYS_MEMORY_BY_USER_BY_CURRENT_BYTES_AVG_EXPR          \
    ") AS `current_avg_alloc`,format_bytes(max(" SYS_MEMORY_BY_USER_BY_CURRENT_BYTES_SOURCE        \
    ".`CURRENT_NUMBER_OF_BYTES_USED`)) AS "                                                        \
    "`current_max_alloc`,format_bytes(sum(" SYS_MEMORY_BY_USER_BY_CURRENT_BYTES_SOURCE             \
    ".`SUM_NUMBER_OF_BYTES_ALLOC`)) AS `total_allocated` "                                         \
    "from " SYS_MEMORY_BY_USER_BY_CURRENT_BYTES_SOURCE                                             \
    " group by " SYS_MEMORY_BY_USER_BY_CURRENT_BYTES_USER_EXPR                                     \
    " order by " SYS_MEMORY_BY_USER_BY_CURRENT_BYTES_CURRENT_BYTES_EXPR " desc"

#define SYS_X_MEMORY_BY_USER_BY_CURRENT_BYTES_VIEW_DEFINITION                                      \
    SYS_MEMORY_BY_USER_BY_CURRENT_BYTES_SELECT_PREFIX                                              \
    SYS_MEMORY_BY_USER_BY_CURRENT_BYTES_CURRENT_BYTES_EXPR                                         \
    " AS `current_allocated`," SYS_MEMORY_BY_USER_BY_CURRENT_BYTES_AVG_EXPR                        \
    " AS `current_avg_alloc`," SYS_MEMORY_BY_USER_BY_CURRENT_BYTES_SELECT_SUFFIX

static const char sys_memory_by_user_by_current_bytes_view_definition[] =
    SYS_MEMORY_BY_USER_BY_CURRENT_BYTES_VIEW_DEFINITION;

static const char sys_memory_by_user_by_current_bytes_show_create_view_sql[] =
    "CREATE ALGORITHM=TEMPTABLE DEFINER=`mysql.sys`@`localhost` SQL SECURITY INVOKER VIEW "
    "`memory_by_user_by_current_bytes` " SYS_MEMORY_BY_USER_BY_CURRENT_BYTES_VIEW_COLUMNS
    " AS " SYS_MEMORY_BY_USER_BY_CURRENT_BYTES_VIEW_DEFINITION;

static const char sys_memory_by_user_by_current_bytes_show_create_qualified_view_sql[] =
    "CREATE ALGORITHM=TEMPTABLE DEFINER=`mysql.sys`@`localhost` SQL SECURITY INVOKER VIEW "
    "`sys`.`memory_by_user_by_current_bytes` " SYS_MEMORY_BY_USER_BY_CURRENT_BYTES_VIEW_COLUMNS
    " AS " SYS_MEMORY_BY_USER_BY_CURRENT_BYTES_VIEW_DEFINITION;

static const char sys_x_memory_by_user_by_current_bytes_view_definition[] =
    SYS_X_MEMORY_BY_USER_BY_CURRENT_BYTES_VIEW_DEFINITION;

static const char sys_x_memory_by_user_by_current_bytes_show_create_view_sql[] =
    "CREATE ALGORITHM=TEMPTABLE DEFINER=`mysql.sys`@`localhost` SQL SECURITY INVOKER VIEW "
    "`x$memory_by_user_by_current_bytes` " SYS_MEMORY_BY_USER_BY_CURRENT_BYTES_VIEW_COLUMNS
    " AS " SYS_X_MEMORY_BY_USER_BY_CURRENT_BYTES_VIEW_DEFINITION;

static const char sys_x_memory_by_user_by_current_bytes_show_create_qualified_view_sql[] =
    "CREATE ALGORITHM=TEMPTABLE DEFINER=`mysql.sys`@`localhost` SQL SECURITY INVOKER VIEW "
    "`sys`.`x$memory_by_user_by_current_bytes` " SYS_MEMORY_BY_USER_BY_CURRENT_BYTES_VIEW_COLUMNS
    " AS " SYS_X_MEMORY_BY_USER_BY_CURRENT_BYTES_VIEW_DEFINITION;

#undef SYS_MEMORY_BY_USER_BY_CURRENT_BYTES_VIEW_COLUMNS
#undef SYS_MEMORY_BY_USER_BY_CURRENT_BYTES_SOURCE
#undef SYS_MEMORY_BY_USER_BY_CURRENT_BYTES_USER_EXPR
#undef SYS_MEMORY_BY_USER_BY_CURRENT_BYTES_CURRENT_BYTES_EXPR
#undef SYS_MEMORY_BY_USER_BY_CURRENT_BYTES_COUNT_EXPR
#undef SYS_MEMORY_BY_USER_BY_CURRENT_BYTES_SELECT_PREFIX
#undef SYS_MEMORY_BY_USER_BY_CURRENT_BYTES_AVG_EXPR
#undef SYS_MEMORY_BY_USER_BY_CURRENT_BYTES_SELECT_SUFFIX
#undef SYS_MEMORY_BY_USER_BY_CURRENT_BYTES_VIEW_DEFINITION
#undef SYS_X_MEMORY_BY_USER_BY_CURRENT_BYTES_VIEW_DEFINITION

#define SYS_MEMORY_GLOBAL_BY_CURRENT_BYTES_VIEW_COLUMNS                                            \
    "(`event_name`,`current_count`,`current_alloc`,`current_avg_alloc`,`high_count`,`high_alloc`," \
    "`high_avg_alloc`)"

#define SYS_MEMORY_GLOBAL_BY_CURRENT_BYTES_SOURCE                                                  \
    "`performance_schema`.`memory_summary_global_by_event_name`"

#define SYS_MEMORY_GLOBAL_BY_CURRENT_BYTES_CURRENT_AVG_EXPR                                        \
    "ifnull((" SYS_MEMORY_GLOBAL_BY_CURRENT_BYTES_SOURCE                                           \
    ".`CURRENT_NUMBER_OF_BYTES_USED` / nullif(" SYS_MEMORY_GLOBAL_BY_CURRENT_BYTES_SOURCE          \
    ".`CURRENT_COUNT_USED`,0)),0)"

#define SYS_MEMORY_GLOBAL_BY_CURRENT_BYTES_HIGH_AVG_EXPR                                           \
    "ifnull((" SYS_MEMORY_GLOBAL_BY_CURRENT_BYTES_SOURCE                                           \
    ".`HIGH_NUMBER_OF_BYTES_USED` / nullif(" SYS_MEMORY_GLOBAL_BY_CURRENT_BYTES_SOURCE             \
    ".`HIGH_COUNT_USED`,0)),0)"

#define SYS_MEMORY_GLOBAL_BY_CURRENT_BYTES_SELECT_PREFIX                                           \
    "select " SYS_MEMORY_GLOBAL_BY_CURRENT_BYTES_SOURCE                                            \
    ".`EVENT_NAME` AS `event_name`," SYS_MEMORY_GLOBAL_BY_CURRENT_BYTES_SOURCE                     \
    ".`CURRENT_COUNT_USED` AS `current_count`,"

#define SYS_MEMORY_GLOBAL_BY_CURRENT_BYTES_SELECT_MIDDLE                                           \
    SYS_MEMORY_GLOBAL_BY_CURRENT_BYTES_SOURCE ".`HIGH_COUNT_USED` AS `high_count`,"

#define SYS_MEMORY_GLOBAL_BY_CURRENT_BYTES_FROM_SUFFIX                                             \
    " from " SYS_MEMORY_GLOBAL_BY_CURRENT_BYTES_SOURCE                                             \
    " where (" SYS_MEMORY_GLOBAL_BY_CURRENT_BYTES_SOURCE                                           \
    ".`CURRENT_NUMBER_OF_BYTES_USED` > 0) order by " SYS_MEMORY_GLOBAL_BY_CURRENT_BYTES_SOURCE     \
    ".`CURRENT_NUMBER_OF_BYTES_USED` desc"

#define SYS_MEMORY_GLOBAL_BY_CURRENT_BYTES_VIEW_DEFINITION                                         \
    SYS_MEMORY_GLOBAL_BY_CURRENT_BYTES_SELECT_PREFIX                                               \
    "format_bytes(" SYS_MEMORY_GLOBAL_BY_CURRENT_BYTES_SOURCE                                      \
    ".`CURRENT_NUMBER_OF_BYTES_USED`) AS "                                                         \
    "`current_alloc`,format_bytes(" SYS_MEMORY_GLOBAL_BY_CURRENT_BYTES_CURRENT_AVG_EXPR            \
    ") AS `current_avg_alloc`," SYS_MEMORY_GLOBAL_BY_CURRENT_BYTES_SELECT_MIDDLE                   \
    "format_bytes(" SYS_MEMORY_GLOBAL_BY_CURRENT_BYTES_SOURCE ".`HIGH_NUMBER_OF_BYTES_USED`) AS "  \
    "`high_alloc`,format_bytes(" SYS_MEMORY_GLOBAL_BY_CURRENT_BYTES_HIGH_AVG_EXPR                  \
    ") AS `high_avg_alloc`" SYS_MEMORY_GLOBAL_BY_CURRENT_BYTES_FROM_SUFFIX

#define SYS_X_MEMORY_GLOBAL_BY_CURRENT_BYTES_VIEW_DEFINITION                                       \
    SYS_MEMORY_GLOBAL_BY_CURRENT_BYTES_SELECT_PREFIX                                               \
    SYS_MEMORY_GLOBAL_BY_CURRENT_BYTES_SOURCE                                                      \
    ".`CURRENT_NUMBER_OF_BYTES_USED` AS "                                                          \
    "`current_alloc`," SYS_MEMORY_GLOBAL_BY_CURRENT_BYTES_CURRENT_AVG_EXPR " AS "                  \
    "`current_avg_alloc`"                                                                          \
    "," SYS_MEMORY_GLOBAL_BY_CURRENT_BYTES_SELECT_MIDDLE SYS_MEMORY_GLOBAL_BY_CURRENT_BYTES_SOURCE \
    ".`HIGH_NUMBER_OF_BYTES_USED` AS "                                                             \
    "`high_alloc`," SYS_MEMORY_GLOBAL_BY_CURRENT_BYTES_HIGH_AVG_EXPR                               \
    " AS `high_avg_alloc`" SYS_MEMORY_GLOBAL_BY_CURRENT_BYTES_FROM_SUFFIX

static const char sys_memory_global_by_current_bytes_view_definition[] =
    SYS_MEMORY_GLOBAL_BY_CURRENT_BYTES_VIEW_DEFINITION;

static const char sys_memory_global_by_current_bytes_show_create_view_sql[] =
    "CREATE ALGORITHM=MERGE DEFINER=`mysql.sys`@`localhost` SQL SECURITY INVOKER VIEW "
    "`memory_global_by_current_bytes` " SYS_MEMORY_GLOBAL_BY_CURRENT_BYTES_VIEW_COLUMNS
    " AS " SYS_MEMORY_GLOBAL_BY_CURRENT_BYTES_VIEW_DEFINITION;

static const char sys_memory_global_by_current_bytes_show_create_qualified_view_sql[] =
    "CREATE ALGORITHM=MERGE DEFINER=`mysql.sys`@`localhost` SQL SECURITY INVOKER VIEW "
    "`sys`.`memory_global_by_current_bytes` " SYS_MEMORY_GLOBAL_BY_CURRENT_BYTES_VIEW_COLUMNS
    " AS " SYS_MEMORY_GLOBAL_BY_CURRENT_BYTES_VIEW_DEFINITION;

static const char sys_x_memory_global_by_current_bytes_view_definition[] =
    SYS_X_MEMORY_GLOBAL_BY_CURRENT_BYTES_VIEW_DEFINITION;

static const char sys_x_memory_global_by_current_bytes_show_create_view_sql[] =
    "CREATE ALGORITHM=MERGE DEFINER=`mysql.sys`@`localhost` SQL SECURITY INVOKER VIEW "
    "`x$memory_global_by_current_bytes` " SYS_MEMORY_GLOBAL_BY_CURRENT_BYTES_VIEW_COLUMNS
    " AS " SYS_X_MEMORY_GLOBAL_BY_CURRENT_BYTES_VIEW_DEFINITION;

static const char sys_x_memory_global_by_current_bytes_show_create_qualified_view_sql[] =
    "CREATE ALGORITHM=MERGE DEFINER=`mysql.sys`@`localhost` SQL SECURITY INVOKER VIEW "
    "`sys`.`x$memory_global_by_current_bytes` " SYS_MEMORY_GLOBAL_BY_CURRENT_BYTES_VIEW_COLUMNS
    " AS " SYS_X_MEMORY_GLOBAL_BY_CURRENT_BYTES_VIEW_DEFINITION;

#undef SYS_MEMORY_GLOBAL_BY_CURRENT_BYTES_VIEW_COLUMNS
#undef SYS_MEMORY_GLOBAL_BY_CURRENT_BYTES_SOURCE
#undef SYS_MEMORY_GLOBAL_BY_CURRENT_BYTES_CURRENT_AVG_EXPR
#undef SYS_MEMORY_GLOBAL_BY_CURRENT_BYTES_HIGH_AVG_EXPR
#undef SYS_MEMORY_GLOBAL_BY_CURRENT_BYTES_SELECT_PREFIX
#undef SYS_MEMORY_GLOBAL_BY_CURRENT_BYTES_SELECT_MIDDLE
#undef SYS_MEMORY_GLOBAL_BY_CURRENT_BYTES_FROM_SUFFIX
#undef SYS_MEMORY_GLOBAL_BY_CURRENT_BYTES_VIEW_DEFINITION
#undef SYS_X_MEMORY_GLOBAL_BY_CURRENT_BYTES_VIEW_DEFINITION

#define SYS_INNODB_BUFFER_STATS_BY_SCHEMA_VIEW_COLUMNS                                             \
    "(`object_schema`,`allocated`,`data`,`pages`,`pages_hashed`,`pages_old`,`rows_cached`)"

#define SYS_INNODB_BUFFER_STATS_BY_SCHEMA_OBJECT_EXPR                                              \
    "if((locate('.',`ibp`.`TABLE_NAME`) = 0),'InnoDB System',"                                     \
    "replace(substring_index(`ibp`.`TABLE_NAME`,'.',1),'`',''))"

#define SYS_INNODB_BUFFER_STATS_BY_SCHEMA_ALLOCATED_EXPR                                           \
    "sum(if((`ibp`.`COMPRESSED_SIZE` = 0),16384,`ibp`.`COMPRESSED_SIZE`))"

#define SYS_INNODB_BUFFER_STATS_BY_SCHEMA_SELECT_PREFIX                                            \
    "select " SYS_INNODB_BUFFER_STATS_BY_SCHEMA_OBJECT_EXPR " AS `object_schema`,"

#define SYS_INNODB_BUFFER_STATS_BY_SCHEMA_SELECT_SUFFIX                                            \
    "`ibp`.`DATA_SIZE`)) AS `data`,count(`ibp`.`PAGE_NUMBER`) AS `pages`,"                         \
    "count(if((`ibp`.`IS_HASHED` = 'YES'),1,NULL)) AS `pages_hashed`,"                             \
    "count(if((`ibp`.`IS_OLD` = 'YES'),1,NULL)) AS `pages_old`,"

#define SYS_INNODB_BUFFER_STATS_BY_SCHEMA_FROM_SUFFIX                                              \
    " from `information_schema`.`INNODB_BUFFER_PAGE` `ibp` where (`ibp`.`TABLE_NAME` is not null)" \
    " group by `object_schema` order by " SYS_INNODB_BUFFER_STATS_BY_SCHEMA_ALLOCATED_EXPR " desc"

#define SYS_INNODB_BUFFER_STATS_BY_SCHEMA_VIEW_DEFINITION                                          \
    SYS_INNODB_BUFFER_STATS_BY_SCHEMA_SELECT_PREFIX                                                \
    "format_bytes(" SYS_INNODB_BUFFER_STATS_BY_SCHEMA_ALLOCATED_EXPR ") AS `allocated`,"           \
    "format_bytes(sum(" SYS_INNODB_BUFFER_STATS_BY_SCHEMA_SELECT_SUFFIX                            \
    "round((sum(`ibp`.`NUMBER_RECORDS`) / count(distinct `ibp`.`INDEX_NAME`)),0) AS "              \
    "`rows_cached`" SYS_INNODB_BUFFER_STATS_BY_SCHEMA_FROM_SUFFIX

#define SYS_X_INNODB_BUFFER_STATS_BY_SCHEMA_VIEW_DEFINITION                                        \
    SYS_INNODB_BUFFER_STATS_BY_SCHEMA_SELECT_PREFIX                                                \
    SYS_INNODB_BUFFER_STATS_BY_SCHEMA_ALLOCATED_EXPR                                               \
    " AS `allocated`,sum(" SYS_INNODB_BUFFER_STATS_BY_SCHEMA_SELECT_SUFFIX                         \
    "round(ifnull((sum(`ibp`.`NUMBER_RECORDS`) / nullif(count(distinct "                           \
    "`ibp`.`INDEX_NAME`),0)),0),"                                                                  \
    "0) AS `rows_cached`" SYS_INNODB_BUFFER_STATS_BY_SCHEMA_FROM_SUFFIX

static const char sys_innodb_buffer_stats_by_schema_view_definition[] =
    SYS_INNODB_BUFFER_STATS_BY_SCHEMA_VIEW_DEFINITION;

static const char sys_innodb_buffer_stats_by_schema_show_create_view_sql[] =
    "CREATE ALGORITHM=TEMPTABLE DEFINER=`mysql.sys`@`localhost` SQL SECURITY INVOKER VIEW "
    "`innodb_buffer_stats_by_schema` " SYS_INNODB_BUFFER_STATS_BY_SCHEMA_VIEW_COLUMNS
    " AS " SYS_INNODB_BUFFER_STATS_BY_SCHEMA_VIEW_DEFINITION;

static const char sys_innodb_buffer_stats_by_schema_show_create_qualified_view_sql[] =
    "CREATE ALGORITHM=TEMPTABLE DEFINER=`mysql.sys`@`localhost` SQL SECURITY INVOKER VIEW "
    "`sys`.`innodb_buffer_stats_by_schema` " SYS_INNODB_BUFFER_STATS_BY_SCHEMA_VIEW_COLUMNS
    " AS " SYS_INNODB_BUFFER_STATS_BY_SCHEMA_VIEW_DEFINITION;

static const char sys_x_innodb_buffer_stats_by_schema_view_definition[] =
    SYS_X_INNODB_BUFFER_STATS_BY_SCHEMA_VIEW_DEFINITION;

static const char sys_x_innodb_buffer_stats_by_schema_show_create_view_sql[] =
    "CREATE ALGORITHM=TEMPTABLE DEFINER=`mysql.sys`@`localhost` SQL SECURITY INVOKER VIEW "
    "`x$innodb_buffer_stats_by_schema` " SYS_INNODB_BUFFER_STATS_BY_SCHEMA_VIEW_COLUMNS
    " AS " SYS_X_INNODB_BUFFER_STATS_BY_SCHEMA_VIEW_DEFINITION;

static const char sys_x_innodb_buffer_stats_by_schema_show_create_qualified_view_sql[] =
    "CREATE ALGORITHM=TEMPTABLE DEFINER=`mysql.sys`@`localhost` SQL SECURITY INVOKER VIEW "
    "`sys`.`x$innodb_buffer_stats_by_schema` " SYS_INNODB_BUFFER_STATS_BY_SCHEMA_VIEW_COLUMNS
    " AS " SYS_X_INNODB_BUFFER_STATS_BY_SCHEMA_VIEW_DEFINITION;

#undef SYS_INNODB_BUFFER_STATS_BY_SCHEMA_VIEW_COLUMNS
#undef SYS_INNODB_BUFFER_STATS_BY_SCHEMA_OBJECT_EXPR
#undef SYS_INNODB_BUFFER_STATS_BY_SCHEMA_ALLOCATED_EXPR
#undef SYS_INNODB_BUFFER_STATS_BY_SCHEMA_SELECT_PREFIX
#undef SYS_INNODB_BUFFER_STATS_BY_SCHEMA_SELECT_SUFFIX
#undef SYS_INNODB_BUFFER_STATS_BY_SCHEMA_FROM_SUFFIX
#undef SYS_INNODB_BUFFER_STATS_BY_SCHEMA_VIEW_DEFINITION
#undef SYS_X_INNODB_BUFFER_STATS_BY_SCHEMA_VIEW_DEFINITION

#define SYS_INNODB_BUFFER_STATS_BY_TABLE_VIEW_COLUMNS                                              \
    "(`object_schema`,`object_name`,`allocated`,`data`,`pages`,`pages_hashed`,`pages_old`,"        \
    "`rows_cached`)"

#define SYS_INNODB_BUFFER_STATS_BY_TABLE_OBJECT_SCHEMA_EXPR                                        \
    "if((locate('.',`ibp`.`TABLE_NAME`) = 0),'InnoDB System',"                                     \
    "replace(substring_index(`ibp`.`TABLE_NAME`,'.',1),'`',''))"

#define SYS_INNODB_BUFFER_STATS_BY_TABLE_OBJECT_NAME_EXPR                                          \
    "replace(substring_index(`ibp`.`TABLE_NAME`,'.',-(1)),'`','')"

#define SYS_INNODB_BUFFER_STATS_BY_TABLE_ALLOCATED_EXPR                                            \
    "sum(if((`ibp`.`COMPRESSED_SIZE` = 0),16384,`ibp`.`COMPRESSED_SIZE`))"

#define SYS_INNODB_BUFFER_STATS_BY_TABLE_SELECT_PREFIX                                             \
    "select " SYS_INNODB_BUFFER_STATS_BY_TABLE_OBJECT_SCHEMA_EXPR                                  \
    " AS `object_schema`," SYS_INNODB_BUFFER_STATS_BY_TABLE_OBJECT_NAME_EXPR " AS `object_name`,"

#define SYS_INNODB_BUFFER_STATS_BY_TABLE_SELECT_SUFFIX                                             \
    "`ibp`.`DATA_SIZE`)) AS `data`,count(`ibp`.`PAGE_NUMBER`) AS `pages`,"                         \
    "count(if((`ibp`.`IS_HASHED` = 'YES'),1,NULL)) AS `pages_hashed`,"                             \
    "count(if((`ibp`.`IS_OLD` = 'YES'),1,NULL)) AS `pages_old`,"

#define SYS_INNODB_BUFFER_STATS_BY_TABLE_FROM_SUFFIX                                               \
    " from `information_schema`.`INNODB_BUFFER_PAGE` `ibp` where (`ibp`.`TABLE_NAME` is not null)" \
    " group by `object_schema`,`object_name` order "                                               \
    "by " SYS_INNODB_BUFFER_STATS_BY_TABLE_ALLOCATED_EXPR " desc"

#define SYS_INNODB_BUFFER_STATS_BY_TABLE_VIEW_DEFINITION                                           \
    SYS_INNODB_BUFFER_STATS_BY_TABLE_SELECT_PREFIX                                                 \
    "format_bytes(" SYS_INNODB_BUFFER_STATS_BY_TABLE_ALLOCATED_EXPR ") AS `allocated`,"            \
    "format_bytes(sum(" SYS_INNODB_BUFFER_STATS_BY_TABLE_SELECT_SUFFIX                             \
    "round((sum(`ibp`.`NUMBER_RECORDS`) / count(distinct `ibp`.`INDEX_NAME`)),0) AS "              \
    "`rows_cached`" SYS_INNODB_BUFFER_STATS_BY_TABLE_FROM_SUFFIX

#define SYS_X_INNODB_BUFFER_STATS_BY_TABLE_VIEW_DEFINITION                                         \
    SYS_INNODB_BUFFER_STATS_BY_TABLE_SELECT_PREFIX                                                 \
    SYS_INNODB_BUFFER_STATS_BY_TABLE_ALLOCATED_EXPR                                                \
    " AS `allocated`,sum(" SYS_INNODB_BUFFER_STATS_BY_TABLE_SELECT_SUFFIX                          \
    "round(ifnull((sum(`ibp`.`NUMBER_RECORDS`) / nullif(count(distinct "                           \
    "`ibp`.`INDEX_NAME`),0)),0),"                                                                  \
    "0) AS `rows_cached`" SYS_INNODB_BUFFER_STATS_BY_TABLE_FROM_SUFFIX

static const char sys_innodb_buffer_stats_by_table_view_definition[] =
    SYS_INNODB_BUFFER_STATS_BY_TABLE_VIEW_DEFINITION;

static const char sys_innodb_buffer_stats_by_table_show_create_view_sql[] =
    "CREATE ALGORITHM=TEMPTABLE DEFINER=`mysql.sys`@`localhost` SQL SECURITY INVOKER VIEW "
    "`innodb_buffer_stats_by_table` " SYS_INNODB_BUFFER_STATS_BY_TABLE_VIEW_COLUMNS
    " AS " SYS_INNODB_BUFFER_STATS_BY_TABLE_VIEW_DEFINITION;

static const char sys_innodb_buffer_stats_by_table_show_create_qualified_view_sql[] =
    "CREATE ALGORITHM=TEMPTABLE DEFINER=`mysql.sys`@`localhost` SQL SECURITY INVOKER VIEW "
    "`sys`.`innodb_buffer_stats_by_table` " SYS_INNODB_BUFFER_STATS_BY_TABLE_VIEW_COLUMNS
    " AS " SYS_INNODB_BUFFER_STATS_BY_TABLE_VIEW_DEFINITION;

static const char sys_x_innodb_buffer_stats_by_table_view_definition[] =
    SYS_X_INNODB_BUFFER_STATS_BY_TABLE_VIEW_DEFINITION;

static const char sys_x_innodb_buffer_stats_by_table_show_create_view_sql[] =
    "CREATE ALGORITHM=TEMPTABLE DEFINER=`mysql.sys`@`localhost` SQL SECURITY INVOKER VIEW "
    "`x$innodb_buffer_stats_by_table` " SYS_INNODB_BUFFER_STATS_BY_TABLE_VIEW_COLUMNS
    " AS " SYS_X_INNODB_BUFFER_STATS_BY_TABLE_VIEW_DEFINITION;

static const char sys_x_innodb_buffer_stats_by_table_show_create_qualified_view_sql[] =
    "CREATE ALGORITHM=TEMPTABLE DEFINER=`mysql.sys`@`localhost` SQL SECURITY INVOKER VIEW "
    "`sys`.`x$innodb_buffer_stats_by_table` " SYS_INNODB_BUFFER_STATS_BY_TABLE_VIEW_COLUMNS
    " AS " SYS_X_INNODB_BUFFER_STATS_BY_TABLE_VIEW_DEFINITION;

#undef SYS_INNODB_BUFFER_STATS_BY_TABLE_VIEW_COLUMNS
#undef SYS_INNODB_BUFFER_STATS_BY_TABLE_OBJECT_SCHEMA_EXPR
#undef SYS_INNODB_BUFFER_STATS_BY_TABLE_OBJECT_NAME_EXPR
#undef SYS_INNODB_BUFFER_STATS_BY_TABLE_ALLOCATED_EXPR
#undef SYS_INNODB_BUFFER_STATS_BY_TABLE_SELECT_PREFIX
#undef SYS_INNODB_BUFFER_STATS_BY_TABLE_SELECT_SUFFIX
#undef SYS_INNODB_BUFFER_STATS_BY_TABLE_FROM_SUFFIX
#undef SYS_INNODB_BUFFER_STATS_BY_TABLE_VIEW_DEFINITION
#undef SYS_X_INNODB_BUFFER_STATS_BY_TABLE_VIEW_DEFINITION

#define SYS_INNODB_LOCK_WAITS_VIEW_COLUMNS                                                         \
    "(`wait_started`,`wait_age`,`wait_age_secs`,`locked_table`,`locked_table_schema`,"             \
    "`locked_table_name`,`locked_table_partition`,`locked_table_subpartition`,`locked_index`,"     \
    "`locked_type`,`waiting_trx_id`,`waiting_trx_started`,`waiting_trx_age`,"                      \
    "`waiting_trx_rows_locked`,`waiting_trx_rows_modified`,`waiting_pid`,`waiting_query`,"         \
    "`waiting_lock_id`,`waiting_lock_mode`,`blocking_trx_id`,`blocking_pid`,`blocking_query`,"     \
    "`blocking_lock_id`,`blocking_lock_mode`,`blocking_trx_started`,`blocking_trx_age`,"           \
    "`blocking_trx_rows_locked`,`blocking_trx_rows_modified`,`sql_kill_blocking_query`,"           \
    "`sql_kill_blocking_connection`)"

#define SYS_INNODB_LOCK_WAITS_SELECT_PREFIX                                                        \
    "select `r`.`trx_wait_started` AS `wait_started`,timediff(now(),"                              \
    "`r`.`trx_wait_started`) AS `wait_age`,timestampdiff(SECOND,`r`.`trx_wait_started`,now()) "    \
    "AS `wait_age_secs`,concat(`sys`.`quote_identifier`(`rl`.`OBJECT_SCHEMA`),'.',"                \
    "`sys`.`quote_identifier`(`rl`.`OBJECT_NAME`)) AS `locked_table`,`rl`.`OBJECT_SCHEMA` AS "     \
    "`locked_table_schema`,`rl`.`OBJECT_NAME` AS `locked_table_name`,`rl`.`PARTITION_NAME` AS "    \
    "`locked_table_partition`,`rl`.`SUBPARTITION_NAME` AS `locked_table_subpartition`,"            \
    "`rl`.`INDEX_NAME` AS `locked_index`,`rl`.`LOCK_TYPE` AS `locked_type`,`r`.`trx_id` AS "       \
    "`waiting_trx_id`,`r`.`trx_started` AS `waiting_trx_started`,timediff(now(),"                  \
    "`r`.`trx_started`) AS `waiting_trx_age`,`r`.`trx_rows_locked` AS "                            \
    "`waiting_trx_rows_locked`,`r`.`trx_rows_modified` AS `waiting_trx_rows_modified`,"            \
    "`r`.`trx_mysql_thread_id` AS `waiting_pid`,"

#define SYS_INNODB_LOCK_WAITS_SELECT_BRIDGE                                                        \
    " AS `waiting_query`,`rl`.`ENGINE_LOCK_ID` AS `waiting_lock_id`,`rl`.`LOCK_MODE` AS "          \
    "`waiting_lock_mode`,`b`.`trx_id` AS `blocking_trx_id`,`b`.`trx_mysql_thread_id` AS "          \
    "`blocking_pid`,"

#define SYS_INNODB_LOCK_WAITS_SELECT_SUFFIX                                                        \
    " AS `blocking_query`,`bl`.`ENGINE_LOCK_ID` AS `blocking_lock_id`,`bl`.`LOCK_MODE` AS "        \
    "`blocking_lock_mode`,`b`.`trx_started` AS `blocking_trx_started`,timediff(now(),"             \
    "`b`.`trx_started`) AS `blocking_trx_age`,`b`.`trx_rows_locked` AS "                           \
    "`blocking_trx_rows_locked`,`b`.`trx_rows_modified` AS `blocking_trx_rows_modified`,"          \
    "concat('KILL QUERY ',`b`.`trx_mysql_thread_id`) AS `sql_kill_blocking_query`,"                \
    "concat('KILL ',`b`.`trx_mysql_thread_id`) AS `sql_kill_blocking_connection` from "            \
    "((((`performance_schema`.`data_lock_waits` `w` join `information_schema`.`INNODB_TRX` "       \
    "`b` on((`b`.`trx_id` = cast(`w`.`BLOCKING_ENGINE_TRANSACTION_ID` as char charset "            \
    "utf8mb4)))) join `information_schema`.`INNODB_TRX` `r` on((`r`.`trx_id` = cast("              \
    "`w`.`REQUESTING_ENGINE_TRANSACTION_ID` as char charset utf8mb4)))) join "                     \
    "`performance_schema`.`data_locks` `bl` on(((`bl`.`ENGINE_LOCK_ID` = "                         \
    "`w`.`BLOCKING_ENGINE_LOCK_ID`) and (`bl`.`ENGINE` = `w`.`ENGINE`)))) join "                   \
    "`performance_schema`.`data_locks` `rl` on(((`rl`.`ENGINE_LOCK_ID` = "                         \
    "`w`.`REQUESTING_ENGINE_LOCK_ID`) and (`rl`.`ENGINE` = `w`.`ENGINE`)))) order by "             \
    "`r`.`trx_wait_started`"

#define SYS_INNODB_LOCK_WAITS_VIEW_DEFINITION                                                      \
    SYS_INNODB_LOCK_WAITS_SELECT_PREFIX                                                            \
    "`sys`.`format_statement`(`r`.`trx_query`)" SYS_INNODB_LOCK_WAITS_SELECT_BRIDGE                \
    "`sys`.`format_statement`(`b`.`trx_query`)" SYS_INNODB_LOCK_WAITS_SELECT_SUFFIX

#define SYS_X_INNODB_LOCK_WAITS_VIEW_DEFINITION                                                    \
    SYS_INNODB_LOCK_WAITS_SELECT_PREFIX "`r`.`trx_query`" SYS_INNODB_LOCK_WAITS_SELECT_BRIDGE      \
                                        "`b`.`trx_query`" SYS_INNODB_LOCK_WAITS_SELECT_SUFFIX

static const char sys_innodb_lock_waits_view_definition[] = SYS_INNODB_LOCK_WAITS_VIEW_DEFINITION;

static const char sys_innodb_lock_waits_show_create_view_sql[] =
    "CREATE ALGORITHM=TEMPTABLE DEFINER=`mysql.sys`@`localhost` SQL SECURITY INVOKER VIEW "
    "`innodb_lock_waits` " SYS_INNODB_LOCK_WAITS_VIEW_COLUMNS
    " AS " SYS_INNODB_LOCK_WAITS_VIEW_DEFINITION;

static const char sys_innodb_lock_waits_show_create_qualified_view_sql[] =
    "CREATE ALGORITHM=TEMPTABLE DEFINER=`mysql.sys`@`localhost` SQL SECURITY INVOKER VIEW "
    "`sys`.`innodb_lock_waits` " SYS_INNODB_LOCK_WAITS_VIEW_COLUMNS
    " AS " SYS_INNODB_LOCK_WAITS_VIEW_DEFINITION;

static const char sys_x_innodb_lock_waits_view_definition[] =
    SYS_X_INNODB_LOCK_WAITS_VIEW_DEFINITION;

static const char sys_x_innodb_lock_waits_show_create_view_sql[] =
    "CREATE ALGORITHM=TEMPTABLE DEFINER=`mysql.sys`@`localhost` SQL SECURITY INVOKER VIEW "
    "`x$innodb_lock_waits` " SYS_INNODB_LOCK_WAITS_VIEW_COLUMNS
    " AS " SYS_X_INNODB_LOCK_WAITS_VIEW_DEFINITION;

static const char sys_x_innodb_lock_waits_show_create_qualified_view_sql[] =
    "CREATE ALGORITHM=TEMPTABLE DEFINER=`mysql.sys`@`localhost` SQL SECURITY INVOKER VIEW "
    "`sys`.`x$innodb_lock_waits` " SYS_INNODB_LOCK_WAITS_VIEW_COLUMNS
    " AS " SYS_X_INNODB_LOCK_WAITS_VIEW_DEFINITION;

#undef SYS_INNODB_LOCK_WAITS_VIEW_COLUMNS
#undef SYS_INNODB_LOCK_WAITS_SELECT_PREFIX
#undef SYS_INNODB_LOCK_WAITS_SELECT_BRIDGE
#undef SYS_INNODB_LOCK_WAITS_SELECT_SUFFIX
#undef SYS_INNODB_LOCK_WAITS_VIEW_DEFINITION
#undef SYS_X_INNODB_LOCK_WAITS_VIEW_DEFINITION

#define SYS_IO_BY_THREAD_BY_LATENCY_VIEW_COLUMNS                                                   \
    "(`user`,`total`,`total_latency`,`min_latency`,`avg_latency`,`max_latency`,`thread_id`,"       \
    "`processlist_id`)"

#define SYS_IO_BY_THREAD_BY_LATENCY_SELECT_PREFIX                                                  \
    "select if((`performance_schema`.`threads`.`PROCESSLIST_ID` is null),substring_index("         \
    "`performance_schema`.`threads`.`NAME`,'/',-(1)),concat(`performance_schema`.`threads`."       \
    "`PROCESSLIST_USER`,'@',convert(`performance_schema`.`threads`.`PROCESSLIST_HOST` using "      \
    "utf8mb4))) AS `user`,sum(`performance_schema`."                                               \
    "`events_waits_summary_by_thread_by_event_name`.`COUNT_STAR`) AS `total`,"

#define SYS_IO_BY_THREAD_BY_LATENCY_SELECT_SUFFIX                                                  \
    "`performance_schema`.`events_waits_summary_by_thread_by_event_name`.`THREAD_ID` AS "          \
    "`thread_id`,`performance_schema`.`threads`.`PROCESSLIST_ID` AS `processlist_id` from "        \
    "(`performance_schema`.`events_waits_summary_by_thread_by_event_name` left join "              \
    "`performance_schema`.`threads` on((`performance_schema`."                                     \
    "`events_waits_summary_by_thread_by_event_name`.`THREAD_ID` = `performance_schema`."           \
    "`threads`.`THREAD_ID`))) where ((`performance_schema`."                                       \
    "`events_waits_summary_by_thread_by_event_name`.`EVENT_NAME` like 'wait/io/file/%') and "      \
    "(`performance_schema`.`events_waits_summary_by_thread_by_event_name`.`SUM_TIMER_WAIT` > "     \
    "0)) group by `performance_schema`.`events_waits_summary_by_thread_by_event_name`."            \
    "`THREAD_ID`,`performance_schema`.`threads`.`PROCESSLIST_ID`,`user` order by sum("             \
    "`performance_schema`.`events_waits_summary_by_thread_by_event_name`.`SUM_TIMER_WAIT`) desc"

#define SYS_IO_BY_THREAD_BY_LATENCY_VIEW_DEFINITION                                                \
    SYS_IO_BY_THREAD_BY_LATENCY_SELECT_PREFIX                                                      \
    "format_pico_time(sum(`performance_schema`.`events_waits_summary_by_thread_by_event_name`."    \
    "`SUM_TIMER_WAIT`)) AS `total_latency`,format_pico_time(min(`performance_schema`."             \
    "`events_waits_summary_by_thread_by_event_name`.`MIN_TIMER_WAIT`)) AS "                        \
    "`min_latency`,format_pico_time(avg(`performance_schema`."                                     \
    "`events_waits_summary_by_thread_by_event_name`.`AVG_TIMER_WAIT`)) AS "                        \
    "`avg_latency`,format_pico_time(max(`performance_schema`."                                     \
    "`events_waits_summary_by_thread_by_event_name`.`MAX_TIMER_WAIT`)) AS "                        \
    "`max_latency`," SYS_IO_BY_THREAD_BY_LATENCY_SELECT_SUFFIX

#define SYS_X_IO_BY_THREAD_BY_LATENCY_VIEW_DEFINITION                                              \
    SYS_IO_BY_THREAD_BY_LATENCY_SELECT_PREFIX                                                      \
    "sum(`performance_schema`.`events_waits_summary_by_thread_by_event_name`.`SUM_TIMER_WAIT`) "   \
    "AS `total_latency`,min(`performance_schema`."                                                 \
    "`events_waits_summary_by_thread_by_event_name`.`MIN_TIMER_WAIT`) AS "                         \
    "`min_latency`,avg(`performance_schema`."                                                      \
    "`events_waits_summary_by_thread_by_event_name`.`AVG_TIMER_WAIT`) AS "                         \
    "`avg_latency`,max(`performance_schema`."                                                      \
    "`events_waits_summary_by_thread_by_event_name`.`MAX_TIMER_WAIT`) AS "                         \
    "`max_latency`," SYS_IO_BY_THREAD_BY_LATENCY_SELECT_SUFFIX

static const char sys_io_by_thread_by_latency_view_definition[] =
    SYS_IO_BY_THREAD_BY_LATENCY_VIEW_DEFINITION;

static const char sys_io_by_thread_by_latency_show_create_view_sql[] =
    "CREATE ALGORITHM=TEMPTABLE DEFINER=`mysql.sys`@`localhost` SQL SECURITY INVOKER VIEW "
    "`io_by_thread_by_latency` " SYS_IO_BY_THREAD_BY_LATENCY_VIEW_COLUMNS
    " AS " SYS_IO_BY_THREAD_BY_LATENCY_VIEW_DEFINITION;

static const char sys_io_by_thread_by_latency_show_create_qualified_view_sql[] =
    "CREATE ALGORITHM=TEMPTABLE DEFINER=`mysql.sys`@`localhost` SQL SECURITY INVOKER VIEW "
    "`sys`.`io_by_thread_by_latency` " SYS_IO_BY_THREAD_BY_LATENCY_VIEW_COLUMNS
    " AS " SYS_IO_BY_THREAD_BY_LATENCY_VIEW_DEFINITION;

static const char sys_x_io_by_thread_by_latency_view_definition[] =
    SYS_X_IO_BY_THREAD_BY_LATENCY_VIEW_DEFINITION;

static const char sys_x_io_by_thread_by_latency_show_create_view_sql[] =
    "CREATE ALGORITHM=TEMPTABLE DEFINER=`mysql.sys`@`localhost` SQL SECURITY INVOKER VIEW "
    "`x$io_by_thread_by_latency` " SYS_IO_BY_THREAD_BY_LATENCY_VIEW_COLUMNS
    " AS " SYS_X_IO_BY_THREAD_BY_LATENCY_VIEW_DEFINITION;

static const char sys_x_io_by_thread_by_latency_show_create_qualified_view_sql[] =
    "CREATE ALGORITHM=TEMPTABLE DEFINER=`mysql.sys`@`localhost` SQL SECURITY INVOKER VIEW "
    "`sys`.`x$io_by_thread_by_latency` " SYS_IO_BY_THREAD_BY_LATENCY_VIEW_COLUMNS
    " AS " SYS_X_IO_BY_THREAD_BY_LATENCY_VIEW_DEFINITION;

#undef SYS_IO_BY_THREAD_BY_LATENCY_VIEW_COLUMNS
#undef SYS_IO_BY_THREAD_BY_LATENCY_SELECT_PREFIX
#undef SYS_IO_BY_THREAD_BY_LATENCY_SELECT_SUFFIX
#undef SYS_IO_BY_THREAD_BY_LATENCY_VIEW_DEFINITION
#undef SYS_X_IO_BY_THREAD_BY_LATENCY_VIEW_DEFINITION

#define SYS_IO_GLOBAL_BY_FILE_BY_BYTES_VIEW_COLUMNS                                                \
    "(`file`,`count_read`,`total_read`,`avg_read`,`count_write`,`total_written`,`avg_write`,"      \
    "`total`,`write_pct`)"

#define SYS_IO_GLOBAL_BY_FILE_BY_BYTES_VIEW_DEFINITION                                             \
    "select `sys`.`format_path`(`performance_schema`.`file_summary_by_instance`.`FILE_NAME`) AS "  \
    "`file`,`performance_schema`.`file_summary_by_instance`.`COUNT_READ` AS "                      \
    "`count_read`,format_bytes(`performance_schema`.`file_summary_by_instance`."                   \
    "`SUM_NUMBER_OF_BYTES_READ`) AS `total_read`,format_bytes(ifnull(("                            \
    "`performance_schema`.`file_summary_by_instance`.`SUM_NUMBER_OF_BYTES_READ` / nullif("         \
    "`performance_schema`.`file_summary_by_instance`.`COUNT_READ`,0)),0)) AS "                     \
    "`avg_read`,`performance_schema`.`file_summary_by_instance`.`COUNT_WRITE` AS "                 \
    "`count_write`,format_bytes(`performance_schema`.`file_summary_by_instance`."                  \
    "`SUM_NUMBER_OF_BYTES_WRITE`) AS `total_written`,format_bytes(ifnull(("                        \
    "`performance_schema`.`file_summary_by_instance`.`SUM_NUMBER_OF_BYTES_WRITE` / nullif("        \
    "`performance_schema`.`file_summary_by_instance`.`COUNT_WRITE`,0)),0.00)) AS "                 \
    "`avg_write`,format_bytes((`performance_schema`.`file_summary_by_instance`."                   \
    "`SUM_NUMBER_OF_BYTES_READ` + `performance_schema`.`file_summary_by_instance`."                \
    "`SUM_NUMBER_OF_BYTES_WRITE`)) AS `total`,ifnull(round((100 - (("                              \
    "`performance_schema`.`file_summary_by_instance`.`SUM_NUMBER_OF_BYTES_READ` / nullif(("        \
    "`performance_schema`.`file_summary_by_instance`.`SUM_NUMBER_OF_BYTES_READ` + "                \
    "`performance_schema`.`file_summary_by_instance`.`SUM_NUMBER_OF_BYTES_WRITE`),0)) * "          \
    "100)),2),0.00) AS `write_pct` from `performance_schema`.`file_summary_by_instance` "          \
    "order by (`performance_schema`.`file_summary_by_instance`.`SUM_NUMBER_OF_BYTES_READ` + "      \
    "`performance_schema`.`file_summary_by_instance`.`SUM_NUMBER_OF_BYTES_WRITE`) desc"

#define SYS_X_IO_GLOBAL_BY_FILE_BY_BYTES_VIEW_DEFINITION                                           \
    "select `performance_schema`.`file_summary_by_instance`.`FILE_NAME` AS "                       \
    "`file`,`performance_schema`.`file_summary_by_instance`.`COUNT_READ` AS "                      \
    "`count_read`,`performance_schema`.`file_summary_by_instance`.`SUM_NUMBER_OF_BYTES_READ` AS "  \
    "`total_read`,ifnull((`performance_schema`.`file_summary_by_instance`."                        \
    "`SUM_NUMBER_OF_BYTES_READ` / nullif(`performance_schema`.`file_summary_by_instance`."         \
    "`COUNT_READ`,0)),0) AS `avg_read`,`performance_schema`.`file_summary_by_instance`."           \
    "`COUNT_WRITE` AS `count_write`,`performance_schema`.`file_summary_by_instance`."              \
    "`SUM_NUMBER_OF_BYTES_WRITE` AS `total_written`,ifnull(("                                      \
    "`performance_schema`.`file_summary_by_instance`.`SUM_NUMBER_OF_BYTES_WRITE` / nullif("        \
    "`performance_schema`.`file_summary_by_instance`.`COUNT_WRITE`,0)),0.00) AS "                  \
    "`avg_write`,(`performance_schema`.`file_summary_by_instance`.`SUM_NUMBER_OF_BYTES_READ` + "   \
    "`performance_schema`.`file_summary_by_instance`.`SUM_NUMBER_OF_BYTES_WRITE`) AS "             \
    "`total`,ifnull(round((100 - ((`performance_schema`.`file_summary_by_instance`."               \
    "`SUM_NUMBER_OF_BYTES_READ` / nullif((`performance_schema`.`file_summary_by_instance`."        \
    "`SUM_NUMBER_OF_BYTES_READ` + `performance_schema`.`file_summary_by_instance`."                \
    "`SUM_NUMBER_OF_BYTES_WRITE`),0)) * 100)),2),0.00) AS `write_pct` from "                       \
    "`performance_schema`.`file_summary_by_instance` order by ("                                   \
    "`performance_schema`.`file_summary_by_instance`.`SUM_NUMBER_OF_BYTES_READ` + "                \
    "`performance_schema`.`file_summary_by_instance`.`SUM_NUMBER_OF_BYTES_WRITE`) desc"

static const char sys_io_global_by_file_by_bytes_view_definition[] =
    SYS_IO_GLOBAL_BY_FILE_BY_BYTES_VIEW_DEFINITION;

static const char sys_io_global_by_file_by_bytes_show_create_view_sql[] =
    "CREATE ALGORITHM=MERGE DEFINER=`mysql.sys`@`localhost` SQL SECURITY INVOKER VIEW "
    "`io_global_by_file_by_bytes` " SYS_IO_GLOBAL_BY_FILE_BY_BYTES_VIEW_COLUMNS
    " AS " SYS_IO_GLOBAL_BY_FILE_BY_BYTES_VIEW_DEFINITION;

static const char sys_io_global_by_file_by_bytes_show_create_qualified_view_sql[] =
    "CREATE ALGORITHM=MERGE DEFINER=`mysql.sys`@`localhost` SQL SECURITY INVOKER VIEW "
    "`sys`.`io_global_by_file_by_bytes` " SYS_IO_GLOBAL_BY_FILE_BY_BYTES_VIEW_COLUMNS
    " AS " SYS_IO_GLOBAL_BY_FILE_BY_BYTES_VIEW_DEFINITION;

static const char sys_x_io_global_by_file_by_bytes_view_definition[] =
    SYS_X_IO_GLOBAL_BY_FILE_BY_BYTES_VIEW_DEFINITION;

static const char sys_x_io_global_by_file_by_bytes_show_create_view_sql[] =
    "CREATE ALGORITHM=MERGE DEFINER=`mysql.sys`@`localhost` SQL SECURITY INVOKER VIEW "
    "`x$io_global_by_file_by_bytes` " SYS_IO_GLOBAL_BY_FILE_BY_BYTES_VIEW_COLUMNS
    " AS " SYS_X_IO_GLOBAL_BY_FILE_BY_BYTES_VIEW_DEFINITION;

static const char sys_x_io_global_by_file_by_bytes_show_create_qualified_view_sql[] =
    "CREATE ALGORITHM=MERGE DEFINER=`mysql.sys`@`localhost` SQL SECURITY INVOKER VIEW "
    "`sys`.`x$io_global_by_file_by_bytes` " SYS_IO_GLOBAL_BY_FILE_BY_BYTES_VIEW_COLUMNS
    " AS " SYS_X_IO_GLOBAL_BY_FILE_BY_BYTES_VIEW_DEFINITION;

#undef SYS_IO_GLOBAL_BY_FILE_BY_BYTES_VIEW_COLUMNS
#undef SYS_IO_GLOBAL_BY_FILE_BY_BYTES_VIEW_DEFINITION
#undef SYS_X_IO_GLOBAL_BY_FILE_BY_BYTES_VIEW_DEFINITION

#define SYS_IO_GLOBAL_BY_FILE_BY_LATENCY_VIEW_COLUMNS                                              \
    "(`file`,`total`,`total_latency`,`count_read`,`read_latency`,`count_write`,`write_latency`,"   \
    "`count_misc`,`misc_latency`)"

#define SYS_IO_GLOBAL_BY_FILE_BY_LATENCY_VIEW_DEFINITION                                           \
    "select `sys`.`format_path`(`performance_schema`.`file_summary_by_instance`.`FILE_NAME`) AS "  \
    "`file`,`performance_schema`.`file_summary_by_instance`.`COUNT_STAR` AS "                      \
    "`total`,format_pico_time(`performance_schema`.`file_summary_by_instance`.`SUM_TIMER_WAIT`) "  \
    "AS `total_latency`,`performance_schema`.`file_summary_by_instance`.`COUNT_READ` AS "          \
    "`count_read`,format_pico_time(`performance_schema`.`file_summary_by_instance`."               \
    "`SUM_TIMER_READ`) AS `read_latency`,`performance_schema`.`file_summary_by_instance`."         \
    "`COUNT_WRITE` AS `count_write`,format_pico_time(`performance_schema`."                        \
    "`file_summary_by_instance`.`SUM_TIMER_WRITE`) AS `write_latency`,`performance_schema`."       \
    "`file_summary_by_instance`.`COUNT_MISC` AS `count_misc`,format_pico_time("                    \
    "`performance_schema`.`file_summary_by_instance`.`SUM_TIMER_MISC`) AS "                        \
    "`misc_latency` from `performance_schema`.`file_summary_by_instance` order by "                \
    "`performance_schema`.`file_summary_by_instance`.`SUM_TIMER_WAIT` desc"

#define SYS_X_IO_GLOBAL_BY_FILE_BY_LATENCY_VIEW_DEFINITION                                         \
    "select `performance_schema`.`file_summary_by_instance`.`FILE_NAME` AS "                       \
    "`file`,`performance_schema`.`file_summary_by_instance`.`COUNT_STAR` AS "                      \
    "`total`,`performance_schema`.`file_summary_by_instance`.`SUM_TIMER_WAIT` AS "                 \
    "`total_latency`,`performance_schema`.`file_summary_by_instance`.`COUNT_READ` AS "             \
    "`count_read`,`performance_schema`.`file_summary_by_instance`.`SUM_TIMER_READ` AS "            \
    "`read_latency`,`performance_schema`.`file_summary_by_instance`.`COUNT_WRITE` AS "             \
    "`count_write`,`performance_schema`.`file_summary_by_instance`.`SUM_TIMER_WRITE` AS "          \
    "`write_latency`,`performance_schema`.`file_summary_by_instance`.`COUNT_MISC` AS "             \
    "`count_misc`,`performance_schema`.`file_summary_by_instance`.`SUM_TIMER_MISC` AS "            \
    "`misc_latency` from `performance_schema`.`file_summary_by_instance` order by "                \
    "`performance_schema`.`file_summary_by_instance`.`SUM_TIMER_WAIT` desc"

static const char sys_io_global_by_file_by_latency_view_definition[] =
    SYS_IO_GLOBAL_BY_FILE_BY_LATENCY_VIEW_DEFINITION;

static const char sys_io_global_by_file_by_latency_show_create_view_sql[] =
    "CREATE ALGORITHM=MERGE DEFINER=`mysql.sys`@`localhost` SQL SECURITY INVOKER VIEW "
    "`io_global_by_file_by_latency` " SYS_IO_GLOBAL_BY_FILE_BY_LATENCY_VIEW_COLUMNS
    " AS " SYS_IO_GLOBAL_BY_FILE_BY_LATENCY_VIEW_DEFINITION;

static const char sys_io_global_by_file_by_latency_show_create_qualified_view_sql[] =
    "CREATE ALGORITHM=MERGE DEFINER=`mysql.sys`@`localhost` SQL SECURITY INVOKER VIEW "
    "`sys`.`io_global_by_file_by_latency` " SYS_IO_GLOBAL_BY_FILE_BY_LATENCY_VIEW_COLUMNS
    " AS " SYS_IO_GLOBAL_BY_FILE_BY_LATENCY_VIEW_DEFINITION;

static const char sys_x_io_global_by_file_by_latency_view_definition[] =
    SYS_X_IO_GLOBAL_BY_FILE_BY_LATENCY_VIEW_DEFINITION;

static const char sys_x_io_global_by_file_by_latency_show_create_view_sql[] =
    "CREATE ALGORITHM=MERGE DEFINER=`mysql.sys`@`localhost` SQL SECURITY INVOKER VIEW "
    "`x$io_global_by_file_by_latency` " SYS_IO_GLOBAL_BY_FILE_BY_LATENCY_VIEW_COLUMNS
    " AS " SYS_X_IO_GLOBAL_BY_FILE_BY_LATENCY_VIEW_DEFINITION;

static const char sys_x_io_global_by_file_by_latency_show_create_qualified_view_sql[] =
    "CREATE ALGORITHM=MERGE DEFINER=`mysql.sys`@`localhost` SQL SECURITY INVOKER VIEW "
    "`sys`.`x$io_global_by_file_by_latency` " SYS_IO_GLOBAL_BY_FILE_BY_LATENCY_VIEW_COLUMNS
    " AS " SYS_X_IO_GLOBAL_BY_FILE_BY_LATENCY_VIEW_DEFINITION;

#undef SYS_IO_GLOBAL_BY_FILE_BY_LATENCY_VIEW_COLUMNS
#undef SYS_IO_GLOBAL_BY_FILE_BY_LATENCY_VIEW_DEFINITION
#undef SYS_X_IO_GLOBAL_BY_FILE_BY_LATENCY_VIEW_DEFINITION

#define SYS_IO_GLOBAL_BY_WAIT_BY_BYTES_VIEW_COLUMNS                                                \
    "(`event_name`,`total`,`total_latency`,`min_latency`,`avg_latency`,`max_latency`,"             \
    "`count_read`,`total_read`,`avg_read`,`count_write`,`total_written`,`avg_written`,"            \
    "`total_requested`)"

#define SYS_IO_GLOBAL_BY_WAIT_BY_BYTES_VIEW_DEFINITION                                             \
    "select substring_index(`performance_schema`.`file_summary_by_event_name`.`EVENT_NAME`,"       \
    "'/',-(2)) AS `event_name`,`performance_schema`.`file_summary_by_event_name`.`COUNT_STAR` AS " \
    "`total`,format_pico_time(`performance_schema`.`file_summary_by_event_name`."                  \
    "`SUM_TIMER_WAIT`) AS `total_latency`,format_pico_time(`performance_schema`."                  \
    "`file_summary_by_event_name`.`MIN_TIMER_WAIT`) AS `min_latency`,format_pico_time("            \
    "`performance_schema`.`file_summary_by_event_name`.`AVG_TIMER_WAIT`) AS "                      \
    "`avg_latency`,format_pico_time(`performance_schema`.`file_summary_by_event_name`."            \
    "`MAX_TIMER_WAIT`) AS `max_latency`,`performance_schema`.`file_summary_by_event_name`."        \
    "`COUNT_READ` AS `count_read`,format_bytes(`performance_schema`.`file_summary_by_event_name`." \
    "`SUM_NUMBER_OF_BYTES_READ`) AS `total_read`,format_bytes(ifnull(("                            \
    "`performance_schema`.`file_summary_by_event_name`.`SUM_NUMBER_OF_BYTES_READ` / nullif("       \
    "`performance_schema`.`file_summary_by_event_name`.`COUNT_READ`,0)),0)) AS "                   \
    "`avg_read`,`performance_schema`.`file_summary_by_event_name`.`COUNT_WRITE` AS "               \
    "`count_write`,format_bytes(`performance_schema`.`file_summary_by_event_name`."                \
    "`SUM_NUMBER_OF_BYTES_WRITE`) AS `total_written`,format_bytes(ifnull(("                        \
    "`performance_schema`.`file_summary_by_event_name`.`SUM_NUMBER_OF_BYTES_WRITE` / nullif("      \
    "`performance_schema`.`file_summary_by_event_name`.`COUNT_WRITE`,0)),0)) AS "                  \
    "`avg_written`,format_bytes((`performance_schema`.`file_summary_by_event_name`."               \
    "`SUM_NUMBER_OF_BYTES_WRITE` + `performance_schema`.`file_summary_by_event_name`."             \
    "`SUM_NUMBER_OF_BYTES_READ`)) AS `total_requested` from "                                      \
    "`performance_schema`.`file_summary_by_event_name` where (("                                   \
    "`performance_schema`.`file_summary_by_event_name`.`EVENT_NAME` like 'wait/io/file/%') and "   \
    "(`performance_schema`.`file_summary_by_event_name`.`COUNT_STAR` > 0)) order by ("             \
    "`performance_schema`.`file_summary_by_event_name`.`SUM_NUMBER_OF_BYTES_WRITE` + "             \
    "`performance_schema`.`file_summary_by_event_name`.`SUM_NUMBER_OF_BYTES_READ`) desc"

#define SYS_X_IO_GLOBAL_BY_WAIT_BY_BYTES_VIEW_DEFINITION                                           \
    "select substring_index(`performance_schema`.`file_summary_by_event_name`.`EVENT_NAME`,"       \
    "'/',-(2)) AS `event_name`,`performance_schema`.`file_summary_by_event_name`.`COUNT_STAR` AS " \
    "`total`,`performance_schema`.`file_summary_by_event_name`.`SUM_TIMER_WAIT` AS "               \
    "`total_latency`,`performance_schema`.`file_summary_by_event_name`.`MIN_TIMER_WAIT` AS "       \
    "`min_latency`,`performance_schema`.`file_summary_by_event_name`.`AVG_TIMER_WAIT` AS "         \
    "`avg_latency`,`performance_schema`.`file_summary_by_event_name`.`MAX_TIMER_WAIT` AS "         \
    "`max_latency`,`performance_schema`.`file_summary_by_event_name`.`COUNT_READ` AS "             \
    "`count_read`,`performance_schema`.`file_summary_by_event_name`.`SUM_NUMBER_OF_BYTES_READ` "   \
    "AS `total_read`,ifnull((`performance_schema`.`file_summary_by_event_name`."                   \
    "`SUM_NUMBER_OF_BYTES_READ` / nullif(`performance_schema`.`file_summary_by_event_name`."       \
    "`COUNT_READ`,0)),0) AS `avg_read`,`performance_schema`.`file_summary_by_event_name`."         \
    "`COUNT_WRITE` AS `count_write`,`performance_schema`.`file_summary_by_event_name`."            \
    "`SUM_NUMBER_OF_BYTES_WRITE` AS `total_written`,ifnull((`performance_schema`."                 \
    "`file_summary_by_event_name`.`SUM_NUMBER_OF_BYTES_WRITE` / nullif(`performance_schema`."      \
    "`file_summary_by_event_name`.`COUNT_WRITE`,0)),0) AS `avg_written`,("                         \
    "`performance_schema`.`file_summary_by_event_name`.`SUM_NUMBER_OF_BYTES_WRITE` + "             \
    "`performance_schema`.`file_summary_by_event_name`.`SUM_NUMBER_OF_BYTES_READ`) AS "            \
    "`total_requested` from `performance_schema`.`file_summary_by_event_name` where (("            \
    "`performance_schema`.`file_summary_by_event_name`.`EVENT_NAME` like 'wait/io/file/%') and "   \
    "(`performance_schema`.`file_summary_by_event_name`.`COUNT_STAR` > 0)) order by ("             \
    "`performance_schema`.`file_summary_by_event_name`.`SUM_NUMBER_OF_BYTES_WRITE` + "             \
    "`performance_schema`.`file_summary_by_event_name`.`SUM_NUMBER_OF_BYTES_READ`) desc"

static const char sys_io_global_by_wait_by_bytes_view_definition[] =
    SYS_IO_GLOBAL_BY_WAIT_BY_BYTES_VIEW_DEFINITION;

static const char sys_io_global_by_wait_by_bytes_show_create_view_sql[] =
    "CREATE ALGORITHM=MERGE DEFINER=`mysql.sys`@`localhost` SQL SECURITY INVOKER VIEW "
    "`io_global_by_wait_by_bytes` " SYS_IO_GLOBAL_BY_WAIT_BY_BYTES_VIEW_COLUMNS
    " AS " SYS_IO_GLOBAL_BY_WAIT_BY_BYTES_VIEW_DEFINITION;

static const char sys_io_global_by_wait_by_bytes_show_create_qualified_view_sql[] =
    "CREATE ALGORITHM=MERGE DEFINER=`mysql.sys`@`localhost` SQL SECURITY INVOKER VIEW "
    "`sys`.`io_global_by_wait_by_bytes` " SYS_IO_GLOBAL_BY_WAIT_BY_BYTES_VIEW_COLUMNS
    " AS " SYS_IO_GLOBAL_BY_WAIT_BY_BYTES_VIEW_DEFINITION;

static const char sys_x_io_global_by_wait_by_bytes_view_definition[] =
    SYS_X_IO_GLOBAL_BY_WAIT_BY_BYTES_VIEW_DEFINITION;

static const char sys_x_io_global_by_wait_by_bytes_show_create_view_sql[] =
    "CREATE ALGORITHM=MERGE DEFINER=`mysql.sys`@`localhost` SQL SECURITY INVOKER VIEW "
    "`x$io_global_by_wait_by_bytes` " SYS_IO_GLOBAL_BY_WAIT_BY_BYTES_VIEW_COLUMNS
    " AS " SYS_X_IO_GLOBAL_BY_WAIT_BY_BYTES_VIEW_DEFINITION;

static const char sys_x_io_global_by_wait_by_bytes_show_create_qualified_view_sql[] =
    "CREATE ALGORITHM=MERGE DEFINER=`mysql.sys`@`localhost` SQL SECURITY INVOKER VIEW "
    "`sys`.`x$io_global_by_wait_by_bytes` " SYS_IO_GLOBAL_BY_WAIT_BY_BYTES_VIEW_COLUMNS
    " AS " SYS_X_IO_GLOBAL_BY_WAIT_BY_BYTES_VIEW_DEFINITION;

#undef SYS_IO_GLOBAL_BY_WAIT_BY_BYTES_VIEW_COLUMNS
#undef SYS_IO_GLOBAL_BY_WAIT_BY_BYTES_VIEW_DEFINITION
#undef SYS_X_IO_GLOBAL_BY_WAIT_BY_BYTES_VIEW_DEFINITION

#define SYS_IO_GLOBAL_BY_WAIT_BY_LATENCY_VIEW_COLUMNS                                              \
    "(`event_name`,`total`,`total_latency`,`avg_latency`,`max_latency`,`read_latency`,"            \
    "`write_latency`,`misc_latency`,`count_read`,`total_read`,`avg_read`,`count_write`,"           \
    "`total_written`,`avg_written`)"

#define SYS_IO_GLOBAL_BY_WAIT_BY_LATENCY_VIEW_DEFINITION                                           \
    "select substring_index(`performance_schema`.`file_summary_by_event_name`.`EVENT_NAME`,"       \
    "'/',-(2)) AS `event_name`,`performance_schema`.`file_summary_by_event_name`.`COUNT_STAR` AS " \
    "`total`,format_pico_time(`performance_schema`.`file_summary_by_event_name`."                  \
    "`SUM_TIMER_WAIT`) AS `total_latency`,format_pico_time(`performance_schema`."                  \
    "`file_summary_by_event_name`.`AVG_TIMER_WAIT`) AS `avg_latency`,format_pico_time("            \
    "`performance_schema`.`file_summary_by_event_name`.`MAX_TIMER_WAIT`) AS "                      \
    "`max_latency`,format_pico_time(`performance_schema`.`file_summary_by_event_name`."            \
    "`SUM_TIMER_READ`) AS `read_latency`,format_pico_time(`performance_schema`."                   \
    "`file_summary_by_event_name`.`SUM_TIMER_WRITE`) AS `write_latency`,format_pico_time("         \
    "`performance_schema`.`file_summary_by_event_name`.`SUM_TIMER_MISC`) AS "                      \
    "`misc_latency`,`performance_schema`.`file_summary_by_event_name`.`COUNT_READ` AS "            \
    "`count_read`,format_bytes(`performance_schema`.`file_summary_by_event_name`."                 \
    "`SUM_NUMBER_OF_BYTES_READ`) AS `total_read`,format_bytes(ifnull(("                            \
    "`performance_schema`.`file_summary_by_event_name`.`SUM_NUMBER_OF_BYTES_READ` / nullif("       \
    "`performance_schema`.`file_summary_by_event_name`.`COUNT_READ`,0)),0)) AS "                   \
    "`avg_read`,`performance_schema`.`file_summary_by_event_name`.`COUNT_WRITE` AS "               \
    "`count_write`,format_bytes(`performance_schema`.`file_summary_by_event_name`."                \
    "`SUM_NUMBER_OF_BYTES_WRITE`) AS `total_written`,format_bytes(ifnull(("                        \
    "`performance_schema`.`file_summary_by_event_name`.`SUM_NUMBER_OF_BYTES_WRITE` / nullif("      \
    "`performance_schema`.`file_summary_by_event_name`.`COUNT_WRITE`,0)),0)) AS "                  \
    "`avg_written` from `performance_schema`.`file_summary_by_event_name` where (("                \
    "`performance_schema`.`file_summary_by_event_name`.`EVENT_NAME` like 'wait/io/file/%') and "   \
    "(`performance_schema`.`file_summary_by_event_name`.`COUNT_STAR` > 0)) order by "              \
    "`performance_schema`.`file_summary_by_event_name`.`SUM_TIMER_WAIT` desc"

#define SYS_X_IO_GLOBAL_BY_WAIT_BY_LATENCY_VIEW_DEFINITION                                         \
    "select substring_index(`performance_schema`.`file_summary_by_event_name`.`EVENT_NAME`,"       \
    "'/',-(2)) AS `event_name`,`performance_schema`.`file_summary_by_event_name`.`COUNT_STAR` AS " \
    "`total`,`performance_schema`.`file_summary_by_event_name`.`SUM_TIMER_WAIT` AS "               \
    "`total_latency`,`performance_schema`.`file_summary_by_event_name`.`AVG_TIMER_WAIT` AS "       \
    "`avg_latency`,`performance_schema`.`file_summary_by_event_name`.`MAX_TIMER_WAIT` AS "         \
    "`max_latency`,`performance_schema`.`file_summary_by_event_name`.`SUM_TIMER_READ` AS "         \
    "`read_latency`,`performance_schema`.`file_summary_by_event_name`.`SUM_TIMER_WRITE` AS "       \
    "`write_latency`,`performance_schema`.`file_summary_by_event_name`.`SUM_TIMER_MISC` AS "       \
    "`misc_latency`,`performance_schema`.`file_summary_by_event_name`.`COUNT_READ` AS "            \
    "`count_read`,`performance_schema`.`file_summary_by_event_name`.`SUM_NUMBER_OF_BYTES_READ` "   \
    "AS `total_read`,ifnull((`performance_schema`.`file_summary_by_event_name`."                   \
    "`SUM_NUMBER_OF_BYTES_READ` / nullif(`performance_schema`.`file_summary_by_event_name`."       \
    "`COUNT_READ`,0)),0) AS `avg_read`,`performance_schema`.`file_summary_by_event_name`."         \
    "`COUNT_WRITE` AS `count_write`,`performance_schema`.`file_summary_by_event_name`."            \
    "`SUM_NUMBER_OF_BYTES_WRITE` AS `total_written`,ifnull((`performance_schema`."                 \
    "`file_summary_by_event_name`.`SUM_NUMBER_OF_BYTES_WRITE` / nullif(`performance_schema`."      \
    "`file_summary_by_event_name`.`COUNT_WRITE`,0)),0) AS `avg_written` from "                     \
    "`performance_schema`.`file_summary_by_event_name` where (("                                   \
    "`performance_schema`.`file_summary_by_event_name`.`EVENT_NAME` like 'wait/io/file/%') and "   \
    "(`performance_schema`.`file_summary_by_event_name`.`COUNT_STAR` > 0)) order by "              \
    "`performance_schema`.`file_summary_by_event_name`.`SUM_TIMER_WAIT` desc"

static const char sys_io_global_by_wait_by_latency_view_definition[] =
    SYS_IO_GLOBAL_BY_WAIT_BY_LATENCY_VIEW_DEFINITION;

static const char sys_io_global_by_wait_by_latency_show_create_view_sql[] =
    "CREATE ALGORITHM=MERGE DEFINER=`mysql.sys`@`localhost` SQL SECURITY INVOKER VIEW "
    "`io_global_by_wait_by_latency` " SYS_IO_GLOBAL_BY_WAIT_BY_LATENCY_VIEW_COLUMNS
    " AS " SYS_IO_GLOBAL_BY_WAIT_BY_LATENCY_VIEW_DEFINITION;

static const char sys_io_global_by_wait_by_latency_show_create_qualified_view_sql[] =
    "CREATE ALGORITHM=MERGE DEFINER=`mysql.sys`@`localhost` SQL SECURITY INVOKER VIEW "
    "`sys`.`io_global_by_wait_by_latency` " SYS_IO_GLOBAL_BY_WAIT_BY_LATENCY_VIEW_COLUMNS
    " AS " SYS_IO_GLOBAL_BY_WAIT_BY_LATENCY_VIEW_DEFINITION;

static const char sys_x_io_global_by_wait_by_latency_view_definition[] =
    SYS_X_IO_GLOBAL_BY_WAIT_BY_LATENCY_VIEW_DEFINITION;

static const char sys_x_io_global_by_wait_by_latency_show_create_view_sql[] =
    "CREATE ALGORITHM=MERGE DEFINER=`mysql.sys`@`localhost` SQL SECURITY INVOKER VIEW "
    "`x$io_global_by_wait_by_latency` " SYS_IO_GLOBAL_BY_WAIT_BY_LATENCY_VIEW_COLUMNS
    " AS " SYS_X_IO_GLOBAL_BY_WAIT_BY_LATENCY_VIEW_DEFINITION;

static const char sys_x_io_global_by_wait_by_latency_show_create_qualified_view_sql[] =
    "CREATE ALGORITHM=MERGE DEFINER=`mysql.sys`@`localhost` SQL SECURITY INVOKER VIEW "
    "`sys`.`x$io_global_by_wait_by_latency` " SYS_IO_GLOBAL_BY_WAIT_BY_LATENCY_VIEW_COLUMNS
    " AS " SYS_X_IO_GLOBAL_BY_WAIT_BY_LATENCY_VIEW_DEFINITION;

#undef SYS_IO_GLOBAL_BY_WAIT_BY_LATENCY_VIEW_COLUMNS
#undef SYS_IO_GLOBAL_BY_WAIT_BY_LATENCY_VIEW_DEFINITION
#undef SYS_X_IO_GLOBAL_BY_WAIT_BY_LATENCY_VIEW_DEFINITION

#define SYS_LATEST_FILE_IO_VIEW_COLUMNS "(`thread`,`file`,`latency`,`operation`,`requested`)"

#define SYS_LATEST_FILE_IO_SELECT_PREFIX                                                           \
    "select if((`processlist`.`ID` is null),concat(substring_index("                               \
    "`performance_schema`.`threads`.`NAME`,'/',-(1)),':',"                                         \
    "`performance_schema`.`events_waits_history_long`.`THREAD_ID`),convert(concat("                \
    "`processlist`.`USER`,'@',`processlist`.`HOST`,':',`processlist`.`ID`) using utf8mb4)) AS "    \
    "`thread`,"

#define SYS_LATEST_FILE_IO_SELECT_SUFFIX                                                           \
    " AS `operation`,format_bytes(`performance_schema`.`events_waits_history_long`."               \
    "`NUMBER_OF_BYTES`) AS `requested` from ((`performance_schema`.`events_waits_history_long` "   \
    "join "                                                                                        \
    "`performance_schema`.`threads` on((`performance_schema`.`events_waits_history_long`."         \
    "`THREAD_ID` = `performance_schema`.`threads`.`THREAD_ID`))) left join "                       \
    "`information_schema`.`PROCESSLIST` `processlist` on((`performance_schema`.`threads`."         \
    "`PROCESSLIST_ID` = `processlist`.`ID`))) where (("                                            \
    "`performance_schema`.`events_waits_history_long`.`OBJECT_NAME` is not null) and ("            \
    "`performance_schema`.`events_waits_history_long`.`EVENT_NAME` like 'wait/io/file/%')) order " \
    "by "                                                                                          \
    "`performance_schema`.`events_waits_history_long`.`TIMER_START`"

#define SYS_X_LATEST_FILE_IO_SELECT_SUFFIX                                                         \
    " AS `operation`,`performance_schema`.`events_waits_history_long`.`NUMBER_OF_BYTES` AS "       \
    "`requested` from ((`performance_schema`.`events_waits_history_long` join "                    \
    "`performance_schema`.`threads` on((`performance_schema`.`events_waits_history_long`."         \
    "`THREAD_ID` = `performance_schema`.`threads`.`THREAD_ID`))) left join "                       \
    "`information_schema`.`PROCESSLIST` `processlist` on((`performance_schema`.`threads`."         \
    "`PROCESSLIST_ID` = `processlist`.`ID`))) where (("                                            \
    "`performance_schema`.`events_waits_history_long`.`OBJECT_NAME` is not null) and ("            \
    "`performance_schema`.`events_waits_history_long`.`EVENT_NAME` like 'wait/io/file/%')) order " \
    "by "                                                                                          \
    "`performance_schema`.`events_waits_history_long`.`TIMER_START`"

#define SYS_LATEST_FILE_IO_VIEW_DEFINITION                                                         \
    SYS_LATEST_FILE_IO_SELECT_PREFIX                                                               \
    "`sys`.`format_path`(`performance_schema`.`events_waits_history_long`.`OBJECT_NAME`) AS "      \
    "`file`,format_pico_time(`performance_schema`.`events_waits_history_long`.`TIMER_WAIT`) AS "   \
    "`latency`,`performance_schema`.`events_waits_history_long`.`"                                 \
    "OPERATION`" SYS_LATEST_FILE_IO_SELECT_SUFFIX

#define SYS_X_LATEST_FILE_IO_VIEW_DEFINITION                                                       \
    SYS_LATEST_FILE_IO_SELECT_PREFIX                                                               \
    "`performance_schema`.`events_waits_history_long`.`OBJECT_NAME` AS `file`,"                    \
    "`performance_schema`.`events_waits_history_long`.`TIMER_WAIT` AS `latency`,"                  \
    "`performance_schema`.`events_waits_history_long`.`"                                           \
    "OPERATION`" SYS_X_LATEST_FILE_IO_SELECT_SUFFIX

static const char sys_latest_file_io_view_definition[] = SYS_LATEST_FILE_IO_VIEW_DEFINITION;

static const char sys_latest_file_io_show_create_view_sql[] =
    "CREATE ALGORITHM=MERGE DEFINER=`mysql.sys`@`localhost` SQL SECURITY INVOKER VIEW "
    "`latest_file_io` " SYS_LATEST_FILE_IO_VIEW_COLUMNS " AS " SYS_LATEST_FILE_IO_VIEW_DEFINITION;

static const char sys_latest_file_io_show_create_qualified_view_sql[] =
    "CREATE ALGORITHM=MERGE DEFINER=`mysql.sys`@`localhost` SQL SECURITY INVOKER VIEW "
    "`sys`.`latest_file_io` " SYS_LATEST_FILE_IO_VIEW_COLUMNS
    " AS " SYS_LATEST_FILE_IO_VIEW_DEFINITION;

static const char sys_x_latest_file_io_view_definition[] = SYS_X_LATEST_FILE_IO_VIEW_DEFINITION;

static const char sys_x_latest_file_io_show_create_view_sql[] =
    "CREATE ALGORITHM=MERGE DEFINER=`mysql.sys`@`localhost` SQL SECURITY INVOKER VIEW "
    "`x$latest_file_io` " SYS_LATEST_FILE_IO_VIEW_COLUMNS
    " AS " SYS_X_LATEST_FILE_IO_VIEW_DEFINITION;

static const char sys_x_latest_file_io_show_create_qualified_view_sql[] =
    "CREATE ALGORITHM=MERGE DEFINER=`mysql.sys`@`localhost` SQL SECURITY INVOKER VIEW "
    "`sys`.`x$latest_file_io` " SYS_LATEST_FILE_IO_VIEW_COLUMNS
    " AS " SYS_X_LATEST_FILE_IO_VIEW_DEFINITION;

#undef SYS_LATEST_FILE_IO_VIEW_COLUMNS
#undef SYS_LATEST_FILE_IO_SELECT_PREFIX
#undef SYS_LATEST_FILE_IO_SELECT_SUFFIX
#undef SYS_X_LATEST_FILE_IO_SELECT_SUFFIX
#undef SYS_LATEST_FILE_IO_VIEW_DEFINITION
#undef SYS_X_LATEST_FILE_IO_VIEW_DEFINITION

#define SYS_PS_CHECK_LOST_INSTRUMENTATION_VIEW_COLUMNS "(`variable_name`,`variable_value`)"

#define SYS_PS_CHECK_LOST_INSTRUMENTATION_VIEW_DEFINITION                                          \
    "select `performance_schema`.`global_status`.`VARIABLE_NAME` AS `variable_name`,"              \
    "`performance_schema`.`global_status`.`VARIABLE_VALUE` AS `variable_value` from "              \
    "`performance_schema`.`global_status` where (("                                                \
    "`performance_schema`.`global_status`.`VARIABLE_NAME` like 'perf%lost') and "                  \
    "(`performance_schema`.`global_status`.`VARIABLE_VALUE` > 0))"

static const char sys_ps_check_lost_instrumentation_view_definition[] =
    SYS_PS_CHECK_LOST_INSTRUMENTATION_VIEW_DEFINITION;

static const char sys_ps_check_lost_instrumentation_show_create_view_sql[] =
    "CREATE ALGORITHM=MERGE DEFINER=`mysql.sys`@`localhost` SQL SECURITY INVOKER VIEW "
    "`ps_check_lost_instrumentation` " SYS_PS_CHECK_LOST_INSTRUMENTATION_VIEW_COLUMNS
    " AS " SYS_PS_CHECK_LOST_INSTRUMENTATION_VIEW_DEFINITION;

static const char sys_ps_check_lost_instrumentation_show_create_qualified_view_sql[] =
    "CREATE ALGORITHM=MERGE DEFINER=`mysql.sys`@`localhost` SQL SECURITY INVOKER VIEW "
    "`sys`.`ps_check_lost_instrumentation` " SYS_PS_CHECK_LOST_INSTRUMENTATION_VIEW_COLUMNS
    " AS " SYS_PS_CHECK_LOST_INSTRUMENTATION_VIEW_DEFINITION;

#undef SYS_PS_CHECK_LOST_INSTRUMENTATION_VIEW_COLUMNS
#undef SYS_PS_CHECK_LOST_INSTRUMENTATION_VIEW_DEFINITION

#define SYS_SCHEMA_AUTO_INCREMENT_COLUMNS_VIEW_COLUMNS                                             \
    "(`table_schema`,`table_name`,`column_name`,`data_type`,`column_type`,`is_signed`,"            \
    "`is_unsigned`,`max_value`,`auto_increment`,`auto_increment_ratio`)"

#define SYS_SCHEMA_AUTO_INCREMENT_COLUMNS_VIEW_DEFINITION                                          \
    "select `information_schema`.`COLUMNS`.`TABLE_SCHEMA` AS `TABLE_SCHEMA`,"                      \
    "`information_schema`.`COLUMNS`.`TABLE_NAME` AS `TABLE_NAME`,"                                 \
    "`information_schema`.`COLUMNS`.`COLUMN_NAME` AS `COLUMN_NAME`,"                               \
    "`information_schema`.`COLUMNS`.`DATA_TYPE` AS `DATA_TYPE`,"                                   \
    "`information_schema`.`COLUMNS`.`COLUMN_TYPE` AS `COLUMN_TYPE`,"                               \
    "(locate('unsigned',`information_schema`.`COLUMNS`.`COLUMN_TYPE`) = 0) AS `is_signed`,"        \
    "(locate('unsigned',`information_schema`.`COLUMNS`.`COLUMN_TYPE`) > 0) AS `is_unsigned`,"      \
    "((case `information_schema`.`COLUMNS`.`DATA_TYPE` when 'tinyint' then 255 when "              \
    "'smallint' then 65535 when 'mediumint' then 16777215 when 'int' then 4294967295 when "        \
    "'bigint' then 18446744073709551615 end) >> if((locate('unsigned',"                            \
    "`information_schema`.`COLUMNS`.`COLUMN_TYPE`) > 0),0,1)) AS `max_value`,"                     \
    "`information_schema`.`TABLES`.`AUTO_INCREMENT` AS `AUTO_INCREMENT`,"                          \
    "(`information_schema`.`TABLES`.`AUTO_INCREMENT` / ((case "                                    \
    "`information_schema`.`COLUMNS`.`DATA_TYPE` when 'tinyint' then 255 when 'smallint' then "     \
    "65535 when 'mediumint' then 16777215 when 'int' then 4294967295 when 'bigint' then "          \
    "18446744073709551615 end) >> if((locate('unsigned',"                                          \
    "`information_schema`.`COLUMNS`.`COLUMN_TYPE`) > 0),0,1))) AS `auto_increment_ratio` "         \
    "from (`information_schema`.`COLUMNS` join `information_schema`.`TABLES` on((("                \
    "`information_schema`.`COLUMNS`.`TABLE_SCHEMA` = "                                             \
    "`information_schema`.`TABLES`.`TABLE_SCHEMA`) and "                                           \
    "(`information_schema`.`COLUMNS`.`TABLE_NAME` = "                                              \
    "`information_schema`.`TABLES`.`TABLE_NAME`)))) where (("                                      \
    "`information_schema`.`COLUMNS`.`TABLE_SCHEMA` not in ('mysql','sys','INFORMATION_SCHEMA',"    \
    "'performance_schema')) and (`information_schema`.`TABLES`.`TABLE_TYPE` = 'BASE TABLE') "      \
    "and (`information_schema`.`COLUMNS`.`EXTRA` = 'auto_increment')) order by "                   \
    "(`information_schema`.`TABLES`.`AUTO_INCREMENT` / ((case "                                    \
    "`information_schema`.`COLUMNS`.`DATA_TYPE` when 'tinyint' then 255 when 'smallint' then "     \
    "65535 when 'mediumint' then 16777215 when 'int' then 4294967295 when 'bigint' then "          \
    "18446744073709551615 end) >> if((locate('unsigned',"                                          \
    "`information_schema`.`COLUMNS`.`COLUMN_TYPE`) > 0),0,1))) desc,((case "                       \
    "`information_schema`.`COLUMNS`.`DATA_TYPE` when 'tinyint' then 255 when 'smallint' then "     \
    "65535 when 'mediumint' then 16777215 when 'int' then 4294967295 when 'bigint' then "          \
    "18446744073709551615 end) >> if((locate('unsigned',"                                          \
    "`information_schema`.`COLUMNS`.`COLUMN_TYPE`) > 0),0,1))"

static const char sys_schema_auto_increment_columns_view_definition[] =
    SYS_SCHEMA_AUTO_INCREMENT_COLUMNS_VIEW_DEFINITION;

static const char sys_schema_auto_increment_columns_show_create_view_sql[] =
    "CREATE ALGORITHM=MERGE DEFINER=`mysql.sys`@`localhost` SQL SECURITY INVOKER VIEW "
    "`schema_auto_increment_columns` " SYS_SCHEMA_AUTO_INCREMENT_COLUMNS_VIEW_COLUMNS
    " AS " SYS_SCHEMA_AUTO_INCREMENT_COLUMNS_VIEW_DEFINITION;

static const char sys_schema_auto_increment_columns_show_create_qualified_view_sql[] =
    "CREATE ALGORITHM=MERGE DEFINER=`mysql.sys`@`localhost` SQL SECURITY INVOKER VIEW "
    "`sys`.`schema_auto_increment_columns` " SYS_SCHEMA_AUTO_INCREMENT_COLUMNS_VIEW_COLUMNS
    " AS " SYS_SCHEMA_AUTO_INCREMENT_COLUMNS_VIEW_DEFINITION;

#undef SYS_SCHEMA_AUTO_INCREMENT_COLUMNS_VIEW_COLUMNS
#undef SYS_SCHEMA_AUTO_INCREMENT_COLUMNS_VIEW_DEFINITION

#define SYS_SCHEMA_INDEX_STATISTICS_VIEW_COLUMNS                                                   \
    "(`table_schema`,`table_name`,`index_name`,`rows_selected`,`select_latency`,"                  \
    "`rows_inserted`,`insert_latency`,`rows_updated`,`update_latency`,`rows_deleted`,"             \
    "`delete_latency`)"

#define SYS_SCHEMA_INDEX_STATISTICS_VIEW_DEFINITION                                                \
    "select `performance_schema`.`table_io_waits_summary_by_index_usage`.`OBJECT_SCHEMA` AS "      \
    "`table_schema`,`performance_schema`.`table_io_waits_summary_by_index_usage`.`OBJECT_NAME` "   \
    "AS `table_name`,`performance_schema`.`table_io_waits_summary_by_index_usage`.`INDEX_NAME` "   \
    "AS `index_name`,`performance_schema`.`table_io_waits_summary_by_index_usage`.`COUNT_FETCH` "  \
    "AS `rows_selected`,format_pico_time("                                                         \
    "`performance_schema`.`table_io_waits_summary_by_index_usage`.`SUM_TIMER_FETCH`) AS "          \
    "`select_latency`,"                                                                            \
    "`performance_schema`.`table_io_waits_summary_by_index_usage`.`COUNT_INSERT` AS "              \
    "`rows_inserted`,format_pico_time("                                                            \
    "`performance_schema`.`table_io_waits_summary_by_index_usage`.`SUM_TIMER_INSERT`) AS "         \
    "`insert_latency`,"                                                                            \
    "`performance_schema`.`table_io_waits_summary_by_index_usage`.`COUNT_UPDATE` AS "              \
    "`rows_updated`,format_pico_time("                                                             \
    "`performance_schema`.`table_io_waits_summary_by_index_usage`.`SUM_TIMER_UPDATE`) AS "         \
    "`update_latency`,"                                                                            \
    "`performance_schema`.`table_io_waits_summary_by_index_usage`.`COUNT_DELETE` AS "              \
    "`rows_deleted`,format_pico_time("                                                             \
    "`performance_schema`.`table_io_waits_summary_by_index_usage`.`SUM_TIMER_DELETE`) AS "         \
    "`delete_latency` from `performance_schema`.`table_io_waits_summary_by_index_usage` where "    \
    "(`performance_schema`.`table_io_waits_summary_by_index_usage`.`INDEX_NAME` is not null) "     \
    "order by `performance_schema`.`table_io_waits_summary_by_index_usage`.`SUM_TIMER_WAIT` desc"

#define SYS_X_SCHEMA_INDEX_STATISTICS_VIEW_DEFINITION                                              \
    "select `performance_schema`.`table_io_waits_summary_by_index_usage`.`OBJECT_SCHEMA` AS "      \
    "`table_schema`,`performance_schema`.`table_io_waits_summary_by_index_usage`.`OBJECT_NAME` "   \
    "AS `table_name`,`performance_schema`.`table_io_waits_summary_by_index_usage`.`INDEX_NAME` "   \
    "AS `index_name`,`performance_schema`.`table_io_waits_summary_by_index_usage`.`COUNT_FETCH` "  \
    "AS `rows_selected`,"                                                                          \
    "`performance_schema`.`table_io_waits_summary_by_index_usage`.`SUM_TIMER_FETCH` AS "           \
    "`select_latency`,"                                                                            \
    "`performance_schema`.`table_io_waits_summary_by_index_usage`.`COUNT_INSERT` AS "              \
    "`rows_inserted`,"                                                                             \
    "`performance_schema`.`table_io_waits_summary_by_index_usage`.`SUM_TIMER_INSERT` AS "          \
    "`insert_latency`,"                                                                            \
    "`performance_schema`.`table_io_waits_summary_by_index_usage`.`COUNT_UPDATE` AS "              \
    "`rows_updated`,"                                                                              \
    "`performance_schema`.`table_io_waits_summary_by_index_usage`.`SUM_TIMER_UPDATE` AS "          \
    "`update_latency`,"                                                                            \
    "`performance_schema`.`table_io_waits_summary_by_index_usage`.`COUNT_DELETE` AS "              \
    "`rows_deleted`,"                                                                              \
    "`performance_schema`.`table_io_waits_summary_by_index_usage`.`SUM_TIMER_DELETE` AS "          \
    "`delete_latency` from `performance_schema`.`table_io_waits_summary_by_index_usage` where "    \
    "(`performance_schema`.`table_io_waits_summary_by_index_usage`.`INDEX_NAME` is not null) "     \
    "order by `performance_schema`.`table_io_waits_summary_by_index_usage`.`SUM_TIMER_WAIT` desc"

static const char sys_schema_index_statistics_view_definition[] =
    SYS_SCHEMA_INDEX_STATISTICS_VIEW_DEFINITION;

static const char sys_schema_index_statistics_show_create_view_sql[] =
    "CREATE ALGORITHM=MERGE DEFINER=`mysql.sys`@`localhost` SQL SECURITY INVOKER VIEW "
    "`schema_index_statistics` " SYS_SCHEMA_INDEX_STATISTICS_VIEW_COLUMNS
    " AS " SYS_SCHEMA_INDEX_STATISTICS_VIEW_DEFINITION;

static const char sys_schema_index_statistics_show_create_qualified_view_sql[] =
    "CREATE ALGORITHM=MERGE DEFINER=`mysql.sys`@`localhost` SQL SECURITY INVOKER VIEW "
    "`sys`.`schema_index_statistics` " SYS_SCHEMA_INDEX_STATISTICS_VIEW_COLUMNS
    " AS " SYS_SCHEMA_INDEX_STATISTICS_VIEW_DEFINITION;

static const char sys_x_schema_index_statistics_view_definition[] =
    SYS_X_SCHEMA_INDEX_STATISTICS_VIEW_DEFINITION;

static const char sys_x_schema_index_statistics_show_create_view_sql[] =
    "CREATE ALGORITHM=MERGE DEFINER=`mysql.sys`@`localhost` SQL SECURITY INVOKER VIEW "
    "`x$schema_index_statistics` " SYS_SCHEMA_INDEX_STATISTICS_VIEW_COLUMNS
    " AS " SYS_X_SCHEMA_INDEX_STATISTICS_VIEW_DEFINITION;

static const char sys_x_schema_index_statistics_show_create_qualified_view_sql[] =
    "CREATE ALGORITHM=MERGE DEFINER=`mysql.sys`@`localhost` SQL SECURITY INVOKER VIEW "
    "`sys`.`x$schema_index_statistics` " SYS_SCHEMA_INDEX_STATISTICS_VIEW_COLUMNS
    " AS " SYS_X_SCHEMA_INDEX_STATISTICS_VIEW_DEFINITION;

#undef SYS_SCHEMA_INDEX_STATISTICS_VIEW_COLUMNS
#undef SYS_SCHEMA_INDEX_STATISTICS_VIEW_DEFINITION
#undef SYS_X_SCHEMA_INDEX_STATISTICS_VIEW_DEFINITION

#define SYS_SCHEMA_REDUNDANT_INDEXES_VIEW_COLUMNS                                                  \
    "(`table_schema`,`table_name`,`redundant_index_name`,`redundant_index_columns`,"               \
    "`redundant_index_non_unique`,`dominant_index_name`,`dominant_index_columns`,"                 \
    "`dominant_index_non_unique`,`subpart_exists`,`sql_drop_index`)"

#define SYS_SCHEMA_REDUNDANT_INDEXES_VIEW_DEFINITION                                               \
    "select `sys`.`redundant_keys`.`table_schema` AS `table_schema`,"                              \
    "`sys`.`redundant_keys`.`table_name` AS `table_name`,"                                         \
    "`sys`.`redundant_keys`.`index_name` AS `redundant_index_name`,"                               \
    "`sys`.`redundant_keys`.`index_columns` AS `redundant_index_columns`,"                         \
    "`sys`.`redundant_keys`.`non_unique` AS `redundant_index_non_unique`,"                         \
    "`sys`.`dominant_keys`.`index_name` AS `dominant_index_name`,"                                 \
    "`sys`.`dominant_keys`.`index_columns` AS `dominant_index_columns`,"                           \
    "`sys`.`dominant_keys`.`non_unique` AS `dominant_index_non_unique`,"                           \
    "if(((0 <> `sys`.`redundant_keys`.`subpart_exists`) or (0 <> "                                 \
    "`sys`.`dominant_keys`.`subpart_exists`)),1,0) AS `subpart_exists`,"                           \
    "concat('ALTER TABLE `',`sys`.`redundant_keys`.`table_schema`,'`.`',"                          \
    "`sys`.`redundant_keys`.`table_name`,'` DROP INDEX `',"                                        \
    "`sys`.`redundant_keys`.`index_name`,'`') AS `sql_drop_index` from "                           \
    "(`sys`.`x$schema_flattened_keys` `redundant_keys` join "                                      \
    "`sys`.`x$schema_flattened_keys` `dominant_keys` on((("                                        \
    "`sys`.`redundant_keys`.`table_schema` = `sys`.`dominant_keys`.`table_schema`) and ("          \
    "`sys`.`redundant_keys`.`table_name` = `sys`.`dominant_keys`.`table_name`)))) where (("        \
    "`sys`.`redundant_keys`.`index_name` <> `sys`.`dominant_keys`.`index_name`) and ((("           \
    "`sys`.`redundant_keys`.`index_columns` = `sys`.`dominant_keys`.`index_columns`) and (("       \
    "`sys`.`redundant_keys`.`non_unique` > `sys`.`dominant_keys`.`non_unique`) or (("              \
    "`sys`.`redundant_keys`.`non_unique` = `sys`.`dominant_keys`.`non_unique`) and (if(("          \
    "`sys`.`redundant_keys`.`index_name` = 'PRIMARY'),'',`sys`.`redundant_keys`.`index_name`) "    \
    "> if((`sys`.`dominant_keys`.`index_name` = 'PRIMARY'),'',"                                    \
    "`sys`.`dominant_keys`.`index_name`))))) or ((locate(concat("                                  \
    "`sys`.`redundant_keys`.`index_columns`,','),`sys`.`dominant_keys`.`index_columns`) = 1) "     \
    "and (`sys`.`redundant_keys`.`non_unique` = 1)) or ((locate(concat("                           \
    "`sys`.`dominant_keys`.`index_columns`,','),`sys`.`redundant_keys`.`index_columns`) = 1) "     \
    "and (`sys`.`dominant_keys`.`non_unique` = 0))))"

#define SYS_X_SCHEMA_FLATTENED_KEYS_VIEW_COLUMNS                                                   \
    "(`table_schema`,`table_name`,`index_name`,`non_unique`,`subpart_exists`,`index_columns`)"

#define SYS_X_SCHEMA_FLATTENED_KEYS_VIEW_DEFINITION                                                \
    "select `information_schema`.`STATISTICS`.`TABLE_SCHEMA` AS `TABLE_SCHEMA`,"                   \
    "`information_schema`.`STATISTICS`.`TABLE_NAME` AS `TABLE_NAME`,"                              \
    "`information_schema`.`STATISTICS`.`INDEX_NAME` AS `INDEX_NAME`,"                              \
    "max(`information_schema`.`STATISTICS`.`NON_UNIQUE`) AS `non_unique`,max(if(("                 \
    "`information_schema`.`STATISTICS`.`SUB_PART` is null),0,1)) AS `subpart_exists`,"             \
    "group_concat(`information_schema`.`STATISTICS`.`COLUMN_NAME` order by "                       \
    "`information_schema`.`STATISTICS`.`SEQ_IN_INDEX` ASC separator ',') AS `index_columns` "      \
    "from `information_schema`.`STATISTICS` where (("                                              \
    "`information_schema`.`STATISTICS`.`INDEX_TYPE` = 'BTREE') and ("                              \
    "`information_schema`.`STATISTICS`.`TABLE_SCHEMA` not in ('mysql','sys',"                      \
    "'INFORMATION_SCHEMA','PERFORMANCE_SCHEMA'))) group by "                                       \
    "`information_schema`.`STATISTICS`.`TABLE_SCHEMA`,"                                            \
    "`information_schema`.`STATISTICS`.`TABLE_NAME`,"                                              \
    "`information_schema`.`STATISTICS`.`INDEX_NAME`"

static const char sys_schema_redundant_indexes_view_definition[] =
    SYS_SCHEMA_REDUNDANT_INDEXES_VIEW_DEFINITION;

static const char sys_schema_redundant_indexes_show_create_view_sql[] =
    "CREATE ALGORITHM=TEMPTABLE DEFINER=`mysql.sys`@`localhost` SQL SECURITY INVOKER VIEW "
    "`schema_redundant_indexes` " SYS_SCHEMA_REDUNDANT_INDEXES_VIEW_COLUMNS
    " AS " SYS_SCHEMA_REDUNDANT_INDEXES_VIEW_DEFINITION;

static const char sys_schema_redundant_indexes_show_create_qualified_view_sql[] =
    "CREATE ALGORITHM=TEMPTABLE DEFINER=`mysql.sys`@`localhost` SQL SECURITY INVOKER VIEW "
    "`sys`.`schema_redundant_indexes` " SYS_SCHEMA_REDUNDANT_INDEXES_VIEW_COLUMNS
    " AS " SYS_SCHEMA_REDUNDANT_INDEXES_VIEW_DEFINITION;

static const char sys_x_schema_flattened_keys_view_definition[] =
    SYS_X_SCHEMA_FLATTENED_KEYS_VIEW_DEFINITION;

static const char sys_x_schema_flattened_keys_show_create_view_sql[] =
    "CREATE ALGORITHM=TEMPTABLE DEFINER=`mysql.sys`@`localhost` SQL SECURITY INVOKER VIEW "
    "`x$schema_flattened_keys` " SYS_X_SCHEMA_FLATTENED_KEYS_VIEW_COLUMNS
    " AS " SYS_X_SCHEMA_FLATTENED_KEYS_VIEW_DEFINITION;

static const char sys_x_schema_flattened_keys_show_create_qualified_view_sql[] =
    "CREATE ALGORITHM=TEMPTABLE DEFINER=`mysql.sys`@`localhost` SQL SECURITY INVOKER VIEW "
    "`sys`.`x$schema_flattened_keys` " SYS_X_SCHEMA_FLATTENED_KEYS_VIEW_COLUMNS
    " AS " SYS_X_SCHEMA_FLATTENED_KEYS_VIEW_DEFINITION;

#undef SYS_SCHEMA_REDUNDANT_INDEXES_VIEW_COLUMNS
#undef SYS_SCHEMA_REDUNDANT_INDEXES_VIEW_DEFINITION
#undef SYS_X_SCHEMA_FLATTENED_KEYS_VIEW_COLUMNS
#undef SYS_X_SCHEMA_FLATTENED_KEYS_VIEW_DEFINITION

#define SYS_SCHEMA_TABLE_LOCK_WAITS_VIEW_COLUMNS                                                   \
    "(`object_schema`,`object_name`,`waiting_thread_id`,`waiting_pid`,`waiting_account`,"          \
    "`waiting_lock_type`,`waiting_lock_duration`,`waiting_query`,`waiting_query_secs`,"            \
    "`waiting_query_rows_affected`,`waiting_query_rows_examined`,`blocking_thread_id`,"            \
    "`blocking_pid`,`blocking_account`,`blocking_lock_type`,`blocking_lock_duration`,"             \
    "`sql_kill_blocking_query`,`sql_kill_blocking_connection`)"

#define SYS_SCHEMA_TABLE_LOCK_WAITS_SELECT_PREFIX                                                  \
    "select `g`.`OBJECT_SCHEMA` AS `object_schema`,`g`.`OBJECT_NAME` AS `object_name`,"            \
    "`pt`.`THREAD_ID` AS `waiting_thread_id`,`pt`.`PROCESSLIST_ID` AS `waiting_pid`,"              \
    "`sys`.`ps_thread_account`(`p`.`OWNER_THREAD_ID`) AS `waiting_account`,"                       \
    "`p`.`LOCK_TYPE` AS `waiting_lock_type`,`p`.`LOCK_DURATION` AS "                               \
    "`waiting_lock_duration`,"

#define SYS_SCHEMA_TABLE_LOCK_WAITS_SELECT_SUFFIX                                                  \
    " AS `waiting_query`,`pt`.`PROCESSLIST_TIME` AS `waiting_query_secs`,"                         \
    "`ps`.`ROWS_AFFECTED` AS `waiting_query_rows_affected`,`ps`.`ROWS_EXAMINED` AS "               \
    "`waiting_query_rows_examined`,`gt`.`THREAD_ID` AS `blocking_thread_id`,"                      \
    "`gt`.`PROCESSLIST_ID` AS `blocking_pid`,`sys`.`ps_thread_account`("                           \
    "`g`.`OWNER_THREAD_ID`) AS `blocking_account`,`g`.`LOCK_TYPE` AS `blocking_lock_type`,"        \
    "`g`.`LOCK_DURATION` AS `blocking_lock_duration`,concat('KILL QUERY ',"                        \
    "`gt`.`PROCESSLIST_ID`) AS `sql_kill_blocking_query`,concat('KILL ',"                          \
    "`gt`.`PROCESSLIST_ID`) AS `sql_kill_blocking_connection` from "                               \
    "(((((`performance_schema`.`metadata_locks` `g` join "                                         \
    "`performance_schema`.`metadata_locks` `p` on(((`g`.`OBJECT_TYPE` = "                          \
    "`p`.`OBJECT_TYPE`) and (`g`.`OBJECT_SCHEMA` = `p`.`OBJECT_SCHEMA`) and ("                     \
    "`g`.`OBJECT_NAME` = `p`.`OBJECT_NAME`) and (`g`.`LOCK_STATUS` = 'GRANTED') and ("             \
    "`p`.`LOCK_STATUS` = 'PENDING')))) join `performance_schema`.`threads` `gt` on(("              \
    "`g`.`OWNER_THREAD_ID` = `gt`.`THREAD_ID`))) join `performance_schema`.`threads` `pt` "        \
    "on((`p`.`OWNER_THREAD_ID` = `pt`.`THREAD_ID`))) left join "                                   \
    "`performance_schema`.`events_statements_current` `gs` on((`g`.`OWNER_THREAD_ID` = "           \
    "`gs`.`THREAD_ID`))) left join `performance_schema`.`events_statements_current` `ps` on(("     \
    "`p`.`OWNER_THREAD_ID` = `ps`.`THREAD_ID`))) where (`g`.`OBJECT_TYPE` = 'TABLE')"

#define SYS_SCHEMA_TABLE_LOCK_WAITS_VIEW_DEFINITION                                                \
    SYS_SCHEMA_TABLE_LOCK_WAITS_SELECT_PREFIX                                                      \
    "`sys`.`format_statement`(`pt`.`PROCESSLIST_INFO`)" SYS_SCHEMA_TABLE_LOCK_WAITS_SELECT_SUFFIX

#define SYS_X_SCHEMA_TABLE_LOCK_WAITS_VIEW_DEFINITION                                              \
    SYS_SCHEMA_TABLE_LOCK_WAITS_SELECT_PREFIX                                                      \
    "`pt`.`PROCESSLIST_INFO`" SYS_SCHEMA_TABLE_LOCK_WAITS_SELECT_SUFFIX

static const char sys_schema_table_lock_waits_view_definition[] =
    SYS_SCHEMA_TABLE_LOCK_WAITS_VIEW_DEFINITION;

static const char sys_schema_table_lock_waits_show_create_view_sql[] =
    "CREATE ALGORITHM=TEMPTABLE DEFINER=`mysql.sys`@`localhost` SQL SECURITY INVOKER VIEW "
    "`schema_table_lock_waits` " SYS_SCHEMA_TABLE_LOCK_WAITS_VIEW_COLUMNS
    " AS " SYS_SCHEMA_TABLE_LOCK_WAITS_VIEW_DEFINITION;

static const char sys_schema_table_lock_waits_show_create_qualified_view_sql[] =
    "CREATE ALGORITHM=TEMPTABLE DEFINER=`mysql.sys`@`localhost` SQL SECURITY INVOKER VIEW "
    "`sys`.`schema_table_lock_waits` " SYS_SCHEMA_TABLE_LOCK_WAITS_VIEW_COLUMNS
    " AS " SYS_SCHEMA_TABLE_LOCK_WAITS_VIEW_DEFINITION;

static const char sys_x_schema_table_lock_waits_view_definition[] =
    SYS_X_SCHEMA_TABLE_LOCK_WAITS_VIEW_DEFINITION;

static const char sys_x_schema_table_lock_waits_show_create_view_sql[] =
    "CREATE ALGORITHM=TEMPTABLE DEFINER=`mysql.sys`@`localhost` SQL SECURITY INVOKER VIEW "
    "`x$schema_table_lock_waits` " SYS_SCHEMA_TABLE_LOCK_WAITS_VIEW_COLUMNS
    " AS " SYS_X_SCHEMA_TABLE_LOCK_WAITS_VIEW_DEFINITION;

static const char sys_x_schema_table_lock_waits_show_create_qualified_view_sql[] =
    "CREATE ALGORITHM=TEMPTABLE DEFINER=`mysql.sys`@`localhost` SQL SECURITY INVOKER VIEW "
    "`sys`.`x$schema_table_lock_waits` " SYS_SCHEMA_TABLE_LOCK_WAITS_VIEW_COLUMNS
    " AS " SYS_X_SCHEMA_TABLE_LOCK_WAITS_VIEW_DEFINITION;

#undef SYS_SCHEMA_TABLE_LOCK_WAITS_VIEW_COLUMNS
#undef SYS_SCHEMA_TABLE_LOCK_WAITS_SELECT_PREFIX
#undef SYS_SCHEMA_TABLE_LOCK_WAITS_SELECT_SUFFIX
#undef SYS_SCHEMA_TABLE_LOCK_WAITS_VIEW_DEFINITION
#undef SYS_X_SCHEMA_TABLE_LOCK_WAITS_VIEW_DEFINITION

#define SYS_X_PS_SCHEMA_TABLE_STATISTICS_IO_VIEW_COLUMNS                                           \
    "(`table_schema`,`table_name`,`count_read`,`sum_number_of_bytes_read`,"                        \
    "`sum_timer_read`,`count_write`,`sum_number_of_bytes_write`,`sum_timer_write`,"                \
    "`count_misc`,`sum_timer_misc`)"

#define SYS_X_PS_SCHEMA_TABLE_STATISTICS_IO_VIEW_DEFINITION                                        \
    "select `extract_schema_from_file_name`(`performance_schema`.`file_summary_by_instance`."      \
    "`FILE_NAME`) AS `table_schema`,`extract_table_from_file_name`(`performance_schema`."          \
    "`file_summary_by_instance`.`FILE_NAME`) AS `table_name`,sum(`performance_schema`."            \
    "`file_summary_by_instance`.`COUNT_READ`) AS `count_read`,sum(`performance_schema`."           \
    "`file_summary_by_instance`.`SUM_NUMBER_OF_BYTES_READ`) AS "                                   \
    "`sum_number_of_bytes_read`,sum(`performance_schema`.`file_summary_by_instance`."              \
    "`SUM_TIMER_READ`) AS `sum_timer_read`,sum(`performance_schema`."                              \
    "`file_summary_by_instance`.`COUNT_WRITE`) AS `count_write`,sum(`performance_schema`."         \
    "`file_summary_by_instance`.`SUM_NUMBER_OF_BYTES_WRITE`) AS "                                  \
    "`sum_number_of_bytes_write`,sum(`performance_schema`.`file_summary_by_instance`."             \
    "`SUM_TIMER_WRITE`) AS `sum_timer_write`,sum(`performance_schema`."                            \
    "`file_summary_by_instance`.`COUNT_MISC`) AS `count_misc`,sum(`performance_schema`."           \
    "`file_summary_by_instance`.`SUM_TIMER_MISC`) AS `sum_timer_misc` from "                       \
    "`performance_schema`.`file_summary_by_instance` group by `table_schema`,`table_name`"

static const char sys_x_ps_schema_table_statistics_io_view_definition[] =
    SYS_X_PS_SCHEMA_TABLE_STATISTICS_IO_VIEW_DEFINITION;

static const char sys_x_ps_schema_table_statistics_io_show_create_view_sql[] =
    "CREATE ALGORITHM=TEMPTABLE DEFINER=`mysql.sys`@`localhost` SQL SECURITY INVOKER VIEW "
    "`x$ps_schema_table_statistics_io` " SYS_X_PS_SCHEMA_TABLE_STATISTICS_IO_VIEW_COLUMNS
    " AS " SYS_X_PS_SCHEMA_TABLE_STATISTICS_IO_VIEW_DEFINITION;

static const char sys_x_ps_schema_table_statistics_io_show_create_qualified_view_sql[] =
    "CREATE ALGORITHM=TEMPTABLE DEFINER=`mysql.sys`@`localhost` SQL SECURITY INVOKER VIEW "
    "`sys`.`x$ps_schema_table_statistics_io` " SYS_X_PS_SCHEMA_TABLE_STATISTICS_IO_VIEW_COLUMNS
    " AS " SYS_X_PS_SCHEMA_TABLE_STATISTICS_IO_VIEW_DEFINITION;

#undef SYS_X_PS_SCHEMA_TABLE_STATISTICS_IO_VIEW_COLUMNS
#undef SYS_X_PS_SCHEMA_TABLE_STATISTICS_IO_VIEW_DEFINITION

#define SYS_SCHEMA_TABLE_STATISTICS_VIEW_COLUMNS                                                   \
    "(`table_schema`,`table_name`,`total_latency`,`rows_fetched`,`fetch_latency`,"                 \
    "`rows_inserted`,`insert_latency`,`rows_updated`,`update_latency`,`rows_deleted`,"             \
    "`delete_latency`,`io_read_requests`,`io_read`,`io_read_latency`,`io_write_requests`,"         \
    "`io_write`,`io_write_latency`,`io_misc_requests`,`io_misc_latency`)"

#define SYS_SCHEMA_TABLE_STATISTICS_SELECT_SUFFIX                                                  \
    " from (`performance_schema`.`table_io_waits_summary_by_table` `pst` left join "               \
    "`sys`.`x$ps_schema_table_statistics_io` `fsbi` on(((`pst`.`OBJECT_SCHEMA` = "                 \
    "`sys`.`fsbi`.`table_schema`) and (`pst`.`OBJECT_NAME` = "                                     \
    "`sys`.`fsbi`.`table_name`)))) order by `pst`.`SUM_TIMER_WAIT` desc"

#define SYS_SCHEMA_TABLE_STATISTICS_VIEW_DEFINITION                                                \
    "select `pst`.`OBJECT_SCHEMA` AS `table_schema`,`pst`.`OBJECT_NAME` AS `table_name`,"          \
    "format_pico_time(`pst`.`SUM_TIMER_WAIT`) AS `total_latency`,`pst`.`COUNT_FETCH` AS "          \
    "`rows_fetched`,format_pico_time(`pst`.`SUM_TIMER_FETCH`) AS `fetch_latency`,"                 \
    "`pst`.`COUNT_INSERT` AS `rows_inserted`,format_pico_time(`pst`.`SUM_TIMER_INSERT`) AS "       \
    "`insert_latency`,`pst`.`COUNT_UPDATE` AS `rows_updated`,format_pico_time("                    \
    "`pst`.`SUM_TIMER_UPDATE`) AS `update_latency`,`pst`.`COUNT_DELETE` AS `rows_deleted`,"        \
    "format_pico_time(`pst`.`SUM_TIMER_DELETE`) AS `delete_latency`,"                              \
    "`sys`.`fsbi`.`count_read` AS `io_read_requests`,format_bytes("                                \
    "`sys`.`fsbi`.`sum_number_of_bytes_read`) AS `io_read`,format_pico_time("                      \
    "`sys`.`fsbi`.`sum_timer_read`) AS `io_read_latency`,`sys`.`fsbi`.`count_write` AS "           \
    "`io_write_requests`,format_bytes(`sys`.`fsbi`.`sum_number_of_bytes_write`) AS "               \
    "`io_write`,format_pico_time(`sys`.`fsbi`.`sum_timer_write`) AS `io_write_latency`,"           \
    "`sys`.`fsbi`.`count_misc` AS `io_misc_requests`,format_pico_time("                            \
    "`sys`.`fsbi`.`sum_timer_misc`) AS "                                                           \
    "`io_misc_latency`" SYS_SCHEMA_TABLE_STATISTICS_SELECT_SUFFIX

#define SYS_X_SCHEMA_TABLE_STATISTICS_VIEW_DEFINITION                                              \
    "select `pst`.`OBJECT_SCHEMA` AS `table_schema`,`pst`.`OBJECT_NAME` AS `table_name`,"          \
    "`pst`.`SUM_TIMER_WAIT` AS `total_latency`,`pst`.`COUNT_FETCH` AS `rows_fetched`,"             \
    "`pst`.`SUM_TIMER_FETCH` AS `fetch_latency`,`pst`.`COUNT_INSERT` AS `rows_inserted`,"          \
    "`pst`.`SUM_TIMER_INSERT` AS `insert_latency`,`pst`.`COUNT_UPDATE` AS `rows_updated`,"         \
    "`pst`.`SUM_TIMER_UPDATE` AS `update_latency`,`pst`.`COUNT_DELETE` AS `rows_deleted`,"         \
    "`pst`.`SUM_TIMER_DELETE` AS `delete_latency`,`sys`.`fsbi`.`count_read` AS "                   \
    "`io_read_requests`,`sys`.`fsbi`.`sum_number_of_bytes_read` AS `io_read`,"                     \
    "`sys`.`fsbi`.`sum_timer_read` AS `io_read_latency`,`sys`.`fsbi`.`count_write` AS "            \
    "`io_write_requests`,`sys`.`fsbi`.`sum_number_of_bytes_write` AS `io_write`,"                  \
    "`sys`.`fsbi`.`sum_timer_write` AS `io_write_latency`,`sys`.`fsbi`.`count_misc` AS "           \
    "`io_misc_requests`,`sys`.`fsbi`.`sum_timer_misc` AS "                                         \
    "`io_misc_latency`" SYS_SCHEMA_TABLE_STATISTICS_SELECT_SUFFIX

static const char sys_schema_table_statistics_view_definition[] =
    SYS_SCHEMA_TABLE_STATISTICS_VIEW_DEFINITION;

static const char sys_schema_table_statistics_show_create_view_sql[] =
    "CREATE ALGORITHM=TEMPTABLE DEFINER=`mysql.sys`@`localhost` SQL SECURITY INVOKER VIEW "
    "`schema_table_statistics` " SYS_SCHEMA_TABLE_STATISTICS_VIEW_COLUMNS
    " AS " SYS_SCHEMA_TABLE_STATISTICS_VIEW_DEFINITION;

static const char sys_schema_table_statistics_show_create_qualified_view_sql[] =
    "CREATE ALGORITHM=TEMPTABLE DEFINER=`mysql.sys`@`localhost` SQL SECURITY INVOKER VIEW "
    "`sys`.`schema_table_statistics` " SYS_SCHEMA_TABLE_STATISTICS_VIEW_COLUMNS
    " AS " SYS_SCHEMA_TABLE_STATISTICS_VIEW_DEFINITION;

static const char sys_x_schema_table_statistics_view_definition[] =
    SYS_X_SCHEMA_TABLE_STATISTICS_VIEW_DEFINITION;

static const char sys_x_schema_table_statistics_show_create_view_sql[] =
    "CREATE ALGORITHM=TEMPTABLE DEFINER=`mysql.sys`@`localhost` SQL SECURITY INVOKER VIEW "
    "`x$schema_table_statistics` " SYS_SCHEMA_TABLE_STATISTICS_VIEW_COLUMNS
    " AS " SYS_X_SCHEMA_TABLE_STATISTICS_VIEW_DEFINITION;

static const char sys_x_schema_table_statistics_show_create_qualified_view_sql[] =
    "CREATE ALGORITHM=TEMPTABLE DEFINER=`mysql.sys`@`localhost` SQL SECURITY INVOKER VIEW "
    "`sys`.`x$schema_table_statistics` " SYS_SCHEMA_TABLE_STATISTICS_VIEW_COLUMNS
    " AS " SYS_X_SCHEMA_TABLE_STATISTICS_VIEW_DEFINITION;

#undef SYS_SCHEMA_TABLE_STATISTICS_VIEW_COLUMNS
#undef SYS_SCHEMA_TABLE_STATISTICS_SELECT_SUFFIX
#undef SYS_SCHEMA_TABLE_STATISTICS_VIEW_DEFINITION
#undef SYS_X_SCHEMA_TABLE_STATISTICS_VIEW_DEFINITION

#define SYS_SCHEMA_TABLE_STATISTICS_WITH_BUFFER_VIEW_COLUMNS                                       \
    "(`table_schema`,`table_name`,`rows_fetched`,`fetch_latency`,`rows_inserted`,"                 \
    "`insert_latency`,`rows_updated`,`update_latency`,`rows_deleted`,`delete_latency`,"            \
    "`io_read_requests`,`io_read`,`io_read_latency`,`io_write_requests`,`io_write`,"               \
    "`io_write_latency`,`io_misc_requests`,`io_misc_latency`,`innodb_buffer_allocated`,"           \
    "`innodb_buffer_data`,`innodb_buffer_free`,`innodb_buffer_pages`,"                             \
    "`innodb_buffer_pages_hashed`,`innodb_buffer_pages_old`,`innodb_buffer_rows_cached`)"

#define SYS_SCHEMA_TABLE_STATISTICS_WITH_BUFFER_SELECT_SUFFIX                                      \
    " from ((`performance_schema`.`table_io_waits_summary_by_table` `pst` left join "              \
    "`sys`.`x$ps_schema_table_statistics_io` `fsbi` on(((`pst`.`OBJECT_SCHEMA` = "                 \
    "`sys`.`fsbi`.`table_schema`) and (`pst`.`OBJECT_NAME` = "                                     \
    "`sys`.`fsbi`.`table_name`)))) left join `sys`.`x$innodb_buffer_stats_by_table` `ibp` "        \
    "on(((`pst`.`OBJECT_SCHEMA` = `sys`.`ibp`.`object_schema`) and ("                              \
    "`pst`.`OBJECT_NAME` = `sys`.`ibp`.`object_name`)))) order by `pst`.`SUM_TIMER_WAIT` desc"

#define SYS_SCHEMA_TABLE_STATISTICS_WITH_BUFFER_VIEW_DEFINITION                                    \
    "select `pst`.`OBJECT_SCHEMA` AS `table_schema`,`pst`.`OBJECT_NAME` AS `table_name`,"          \
    "`pst`.`COUNT_FETCH` AS `rows_fetched`,format_pico_time(`pst`.`SUM_TIMER_FETCH`) AS "          \
    "`fetch_latency`,`pst`.`COUNT_INSERT` AS `rows_inserted`,format_pico_time("                    \
    "`pst`.`SUM_TIMER_INSERT`) AS `insert_latency`,`pst`.`COUNT_UPDATE` AS `rows_updated`,"        \
    "format_pico_time(`pst`.`SUM_TIMER_UPDATE`) AS `update_latency`,`pst`.`COUNT_DELETE` AS "      \
    "`rows_deleted`,format_pico_time(`pst`.`SUM_TIMER_DELETE`) AS `delete_latency`,"               \
    "`sys`.`fsbi`.`count_read` AS `io_read_requests`,format_bytes("                                \
    "`sys`.`fsbi`.`sum_number_of_bytes_read`) AS `io_read`,format_pico_time("                      \
    "`sys`.`fsbi`.`sum_timer_read`) AS `io_read_latency`,`sys`.`fsbi`.`count_write` AS "           \
    "`io_write_requests`,format_bytes(`sys`.`fsbi`.`sum_number_of_bytes_write`) AS "               \
    "`io_write`,format_pico_time(`sys`.`fsbi`.`sum_timer_write`) AS `io_write_latency`,"           \
    "`sys`.`fsbi`.`count_misc` AS `io_misc_requests`,format_pico_time("                            \
    "`sys`.`fsbi`.`sum_timer_misc`) AS `io_misc_latency`,format_bytes("                            \
    "`sys`.`ibp`.`allocated`) AS `innodb_buffer_allocated`,format_bytes("                          \
    "`sys`.`ibp`.`data`) AS `innodb_buffer_data`,format_bytes((`sys`.`ibp`.`allocated` - "         \
    "`sys`.`ibp`.`data`)) AS `innodb_buffer_free`,`sys`.`ibp`.`pages` AS "                         \
    "`innodb_buffer_pages`,`sys`.`ibp`.`pages_hashed` AS `innodb_buffer_pages_hashed`,"            \
    "`sys`.`ibp`.`pages_old` AS `innodb_buffer_pages_old`,`sys`.`ibp`.`rows_cached` AS "           \
    "`innodb_buffer_rows_cached`" SYS_SCHEMA_TABLE_STATISTICS_WITH_BUFFER_SELECT_SUFFIX

#define SYS_X_SCHEMA_TABLE_STATISTICS_WITH_BUFFER_SELECT_SUFFIX                                    \
    " from ((`performance_schema`.`table_io_waits_summary_by_table` `pst` left join "              \
    "`x$ps_schema_table_statistics_io` `fsbi` on(((`pst`.`OBJECT_SCHEMA` = "                       \
    "`fsbi`.`table_schema`) and (`pst`.`OBJECT_NAME` = `fsbi`.`table_name`)))) left join "         \
    "`x$innodb_buffer_stats_by_table` `ibp` on(((`pst`.`OBJECT_SCHEMA` = "                         \
    "`ibp`.`object_schema`) and (`pst`.`OBJECT_NAME` = `ibp`.`object_name`)))) order by "          \
    "`pst`.`SUM_TIMER_WAIT` desc"

#define SYS_X_SCHEMA_TABLE_STATISTICS_WITH_BUFFER_VIEW_DEFINITION                                  \
    "select `pst`.`OBJECT_SCHEMA` AS `table_schema`,`pst`.`OBJECT_NAME` AS `table_name`,"          \
    "`pst`.`COUNT_FETCH` AS `rows_fetched`,`pst`.`SUM_TIMER_FETCH` AS `fetch_latency`,"            \
    "`pst`.`COUNT_INSERT` AS `rows_inserted`,`pst`.`SUM_TIMER_INSERT` AS `insert_latency`,"        \
    "`pst`.`COUNT_UPDATE` AS `rows_updated`,`pst`.`SUM_TIMER_UPDATE` AS `update_latency`,"         \
    "`pst`.`COUNT_DELETE` AS `rows_deleted`,`pst`.`SUM_TIMER_DELETE` AS `delete_latency`,"         \
    "`fsbi`.`count_read` AS `io_read_requests`,`fsbi`.`sum_number_of_bytes_read` AS "              \
    "`io_read`,`fsbi`.`sum_timer_read` AS `io_read_latency`,`fsbi`.`count_write` AS "              \
    "`io_write_requests`,`fsbi`.`sum_number_of_bytes_write` AS `io_write`,"                        \
    "`fsbi`.`sum_timer_write` AS `io_write_latency`,`fsbi`.`count_misc` AS "                       \
    "`io_misc_requests`,`fsbi`.`sum_timer_misc` AS `io_misc_latency`,`ibp`.`allocated` AS "        \
    "`innodb_buffer_allocated`,`ibp`.`data` AS `innodb_buffer_data`,("                             \
    "`ibp`.`allocated` - `ibp`.`data`) AS `innodb_buffer_free`,`ibp`.`pages` AS "                  \
    "`innodb_buffer_pages`,`ibp`.`pages_hashed` AS `innodb_buffer_pages_hashed`,"                  \
    "`ibp`.`pages_old` AS `innodb_buffer_pages_old`,`ibp`.`rows_cached` AS "                       \
    "`innodb_buffer_rows_cached`" SYS_X_SCHEMA_TABLE_STATISTICS_WITH_BUFFER_SELECT_SUFFIX

static const char sys_schema_table_statistics_with_buffer_view_definition[] =
    SYS_SCHEMA_TABLE_STATISTICS_WITH_BUFFER_VIEW_DEFINITION;

static const char sys_schema_table_statistics_with_buffer_show_create_view_sql[] =
    "CREATE ALGORITHM=TEMPTABLE DEFINER=`mysql.sys`@`localhost` SQL SECURITY INVOKER VIEW "
    "`schema_table_statistics_with_buffer` " SYS_SCHEMA_TABLE_STATISTICS_WITH_BUFFER_VIEW_COLUMNS
    " AS " SYS_SCHEMA_TABLE_STATISTICS_WITH_BUFFER_VIEW_DEFINITION;

static const char sys_schema_table_statistics_with_buffer_show_create_qualified_view_sql[] =
    "CREATE ALGORITHM=TEMPTABLE DEFINER=`mysql.sys`@`localhost` SQL SECURITY INVOKER VIEW "
    "`sys`.`schema_table_statistics_with_buffer`"
    " " SYS_SCHEMA_TABLE_STATISTICS_WITH_BUFFER_VIEW_COLUMNS
    " AS " SYS_SCHEMA_TABLE_STATISTICS_WITH_BUFFER_VIEW_DEFINITION;

static const char sys_x_schema_table_statistics_with_buffer_view_definition[] =
    SYS_X_SCHEMA_TABLE_STATISTICS_WITH_BUFFER_VIEW_DEFINITION;

static const char sys_x_schema_table_statistics_with_buffer_show_create_view_sql[] =
    "CREATE ALGORITHM=TEMPTABLE DEFINER=`mysql.sys`@`localhost` SQL SECURITY INVOKER VIEW "
    "`x$schema_table_statistics_with_buffer` " SYS_SCHEMA_TABLE_STATISTICS_WITH_BUFFER_VIEW_COLUMNS
    " AS " SYS_X_SCHEMA_TABLE_STATISTICS_WITH_BUFFER_VIEW_DEFINITION;

static const char sys_x_schema_table_statistics_with_buffer_show_create_qualified_view_sql[] =
    "CREATE ALGORITHM=TEMPTABLE DEFINER=`mysql.sys`@`localhost` SQL SECURITY INVOKER VIEW "
    "`sys`.`x$schema_table_statistics_with_buffer`"
    " " SYS_SCHEMA_TABLE_STATISTICS_WITH_BUFFER_VIEW_COLUMNS
    " AS " SYS_X_SCHEMA_TABLE_STATISTICS_WITH_BUFFER_VIEW_DEFINITION;

#undef SYS_SCHEMA_TABLE_STATISTICS_WITH_BUFFER_VIEW_COLUMNS
#undef SYS_SCHEMA_TABLE_STATISTICS_WITH_BUFFER_SELECT_SUFFIX
#undef SYS_SCHEMA_TABLE_STATISTICS_WITH_BUFFER_VIEW_DEFINITION
#undef SYS_X_SCHEMA_TABLE_STATISTICS_WITH_BUFFER_SELECT_SUFFIX
#undef SYS_X_SCHEMA_TABLE_STATISTICS_WITH_BUFFER_VIEW_DEFINITION

#define SYS_SCHEMA_TABLES_WITH_FULL_TABLE_SCANS_VIEW_COLUMNS                                       \
    "(`object_schema`,`object_name`,`rows_full_scanned`,`latency`)"

#define SYS_SCHEMA_TABLES_WITH_FULL_TABLE_SCANS_SELECT_SUFFIX                                      \
    " from `performance_schema`.`table_io_waits_summary_by_index_usage` where (("                  \
    "`performance_schema`.`table_io_waits_summary_by_index_usage`.`INDEX_NAME` is null) and ("     \
    "`performance_schema`.`table_io_waits_summary_by_index_usage`.`COUNT_READ` > 0)) order by "    \
    "`performance_schema`.`table_io_waits_summary_by_index_usage`.`COUNT_READ` desc"

#define SYS_SCHEMA_TABLES_WITH_FULL_TABLE_SCANS_SELECT_PREFIX                                      \
    "select `performance_schema`.`table_io_waits_summary_by_index_usage`.`OBJECT_SCHEMA` AS "      \
    "`object_schema`,`performance_schema`.`table_io_waits_summary_by_index_usage`.`OBJECT_NAME` "  \
    "AS `object_name`,`performance_schema`.`table_io_waits_summary_by_index_usage`.`COUNT_READ` "  \
    "AS `rows_full_scanned`,"

#define SYS_SCHEMA_TABLES_WITH_FULL_TABLE_SCANS_VIEW_DEFINITION                                    \
    SYS_SCHEMA_TABLES_WITH_FULL_TABLE_SCANS_SELECT_PREFIX                                          \
    "format_pico_time(`performance_schema`.`table_io_waits_summary_by_index_usage`."               \
    "`SUM_TIMER_WAIT`) AS `latency`" SYS_SCHEMA_TABLES_WITH_FULL_TABLE_SCANS_SELECT_SUFFIX

#define SYS_X_SCHEMA_TABLES_WITH_FULL_TABLE_SCANS_VIEW_DEFINITION                                  \
    SYS_SCHEMA_TABLES_WITH_FULL_TABLE_SCANS_SELECT_PREFIX                                          \
    "`performance_schema`.`table_io_waits_summary_by_index_usage`.`SUM_TIMER_WAIT` AS "            \
    "`latency`" SYS_SCHEMA_TABLES_WITH_FULL_TABLE_SCANS_SELECT_SUFFIX

static const char sys_schema_tables_with_full_table_scans_view_definition[] =
    SYS_SCHEMA_TABLES_WITH_FULL_TABLE_SCANS_VIEW_DEFINITION;

static const char sys_schema_tables_with_full_table_scans_show_create_view_sql[] =
    "CREATE ALGORITHM=MERGE DEFINER=`mysql.sys`@`localhost` SQL SECURITY INVOKER VIEW "
    "`schema_tables_with_full_table_scans` " SYS_SCHEMA_TABLES_WITH_FULL_TABLE_SCANS_VIEW_COLUMNS
    " AS " SYS_SCHEMA_TABLES_WITH_FULL_TABLE_SCANS_VIEW_DEFINITION;

static const char sys_schema_tables_with_full_table_scans_show_create_qualified_view_sql[] =
    "CREATE ALGORITHM=MERGE DEFINER=`mysql.sys`@`localhost` SQL SECURITY INVOKER VIEW "
    "`sys`.`schema_tables_with_full_table_scans`"
    " " SYS_SCHEMA_TABLES_WITH_FULL_TABLE_SCANS_VIEW_COLUMNS
    " AS " SYS_SCHEMA_TABLES_WITH_FULL_TABLE_SCANS_VIEW_DEFINITION;

static const char sys_x_schema_tables_with_full_table_scans_view_definition[] =
    SYS_X_SCHEMA_TABLES_WITH_FULL_TABLE_SCANS_VIEW_DEFINITION;

static const char sys_x_schema_tables_with_full_table_scans_show_create_view_sql[] =
    "CREATE ALGORITHM=MERGE DEFINER=`mysql.sys`@`localhost` SQL SECURITY INVOKER VIEW "
    "`x$schema_tables_with_full_table_scans` " SYS_SCHEMA_TABLES_WITH_FULL_TABLE_SCANS_VIEW_COLUMNS
    " AS " SYS_X_SCHEMA_TABLES_WITH_FULL_TABLE_SCANS_VIEW_DEFINITION;

static const char sys_x_schema_tables_with_full_table_scans_show_create_qualified_view_sql[] =
    "CREATE ALGORITHM=MERGE DEFINER=`mysql.sys`@`localhost` SQL SECURITY INVOKER VIEW "
    "`sys`.`x$schema_tables_with_full_table_scans`"
    " " SYS_SCHEMA_TABLES_WITH_FULL_TABLE_SCANS_VIEW_COLUMNS
    " AS " SYS_X_SCHEMA_TABLES_WITH_FULL_TABLE_SCANS_VIEW_DEFINITION;

#undef SYS_SCHEMA_TABLES_WITH_FULL_TABLE_SCANS_VIEW_COLUMNS
#undef SYS_SCHEMA_TABLES_WITH_FULL_TABLE_SCANS_SELECT_SUFFIX
#undef SYS_SCHEMA_TABLES_WITH_FULL_TABLE_SCANS_SELECT_PREFIX
#undef SYS_SCHEMA_TABLES_WITH_FULL_TABLE_SCANS_VIEW_DEFINITION
#undef SYS_X_SCHEMA_TABLES_WITH_FULL_TABLE_SCANS_VIEW_DEFINITION

#define SYS_SCHEMA_UNUSED_INDEXES_VIEW_COLUMNS "(`object_schema`,`object_name`,`index_name`)"

#define SYS_SCHEMA_UNUSED_INDEXES_VIEW_DEFINITION                                                  \
    "select `t`.`OBJECT_SCHEMA` AS `object_schema`,`t`.`OBJECT_NAME` AS `object_name`,"            \
    "`t`.`INDEX_NAME` AS `index_name` from ("                                                      \
    "`performance_schema`.`table_io_waits_summary_by_index_usage` `t` join "                       \
    "`information_schema`.`STATISTICS` `s` on(((`t`.`OBJECT_SCHEMA` = "                            \
    "`information_schema`.`s`.`TABLE_SCHEMA`) and (`t`.`OBJECT_NAME` = "                           \
    "`information_schema`.`s`.`TABLE_NAME`) and (`t`.`INDEX_NAME` = "                              \
    "`information_schema`.`s`.`INDEX_NAME`)))) where ((`t`.`INDEX_NAME` is not null) "             \
    "and (`t`.`COUNT_STAR` = 0) and (`t`.`OBJECT_SCHEMA` <> 'mysql') and "                         \
    "(`t`.`INDEX_NAME` <> 'PRIMARY') and (`information_schema`.`s`.`NON_UNIQUE` = 1) "             \
    "and (`information_schema`.`s`.`SEQ_IN_INDEX` = 1)) order by `t`.`OBJECT_SCHEMA`,"             \
    "`t`.`OBJECT_NAME`"

static const char sys_schema_unused_indexes_view_definition[] =
    SYS_SCHEMA_UNUSED_INDEXES_VIEW_DEFINITION;

static const char sys_schema_unused_indexes_show_create_view_sql[] =
    "CREATE ALGORITHM=MERGE DEFINER=`mysql.sys`@`localhost` SQL SECURITY INVOKER VIEW "
    "`schema_unused_indexes` " SYS_SCHEMA_UNUSED_INDEXES_VIEW_COLUMNS
    " AS " SYS_SCHEMA_UNUSED_INDEXES_VIEW_DEFINITION;

static const char sys_schema_unused_indexes_show_create_qualified_view_sql[] =
    "CREATE ALGORITHM=MERGE DEFINER=`mysql.sys`@`localhost` SQL SECURITY INVOKER VIEW "
    "`sys`.`schema_unused_indexes` " SYS_SCHEMA_UNUSED_INDEXES_VIEW_COLUMNS
    " AS " SYS_SCHEMA_UNUSED_INDEXES_VIEW_DEFINITION;

#undef SYS_SCHEMA_UNUSED_INDEXES_VIEW_COLUMNS
#undef SYS_SCHEMA_UNUSED_INDEXES_VIEW_DEFINITION

#define SYS_SCHEMA_OBJECT_OVERVIEW_VIEW_COLUMNS "(`db`,`object_type`,`count`)"

#define SYS_SCHEMA_OBJECT_OVERVIEW_VIEW_DEFINITION                                                 \
    "select `information_schema`.`routines`.`ROUTINE_SCHEMA` AS `db`,"                             \
    "`information_schema`.`routines`.`ROUTINE_TYPE` AS `object_type`,count(0) AS `count` "         \
    "from `information_schema`.`ROUTINES` `routines` group by "                                    \
    "`information_schema`.`routines`.`ROUTINE_SCHEMA`,"                                            \
    "`information_schema`.`routines`.`ROUTINE_TYPE` union select "                                 \
    "`information_schema`.`tables`.`TABLE_SCHEMA` AS `TABLE_SCHEMA`,"                              \
    "`information_schema`.`tables`.`TABLE_TYPE` AS `TABLE_TYPE`,count(0) AS `COUNT(*)` "           \
    "from `information_schema`.`TABLES` `tables` group by "                                        \
    "`information_schema`.`tables`.`TABLE_SCHEMA`,"                                                \
    "`information_schema`.`tables`.`TABLE_TYPE` union select "                                     \
    "`information_schema`.`statistics`.`TABLE_SCHEMA` AS `TABLE_SCHEMA`,"                          \
    "concat('INDEX (',`information_schema`.`statistics`.`INDEX_TYPE`,')') AS "                     \
    "`CONCAT('INDEX (', INDEX_TYPE, ')')`,count(0) AS `COUNT(*)` from "                            \
    "`information_schema`.`STATISTICS` `statistics` group by "                                     \
    "`information_schema`.`statistics`.`TABLE_SCHEMA`,"                                            \
    "`information_schema`.`statistics`.`INDEX_TYPE` union select "                                 \
    "`information_schema`.`triggers`.`TRIGGER_SCHEMA` AS `TRIGGER_SCHEMA`,'TRIGGER' AS "           \
    "`TRIGGER`,count(0) AS `COUNT(*)` from `information_schema`.`TRIGGERS` `triggers` "            \
    "group by `information_schema`.`triggers`.`TRIGGER_SCHEMA` union select "                      \
    "`information_schema`.`events`.`EVENT_SCHEMA` AS `EVENT_SCHEMA`,'EVENT' AS `EVENT`,"           \
    "count(0) AS `COUNT(*)` from `information_schema`.`EVENTS` `events` group by "                 \
    "`information_schema`.`events`.`EVENT_SCHEMA` order by `db`,`object_type`"

static const char sys_schema_object_overview_view_definition[] =
    SYS_SCHEMA_OBJECT_OVERVIEW_VIEW_DEFINITION;

static const char sys_schema_object_overview_show_create_view_sql[] =
    "CREATE ALGORITHM=TEMPTABLE DEFINER=`mysql.sys`@`localhost` SQL SECURITY INVOKER VIEW "
    "`schema_object_overview` " SYS_SCHEMA_OBJECT_OVERVIEW_VIEW_COLUMNS
    " AS " SYS_SCHEMA_OBJECT_OVERVIEW_VIEW_DEFINITION;

static const char sys_schema_object_overview_show_create_qualified_view_sql[] =
    "CREATE ALGORITHM=TEMPTABLE DEFINER=`mysql.sys`@`localhost` SQL SECURITY INVOKER VIEW "
    "`sys`.`schema_object_overview` " SYS_SCHEMA_OBJECT_OVERVIEW_VIEW_COLUMNS
    " AS " SYS_SCHEMA_OBJECT_OVERVIEW_VIEW_DEFINITION;

#undef SYS_SCHEMA_OBJECT_OVERVIEW_VIEW_COLUMNS
#undef SYS_SCHEMA_OBJECT_OVERVIEW_VIEW_DEFINITION

static const struct mylite_execution_catalog_builtin_sys_view builtin_sys_view_definitions[] = {
    {"version",
     sys_version_view_definition,
     sys_version_show_create_view_sql,
     sys_version_show_create_qualified_view_sql},
    {"host_summary",
     sys_host_summary_view_definition,
     sys_host_summary_show_create_view_sql,
     sys_host_summary_show_create_qualified_view_sql},
    {"host_summary_by_file_io",
     sys_host_summary_by_file_io_view_definition,
     sys_host_summary_by_file_io_show_create_view_sql,
     sys_host_summary_by_file_io_show_create_qualified_view_sql},
    {"host_summary_by_file_io_type",
     sys_host_summary_by_file_io_type_view_definition,
     sys_host_summary_by_file_io_type_show_create_view_sql,
     sys_host_summary_by_file_io_type_show_create_qualified_view_sql},
    {"host_summary_by_stages",
     sys_host_summary_by_stages_view_definition,
     sys_host_summary_by_stages_show_create_view_sql,
     sys_host_summary_by_stages_show_create_qualified_view_sql},
    {"host_summary_by_statement_latency",
     sys_host_summary_by_statement_latency_view_definition,
     sys_host_summary_by_statement_latency_show_create_view_sql,
     sys_host_summary_by_statement_latency_show_create_qualified_view_sql},
    {"host_summary_by_statement_type",
     sys_host_summary_by_statement_type_view_definition,
     sys_host_summary_by_statement_type_show_create_view_sql,
     sys_host_summary_by_statement_type_show_create_qualified_view_sql},
    {"innodb_buffer_stats_by_schema",
     sys_innodb_buffer_stats_by_schema_view_definition,
     sys_innodb_buffer_stats_by_schema_show_create_view_sql,
     sys_innodb_buffer_stats_by_schema_show_create_qualified_view_sql},
    {"innodb_buffer_stats_by_table",
     sys_innodb_buffer_stats_by_table_view_definition,
     sys_innodb_buffer_stats_by_table_show_create_view_sql,
     sys_innodb_buffer_stats_by_table_show_create_qualified_view_sql},
    {"innodb_lock_waits",
     sys_innodb_lock_waits_view_definition,
     sys_innodb_lock_waits_show_create_view_sql,
     sys_innodb_lock_waits_show_create_qualified_view_sql},
    {"io_by_thread_by_latency",
     sys_io_by_thread_by_latency_view_definition,
     sys_io_by_thread_by_latency_show_create_view_sql,
     sys_io_by_thread_by_latency_show_create_qualified_view_sql},
    {"io_global_by_file_by_bytes",
     sys_io_global_by_file_by_bytes_view_definition,
     sys_io_global_by_file_by_bytes_show_create_view_sql,
     sys_io_global_by_file_by_bytes_show_create_qualified_view_sql},
    {"io_global_by_file_by_latency",
     sys_io_global_by_file_by_latency_view_definition,
     sys_io_global_by_file_by_latency_show_create_view_sql,
     sys_io_global_by_file_by_latency_show_create_qualified_view_sql},
    {"io_global_by_wait_by_bytes",
     sys_io_global_by_wait_by_bytes_view_definition,
     sys_io_global_by_wait_by_bytes_show_create_view_sql,
     sys_io_global_by_wait_by_bytes_show_create_qualified_view_sql},
    {"io_global_by_wait_by_latency",
     sys_io_global_by_wait_by_latency_view_definition,
     sys_io_global_by_wait_by_latency_show_create_view_sql,
     sys_io_global_by_wait_by_latency_show_create_qualified_view_sql},
    {"latest_file_io",
     sys_latest_file_io_view_definition,
     sys_latest_file_io_show_create_view_sql,
     sys_latest_file_io_show_create_qualified_view_sql},
    {"memory_by_host_by_current_bytes",
     sys_memory_by_host_by_current_bytes_view_definition,
     sys_memory_by_host_by_current_bytes_show_create_view_sql,
     sys_memory_by_host_by_current_bytes_show_create_qualified_view_sql},
    {"memory_by_thread_by_current_bytes",
     sys_memory_by_thread_by_current_bytes_view_definition,
     sys_memory_by_thread_by_current_bytes_show_create_view_sql,
     sys_memory_by_thread_by_current_bytes_show_create_qualified_view_sql},
    {"memory_by_user_by_current_bytes",
     sys_memory_by_user_by_current_bytes_view_definition,
     sys_memory_by_user_by_current_bytes_show_create_view_sql,
     sys_memory_by_user_by_current_bytes_show_create_qualified_view_sql},
    {"memory_global_by_current_bytes",
     sys_memory_global_by_current_bytes_view_definition,
     sys_memory_global_by_current_bytes_show_create_view_sql,
     sys_memory_global_by_current_bytes_show_create_qualified_view_sql},
    {"ps_check_lost_instrumentation",
     sys_ps_check_lost_instrumentation_view_definition,
     sys_ps_check_lost_instrumentation_show_create_view_sql,
     sys_ps_check_lost_instrumentation_show_create_qualified_view_sql},
    {"schema_auto_increment_columns",
     sys_schema_auto_increment_columns_view_definition,
     sys_schema_auto_increment_columns_show_create_view_sql,
     sys_schema_auto_increment_columns_show_create_qualified_view_sql},
    {"schema_index_statistics",
     sys_schema_index_statistics_view_definition,
     sys_schema_index_statistics_show_create_view_sql,
     sys_schema_index_statistics_show_create_qualified_view_sql},
    {"schema_object_overview",
     sys_schema_object_overview_view_definition,
     sys_schema_object_overview_show_create_view_sql,
     sys_schema_object_overview_show_create_qualified_view_sql},
    {"schema_redundant_indexes",
     sys_schema_redundant_indexes_view_definition,
     sys_schema_redundant_indexes_show_create_view_sql,
     sys_schema_redundant_indexes_show_create_qualified_view_sql},
    {"schema_table_lock_waits",
     sys_schema_table_lock_waits_view_definition,
     sys_schema_table_lock_waits_show_create_view_sql,
     sys_schema_table_lock_waits_show_create_qualified_view_sql},
    {"schema_table_statistics",
     sys_schema_table_statistics_view_definition,
     sys_schema_table_statistics_show_create_view_sql,
     sys_schema_table_statistics_show_create_qualified_view_sql},
    {"schema_table_statistics_with_buffer",
     sys_schema_table_statistics_with_buffer_view_definition,
     sys_schema_table_statistics_with_buffer_show_create_view_sql,
     sys_schema_table_statistics_with_buffer_show_create_qualified_view_sql},
    {"schema_tables_with_full_table_scans",
     sys_schema_tables_with_full_table_scans_view_definition,
     sys_schema_tables_with_full_table_scans_show_create_view_sql,
     sys_schema_tables_with_full_table_scans_show_create_qualified_view_sql},
    {"schema_unused_indexes",
     sys_schema_unused_indexes_view_definition,
     sys_schema_unused_indexes_show_create_view_sql,
     sys_schema_unused_indexes_show_create_qualified_view_sql},
    {"x$schema_flattened_keys",
     sys_x_schema_flattened_keys_view_definition,
     sys_x_schema_flattened_keys_show_create_view_sql,
     sys_x_schema_flattened_keys_show_create_qualified_view_sql},
    {"x$host_summary",
     sys_x_host_summary_view_definition,
     sys_x_host_summary_show_create_view_sql,
     sys_x_host_summary_show_create_qualified_view_sql},
    {"x$host_summary_by_file_io",
     sys_x_host_summary_by_file_io_view_definition,
     sys_x_host_summary_by_file_io_show_create_view_sql,
     sys_x_host_summary_by_file_io_show_create_qualified_view_sql},
    {"x$host_summary_by_file_io_type",
     sys_x_host_summary_by_file_io_type_view_definition,
     sys_x_host_summary_by_file_io_type_show_create_view_sql,
     sys_x_host_summary_by_file_io_type_show_create_qualified_view_sql},
    {"x$host_summary_by_stages",
     sys_x_host_summary_by_stages_view_definition,
     sys_x_host_summary_by_stages_show_create_view_sql,
     sys_x_host_summary_by_stages_show_create_qualified_view_sql},
    {"x$host_summary_by_statement_latency",
     sys_x_host_summary_by_statement_latency_view_definition,
     sys_x_host_summary_by_statement_latency_show_create_view_sql,
     sys_x_host_summary_by_statement_latency_show_create_qualified_view_sql},
    {"x$host_summary_by_statement_type",
     sys_x_host_summary_by_statement_type_view_definition,
     sys_x_host_summary_by_statement_type_show_create_view_sql,
     sys_x_host_summary_by_statement_type_show_create_qualified_view_sql},
    {"x$innodb_buffer_stats_by_schema",
     sys_x_innodb_buffer_stats_by_schema_view_definition,
     sys_x_innodb_buffer_stats_by_schema_show_create_view_sql,
     sys_x_innodb_buffer_stats_by_schema_show_create_qualified_view_sql},
    {"x$innodb_buffer_stats_by_table",
     sys_x_innodb_buffer_stats_by_table_view_definition,
     sys_x_innodb_buffer_stats_by_table_show_create_view_sql,
     sys_x_innodb_buffer_stats_by_table_show_create_qualified_view_sql},
    {"x$innodb_lock_waits",
     sys_x_innodb_lock_waits_view_definition,
     sys_x_innodb_lock_waits_show_create_view_sql,
     sys_x_innodb_lock_waits_show_create_qualified_view_sql},
    {"x$io_by_thread_by_latency",
     sys_x_io_by_thread_by_latency_view_definition,
     sys_x_io_by_thread_by_latency_show_create_view_sql,
     sys_x_io_by_thread_by_latency_show_create_qualified_view_sql},
    {"x$io_global_by_file_by_bytes",
     sys_x_io_global_by_file_by_bytes_view_definition,
     sys_x_io_global_by_file_by_bytes_show_create_view_sql,
     sys_x_io_global_by_file_by_bytes_show_create_qualified_view_sql},
    {"x$io_global_by_file_by_latency",
     sys_x_io_global_by_file_by_latency_view_definition,
     sys_x_io_global_by_file_by_latency_show_create_view_sql,
     sys_x_io_global_by_file_by_latency_show_create_qualified_view_sql},
    {"x$io_global_by_wait_by_bytes",
     sys_x_io_global_by_wait_by_bytes_view_definition,
     sys_x_io_global_by_wait_by_bytes_show_create_view_sql,
     sys_x_io_global_by_wait_by_bytes_show_create_qualified_view_sql},
    {"x$io_global_by_wait_by_latency",
     sys_x_io_global_by_wait_by_latency_view_definition,
     sys_x_io_global_by_wait_by_latency_show_create_view_sql,
     sys_x_io_global_by_wait_by_latency_show_create_qualified_view_sql},
    {"x$latest_file_io",
     sys_x_latest_file_io_view_definition,
     sys_x_latest_file_io_show_create_view_sql,
     sys_x_latest_file_io_show_create_qualified_view_sql},
    {"x$memory_by_host_by_current_bytes",
     sys_x_memory_by_host_by_current_bytes_view_definition,
     sys_x_memory_by_host_by_current_bytes_show_create_view_sql,
     sys_x_memory_by_host_by_current_bytes_show_create_qualified_view_sql},
    {"x$memory_by_thread_by_current_bytes",
     sys_x_memory_by_thread_by_current_bytes_view_definition,
     sys_x_memory_by_thread_by_current_bytes_show_create_view_sql,
     sys_x_memory_by_thread_by_current_bytes_show_create_qualified_view_sql},
    {"x$memory_by_user_by_current_bytes",
     sys_x_memory_by_user_by_current_bytes_view_definition,
     sys_x_memory_by_user_by_current_bytes_show_create_view_sql,
     sys_x_memory_by_user_by_current_bytes_show_create_qualified_view_sql},
    {"x$memory_global_by_current_bytes",
     sys_x_memory_global_by_current_bytes_view_definition,
     sys_x_memory_global_by_current_bytes_show_create_view_sql,
     sys_x_memory_global_by_current_bytes_show_create_qualified_view_sql},
    {"x$ps_schema_table_statistics_io",
     sys_x_ps_schema_table_statistics_io_view_definition,
     sys_x_ps_schema_table_statistics_io_show_create_view_sql,
     sys_x_ps_schema_table_statistics_io_show_create_qualified_view_sql},
    {"x$schema_index_statistics",
     sys_x_schema_index_statistics_view_definition,
     sys_x_schema_index_statistics_show_create_view_sql,
     sys_x_schema_index_statistics_show_create_qualified_view_sql},
    {"x$schema_table_lock_waits",
     sys_x_schema_table_lock_waits_view_definition,
     sys_x_schema_table_lock_waits_show_create_view_sql,
     sys_x_schema_table_lock_waits_show_create_qualified_view_sql},
    {"x$schema_table_statistics",
     sys_x_schema_table_statistics_view_definition,
     sys_x_schema_table_statistics_show_create_view_sql,
     sys_x_schema_table_statistics_show_create_qualified_view_sql},
    {"x$schema_table_statistics_with_buffer",
     sys_x_schema_table_statistics_with_buffer_view_definition,
     sys_x_schema_table_statistics_with_buffer_show_create_view_sql,
     sys_x_schema_table_statistics_with_buffer_show_create_qualified_view_sql},
    {"x$schema_tables_with_full_table_scans",
     sys_x_schema_tables_with_full_table_scans_view_definition,
     sys_x_schema_tables_with_full_table_scans_show_create_view_sql,
     sys_x_schema_tables_with_full_table_scans_show_create_qualified_view_sql},
};

size_t mylite_execution_catalog_builtin_sys_view_definition_count(void) {
    return sizeof(builtin_sys_view_definitions) / sizeof(builtin_sys_view_definitions[0]);
}

const struct mylite_execution_catalog_builtin_sys_view *mylite_execution_catalog_builtin_sys_view_definition_at(
    size_t index
) {
    if (index >= mylite_execution_catalog_builtin_sys_view_definition_count()) {
        return NULL;
    }
    return &builtin_sys_view_definitions[index];
}

const struct mylite_execution_catalog_builtin_sys_view *mylite_execution_catalog_builtin_sys_view_definition_by_name(
    const char *view_name
) {
    if (view_name == NULL) {
        return NULL;
    }
    for (size_t index = 0U; index < mylite_execution_catalog_builtin_sys_view_definition_count();
         ++index) {
        if (strcmp(view_name, builtin_sys_view_definitions[index].name) == 0) {
            return &builtin_sys_view_definitions[index];
        }
    }
    return NULL;
}
