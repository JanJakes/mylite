# Baseline UUID Conversion Functions

## Summary

This phase adds a narrow deterministic UUID helper slice:

- `IS_UUID(expr)` validates MySQL string-format UUID values;
- `UUID_TO_BIN(expr[, swap_flag])` converts supported UUID text to
  `VARBINARY(16)` bytes;
- `BIN_TO_UUID(expr[, swap_flag])` converts supported 16-byte values to
  lowercase canonical UUID text.

`UUID()` and `UUID_SHORT()` generation remain outside this phase. They need a
separate design for process/session state, time source behavior, uniqueness
guarantees, replication warnings, and deterministic tests.

The supported execution envelope covers no-source scalar `SELECT`,
`SELECT ... FROM DUAL`, `DO`, and single-table row-scalar `SELECT` projection
contexts. The functions are not admitted in predicates, DML assignments,
defaults, generated columns, indexes, grouping, ordering expressions, joins,
subqueries, parameters, user variables, or arbitrary expression trees beyond
the existing scalar projection envelope.

## Sources And Evidence

- MyLite architecture and engineering standards:
  - `README.md`
  - `docs/architecture/engineering-standards.md`
  - `third_party/sqlite/README.md`
- Related baseline scalar-function slices:
  - `docs/specs/baseline-hex-function/specs.md`
  - `docs/specs/baseline-unhex-function/specs.md`
  - `docs/specs/baseline-json-valid-function/specs.md`
- Official MySQL 8.4 Reference Manual:
  - miscellaneous functions:
    <https://dev.mysql.com/doc/refman/8.4/en/miscellaneous-functions.html>
- MySQL runtime probes are captured in
  `packages/libmylite/tests/mysql_baseline_uuid_conversion_functions_expectations.sh`
  and verified against MySQL 8.4.9.

This specification is independently authored from project documentation,
official MySQL 8.4 documentation, observed MySQL 8.4.9 runtime behavior, public
SQLite APIs, and existing MyLite code. It does not copy MySQL, MariaDB,
Percona, SQLite implementation internals, or restrictively licensed sources.

## MySQL 8.4.9 Runtime Observations

Runtime probes establish the behavior used by this phase:

- each function accepts one or two arguments as documented below; zero
  arguments and too many arguments fail with native function count error
  `1582 / 42000`;
- `IS_UUID(NULL)` returns `NULL`, valid UUID strings return `1`, and invalid
  strings return `0`;
- `IS_UUID()` accepts lowercase or uppercase hexadecimal digits, canonical
  dashed form, 32-hex-digit form without dashes, and canonical dashed form
  wrapped in curly braces;
- `UUID_TO_BIN(NULL)` returns `NULL`;
- `UUID_TO_BIN()` accepts the same string UUID formats accepted by
  `IS_UUID()` and returns 16 bytes;
- invalid `UUID_TO_BIN()` input fails with `1411 / HY000` and an incorrect
  string value diagnostic for function `uuid_to_bin`;
- `UUID_TO_BIN('6ccd780c-baba-1026-9564-5b8c656024db')` and the two-argument
  form with `swap_flag = 0` produce bytes
  `6C CD 78 0C BA BA 10 26 95 64 5B 8C 65 60 24 DB`;
- the two-argument form with a nonzero `swap_flag` swaps the first and third
  UUID groups before returning bytes, producing
  `10 26 BA BA 6C CD 78 0C 95 64 5B 8C 65 60 24 DB` for the value above;
- `BIN_TO_UUID(NULL)` returns `NULL`;
- `BIN_TO_UUID()` requires the converted input to be exactly 16 bytes;
  shorter or longer byte strings fail with `1411 / HY000` and an incorrect
  string value diagnostic for function `bin_to_uuid`;
- `BIN_TO_UUID(X'6CCD780CBABA102695645B8C656024DB')` returns
  `6ccd780c-baba-1026-9564-5b8c656024db`;
- the two-argument `BIN_TO_UUID()` form with nonzero `swap_flag` swaps the
  first and third UUID groups back into string order;
