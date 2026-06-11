# Baseline REGEXP String Functions

## Scope

This phase adds a limited, descriptor-safe baseline for MySQL's string-valued
regular-expression helper functions:

```sql
REGEXP_INSTR(expr, pat)
REGEXP_SUBSTR(expr, pat)
REGEXP_REPLACE(expr, pat, repl)
```

The supported surface is deliberately narrow:

- no-source `SELECT`, `SELECT ... FROM DUAL`, and `DO` scalar execution;
- single persistent or temporary descriptor-backed table `SELECT` projections;
- scalar string, integer, boolean, `NULL`, admitted session scalar, and
  descriptor column arguments where the existing string-scalar expression
  pipeline supports them;
- nonbinary descriptor-backed integer, exact `DECIMAL`, string, baseline `TEXT`,
  `YEAR`, and temporal columns through the existing text conversion helpers;
- MyLite's existing baseline ASCII regular-expression language;
- MySQL's default case-insensitive matching for this baseline;
- `REGEXP_REPLACE()` replaces all matches, matching MySQL's default occurrence
  behavior.

Deferred surfaces include optional `pos` values other than the source-free
scalar/DML `1` subset, `occurrence`, `return_option`, and `match_type`
arguments; binary-string regex semantics; non-ASCII collations; nested
functions beyond already admitted scalar/session helpers; predicates; DML
assignments outside the constant descriptor-backed value subset;
ordering/grouping expressions; subqueries except already admitted
no-source/`DUAL` scalar subqueries; parameters; and arbitrary expressions.

The official MySQL 8.4 regular-expression function documentation is the syntax
reference, with behavior pinned by local MySQL 8.4.9 runtime probes:

- <https://dev.mysql.com/doc/refman/8.4/en/regexp.html>
- `docker exec mylite-mysql-849 mysql -uroot --batch --raw --skip-column-names`

## Ownership

The public API does not change. `mylite_execute()` keeps returning the existing
result object shape for `SELECT`/`DO`.

The parser owns recognition of the three function names and required-argument
forms. Unsupported argument counts parse to deterministic AST nodes so the
runtime can return function-specific diagnostics rather than a generic syntax
failure.

The analyzer/planner owns descriptor resolution for table-backed projection
arguments. Descriptor metadata remains authoritative for deciding whether a
column can be converted to text for regex evaluation; SQLite metadata is not
consulted.

The runtime owns scalar evaluation, result metadata, and translation of
table-backed row-scalar expressions to SQLite. Generated SQL calls private
MyLite scalar helpers and binds non-column arguments as parameters.

The regex module owns compilation, matching, match-span discovery, substring
extraction, and replacement. It is registered with SQLite through the public
`sqlite3_create_function_v2()` path used by the existing private regex
predicates. No SQLite fork patch is required.

Storage and the MyLite file format are unaffected. These functions read row
values already stored in SQLite physical tables and do not mutate catalog rows,
descriptor versions, descriptor caches, `.mylite` preambles, or SQLite schema
text.

## Grammar

The MyLite Lemon grammar extensions are independently authored for the admitted
subset:

```lemon
expression(A) ::= REGEXP_INSTR(T) LPAREN function_argument_list(B) RPAREN(R).
expression(A) ::= REGEXP_SUBSTR(T) LPAREN function_argument_list(B) RPAREN(R).
expression(A) ::= REGEXP_REPLACE(T) LPAREN function_argument_list(B) RPAREN(R).

expression(A) ::= REGEXP_INSTR(T) LPAREN RPAREN(R).
expression(A) ::= REGEXP_SUBSTR(T) LPAREN RPAREN(R).
expression(A) ::= REGEXP_REPLACE(T) LPAREN RPAREN(R).
```

The non-empty argument-list productions create function AST nodes. The zero
argument productions create argument-count error AST nodes. Runtime validation
admits exactly two arguments for `REGEXP_INSTR()` and `REGEXP_SUBSTR()`, plus
the source-free scalar/DML `REGEXP_SUBSTR(expr, pat, 1)` subset, and exactly
three for `REGEXP_REPLACE()`; longer optional-argument forms are parsed so they
can return deterministic unsupported diagnostics until the optional slice is
implemented.

## Semantics

Arguments are evaluated left to right by the existing scalar expression
helpers. If any supported argument is `NULL`, the function result is `NULL`.
For `REGEXP_REPLACE()`, a `NULL` replacement also yields `NULL`.

