#ifndef MYLITE_MYLITE_H
#define MYLITE_MYLITE_H

#include <mylite/version.h>

#ifndef MYLITE_API
#  ifdef _WIN32
#    if defined(MYLITE_BUILDING_SHARED_LIBRARY)
#      define MYLITE_API __declspec(dllexport)
#    elif defined(MYLITE_USING_SHARED_LIBRARY)
#      define MYLITE_API __declspec(dllimport)
#    else
#      define MYLITE_API
#    endif
#  elif defined(__GNUC__) || defined(__clang__)
#    define MYLITE_API __attribute__((visibility("default")))
#  else
#    define MYLITE_API
#  endif
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef struct mylite_db mylite_db;

#define MYLITE_OK 0
#define MYLITE_ERROR 1
#define MYLITE_NOMEM 7
#define MYLITE_MISUSE 21

MYLITE_API const char *mylite_version(void);

MYLITE_API int mylite_open_memory(mylite_db **out_db);
MYLITE_API void mylite_close(mylite_db *database);

MYLITE_API int mylite_errcode(const mylite_db *database);
MYLITE_API const char *mylite_sqlstate(const mylite_db *database);
MYLITE_API const char *mylite_errmsg(const mylite_db *database);

#ifdef __cplusplus
}
#endif

#endif
