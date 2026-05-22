#ifndef MYLITE_RUNTIME_MYLITE_INTEGER_ARITHMETIC_H
#define MYLITE_RUNTIME_MYLITE_INTEGER_ARITHMETIC_H

#include "sqlite3.h"

#define MYLITE_INTEGER_ARITHMETIC_OVERFLOW_MESSAGE "MyLite signed integer arithmetic overflow"

int mylite_sqlite_register_integer_arithmetic_functions(sqlite3 *sqlite);

#endif