- mismatched swap flags do not error; they return the correspondingly
  rearranged UUID text, such as
  `baba1026-780c-6ccd-9564-5b8c656024db`;
- `swap_flag = NULL` behaves as false in observed MySQL 8.4.9 behavior;
- numeric nonzero swap flags behave as true and zero behaves as false;
- string and approximate numeric swap flag coercion is broader in MySQL and can
  produce truncation warnings. MyLite defers those coercions in this baseline
  and supports only integer, boolean, and `NULL` flag values.

## Ownership Boundaries

- Public API: unchanged. Text results use the existing text accessors. Binary
  `UUID_TO_BIN()` results are byte-safe and callers should use
  `mylite_result_value_bytes()` and `mylite_result_value_size()` when embedded
  NUL bytes are possible.
- Statement context: owns diagnostics, warning count, affected-row state, and
  result finalization. Supported in-range calls emit no warnings.
- Lexer/parser/AST: admits the three function names with one- and two-argument
  forms, wrong-arity marker nodes, source spans for labels, and identifier
  fallback for nonreserved function keywords.
- Analyzer/planner: admits the functions only in the current scalar projection
  and row-scalar projection envelopes. Table-backed descriptor columns are
  resolved through MyLite catalog descriptors before generated SQLite SQL is
  built.
- Catalog: read-only. UUID conversion functions do not mutate catalog rows,
  descriptor versions, descriptor caches, catalog generation, or
  `sqlite_schema_generation`.
- SQLite execution: row-backed projection lowers to generated SQLite SQL that
  calls private MyLite scalar helpers registered through SQLite's public scalar
  function API. No SQLite fork patch is required.
- Result builder: returns scalar rows using the existing result conventions and
  byte-safe binary value storage for `UUID_TO_BIN()`.
- Storage/VFS/file format: row reads only. The `.mylite` preamble and shifted
  SQLite payload invariants are unchanged.

## Supported SQL

No-source and `DUAL` forms:

```sql
SELECT uuid_item[, uuid_item ...]
SELECT uuid_item[, uuid_item ...] FROM DUAL
```

`DO` form:

```sql
DO uuid_expr[, uuid_expr ...]
```

Single-table row-backed forms, with at least one select item containing a UUID
conversion function:

```sql
SELECT row_scalar_item[, row_scalar_item ...]
FROM table_name [AS alias]
[WHERE predicate]
[ORDER BY descriptor_column [ASC | DESC]]
[LIMIT row_count]
```

The admitted expression shapes are:

```sql
uuid_expr:
    IS_UUID ( uuid_string_value )
  | UUID_TO_BIN ( uuid_string_value )
  | UUID_TO_BIN ( uuid_string_value , uuid_swap_flag )
  | BIN_TO_UUID ( uuid_binary_value )
  | BIN_TO_UUID ( uuid_binary_value , uuid_swap_flag )

uuid_string_value:
    string_literal
  | hex_literal
  | decimal_integer_literal
  | + decimal_integer_literal
  | - decimal_integer_literal
  | TRUE
  | FALSE
  | NULL
  | descriptor_column_reference        -- table-backed SELECT only
  | supported_scalar_function_result
  | ( uuid_string_value )

uuid_binary_value:
    string_literal
  | hex_literal
  | NULL
  | descriptor_column_reference        -- table-backed SELECT only
  | supported_binary_scalar_function_result
  | ( uuid_binary_value )

uuid_swap_flag:
    decimal_integer_literal
  | + decimal_integer_literal
  | - decimal_integer_literal
  | TRUE
  | FALSE
  | NULL
  | ( uuid_swap_flag )
```

`descriptor_column_reference` follows the existing single-source table alias
policy and may explicitly name invisible descriptor columns. Supported
descriptor column families for this phase are:

- `CHAR`, `VARCHAR`, and baseline `TEXT` family;
- `BINARY`, `VARBINARY`, and baseline `BLOB` family;
- integer-family columns in scalar string contexts, where MySQL observes their
  visible decimal text before UUID validation/conversion.

