# Parser Corpus SQL Mode Replay

This slice improves the local `mylite_benchmark --csv` parser-corpus run. It
does not change MyLite SQL syntax, runtime semantics, compatibility status, or
the external corpus. The goal is to measure parser coverage under the lexer
mode that would be active for each statement when the corpus changes
`@@session.sql_mode`.

## Compatibility Authority

MySQL 8.4 documents that `sql_mode` is global/session state, that `SET SESSION
sql_mode = ...` affects the current client, and that the mode changes accepted
syntax. The benchmark only needs the lexer-affecting subset already implemented
by MyLite:

- `ANSI_QUOTES`: double quotes quote identifiers rather than strings;
- `NO_BACKSLASH_ESCAPES`: backslashes do not escape string characters;
- `IGNORE_SPACE`: spaces are permitted between built-in function names and
  `(`;
- `PIPES_AS_CONCAT`: `||` is parsed as concatenation instead of logical OR.

The MySQL 8.4 manual also defines `ANSI` as including `REAL_AS_FLOAT`,
`PIPES_AS_CONCAT`, `ANSI_QUOTES`, `IGNORE_SPACE`, and `ONLY_FULL_GROUP_BY`.
Only the lexer-affecting members matter for this benchmark.

Reference: <https://dev.mysql.com/doc/refman/8.4/en/sql-mode.html>

## Behavior

The external CSV is a flattened corpus. It does not include reliable test-file
or connection-session boundaries, so SQL-mode replay is not the default
benchmark behavior. By default, CSV rows remain standalone parser inputs using
the default MyLite parser mode mask.

When `--csv-replay-sql-mode` is passed, `mylite_benchmark` assigns an effective
lexer-mode mask to each decoded query before timing begins. The benchmark then
passes that query's mask to lex, parse, and parse-failure dump calls.

Mode replay is intentionally limited to successful, session-affecting patterns
present in the corpus:

- direct session or unscoped `SET sql_mode = value` assignments;
- direct `SET @@sql_mode = value`, `SET @@session.sql_mode = value`, and
  `SET @@local.sql_mode = value` assignments;
- `DEFAULT` and `0` values, which clear all lexer-affecting modes;
- string or bare-identifier mode lists;
- `ANSI` expansion for lexer modes;
- user-variable save and restore of `@@sql_mode`;
- `sys.LIST_ADD(@@sql_mode, mode)`, `sys.LIST_DROP(@@sql_mode, mode)`, and
  `CONCAT(@@sql_mode, mode_list)` forms when the added or dropped modes can be
  read from literal or identifier text.

Global or persisted assignments such as `SET GLOBAL sql_mode = ...`,
`SET @@GLOBAL.sql_mode = ...`, and `SET PERSIST sql_mode = ...` do not affect
the current benchmark session mode. Unknown, invalid, unsupported, or
nonconstant mode values leave the current benchmark mode unchanged.

The assigned mode for a query is the mode in effect before that query executes.
If the query is a session-affecting `SET sql_mode` statement, its effect applies
to later queries.

## Architecture

Mode replay is a benchmark-local module:

- `packages/libmylite/benchmarks/mylite_benchmark_sql_mode.h`
- `packages/libmylite/benchmarks/mylite_benchmark_sql_mode.c`

The module uses MyLite's parser and AST to identify `SET` assignment lists and
then evaluates only the small mode-value subset described above. Because current
parser coverage admits `sys.LIST_ADD` / `sys.LIST_DROP` `SET sql_mode` forms as
unsupported utility placeholders, the annotator also has a narrow textual
fallback for those two placeholder forms. The benchmark does not include
mode-replay work in timed parser or lexer loops; each owned CSV query stores
its precomputed `modes` value.

## Tests

The focused C test covers:

- direct quoted and bare mode assignments;
- `DEFAULT` / `0` clearing;
- `ANSI` expansion to the relevant lexer modes;
- session assignments through `@@sql_mode`, `@@session.sql_mode`, and
  unscoped `sql_mode`;
- ignoring global assignments;
- user-variable save/restore;
- `sys.LIST_ADD`, `sys.LIST_DROP`, and `CONCAT` mode mutations;
- parsing an ANSI-quoted identifier statement with the assigned mode.

The default corpus benchmark must be rerun after implementation to preserve the
standalone-row parser coverage score. The opt-in replay benchmark should also be
sampled as a diagnostic for mode-sensitive parser gaps, but its counts should
not replace the default flattened-corpus score.
