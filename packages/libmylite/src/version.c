#include <mylite/mylite.h>

#include "runtime/mylite_mysql_server_identity.h"

const char *mylite_version(void) {
    return MYLITE_VERSION_STRING;
}

const char *mylite_server_version(void) {
    return MYLITE_MYSQL_SERVER_VERSION_STRING;
}
