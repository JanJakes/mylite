# Baseline SOUNDEX Function

## Summary

This phase adds a narrow MySQL-compatible `SOUNDEX(str)` string function slice.
The supported surface covers no-source scalar `SELECT`, `SELECT ... FROM DUAL`,
`DO`, and single-table row-scalar `SELECT` projection. The follow-up
`baseline-sounds-like-operator` slice adds limited `expr SOUNDS LIKE expr`
support over the current `SOUNDEX()` operand envelope.

Core supported behavior:

- `SOUNDEX(str)` accepts exactly one argument;
- `NULL` input returns SQL `NULL`;
- the result is a text value;
- ASCII lowercase leading letters are uppercased in the result;
- ASCII nonletters before the first significant character are ignored;
- consonant code duplicates are collapsed across ignored or vowel characters;
- results are padded with zeros to at least four characters, but are not
  truncated to four characters;
- successful supported calls produce `warning_count == 0`.

## Sources And Evidence

- MyLite architecture and engineering standards:
  - `README.md`
  - `docs/architecture/engineering-standards.md`
- Existing scalar and row-scalar expression slices:
  - `docs/specs/baseline-reverse-function/specs.md`
  - `docs/specs/baseline-string-case-functions/specs.md`
  - `docs/specs/baseline-regexp-string-functions/specs.md`
- Official MySQL 8.4 Reference Manual:
  - string functions and operators:
    <https://dev.mysql.com/doc/refman/8.4/en/string-functions.html#function_soundex>
- MySQL runtime probes are captured in
  `packages/libmylite/tests/mysql_baseline_soundex_function_expectations.sh`
  and verified against MySQL 8.4.9.

This specification is independently authored from project documentation,
official MySQL 8.4 documentation, observed MySQL 8.4.9 runtime behavior, public
SQLite APIs, and existing MyLite code. It does not copy MySQL, MariaDB,
Percona, SQLite implementation internals, or restrictively licensed sources.

## MySQL 8.4.9 Runtime Observations

Runtime probes establish the behavior used by this phase:

- `SOUNDEX()` requires exactly one argument; zero or more than one argument
  produces `1582 / 42000` for an incorrect parameter count;
- function-name whitespace such as `SOUNDEX ('abc')` is accepted;
- `SOUNDEX(NULL)` returns SQL `NULL`;
- `SOUNDEX('Hello') = 'H400'`;
- `SOUNDEX('Quadratically') = 'Q36324'`, showing that MySQL does not truncate
  to the traditional four-character code;
- `SOUNDEX('Robert')` and `SOUNDEX('Rupert')` both return `R163`;
- `SOUNDEX('Ashcraft') = 'A2613'` and `SOUNDEX('Pfister') = 'P236'`;
- empty strings, strings containing only ASCII digits or punctuation, and
  numeric/boolean scalar values converted to those strings return the empty
  string;
- leading ASCII digits, punctuation, and spaces are ignored before selecting
  the first significant character, so `SOUNDEX('1abc')`,
  `SOUNDEX('-abc')`, and `SOUNDEX('  abc')` all return `A120`;
- later ASCII nonletters, vowels, `H`, and `W` do not emit codes and do not
  reset duplicate-code suppression, so `SOUNDEX('BAB')`,
  `SOUNDEX('BHB')`, and `SOUNDEX('BWB')` all return `B000`;
- MySQL documents that multibyte character sets are not guaranteed to produce
  stable results. The observed MySQL 8.4.9 behavior for this baseline keeps a
  leading non-ASCII character as the result prefix and treats later non-ASCII
  characters as no-code separators, for example `SOUNDEX('éclair') = 'é246'`
  and `SOUNDEX('🙂bcd') = '🙂123'`;
- successful supported calls make a scalar `SELECT` report `ROW_COUNT() = -1`
  and a `DO` statement report `ROW_COUNT() = 0`, both with zero warnings.

