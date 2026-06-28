# Baseline Internal Native Helper Rejections

## Scope

This slice covers MySQL internal metadata helper functions that are present in
MySQL's own data-dictionary and information-schema view definitions, but reject
direct SQL access in MySQL 8.4.9:

- `CAN_ACCESS_COLUMN()`
- `CAN_ACCESS_DATABASE()`
- `CAN_ACCESS_TABLE()`
- `CAN_ACCESS_USER()`
- `CAN_ACCESS_VIEW()`
- `GET_DD_COLUMN_PRIVILEGES()`
- `GET_DD_CREATE_OPTIONS()`
- `GET_DD_INDEX_SUB_PART_LENGTH()`
- `INTERNAL_AUTO_INCREMENT()`
- `INTERNAL_AVG_ROW_LENGTH()`
- `INTERNAL_CHECK_TIME()`
- `INTERNAL_CHECKSUM()`
- `INTERNAL_DATA_FREE()`
- `INTERNAL_DATA_LENGTH()`
- `INTERNAL_DD_CHAR_LENGTH()`
- `INTERNAL_GET_COMMENT_OR_ERROR()`
- `INTERNAL_GET_ENABLED_ROLE_JSON()`
- `INTERNAL_GET_HOSTNAME()`
- `INTERNAL_GET_USERNAME()`
- `INTERNAL_GET_VIEW_WARNING_OR_ERROR()`
- `INTERNAL_INDEX_COLUMN_CARDINALITY()`
- `INTERNAL_INDEX_LENGTH()`
- `INTERNAL_IS_ENABLED_ROLE()`
- `INTERNAL_IS_MANDATORY_ROLE()`
- `INTERNAL_KEYS_DISABLED()`
- `INTERNAL_MAX_DATA_LENGTH()`
- `INTERNAL_TABLE_ROWS()`
- `INTERNAL_UPDATE_TIME()`

The slice does not implement callable `ROLES_GRAPHML()`,
`STATEMENT_DIGEST()`, or `STATEMENT_DIGEST_TEXT()`; those functions have a
public callable MySQL surface and need separate semantics.

## Compatibility Sources

These functions are not documented as public SQL functions in the MySQL 8.4
manual. MySQL 8.4.9 runtime behavior is the compatibility authority for this
slice. Runtime probes verified that unqualified direct calls to the functions
listed above fail with:

- error code `3566`
- SQLSTATE `HY000`
- message `Access to native function '<NAME>' is rejected.`

The rejection happens before native-function arity validation: calls with zero,
too few, correct, or too many arguments all produce the same `3566 / HY000`
diagnostic.

The verification script for this slice is:

```sh
packages/libmylite/tests/mysql_baseline_internal_native_helper_rejections_expectations.sh
```

## Semantics

MyLite must recognize the unqualified names above in scalar expression
contexts and raise the same access-rejected diagnostic as MySQL. The diagnostic
must be name-only and must not evaluate function arguments, resolve column
references, or enforce a declared argument count first.

The diagnostic applies in no-source `SELECT`, `SELECT ... FROM DUAL`, `DO`,
and row-backed scalar contexts. MyLite does not need to expose the native
helpers to SQLite as registered scalar functions because they never produce a
value for user SQL.

Schema-qualified calls such as `mysql.CAN_ACCESS_TABLE(...)` do not use the
native-function path in MySQL; MySQL reports ordinary missing-function error
`1305 / 42000`. MyLite's current expression grammar does not yet parse
schema-qualified function calls, so that broader function-resolution surface is
tracked outside this direct-call rejection slice. It must not be treated as a
native helper access if qualified-call parsing is added later.

MyLite's information-schema and metadata implementation can continue to use
MyLite-owned catalog code rather than modeling MySQL's internal helper
execution. The compatibility surface for these helpers is the MySQL-observed
direct-call rejection.

## Parser, Storage, And SQLite Integration

No grammar changes are required. The existing generic-function AST can carry
the names and arguments.

No SQLite fork patch or public SQLite function registration is required. The
implementation is a MyLite scalar-expression diagnostic path:

```lemon
expression ::= generic_function_call.
generic_function_call ::= identifier LPAREN optional_expression_list RPAREN.
```

If the function name is an unqualified rejected internal native helper, runtime
diagnostics stop the statement with `3566 / HY000`.

## Tests

- Verify the expectation script syntax with `sh -n`.
- Run the expectation script against MySQL 8.4.9.
- Add a runtime test covering:
  - representative rejected helper names;
  - zero, too few, correct, and too many argument counts;
  - no-source, `DUAL`, `DO`, and table-backed scalar contexts.
- Run the focused runtime CTest.
- Run `git diff --check`, `git diff --cached --check`, formatting checks, and
  the full project check workflow before committing.
