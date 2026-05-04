#include "mylite_dml.h"

#include "mylite_catalog.h"
#include "mylite_diagnostics.h"

int mylite_dml_validate_insert_target(mylite_db *database, const char *selected_schema,
                                      const struct mylite_insert_values_plan *plan,
                                      const char **out_schema_name)
{
    const char *schema_name = NULL;
    struct mylite_schema_presence presence = {false};
    bool exists = false;
    int status = MYLITE_OK;

    if (database == NULL || plan == NULL || out_schema_name == NULL) {
        return MYLITE_MISUSE;
    }

    *out_schema_name = NULL;
    schema_name = plan->schema_name == NULL ? selected_schema : plan->schema_name;
    if (schema_name == NULL) {
        (void)mylite_diagnostics_set_error_message(database, "No database selected");
        return MYLITE_EXEC_ERROR;
    }

    status = mylite_catalog_schema_exists(database, schema_name, &presence);
    if (status != MYLITE_OK) {
        return status;
    }
    if (!presence.exists) {
        (void)mylite_diagnostics_set_error_message_parts(database, "Unknown database '",
                                                         schema_name, "'");
        return MYLITE_EXEC_ERROR;
    }
    if (presence.is_system) {
        (void)mylite_diagnostics_set_error_message_parts(database, "Access to system schema '",
                                                         schema_name, "' is rejected.");
        return MYLITE_EXEC_ERROR;
    }

    status = mylite_catalog_table_exists(database, schema_name, plan->table_name, &exists);
    if (status != MYLITE_OK) {
        return status;
    }
    if (!exists) {
        return mylite_diagnostics_set_table_doesnt_exist_error(database, schema_name,
                                                               plan->table_name);
    }

    *out_schema_name = schema_name;
    return MYLITE_OK;
}