MySQL also supports broader expression placement and collation-dependent
details outside this baseline. Those forms remain deferred.

## Ownership Boundaries

- Public API: unchanged. Results are exposed through the existing public result
  object as text values or SQL `NULL`.
- Statement context: owns diagnostics, warning count, affected-row state, and
  result finalization. Supported calls add no warnings.
- Lexer/parser/AST: admits exact one-argument `SOUNDEX()` expressions and
  wrong-arity nodes for MySQL-compatible native-function argument-count
  diagnostics.
- Analyzer/planner: resolves row-backed descriptor columns from MyLite catalog
  descriptors and rejects unsupported expression shapes before generated
  SQLite SQL is built.
- Catalog: read-only for table and column descriptors. No catalog rows,
  descriptor versions, descriptor caches, catalog generation, or
  `sqlite_schema_generation` are mutated.
- SQLite execution: table-backed projections are lowered to a private
  `_mylite_soundex()` scalar helper over generated SQLite SQL. The helper is
  registered through SQLite's public scalar-function API. No SQLite fork patch
  is required.
- Result builder: uses existing row result conventions and result labels from
  source spans or explicit aliases.
- Storage/VFS/file format: read-only row access only. The `.mylite` preamble
  and shifted SQLite payload invariants are unchanged.

## Supported SQL

No-source and `DUAL` forms:

```sql
SELECT soundex_item[, soundex_item ...]
SELECT soundex_item[, soundex_item ...] FROM DUAL
```

`DO` form:

```sql
DO soundex_expr[, soundex_expr ...]
```

Single-table row-backed forms, with at least one select item containing
`SOUNDEX()`:

```sql
SELECT row_scalar_item[, row_scalar_item ...]
FROM table_name [AS alias]
[WHERE predicate]
[ORDER BY descriptor_column [ASC | DESC]]
[LIMIT row_count]
```

The admitted expression shape is:

```sql
soundex_expr:
    SOUNDEX ( soundex_value )

soundex_value:
    string_literal
  | decimal_integer_literal
  | + decimal_integer_literal
  | - decimal_integer_literal
  | TRUE
  | FALSE
  | NULL
  | session_scalar_function
  | system_variable_reference
  | descriptor_column_reference        -- table-backed SELECT only
  | ( soundex_value )
```

`descriptor_column_reference` follows the existing single-source table alias
policy and may explicitly name invisible descriptor columns. Supported
descriptor column families for the string argument are:

- integer-family columns;
- exact `DECIMAL`;
- `YEAR`, `DATE`, `TIME`, `DATETIME`, and `TIMESTAMP`;
- `CHAR`, `VARCHAR`, and baseline `TEXT` family.

The following remain outside this phase:

- `WHERE SOUNDEX(column) ...`, `HAVING SOUNDEX(...) ...`, expression
  `ORDER BY`, grouping, distinct expression rows, and aggregate arguments;
- DML assignment values such as `UPDATE t SET c = SOUNDEX(v)`;
- scalar subqueries, correlated subqueries, CTEs, joins beyond the already
  supported row-scalar source envelope, parameters, user variables, and stored
  functions;
- arbitrary expressions outside the supported nested row-scalar value subset;
- binary-string result typing, binary/BLOB row-backed inputs, approximate
  numeric row-backed inputs, full collation behavior, and full expression
  metadata.

### MyLite Lemon-Syntax Snippet

The parser grammar is authored independently and extends MyLite's expression
grammar:

```lemon
expression(A) ::= SOUNDEX(T) LPAREN expression(B) RPAREN(R). {
    A = mylite_sql_parser_make_one_argument_function(
        state, T, MYLITE_SQL_AST_SOUNDEX_FUNCTION, B, R);
}
expression(A) ::= SOUNDEX(T) LPAREN RPAREN(R). {
    A = mylite_sql_parser_make_function_argument_count_error(
        state, T, MYLITE_SQL_AST_SOUNDEX_ARGUMENT_COUNT_ERROR, NULL, R);
}
expression(A) ::= SOUNDEX(T) LPAREN expression(B) COMMA function_argument_list(C) RPAREN(R). {
    (void)B;
    A = mylite_sql_parser_make_function_argument_count_error(
        state, T, MYLITE_SQL_AST_SOUNDEX_ARGUMENT_COUNT_ERROR, C, R);
}
```

