#include "mylite_schema.h"

#include "mylite_charset.h"
#include "mylite_diagnostics.h"
#include "mylite_span.h"

#include <stdlib.h>
#include <string.h>

static int normalize_schema_charset_and_collation(mylite_db *database,
                                                  struct mylite_schema_options *options);
static int normalize_schema_option_text(mylite_db *database, char **target, const char *value);

void mylite_schema_options_deinit(struct mylite_schema_options *options)
{
    if (options == NULL) {
        return;
    }

    free(options->character_set);
    free(options->collation);
    free(options->encryption);
    *options = (struct mylite_schema_options){0};
}

int mylite_schema_normalize_options(mylite_db *database, struct mylite_schema_options *options)
{
    int status = MYLITE_OK;

    if (options->invalid_encryption) {
        (void)mylite_diagnostics_set_error_message(database,
                                                   "Incorrect argument (should be Y or N) value");
        return MYLITE_EXEC_ERROR;
    }
    if (options->invalid_read_only) {
        (void)mylite_diagnostics_set_error_message(database, "Incorrect READ ONLY value");
        return MYLITE_EXEC_ERROR;
    }

    status = normalize_schema_charset_and_collation(database, options);
    return status;
}

static int normalize_schema_charset_and_collation(mylite_db *database,
                                                  struct mylite_schema_options *options)
{
    const struct mylite_charset *character_set = mylite_charset_lookup(options->character_set);
    const struct mylite_collation *collation = mylite_collation_lookup(options->collation);
    int status = MYLITE_OK;

    if (options->character_set != NULL && character_set == NULL) {
        return mylite_diagnostics_set_unknown_charset_error(database, options->character_set);
    }
    if (options->collation != NULL && collation == NULL) {
        return mylite_diagnostics_set_unknown_collation_error(database, options->collation);
    }
    if (character_set != NULL && collation != NULL &&
        !mylite_charset_collation_match(character_set, collation)) {
        return mylite_diagnostics_set_collation_charset_error(database, collation->name,
                                                              character_set->name);
    }
    if (character_set == NULL && collation == NULL) {
        return MYLITE_OK;
    }

    if (character_set == NULL) {
        character_set = mylite_charset_lookup(collation->character_set);
    }
    if (collation == NULL) {
        collation = mylite_collation_lookup(character_set->default_collation);
    }
    if (character_set == NULL || collation == NULL) {
        (void)mylite_diagnostics_set_error_message(database,
                                                   "Unsupported charset/collation registry entry");
        return MYLITE_EXEC_ERROR;
    }

    status = normalize_schema_option_text(database, &options->character_set, character_set->name);
    if (status != MYLITE_OK) {
        return status;
    }
    return normalize_schema_option_text(database, &options->collation, collation->name);
}

static int normalize_schema_option_text(mylite_db *database, char **target, const char *value)
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
