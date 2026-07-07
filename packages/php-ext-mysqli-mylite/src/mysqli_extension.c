#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include "mysqli_extension.h"

ZEND_DECLARE_MODULE_GLOBALS(mylite_mysqli)

PHP_MINIT_FUNCTION(mysqli) {
    mylite_mysqli_register_classes();
    mylite_mysqli_register_constants(module_number);
    return SUCCESS;
}

PHP_RSHUTDOWN_FUNCTION(mysqli) {
    mylite_mysqli_flush_profile();
    return SUCCESS;
}

PHP_MINFO_FUNCTION(mysqli) {
    php_info_print_table_start();
    php_info_print_table_header(2, "MyLite mysqli support", "enabled");
    php_info_print_table_row(2, "MyLite version", mylite_version());
    php_info_print_table_end();
}

#ifdef __GNUC__
#  pragma GCC diagnostic push
#  pragma GCC diagnostic ignored "-Wshadow"
#endif
PHP_GINIT_FUNCTION(mylite_mysqli) {
#if defined(COMPILE_DL_MYSQLI) && defined(ZTS)
    ZEND_TSRMLS_CACHE_UPDATE();
#endif
    mylite_mysqli_init_globals(mylite_mysqli_globals);
}
#ifdef __GNUC__
#  pragma GCC diagnostic pop
#endif

static const zend_module_dep mylite_mysqli_deps[] = {ZEND_MOD_REQUIRED("mylite") ZEND_MOD_END};

zend_module_entry mysqli_module_entry = {
    STANDARD_MODULE_HEADER_EX,
    NULL,
    mylite_mysqli_deps,
    "mysqli",
    mylite_mysqli_functions,
    PHP_MINIT(mysqli),
    NULL,
    NULL,
    PHP_RSHUTDOWN(mysqli),
    PHP_MINFO(mysqli),
    PHP_MYLITE_MYSQLI_VERSION,
    PHP_MODULE_GLOBALS(mylite_mysqli),
    PHP_GINIT(mylite_mysqli),
    NULL,
    NULL,
    STANDARD_MODULE_PROPERTIES_EX,
};

#ifdef COMPILE_DL_MYSQLI
ZEND_GET_MODULE(mysqli)
#endif