## Runtime Semantics

No-source, `DUAL`, and `DO` evaluation is MyLite-owned:

1. Unwrap supported parentheses.
2. Convert the argument to a text value:
   - ordinary string literal: decoded UTF-8 bytes, provided they are NUL-free;
   - integer literal: canonical signed decimal text;
   - `TRUE` / `FALSE`: `1` / `0`;
   - `NULL`: SQL `NULL`;
   - supported session scalar value or system variable: its existing visible
     string value or SQL `NULL`.
3. Return SQL `NULL` when the converted argument is SQL `NULL`.
4. Skip leading ASCII bytes that are not letters.
5. If no significant character remains, return `''`.
6. Use the first significant character as the result prefix. ASCII lowercase
   letters are uppercased. A leading non-ASCII UTF-8 character is preserved as
   its original byte sequence to match observed MySQL 8.4.9 behavior.
7. Track the Soundex class of the prefix when it is an ASCII letter. Later
   ASCII letters append their class only when it is nonzero and different from
   the last nonzero class. Nonletters, vowels, `H`, `W`, and non-ASCII
   characters emit no class and do not reset the duplicate-class state.
8. Pad with ASCII `0` until the result contains at least the prefix plus three
   digits. Do not truncate longer results.

The class mapping is the observed original Soundex class mapping used by MySQL
for ASCII letters:

- `B`, `F`, `P`, `V`: `1`
- `C`, `G`, `J`, `K`, `Q`, `S`, `X`, `Z`: `2`
- `D`, `T`: `3`
- `L`: `4`
- `M`, `N`: `5`
- `R`: `6`
- all other ASCII letters: no emitted class

Table-backed projection planning lowers to:

```sql
_mylite_soundex(value_expr)
```

`value_expr` is either a quoted descriptor column or a bound scalar value. The
builder binds all scalar values through prepared-statement parameters.
Generated identifiers are always quoted.

## Diagnostics

Supported calls succeed with `warning_count == 0`.

Diagnostics:

- Wrong arity: `1582 / 42000` native-function parameter-count diagnostic for
  `SOUNDEX`.
- Syntax errors and unsupported grammar: existing parse diagnostic.
- Unknown descriptor columns: existing unknown-column diagnostic.
- Unsupported argument expression: deterministic MyLite unsupported diagnostic.
- Unsupported row-backed descriptor column family: deterministic MyLite
  unsupported diagnostic.
- Invalid UTF-8 in admitted text: deterministic runtime error.
- Allocation failure: existing MyLite allocation failure diagnostic.
- SQLite callback misuse or physical execution failure: existing physical
  SQLite failure path.

## Tests

The implementation must add:

- MySQL-runtime expectation script for the exact supported behavior;
- parser coverage for exact one-argument calls, whitespace, identifiers, `DO`,
  and wrong-arity nodes;
- C runtime coverage for no-source/`DUAL`/`DO` calls, NULLs, empty and
  no-significant-character inputs, ASCII duplicate suppression, unbounded
  output length, observed leading non-ASCII behavior, row-backed descriptor
  projection, labels, filters/order/limit envelope, close/reopen persistence,
  preamble preservation, and deterministic diagnostics;
- CMake integration with a dotted CTest name.

## Compatibility Documentation

`COMPATIBILITY.md` and `docs/compatibility/functions-string.md` must move
`SOUNDEX()` from unsupported to limited support, explicitly leaving general
expression placement, binary metadata parity, and full collation behavior
unsupported.