The following remain outside this phase:

- `UUID()` and `UUID_SHORT()`;
- UUID functions in `WHERE`, `HAVING`, `ORDER BY`, `GROUP BY`, aggregate
  arguments, distinct expression rows, DML assignments, generated columns,
  defaults, indexes, and constraints;
- string or approximate numeric swap flag coercion and its warnings;
- user variables, parameters, stored functions, CTEs, subqueries, correlated
  subqueries, and arbitrary nested row expressions beyond existing scalar
  projection support;
- character-set conversion metadata, collation-sensitive behavior, protocol
  field metadata parity, and replication/binlog warnings.

### MyLite Lemon-Syntax Snippet

The parser grammar is authored independently and extends MyLite's expression
grammar:

```lemon
expression(A) ::= IS_UUID(T) LPAREN expression(B) RPAREN(R). {
    A = mylite_sql_parser_make_one_argument_function(
        state, T, MYLITE_SQL_AST_IS_UUID_FUNCTION, B, R);
}
expression(A) ::= IS_UUID(T) LPAREN RPAREN(R). {
    A = mylite_sql_parser_make_function_argument_count_error(
        state, T, MYLITE_SQL_AST_IS_UUID_ARGUMENT_COUNT_ERROR, NULL, R);
}
expression(A) ::= IS_UUID(T) LPAREN expression(B) COMMA function_argument_list(C) RPAREN(R). {
    (void)B;
    A = mylite_sql_parser_make_function_argument_count_error(
        state, T, MYLITE_SQL_AST_IS_UUID_ARGUMENT_COUNT_ERROR, C, R);
}

expression(A) ::= UUID_TO_BIN(T) LPAREN expression(B) RPAREN(R). {
    A = mylite_sql_parser_make_one_argument_function(
        state, T, MYLITE_SQL_AST_UUID_TO_BIN_FUNCTION, B, R);
}
expression(A) ::= UUID_TO_BIN(T) LPAREN expression(B) COMMA expression(C) RPAREN(R). {
    A = mylite_sql_parser_make_two_argument_function(
        state, T, MYLITE_SQL_AST_UUID_TO_BIN_FUNCTION, B, C, R);
}

expression(A) ::= BIN_TO_UUID(T) LPAREN expression(B) RPAREN(R). {
    A = mylite_sql_parser_make_one_argument_function(
        state, T, MYLITE_SQL_AST_BIN_TO_UUID_FUNCTION, B, R);
}
expression(A) ::= BIN_TO_UUID(T) LPAREN expression(B) COMMA expression(C) RPAREN(R). {
    A = mylite_sql_parser_make_two_argument_function(
        state, T, MYLITE_SQL_AST_BIN_TO_UUID_FUNCTION, B, C, R);
}
```

Each function also has zero-argument and three-or-more-argument productions
that build function-specific native argument-count marker nodes.

The function names are nonreserved function keywords and remain available as
identifiers where MyLite's current identifier productions admit function
keywords.

## Runtime Semantics

### UUID String Parsing

MyLite parses UUID input bytes without calling platform UUID libraries. Accepted
string forms are:

- canonical dashed form: `8-4-4-4-12` hexadecimal digits;
- 32 hexadecimal digits without dashes;
- canonical dashed form enclosed in one leading `{` and one trailing `}`.

Hexadecimal letters may use either case. Output text always uses lowercase
hexadecimal letters and canonical dashes.

### `IS_UUID()`

1. Convert the supported argument to visible input bytes.
2. Return SQL `NULL` if the argument is `NULL`.
3. Return integer text `1` if the bytes are an accepted UUID string form.
4. Return integer text `0` otherwise.

Invalid input is not a warning or error for `IS_UUID()`.

### `UUID_TO_BIN()`

1. Convert the first supported argument to visible input bytes.
2. Convert the optional `swap_flag`: `NULL` and zero are false, nonzero integer
   values are true.
