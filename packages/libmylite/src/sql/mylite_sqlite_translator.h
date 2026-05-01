#ifndef MYLITE_SQL_MYLITE_SQLITE_TRANSLATOR_H
#define MYLITE_SQL_MYLITE_SQLITE_TRANSLATOR_H

#include "mylite_ast.h"

enum mylite_sqlite_translate_status {
    MYLITE_SQLITE_TRANSLATE_OK = 0,
    MYLITE_SQLITE_TRANSLATE_NOMEM = 1,
    MYLITE_SQLITE_TRANSLATE_UNSUPPORTED = 2,
};

struct mylite_sqlite_translate_result {
    char *sql;
};

enum mylite_sqlite_translate_status
mylite_sqlite_translate(const struct mylite_sql_ast_node *root,
                        struct mylite_sqlite_translate_result *out_result);

void mylite_sqlite_translate_result_deinit(struct mylite_sqlite_translate_result *result);

#endif
