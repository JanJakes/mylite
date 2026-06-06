# Baseline REGEXP / RLIKE Predicates

## Summary

This phase adds a narrow descriptor-backed `REGEXP` / `RLIKE` predicate slice
for existing single-table row envelopes. It targets common WordPress-style
filters such as:

```sql
DELETE FROM _options WHERE option_name REGEXP '^rss_.+$'
```

The supported surface is deliberately smaller than MySQL's full ICU regular
expression engine. MyLite accepts one descriptor string column on the left and
one ordinary ASCII string literal pattern on the right. The pattern is validated
and bound by MyLite, and SQLite executes the physical row scan using a
registered MyLite scalar function. MyLite does not materialize table rows in C
to evaluate this predicate.

## Sources And Evidence

- MyLite architecture and standards:
  - `README.md`
  - `docs/architecture/engineering-standards.md`
  - `third_party/sqlite/README.md`
- Current predicate and string slices:
  - `docs/specs/baseline-like-predicates/specs.md`
  - `docs/specs/baseline-string-equality-predicates/specs.md`
  - `docs/specs/baseline-where-and-predicates/specs.md`
- Compatibility docs:
  - `COMPATIBILITY.md`
  - `docs/compatibility/sql-query-expressions.md`
  - `docs/compatibility/sql-table-dml.md`
  - `docs/compatibility/operators.md`
  - `docs/compatibility/functions-string.md`
- Official MySQL 8.4 Reference Manual:
  - regular expressions:
    <https://dev.mysql.com/doc/refman/8.4/en/regexp.html>
  - string comparison functions and operators:
    <https://dev.mysql.com/doc/refman/8.4/en/string-comparison-functions.html>
  - string literals:
    <https://dev.mysql.com/doc/refman/8.4/en/string-literals.html>
- MySQL runtime probes are captured in
  `packages/libmylite/tests/mysql_baseline_regexp_rlike_predicates_expectations.sh`
  and verified against MySQL 8.4.9.

This specification is independently authored from project documentation,
official MySQL 8.4 documentation, observed MySQL 8.4.9 runtime behavior, public
SQLite APIs, and existing MyLite code. It does not copy MySQL, MariaDB,
Percona, SQLite implementation internals, or restrictively licensed sources.

## Runtime Observations

MySQL 8.4.9 behavior used for this slice:

- `expr REGEXP pat` and `expr RLIKE pat` are synonyms.
- `expr NOT REGEXP pat` and `expr NOT RLIKE pat` behave as
  `NOT (expr REGEXP pat)` and `NOT (expr RLIKE pat)`.
- `REGEXP` returns `1` for a match, `0` for no match, and `NULL` when either
  operand is `NULL`.
- `NOT REGEXP` does not match `NULL` rows in `WHERE`.
- Under the default `utf8mb4_0900_ai_ci` collation, ASCII matching is
  case-insensitive for the verified subset.
- `CHAR` values follow MySQL's trimmed readback/comparison shape in the
  verified subset; a `CHAR(8)` value inserted as `abc  ` matches `^abc$`.
- The verified baseline pattern atoms include ordinary ASCII literals, `.`,
  `^`, `$`, bracket classes/ranges, negated bracket classes, the `*`, `+`, and
  `?` quantifiers, fixed `{n}` repetition, one-level optional literal groups
  such as `(_ts)?`, and top-level alternation such as
  `administrator|editor|author`.
- Without match-control flags, `.` does not match line terminators.
- SQL string-literal decoding happens before regular-expression matching. To
  pass a literal regex backslash in default SQL mode, the SQL text must contain
  the usual doubled SQL backslash sequence.
- Invalid regular expressions produce MySQL errors verified for the admitted
  subset: `3696 / HY000` for malformed bracket expressions and `3697 / HY000`
  for invalid character ranges.
- Successful supported statements report warning count `0`.

## Ownership Boundaries

- Public API: unchanged. `mylite_execute()` and result ownership conventions do
  not change.
- Statement context: no new public statement state. Supported predicates share
  existing diagnostics, warning-count, affected-row, and result conventions.
- Lexer/parser/AST: `REGEXP` and `RLIKE` are already lexer keywords. This phase
  adds explicit predicate grammar and AST operators. `NOT REGEXP` and
  `NOT RLIKE` are represented as the existing logical `NOT` predicate wrapping
  a regex comparison predicate.
