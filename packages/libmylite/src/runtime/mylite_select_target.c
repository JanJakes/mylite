#include "mylite_select_target.h"

#include "mylite_catalog.h"
#include "mylite_diagnostics.h"
#include "mylite_information_schema.h"
#include "mylite_runtime.h"
#include "mylite_span.h"

static int resolve_select_table_target(mylite_db *database, struct mylite_select_table *table,
                                       bool information_schema_selectable);

int mylite_select_resolve_table_target(mylite_db *database, struct mylite_select_table *table)
{
    return resolve_select_table_target(database, table, false);
}

int mylite_select_resolve_query_table_target(mylite_db *database, struct mylite_select_table *table)
{
    return resolve_select_table_target(database, table, true);
}

static int resolve_select_table_target(mylite_db *database, struct mylite_select_table *table,
                                       bool information_schema_selectable)
{
    struct mylite_schema_presence presence;
    bool exists = false;
    int status = MYLITE_OK;

    if (table->schema_name == NULL) {
        if (database->selected_schema == NULL || database->selected_schema[0] == '\0') {
            (void)mylite_diagnostics_set_error_message(database, "No database selected");
            return MYLITE_EXEC_ERROR;
        }
        table->schema_name = mylite_copy_nonempty_cstring(database->selected_schema);
        if (table->schema_name == NULL) {
            (void)mylite_diagnostics_set_error_message(database, "out of memory");
            return MYLITE_NOMEM;
        }
    }
    if (information_schema_selectable &&
        mylite_ascii_case_equal(table->schema_name, "information_schema")) {
        return mylite_information_schema_prepare_table_view(database, table->table_name,
                                                            &table->physical_name);
    }
    if (mylite_select_schema_name_is_system(table->schema_name)) {
        return MYLITE_UNSUPPORTED;
    }

    status = mylite_catalog_schema_exists(database, table->schema_name, &presence);
    if (status != MYLITE_OK) {
        return status;
    }
    if (!presence.exists) {
        (void)mylite_diagnostics_set_error_message_parts(database, "Unknown database '",
                                                         table->schema_name, "'");
        return MYLITE_EXEC_ERROR;
    }
    if (presence.is_system) {
        return MYLITE_UNSUPPORTED;
    }

    status = mylite_catalog_table_exists(database, table->schema_name, table->table_name, &exists);
    if (status != MYLITE_OK) {
        return status;
    }
    if (!exists) {
        return mylite_diagnostics_set_table_doesnt_exist_error(database, table->schema_name,
                                                               table->table_name);
    }

    table->physical_name =
        mylite_catalog_physical_table_name(table->schema_name, table->table_name);
    if (table->physical_name == NULL) {
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }
    return MYLITE_OK;
}

bool mylite_select_schema_name_is_system(const char *schema_name)
{
    if (mylite_ascii_case_equal(schema_name, "information_schema")) {
        return true;
    }
    if (mylite_ascii_case_equal(schema_name, "mysql")) {
        return true;
    }
    if (mylite_ascii_case_equal(schema_name, "performance_schema")) {
        return true;
    }
    if (mylite_ascii_case_equal(schema_name, "sys")) {
        return true;
    }
    return false;
}
