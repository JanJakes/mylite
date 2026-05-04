#include "mylite_field_descriptor.h"

#include <mylite/mylite.h>

void mylite_field_descriptor_set_nullable(struct mylite_field_descriptor *descriptor, bool nullable)
{
    if (nullable) {
        mylite_field_descriptor_set_not_null(descriptor, false);
    } else {
        mylite_field_descriptor_set_not_null(descriptor, true);
    }
}

void mylite_field_descriptor_set_not_null(struct mylite_field_descriptor *descriptor, bool not_null)
{
    if (descriptor == NULL) {
        return;
    }
    if (not_null) {
        descriptor->nullable = false;
        descriptor->flags |= MYLITE_FIELD_FLAG_NOT_NULL;
    } else {
        descriptor->nullable = true;
        descriptor->flags &= ~(unsigned int)MYLITE_FIELD_FLAG_NOT_NULL;
    }
}