3. Return SQL `NULL` if the UUID argument is `NULL`.
4. Parse the UUID string. If parsing fails, return `1411 / HY000` with function
   name `uuid_to_bin`.
5. If `swap_flag` is false, return bytes in canonical group order.
6. If `swap_flag` is true, swap the first and third UUID groups before
   returning bytes.

### `BIN_TO_UUID()`

1. Convert the first supported argument to input bytes.
2. Convert the optional `swap_flag`: `NULL` and zero are false, nonzero integer
   values are true.
3. Return SQL `NULL` if the binary argument is `NULL`.
4. Require exactly 16 input bytes. If the length differs, return
   `1411 / HY000` with function name `bin_to_uuid`.
5. If `swap_flag` is true, swap the first and third UUID groups before
   formatting.
6. Return lowercase canonical UUID text.

## Diagnostics

Supported diagnostics:

- zero arguments and too many arguments for each function:
  `1582 / 42000 / Incorrect parameter count in the call to native function ...`;
- invalid `UUID_TO_BIN()` string:
  `1411 / HY000 / Incorrect string value: '<value>' for function uuid_to_bin`;
- `BIN_TO_UUID()` input length other than 16 bytes:
  `1411 / HY000 / Incorrect string value: '<value>' for function bin_to_uuid`;
- unsupported argument expression or context: existing deterministic MyLite
  unsupported scalar-expression diagnostics, updated to name UUID functions;
- unsupported swap flag coercion: deterministic MyLite unsupported diagnostic;
- allocation or formatting failures: existing `MYLITE_NOMEM` or runtime error
  paths;
- public API misuse: unchanged existing `mylite_execute()` behavior.

Successful supported calls produce `warning_count == 0`.

## SQLite Integration

No-source, `DUAL`, and `DO` evaluation is fully MyLite-owned and does not call
SQLite for the conversion. Table-backed row-scalar projection uses SQLite only
to scan rows and invoke private scalar helpers over descriptor-resolved physical
column values. The helpers are registered with `sqlite3_create_function_v2()`
and keep the conversion rules in first-party MyLite code.

No SQLite syntax extension, optional SQLite compile-time feature, or SQLite
fork patch is required.

## Tests

Add MySQL-runtime expectations for:

- MySQL 8.4.9 version guard;
- accepted `IS_UUID()` forms, rejected forms, `NULL`, integer/boolean, and
  binary inputs;
- `UUID_TO_BIN()` accepted UUID string forms, lowercase/uppercase input,
  `NULL`, invalid string errors, and swap flag `0`, `1`, `+1`, `-1`, and
  `NULL`;
- `BIN_TO_UUID()` 16-byte inputs, lowercase output, string-literal byte input,
  `NULL`, wrong-length errors, and swap flag `0`, `1`, `+1`, `-1`, and `NULL`;
- no-source scalar, `FROM DUAL`, `DO`, nested conversion, and row-scalar table
  projection behavior;
- native function-count errors;
- successful calls with `@@warning_count = 0`.

Add C tests for:

- parser AST shapes and identifier fallback for the three function names;
- no-source scalar and `DUAL` result values, binary result bytes, labels,
  warning counts, affected rows, and `DO` no-result behavior;
- table-backed row-scalar values over `VARCHAR`, `VARBINARY`, `BINARY`, `TEXT`,
  `BLOB`, nullable values, ordered/limited row envelopes, and reopen
  persistence;
- deterministic diagnostics for invalid UUID strings, wrong binary lengths,
  wrong arity, unsupported swap flag expressions, and unsupported contexts;
- `.mylite` preamble preservation and no catalog generation mutation for
  supported row reads.

## Compatibility Documentation

Update `docs/compatibility/functions-uuid.md` to mark `IS_UUID()`,
`UUID_TO_BIN()`, and `BIN_TO_UUID()` as limited. Keep `UUID()` and
`UUID_SHORT()` unsupported until a generation-state slice exists. Do not claim
full expression placement, UUID generation, broad swap flag coercion,
protocol-grade binary metadata, collation behavior, defaults, generated
columns, or replication semantics.
