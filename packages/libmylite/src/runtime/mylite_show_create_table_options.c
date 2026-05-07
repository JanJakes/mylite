#include "mylite_show_create_table_options.h"

#include "mylite_charset.h"
#include "mylite_show_create_common.h"
#include "mylite_show_create_table_info.h"
#include "sqlite3.h"

void mylite_show_create_table_append_options(
    sqlite3_str *create_sql,
    const struct mylite_show_create_table_info *info
) {
    const struct mylite_collation *collation = mylite_collation_lookup(info->table_collation);
    const char *character_set =
        collation == NULL ? mylite_charset_default_name() : collation->character_set;

    sqlite3_str_appendf(create_sql, " ENGINE=%s", info->engine == NULL ? "InnoDB" : info->engine);
    if (info->has_auto_increment && info->auto_increment > 1U) {
        sqlite3_str_appendf(
            create_sql,
            " AUTO_INCREMENT=%llu",
            (unsigned long long)info->auto_increment
        );
    }
    sqlite3_str_appendf(
        create_sql,
        " DEFAULT CHARSET=%s COLLATE=%s",
        character_set,
        info->table_collation == NULL ? mylite_charset_default_collation_name()
                                      : info->table_collation
    );
    if (info->table_comment != NULL && info->table_comment[0] != '\0') {
        sqlite3_str_appendall(create_sql, " COMMENT=");
        mylite_show_create_append_string_literal(create_sql, info->table_comment);
    }
}