Patterns use MyLite's current baseline ASCII regex subset: literal ASCII bytes,
`.`, `^`, `$`, bracket classes and ranges, bracket negation, escaped literals
for supported metacharacters, and `?`, `*`, `+`. Unsupported syntax such as
groups, alternation, counted repetition, shorthand classes, backreferences,
lookarounds, non-ASCII, embedded `NUL`, and too-large patterns fail
deterministically. Empty patterns are rejected for these functions, matching
the observed MySQL 8.4.9 `3685 / HY000` behavior.

Matching is leftmost and greedy within the admitted regex language. Positions
reported by `REGEXP_INSTR()` are 1-based byte/character positions in this ASCII
baseline. No match returns `0`.

`REGEXP_SUBSTR()` returns the matched text, including the empty string for
zero-length matches such as `$`. No match returns `NULL`.

`REGEXP_REPLACE()` replaces every non-overlapping match. For zero-length
matches, it inserts the replacement at the match position and advances by one
input byte when possible, matching the observed MySQL shape for patterns such
as `a*`. Empty input strings remain empty even when the pattern can match a
zero-length span.

## Metadata

`REGEXP_INSTR()` returns a nullable signed 64-bit numeric result for this
baseline. MySQL reports an integer-compatible numeric result; MyLite uses the
existing scalar numeric descriptor conventions.

`REGEXP_SUBSTR()` and `REGEXP_REPLACE()` return nullable nonbinary string
results using the current scalar string result descriptor conventions. This
phase does not expose full charset, collation, coercibility, maximum length, or
binary-string protocol parity.

Supported successful executions produce `warning_count == 0`.

## Diagnostics

Supported wrong arities return the existing native-function parameter-count
diagnostic for the corresponding function name.

Unsupported optional argument forms return deterministic MyLite-specific
unsupported diagnostics until the optional-argument slice is implemented.
`REGEXP_SUBSTR(expr, pat, 1)` is accepted in source-free scalar and supported
DML value contexts because it is equivalent to MySQL's default starting
position. A `NULL` third argument returns `NULL`.

Pattern and value diagnostics follow the existing baseline regex module:

- allocation failure returns `MYLITE_NOMEM` or SQLite `nomem`;
- unsupported regex syntax returns a deterministic MyLite unsupported error;
- unclosed brackets and invalid ranges preserve the existing MySQL-compatible
  regex diagnostic codes used by baseline regex predicates;
- empty regex patterns for these functions return `3685 / HY000`;
- non-ASCII or embedded-`NUL` values return deterministic MyLite unsupported
  diagnostics;
- physical SQLite callback failures are surfaced through the owning MyLite
  diagnostics when available.

## MySQL 8.4.9 Runtime Evidence

Representative probes:

```sql
SELECT REGEXP_INSTR('abcabc', 'b');           -- 2
SELECT REGEXP_SUBSTR('abcabc', 'b.');         -- 'bc'
SELECT REGEXP_REPLACE('abcabc', 'b.', 'X');   -- 'aXaX'
SELECT REGEXP_INSTR('abc', 'z');              -- 0
SELECT REGEXP_SUBSTR('abc', 'z');             -- NULL
SELECT REGEXP_REPLACE('abc', 'z', 'X');       -- 'abc'
SELECT REGEXP_INSTR('AbC', 'a');              -- 1
SELECT REGEXP_SUBSTR('AbC', 'a');             -- 'A'
SELECT REGEXP_REPLACE('AbC', 'a', 'x');       -- 'xbC'
SELECT REGEXP_SUBSTR('abc', '$');             -- ''
SELECT REGEXP_REPLACE('abc', '$', 'X');       -- 'abcX'
SELECT REGEXP_REPLACE('', 'a*', 'X');         -- ''
```

`REGEXP_INSTR(NULL, 'a')`, `REGEXP_INSTR('a', NULL)`,
`REGEXP_SUBSTR(NULL, 'a')`, `REGEXP_SUBSTR('a', NULL)`,
`REGEXP_REPLACE(NULL, 'a', 'x')`, `REGEXP_REPLACE('a', NULL, 'x')`, and
`REGEXP_REPLACE('a', 'a', NULL)` all return `NULL`.

Empty patterns for all three functions return `3685 / HY000` with message
`Illegal argument to a regular expression.`

## Tests

The C runtime test covers:

- scalar `SELECT`, `DUAL`, and `DO` forms;
- descriptor-backed row-scalar projection over integer and string/text columns;
- no-match, case-insensitive match, greedy match, zero-length match, and
  replacement-all behavior;
- `NULL` propagation;
- empty-pattern diagnostics;
- unsupported optional arities;
- warning count and result metadata shape;
- close/reopen persistence of source table rows to prove functions are read-only.

The MySQL expectation script records the MySQL 8.4.9 result and error behavior
used by the test plan.