- Analyzer/planner: extends the shared descriptor predicate planner. The left
  operand is resolved from MyLite descriptors. The right operand must be a
  decoded MyLite string literal and is validated before generated SQLite SQL is
  prepared.
- Catalog: read-only descriptor authority. This phase does not mutate catalog
  rows, descriptor versions, descriptor caches, catalog generation, or
  `sqlite_schema_generation`.
- SQLite physical execution: generated SQL uses stable MyLite physical table
  names, quoted descriptor column identifiers, and a bound pattern parameter
  passed to a MyLite-owned SQLite scalar function. It uses public SQLite
  function-registration APIs and does not require a SQLite fork patch.
- Storage/VFS: unchanged. Row storage, `.mylite` preamble, and shifted SQLite
  payload invariants are unaffected.

## Supported SQL

The existing descriptor predicate contexts admit:

```sql
regexp_predicate:
    string_column REGEXP regexp_pattern_literal
  | string_column RLIKE regexp_pattern_literal
  | string_column NOT REGEXP regexp_pattern_literal
  | string_column NOT RLIKE regexp_pattern_literal
```

Supported contexts:

```sql
SELECT ... FROM table WHERE regexp_predicate
SELECT aggregate FROM table WHERE regexp_predicate
DELETE FROM table WHERE regexp_predicate
UPDATE table SET column = value WHERE regexp_predicate
```

The predicate may be combined with existing keyword `NOT`, `AND`/`&&`, `XOR`,
`OR`/`||`, and parenthesized predicate support.

`string_column` must resolve to one descriptor column whose logical type is:

- `CHAR`;
- `VARCHAR`;
- `TINYTEXT`;
- `TEXT`;
- `MEDIUMTEXT`;
- `LONGTEXT`.

`regexp_pattern_literal` is an ordinary single- or double-quoted MyLite string
literal after current SQL-mode decoding. The admitted compatibility subset is
ASCII text without embedded `NUL`.

### MyLite Lemon-Syntax Snippet

```lemon
predicate_atom(A) ::= qualified_identifier(C) REGEXP(O) regexp_pattern_literal(P). {
    A = mylite_sql_parser_make_comparison_predicate(
        state, C, O, MYLITE_SQL_AST_OPERATOR_REGEXP, P);
}

predicate_atom(A) ::= qualified_identifier(C) RLIKE(O) regexp_pattern_literal(P). {
    A = mylite_sql_parser_make_comparison_predicate(
        state, C, O, MYLITE_SQL_AST_OPERATOR_RLIKE, P);
}

predicate_atom(A) ::= qualified_identifier(C) NOT(N) REGEXP(O) regexp_pattern_literal(P). {
    A = mylite_sql_parser_make_not_predicate(
        state, N,
        mylite_sql_parser_make_comparison_predicate(
            state, C, O, MYLITE_SQL_AST_OPERATOR_REGEXP, P));
}

predicate_atom(A) ::= qualified_identifier(C) NOT(N) RLIKE(O) regexp_pattern_literal(P). {
    A = mylite_sql_parser_make_not_predicate(
        state, N,
        mylite_sql_parser_make_comparison_predicate(
            state, C, O, MYLITE_SQL_AST_OPERATOR_RLIKE, P));
}

regexp_pattern_literal(A) ::= STRING(T). {
    A = mylite_sql_parser_make_literal(state, T, MYLITE_SQL_AST_LITERAL_STRING);
}
```

These snippets describe MyLite's supported subset, not MySQL's full grammar.

## Pattern Subset

Supported regex pattern syntax after SQL string decoding:

- ordinary printable ASCII literal bytes except regex metacharacters;
- escaped ASCII regex metacharacters `\.`, `\^`, `\$`, `\*`, `\+`, `\?`,
  `\[`, `\]`, `\|`, and `\\`;
- `.` matching one non-line-terminator byte;
- `^` as the first pattern item;
- `$` as the last pattern item;
- bracket classes such as `[abc]` and `[a-z]`;
- negated bracket classes such as `[^0-9]`;
- `*`, `+`, and `?` after one literal, `.`, or bracket class atom;
- fixed repetition `{n}` after one literal, `.`, or bracket class atom;
- one-level optional literal groups such as `(_ts)?`, where the group contents
  are ordinary literal ASCII bytes or escaped literal metacharacters.
- top-level alternation using `|`, where each nonempty branch is independently
  limited to this baseline pattern subset.

Unsupported in this slice:

- grouping outside one-level optional literal groups, nested groups, grouped
  alternation, and empty alternation branches;
