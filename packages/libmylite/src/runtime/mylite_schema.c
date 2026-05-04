#include "mylite_schema.h"

#include <stdlib.h>

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
