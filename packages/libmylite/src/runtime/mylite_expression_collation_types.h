#ifndef MYLITE_RUNTIME_MYLITE_EXPRESSION_COLLATION_TYPES_H
#define MYLITE_RUNTIME_MYLITE_EXPRESSION_COLLATION_TYPES_H

struct mylite_charset_collation_info {
    const char *character_set;
    const char *collation;
    int coercibility;
};

#endif