- variable counted repetition such as `{m,n}`;
- lookaround, inline mode controls, backreferences, word-boundary escapes,
  shorthand classes, named classes, equivalence classes, and collating symbols;
- non-ASCII pattern bytes;
- embedded `NUL`;
- pattern operands from columns, functions, parameters, subqueries, or general
  expressions.

This scope is intentionally compatible with the WordPress-style anchored
prefix/suffix patterns currently targeted, including
`^rss_[0-9a-f]{32}(_ts)?$` and role-name alternation such as
`administrator|editor|author|contributor|subscriber|uploader`, but it is not
MySQL's full ICU regex syntax.

## Semantics

Planning:

1. Resolve the left operand through the existing descriptor-column resolver for
   the current statement context.
2. Require a string descriptor type.
3. Decode the right operand as an ordinary string literal using current MyLite
   string-literal and SQL-mode rules.
4. Reject embedded `NUL`, non-ASCII pattern bytes, unsupported regex syntax,
   and invalid baseline regex syntax before preparing SQLite SQL.
5. Bind the decoded pattern as a `TEXT` parameter.

Generated SQLite predicate SQL:

```sql
_mylite_regexp_ci_ascii(?N, "column_name" COLLATE "utf8mb4_0900_ai_ci")
```

The column identifier is always generated from the descriptor and quoted as a
SQLite identifier. The pattern is always bound as a prepared-statement
parameter. The registered function compiles and caches the bound pattern using
SQLite auxdata for the duration of the prepared statement and evaluates each
candidate row inside SQLite's scan. It rejects non-ASCII or embedded-`NUL`
runtime column values before matching.

Supported comparison semantics:

- `REGEXP` and `RLIKE` share one implementation.
- ASCII matching is case-insensitive for the current default collation slice.
- `^` and `$` anchor to the start and end of the stored value.
- Unanchored patterns may match at any byte position.
- `.` matches one byte other than `\n` or `\r` in the admitted ASCII baseline.
- Bracket classes/ranges and negated classes are evaluated over ASCII bytes
  with the same case-insensitive folding as literals.
- Fixed `{n}` repetition repeats exactly `n` occurrences of the preceding atom.
- One-level optional literal groups match either the full literal sequence or
  no bytes; individual bytes inside the group are not independently optional.
- Top-level alternation evaluates each branch as an independent baseline
  program. Boolean predicates match when any branch matches. Match-producing
  scalar helpers choose the earliest matching byte position and preserve branch
  order for equal start positions in the current limited subset.
- MySQL returns `NULL` when either regex operand is `NULL`, but this MyLite
  slice admits only string-literal pattern operands; `NULL` column values return
  `NULL` after the admitted non-`NULL` pattern has been validated.
- `NOT REGEXP` and `NOT RLIKE` are ordinary `NOT` over the regex predicate and
  therefore do not match `NULL` values in `WHERE`.

## Diagnostics

Supported diagnostics:

- parser syntax errors through existing parse diagnostics;
- unknown columns through existing descriptor-column diagnostics;
- non-string left operands with a deterministic unsupported diagnostic;
- unsupported right operands outside the admitted string-literal pattern grammar
  as syntax errors through existing parse diagnostics;
- non-ASCII or embedded-`NUL` pattern literals with a deterministic
  unsupported diagnostic;
- non-ASCII or embedded-`NUL` runtime column values with a deterministic
  unsupported diagnostic;
- unsupported regex constructs with a deterministic unsupported diagnostic;
- invalid supported-subset regex syntax with MySQL-compatible diagnostics where
  verified: `3696 / HY000` for unclosed bracket expressions and
  `3697 / HY000` for invalid character ranges;
- allocation failures through existing `MYLITE_NOMEM` and diagnostics;
- physical SQLite failures through existing internal SQLite diagnostics.

## Unsupported

Deferred until later slices:

- `REGEXP_INSTR()`, `REGEXP_REPLACE()`, and `REGEXP_SUBSTR()`;
- table-backed scalar projection using regex operators;
- literal-left, expression-left, function, parameter, variable,
  column-to-column, subquery, row-constructor, or generated-column operands;
- pattern values from columns, functions, parameters, or subqueries;
- binary strings and binary collations;
- explicit `COLLATE`, `BINARY`, or match-control options;
- Unicode/ICU syntax and collation-weight parity;
- regex optimizer or index-use parity with MySQL;
- resource-control system variables such as `regexp_stack_limit` and
  `regexp_time_limit`.
