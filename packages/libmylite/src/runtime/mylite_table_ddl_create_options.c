#include "mylite_table_ddl_create_options.h"

#include "mylite_charset.h"
#include "mylite_diagnostics.h"
#include "mylite_schema_types.h"
#include "mylite_span.h"

#include <stdlib.h>
#include <string.h>

static int normalize_create_table_option_text(mylite_db *database, char **target,
                                              const char *value);
static bool is_supported_engine_name(const char *name);

int mylite_table_ddl_normalize_create_table_options(
    mylite_db *database, const char *schema_name,
    const struct mylite_schema_default *schema_default, struct mylite_create_table_options *options)
{
    const struct mylite_charset *character_set = NULL;
    const struct mylite_collation *collation = NULL;
    const char *collation_name = NULL;
    int status = MYLITE_OK;

    (void)schema_name;
    if (options->engine != NULL && !is_supported_engine_name(options->engine)) {
        status = mylite_diagnostics_set_error_message_parts(
            database, "Unsupported storage engine: '", options->engine, "'");
        return status == MYLITE_NOMEM ? MYLITE_NOMEM : MYLITE_EXEC_ERROR;
    }
    if (options->character_set != NULL) {
        character_set = mylite_charset_lookup(options->character_set);
        if (character_set == NULL) {
            return mylite_diagnostics_set_unknown_charset_error(database, options->character_set);
        }
    }
    if (options->collation != NULL) {
        collation = mylite_collation_lookup(options->collation);
        if (collation == NULL) {
            return mylite_diagnostics_set_unknown_collation_error(database, options->collation);
        }
    }
    if (character_set == NULL && collation != NULL) {
        character_set = mylite_charset_lookup(collation->character_set);
    }
    if (character_set == NULL) {
        character_set = mylite_charset_lookup(schema_default->character_set);
    }
    if (collation == NULL) {
        collation_name = options->character_set == NULL ? schema_default->collation
                                                        : character_set->default_collation;
        collation = mylite_collation_lookup(collation_name);
    }
    if (character_set == NULL || collation == NULL) {
        (void)mylite_diagnostics_set_error_message(database,
                                                   "Unsupported charset/collation registry entry");
        return MYLITE_EXEC_ERROR;
    }
    if (!mylite_charset_collation_match(character_set, collation)) {
        return mylite_diagnostics_set_collation_charset_error(database, collation->name,
                                                              character_set->name);
    }

    status =
        normalize_create_table_option_text(database, &options->character_set, character_set->name);
    if (status != MYLITE_OK) {
        return status;
    }
    return normalize_create_table_option_text(database, &options->collation, collation->name);
}

static int normalize_create_table_option_text(mylite_db *database, char **target, const char *value)
{
    char *copy = mylite_copy_span_text(value, strlen(value));

    if (copy == NULL) {
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }

    free(*target);
    *target = copy;
    return MYLITE_OK;
}

static bool is_supported_engine_name(const char *name)
{
    if (name == NULL) {
        return true;
    }
    return mylite_ascii_case_equal(name, "InnoDB");
}
