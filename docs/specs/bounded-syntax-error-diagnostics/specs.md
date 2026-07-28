# Bounded Syntax-Error Diagnostics

## Status

Specified; implementation and release qualification are pending.

## Summary

MyLite reports parser rejection as MySQL error `1064` with SQLSTATE `42000`,
but its ordinary parse-error formatter currently differs from MySQL 8.4.9 in
the message envelope, near-text selection, end-of-input handling, and line
counting. It also converts an untrusted token `size_t` length directly to the
`int` precision consumed by `snprintf()`.

An out-of-range conversion is implementation-defined. On common targets it can
produce a negative precision, which makes `%.*s` behave as though no precision
were supplied. A length-bounded SQL token can then become an unbounded C-string
read. Inputs below `INT_MAX` remain memory-safe, but a one-MiB token already
fills the complete diagnostic buffer with token text and loses the closing
quote and line suffix.

This feature gives every parser-owned syntax diagnostic one bounded formatter.
It reproduces MySQL 8.4.9's English `ER_PARSE_ERROR` envelope, copies at most
80 bytes of near text, never narrows an unchecked length, and preserves the
fixed public diagnostic capacity.

## Sources

- MyLite engineering standards:
  `docs/architecture/engineering-standards.md`
- MyLite parser error token contract:
  `packages/libmylite/src/sql/mylite_lexer.h`
- Current syntax diagnostic formatting:
  `packages/libmylite/src/runtime/mylite_execution_ast_internal.c`
- Follow-up review finding `SQL-04`:
  `docs/architecture/review-2026-07-followup-remediation-plan.md`
- MySQL 8.4 error-message elements:
  <https://dev.mysql.com/doc/refman/8.4/en/error-message-elements.html>
- MySQL 8.4 server error reference for `ER_PARSE_ERROR`:
  <https://dev.mysql.com/doc/mysql-errors/8.4/en/server-error-reference.html>
- MySQL 8.4 diagnostics area:
  <https://dev.mysql.com/doc/refman/8.4/en/diagnostics-area.html>
- MySQL 8.4 error information interfaces:
  <https://dev.mysql.com/doc/refman/8.4/en/error-interfaces.html>
- MySQL 8.4.9 observations pinned by
  `packages/libmylite/tests/mysql_bounded_syntax_error_diagnostics_expectations.sh`.

The specification is independently authored from public documentation,
observed MySQL behavior, and MyLite source. It does not use MySQL parser or
server implementation source.

## Observed MySQL 8.4.9 Behavior

The pinned runtime uses `lc_messages=en_US`. For the covered parser failures,
the diagnostics area reports:

```text
MYSQL_ERRNO=1064
RETURNED_SQLSTATE=42000
MESSAGE_TEXT=You have an error in your SQL syntax; check the manual that corresponds to your MySQL server version for the right syntax to use near '<near>' at line <line>
```

Observed near-text behavior is:

- `SELECT FROM t;` reports `FROM t`, not only the `FROM` token;
- `SELECT;` reports an empty string rather than the delimiter or an
  end-of-input label;
- trailing SQL whitespace and the terminating semicolon are not included;
- the excerpt contains at most 80 source bytes;
- an 81-byte token and a token above one MiB both report exactly the first
  80 bytes and retain the closing quote and line suffix;
- the same envelope is returned while preparing invalid SQL.

Line numbers are one-based and count line-feed bytes before the error
location. A standalone carriage return does not increment the line. CRLF
increments once because it contains one line feed.

The 80-byte limit is byte-oriented, not character-oriented. It can end inside
a multibyte encoded character. MyLite therefore does not reinterpret, repair,
or allocate a transcoded near excerpt.

## Current Baseline

Before implementation, the native runtime reports:

```text
SELECT FROM t;
1064 / 42000 / You have an error in your SQL syntax near 'FROM' at line 1

SELECT;
1064 / 42000 / You have an error in your SQL syntax near ';' at line 1
```

It formats only the lexer token, omits the MySQL manual clause, treats a
standalone carriage return as a line break, and uses the unchecked expression
`(int)error_token.length` as a string precision. The separate
multiple-statement path already has the full message envelope, but uses a
different formatter and permits up to `INT_MAX` bytes of near text.

## Scope

This feature covers:

- direct execution parse failures;
- streaming and buffered prepare parse failures;
- retry-produced syntax failures and deterministic retry-budget failures;
- lexical error tokens;
- end-of-input and statement-delimiter errors;
- MyLite's existing multiple-statement rejection;
- exact error number, SQLSTATE, English message envelope, near text, and line
  number for the pinned cases;
