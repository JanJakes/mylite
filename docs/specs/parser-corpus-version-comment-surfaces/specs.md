# Parser Corpus Version Comment Surfaces

This slice admits MySQL executable version comments in the parser pipeline where
the embedded behavior is parse-only SQL compatibility. It targets valid MySQL
8.4.9 syntax that appears in the WordPress mysql-on-sqlite MySQL server-test
query corpus and currently fails before runtime analysis.

References:

- MySQL 8.4 Reference Manual, Comments:
  https://dev.mysql.com/doc/refman/8.4/en/comments.html
- MySQL 8.4.9 runtime probes run with:
  `docker exec -i mylite-mysql-849 mysql --protocol=TCP -h127.0.0.1 -uroot --batch --raw --force`

## MySQL-observed behavior

MySQL executable comments use `/*! ... */` and may include a version number
immediately after `!`. If the version is absent, MySQL tokenizes the payload.
If the version is present and is less than or equal to the running server
version, MySQL tokenizes the payload. If the version is greater than the running
server version, MySQL ignores the payload.

Observed against MySQL 8.4.9:

- `SELECT 1 /*!80000 + 1 */` returns `2`.
- `SELECT 1 /*!80409 + 1 */` returns `2`.
- `SELECT 1 /*!80410 + 1 */` returns `1`.
- `SELECT 1 /*!99999 + 1 */` returns `1`.
- `SELECT /*! 9 */` returns `9`.
- `SELECT /*!99999 9 */ AS skipped_payload_value` is a syntax error because
  skipping the payload leaves no select expression.
- `SELECT 1 /*!99999 /* */ */` parses and returns `1`.
- `SELECT 2 /*!12345 /* */ */` parses and returns `2`.
- Ordinary nested block comments remain invalid:
  `SELECT 1 /* outer /* inner */ */ AS ordinary_nested` errors near the trailing
  slash.
- Version comments can contribute structural tokens, such as `(` / `)` around
  CTE bodies and foreign-key reference column lists.

## Scope

MyLite will:

- keep lexer token classification for ordinary comments, optimizer hints, and
  executable version comments;
- scan executable version comments through their outer closing `*/`, allowing
  ordinary `/* ... */` comment spans inside the executable-comment payload so
  valid MySQL corpus rows are tokenized as one version-comment token;
- keep ordinary `/* ... */` comments non-nesting;
- at parse time, expand executable version-comment payloads when they have no
  version gate or a numeric gate less than or equal to `80409`;
- skip payloads gated above `80409`;
- feed executable payload tokens through the same Lemon parser token mapping and
  parser-history path as normal SQL text;
- preserve current optimizer-hint behavior as skipped comments.

This is parser compatibility. Runtime support remains whatever the expanded SQL
surface already supports. A version-comment payload that expands to unsupported
SQL may still parse as a utility placeholder or fail at runtime analysis.

## Syntax

The parser treats these as transparent SQL token containers:

```lemon
comment_token ::= ordinary_comment.
comment_token ::= optimizer_hint_comment.
comment_token ::= executable_comment.

executable_comment ::= "/*!" optional_version executable_sql_payload "*/".
optional_version ::= .
optional_version ::= integer_version_gate.
```

The Lemon grammar does not receive comment tokens directly. Instead, the parser
driver either skips a comment token or feeds the executable-comment payload back
through the lexer and then into Lemon.

## Parser and lexer design

The lexer remains single-pass and allocation-free. Ordinary comments continue
to stop at the first `*/`. For `/*!`, the lexer scans until the outer executable
comment terminator. While scanning the payload, an ordinary `/* ... */` span is
skipped as payload text; its inner closing `*/` does not close the executable
comment.

The parser driver decides whether an executable comment is active. It derives
the payload span from the token text:

- skip the initial `/*!`;
- consume consecutive ASCII digits as the optional version gate;
- skip exactly the digit prefix, not surrounding SQL whitespace;
- if no digits exist, execute the payload;
- if digits exist, execute only when the parsed integer is `<= 80409`.

An executable payload is lexed with the same SQL modes as the containing input.
Nested ordinary comments and optimizer hints inside the payload are skipped.
Nested executable comments are not part of this slice; MySQL 8.4.9 has
context-sensitive behavior there, and the corpus rows targeted here only require
ordinary block-comment spans inside executable-comment payloads.

Parser state, token history, previous-token tracking, and row-constructor
injection are shared with the containing statement so version comments can
contribute structural tokens exactly where they appear.

## Compatibility decisions and gaps

- MyLite uses the fixed MySQL 8.4.9 compatibility target for the parser version
  gate rather than a configurable server-version variable.
- MyLite accepts six-or-more digit gates by numeric comparison; too-large gates
  are treated as greater than the compatibility target and skipped.
- Optimizer hints remain skipped comments. This matches the current embedded
  no-op optimizer-hint policy for parser compatibility.
- Nested executable-comment edge cases are not a target for this slice.
- The parser does not surface warnings for skipped or executed executable
  comments.
- No SQLite behavior or fork hook is involved. This is MyLite lexer/parser
  wrapper logic only.

## Tests

Tests must cover:

- lexer tokenization for executable comments containing an inner ordinary block
  comment;
- ordinary nested block comments still not being treated as valid nested
  comments;
- parser execution of no-version, lower-version, and equal-version payloads;
- parser skipping of too-new payloads;
- structural payload tokens in CTE and foreign-key DDL shapes from the corpus;
- MySQL 8.4.9 expectation probes for the version gate and representative corpus
  surfaces;
- parser corpus benchmark movement over
  `build/perf-data/mysql-server-tests-queries.csv`.
