# Baseline named lock and information functions

## Scope

This slice covers the MySQL 8.4.9 baseline behavior for:

- `GET_LOCK(name, timeout)`
- `IS_FREE_LOCK(name)`
- `IS_USED_LOCK(name)`
- `RELEASE_LOCK(name)`
- `RELEASE_ALL_LOCKS()`
- `ICU_VERSION()`
- `BENCHMARK(count, expr)`

`LOAD_FILE()`, `ExtractValue()`, and `UpdateXML()` remain out of scope. File I/O
needs an explicit embedded-security policy, and XML/XPath behavior is a
separate compatibility surface.

Normative documentation:

- https://dev.mysql.com/doc/refman/8.4/en/locking-functions.html
- https://dev.mysql.com/doc/refman/8.4/en/information-functions.html

Expected values are verified against MySQL 8.4.9 using
`packages/libmylite/tests/mysql_baseline_named_lock_and_info_functions_expectations.sh`.

## MySQL 8.4.9 Observations

Named locks are server-wide advisory locks. `GET_LOCK(name, timeout)` returns
`1` when the current session acquires the lock, `0` when timeout expires because
another session owns it, and `NULL` on exceptional errors. A session may hold
multiple lock names and may acquire the same name recursively; each
`RELEASE_LOCK(name)` decrements one held instance and returns `1` until the last
instance is released. After the lock no longer exists, `RELEASE_LOCK(name)`
returns `NULL`.

`IS_FREE_LOCK(name)` returns `1` for an existing free name and `0` while a lock
is held. `IS_USED_LOCK(name)` returns the owning connection id while held and
`NULL` otherwise. `RELEASE_ALL_LOCKS()` releases all lock instances owned by the
current session and returns the number of released instances.

MySQL rejects empty, `NULL`, or too-long lock names. Baseline MyLite supports
ASCII lock names with byte length 1..64 and reports deterministic diagnostics
for invalid names. The timeout argument supports integer-compatible values.
Timeout `0` attempts immediate acquisition, positive integer timeouts wait up
to that many seconds for another MyLite handle in the current process to release
the name, and negative integer timeouts wait indefinitely.

`ICU_VERSION()` returns the ICU version used by the target MySQL build. The
current MySQL 8.4.9 comparison runtime returns `77.1`, so MyLite returns that
target identity string without linking ICU into the embedded runtime.

`BENCHMARK(count, expr)` returns `0` for nonnegative counts, returns `NULL` for
`NULL` or negative counts, and emits warning `1411` for negative counts. MySQL
does not evaluate `expr` when `count` is zero; positive counts evaluate it.
MyLite supports scalar no-source, `DUAL`, and `DO` use where the expression is
already admitted by the scalar evaluator. It evaluates positive-count
expressions `count` times in those scalar contexts, preserving observable side
effects and warnings for admitted expressions, then returns the MySQL-shaped
result. Source-backed row-scalar `BENCHMARK()` remains a SQLite-callback
compatibility surface where SQLite evaluates the operand once before calling
MyLite's `_mylite_benchmark` callback.

## MyLite Design

### Named lock registry

MyLite stores named locks in a process-local registry guarded by SQLite's
static application mutex. Each entry records the lock name, owning connection
id, and recursive hold count.

This is a public SQLite extension API fit: MyLite uses SQLite mutex APIs and
registered scalar functions. Contended waits release the registry mutex, sleep
briefly through `sqlite3_sleep()`, and retry until the integer timeout expires.
No SQLite fork hook is needed.

Locks are released when a MyLite connection closes. They are not attached to
transactions and are not released by commit or rollback.

### Scalar and row execution

No-source scalar expressions call the same registry helpers directly from the
MyLite scalar evaluator. Source-backed row-scalar expressions lower supported
lock calls to MyLite-owned SQLite scalar callbacks:

```lemon
expr ::= ident LP function_argument_list RP.
function_argument_list ::= expr.
function_argument_list ::= function_argument_list COMMA expr.
```

The parser already admits these names as generic functions. The runtime
resolves them by case-insensitive name only at execution time.

### Diagnostics

Wrong argument counts use MySQL's native-function parameter-count diagnostic.
Invalid lock names return `3057 / 42000`; too-long names return `4163 / 42000`.
Unsupported timeout coercions or unsupported `BENCHMARK()` operands use MyLite's
existing unsupported-expression diagnostics.

## Compatibility Boundaries

- Named locks are process-local. Separate MyLite processes do not share the
  registry.
- Timeout coercion is currently integer-compatible; fractional timeout seconds
  are not preserved.
- Lock names are byte-counted ASCII in this baseline. Full character-set
  expression conversion and multibyte character counting remain out of scope.
- Performance Schema `metadata_locks` rows for user-level locks are not
  implemented.
- Statement-based replication warnings for locking functions are not emitted.
- Source-backed row-scalar `BENCHMARK()` does not repeatedly evaluate the
  argument; it is a compatibility acceptance and result-shape surface, not a
  timing harness.
