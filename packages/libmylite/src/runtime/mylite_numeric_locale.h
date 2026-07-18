#ifndef MYLITE_RUNTIME_MYLITE_NUMERIC_LOCALE_H
#define MYLITE_RUNTIME_MYLITE_NUMERIC_LOCALE_H

#include <stdarg.h>
#include <stddef.h>

/* SQL numeric text always uses a period decimal separator, independent of the host locale. */
double mylite_numeric_parse_double(const char *text, char **out_end);
long double mylite_numeric_parse_long_double(const char *text, char **out_end);
int mylite_numeric_format(char *buffer, size_t buffer_size, const char *format, ...);
int mylite_numeric_vformat(char *buffer, size_t buffer_size, const char *format, va_list arguments);

#endif