- non-NUL-terminated and very large length-bounded SQL buffers.

This feature does not:

- change accepted SQL grammar;
- change parser retry budgets (`SQL-02`);
- change AST source spans (`SQL-03`);
- change the Lemon parser stack (`SQL-05`);
- add localized messages or a configurable `lc_messages`;
- claim exact MySQL near-token selection for grammar productions that MyLite
  does not implement;
- change the public diagnostic capacity, ABI, file format, dependencies, or
  SQLite.

## Diagnostic Contract

Every parser-owned syntax rejection uses:

```text
error number: 1064
SQLSTATE:     42000
message:      You have an error in your SQL syntax; check the manual that
              corresponds to your MySQL server version for the right syntax
              to use near '<near>' at line <line>
```

The formatter always emits the complete envelope and suffix. It does not use
an alternate `end of input` phrase. If no valid near location is available,
`<near>` is empty and `<line>` is one.

`snprintf()` failure produces the existing empty fallback message. The
diagnostic code and SQLSTATE remain `1064` and `42000`.

## Near-Text Selection

For an ordinary parse result:

1. validate that the error token offset is within its recorded source length;
2. derive the remaining source interval beginning at the error token;
3. remove trailing SQL whitespace and one final statement delimiter;
4. remove SQL whitespace immediately before that delimiter;
5. use an empty interval for EOF or a delimiter-only error;
6. cap the interval at 80 bytes before converting the precision to `int`.

The formatter never calls `strlen()` on parser input and never reads beyond
the selected 80-byte interval. Embedded NUL is safe: C diagnostic storage
cannot preserve it, so formatted near text stops at that byte while the
closing envelope remains present.

For MyLite's multiple-statement rejection, the second statement's validated
AST source span supplies the near interval. The same 80-byte cap and formatter
apply.

## Line Contract

The line number starts at one. Only `'\n'` bytes strictly before the selected
near location increment it. This matches the pinned MySQL behavior for LF,
CRLF, and standalone CR input.

Counting remains linear in the error offset and allocation-free. If an
impractically large number of lines would exceed `UINT_MAX`, the line number
saturates rather than wrapping.

## Memory And Performance

Formatting is allocation-free and uses the existing
`MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY` stack buffer. At most 80 source bytes are
passed to `%.*s`; the precision is therefore representable as `int`.

Trailing-delimiter discovery and line counting inspect bounded regions of the
already parsed input. No full-input copy, token materialization, or additional
lexer pass is introduced. The maximum English message, including a ten-digit
line number and terminator, fits the existing diagnostic buffer.

## Error And Ownership Behavior

The formatter borrows parser input only for the duration of the call.
`mylite_diagnostics_set_error()` copies the completed message into
connection-owned diagnostics before parser result cleanup.

Formatting changes neither parser-result ownership nor failure precedence:

- allocation failure remains `MYLITE_NOMEM` / `HY001`;
- retry budget exhaustion remains public `1064` / `42000`;
- misuse remains misuse;
- syntax rejection remains `MYLITE_ERROR` at execution and prepare surfaces.

## Tests

The MySQL 8.4.9 fixture pins:

- server version and English message locale;
- exact code, SQLSTATE, envelope, suffix preview, and line number;
- empty near text at a delimiter;
- trailing-whitespace removal;
- LF, CRLF, and standalone-CR line behavior;
- the exact 80-byte boundary at 79, 80, and 81 bytes;
- a generated syntax token above one MiB;
- SQL-level prepare parity;
- connection reuse after every failure.

Native tests must cover:

- exact public diagnostics for direct execution, streaming prepare, and
  buffered prepare;
- source-remainder rather than token-only preview;
- EOF, delimiter, trailing whitespace, LF, CRLF, and CR cases;
- 79-, 80-, and 81-byte excerpts;
- non-NUL-terminated one-MiB input;
- an internal `SIZE_MAX` token-length reproducer backed by only the 80 readable
  bytes the formatter may inspect;
- embedded NUL safety;
- multiple-statement rejection through the shared formatter;
- diagnostic capacity and connection reuse;
- retry-budget and fatal-status regressions.

Qualification must include focused tests in Development, Debug-CI, Release,
and ASan/UBSan; all parser and affected runtime diagnostic suites; the pinned
parser MySQL fixtures; parser fuzzing; formatting; full static analysis;
ABI/install-consumer checks; and the production size gate.
