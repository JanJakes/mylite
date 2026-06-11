# MySQL Lexer

## Status

This feature specifies MyLite's first SQL frontend component: a custom C lexer
for MySQL 8.4.9 SQL text. The lexer is implemented as an internal library
component, not a public ABI surface.

The lexer must be independently implemented from official MySQL documentation
and observed MySQL 8.4.9 behavior. It must not depend on SQLite and must not
perform parsing, semantic analysis, expression evaluation, or statement
execution.

## Sources

- MySQL 8.4 Reference Manual, Chapter 11, Language Structure:
  https://dev.mysql.com/doc/refman/8.4/en/language-structure.html
- MySQL 8.4 Reference Manual, String Literals:
  https://dev.mysql.com/doc/refman/8.4/en/string-literals.html
- MySQL 8.4 Reference Manual, Numeric Literals:
  https://dev.mysql.com/doc/refman/8.4/en/number-literals.html
- MySQL 8.4 Reference Manual, Hexadecimal Literals:
  https://dev.mysql.com/doc/refman/8.4/en/hexadecimal-literals.html
- MySQL 8.4 Reference Manual, Bit-Value Literals:
  https://dev.mysql.com/doc/refman/8.4/en/bit-value-literals.html
- MySQL 8.4 Reference Manual, Schema Object Names:
  https://dev.mysql.com/doc/refman/8.4/en/identifiers.html
- MySQL 8.4 Reference Manual, Keywords and Reserved Words:
  https://dev.mysql.com/doc/refman/8.4/en/keywords.html
- MySQL 8.4 Reference Manual, User-Defined Variables:
  https://dev.mysql.com/doc/refman/8.4/en/user-variables.html
- MySQL 8.4 Reference Manual, Comments:
  https://dev.mysql.com/doc/refman/8.4/en/comments.html
- MySQL 8.4 Reference Manual, Operators:
  https://dev.mysql.com/doc/refman/8.4/en/non-typed-operators.html

## Scope

The lexer recognizes the token boundaries and token classes needed by a future
MySQL parser:

- end of input
- comments, including line comments, block comments, executable version
  comments, and optimizer hint comments
- unquoted identifiers
- quoted identifiers
- MySQL keywords, with reserved, restricted-label, and restricted-role flags
- string literals
- national character string literals
- hexadecimal literals
- bit-value literals
- exact and approximate numeric literals
- user variables
- system variables
- parameter markers
- punctuation
- operators
- error tokens for malformed lexical units

The lexer does not interpret values. For example, string escape replacement,
numeric overflow, character set conversion, collation resolution, and temporal
literal coercion belong to later parser/analyzer/runtime layers.

## SQL Modes

The lexer accepts explicit mode flags for lexical behavior that changes before
semantic analysis:

- `ANSI_QUOTES`: double-quoted text is a quoted identifier; without it,
  double-quoted text is a string literal.
- `NO_BACKSLASH_ESCAPES`: backslash does not escape the following byte inside
  quoted strings.

Other SQL modes may affect later interpretation, but they do not need lexer
flags until a documented token boundary changes.

## Whitespace

The lexer treats ASCII space, horizontal tab, line feed, carriage return, form
feed, and vertical tab as token separators. Line and column positions are
updated for line-feed, carriage-return, and carriage-return plus line-feed
sequences.

## Comments

The lexer recognizes MySQL's three ordinary comment forms:

- `#` through the next line break or end of input
- `--` through the next line break or end of input, only when the second dash
  is followed by a whitespace or control byte
- `/* ... */` block comments

Block comments are not nested. An unterminated block comment is a lexical
error. Executable comments (`/*! ... */`, optionally with a five- or six-digit
version gate) and optimizer hints (`/*+ ... */`) are returned as distinct token
kinds. The parser expands executable comments whose version gate applies to the
fixed MySQL 8.4.9 compatibility target.

## Literals

String literals are quoted with single quotes or, unless `ANSI_QUOTES` is set,
double quotes. A matching quote inside the literal can be doubled. Backslash
escapes keep the token open unless `NO_BACKSLASH_ESCAPES` is set. Adjacent
quoted strings are separate tokens; concatenation is parser/analyzer behavior.
String escape replacement remains runtime behavior; when
`NO_BACKSLASH_ESCAPES` is set, `\0` remains the two bytes backslash and `0`
rather than becoming a NUL byte.

National character strings use `N'...'` or `n'...'` with no whitespace between
the `N` and the quote.

Hexadecimal literals use either `X'...'`/`x'...'` or `0x...`. The quoted form
must contain only hexadecimal digits and must have an even number of digits.
The `0x` prefix is case-sensitive; `0X...` is not a hex literal token.
When a lowercase prefixed candidate has an identifier continuation outside the
valid digit run, such as `0x1G`, MySQL treats the whole digit-leading sequence
as identifier-like rather than as a partial hex literal.

Bit-value literals use either `B'...'`/`b'...'` or `0b...`. The quoted and
prefixed forms must contain only `0` and `1`. The `0b` prefix is case-sensitive;
`0B...` is not a bit literal token. As with prefixed hex literals, a lowercase
prefixed bit candidate with an identifier continuation, such as `0b102`, is
identifier-like. Empty quoted bit literals are lexically valid.

Numeric literals include integer, decimal, and scientific notation forms.
Leading `+` and `-` are operator tokens; signedness is parser behavior. A token
such as `1e+3` is approximate numeric syntax, while `1e + 3` starts with an
identifier-like token because MySQL permits unquoted identifiers to start with
digits when they are not purely numeric.

Character set introducers such as `_utf8mb4'text'` are not collapsed into a
single token. The lexer emits the introducer as an identifier token followed by
the literal token; validation against the character set catalog belongs to the
parser/analyzer.

## Identifiers And Keywords

Unquoted identifiers may contain ASCII letters, digits, `$`, `_`, and bytes
with the high bit set. They may start with a digit if they do not consist solely
of digits. Quoted identifiers use backticks by default, or double quotes when
`ANSI_QUOTES` is set. A quote character inside a quoted identifier is written by
doubling it.

Keyword matching is ASCII case-insensitive and uses the MySQL 8.4.9 keyword
inventory observed from `INFORMATION_SCHEMA.KEYWORDS`. `_FILENAME` is reserved.
The lexer returns one keyword token kind and attaches flags for:

- reserved keyword
- restricted when used as a stored-program label
- restricted when used as a role name

The parser remains responsible for context-sensitive cases, including the rule
that a reserved word after a dot in a qualified name is treated as an identifier
and the rule that whitespace affects selected built-in function names.

## Variables

User variables start with `@`. Their unquoted names may contain ASCII
alphanumeric characters, `.`, `_`, and `$`. Quoted user-variable names may use
single quotes, double quotes, or backticks after the `@`.

System variables start with `@@` and continue through identifier characters,
dots, and backtick-quoted components. Scope validation (`GLOBAL`, `SESSION`,
`LOCAL`, and similar forms) belongs to later parser/analyzer layers.

## Operators And Punctuation

The lexer uses longest-match tokenization for symbolic operators:

- `->>`
- `<=>`
- `<<`
- `>>`
- `<=`
- `>=`
- `<>`
- `!=`
- `&&`
- `||`
- `:=`
- `->`

Single-byte operators include `=`, `<`, `>`, `+`, `-`, `*`, `/`, `%`, `!`, `~`,
`^`, `&`, and `|`. Punctuation includes parentheses, comma, semicolon, dot, and
braces. Additional punctuation may be added as grammar work requires it.

Word operators such as `AND`, `OR`, `XOR`, `DIV`, `MOD`, `LIKE`, `REGEXP`, and
`RLIKE` are keyword tokens.

## Diagnostics

The lexer returns error tokens instead of aborting. Error tokens include the
source span and a small diagnostic code. The first implementation needs these
lexical errors:

- unexpected byte
- unterminated string
- unterminated quoted identifier
- unterminated block comment
- invalid hexadecimal literal
- invalid bit literal
- invalid variable

The lexer must always advance at least one byte for an error token so callers
cannot loop forever.

## Source Spans

Each token records:

- token kind
- borrowed pointer to the first byte
- byte length
- zero-based byte offset
- one-based line
- one-based column
- leading-space flag
- keyword flags when applicable
- operator kind when applicable
- error kind when applicable

Line and column tracking is byte-oriented. Unicode-aware identifier validation
is a future analyzer concern; the lexer accepts non-ASCII bytes as identifier
bytes and records exact spans.

## MyLite Lemon Grammar Snippet

The lexer does not define statement grammar, but it supplies terminal classes
expected by the future Lemon parser:

```text
%token EOF.
%token IDENT QUOTED_IDENT KEYWORD.
%token STRING NATIONAL_STRING HEX_LITERAL BIT_LITERAL NUMBER.
%token USER_VARIABLE SYSTEM_VARIABLE PARAMETER.
%token COMMENT VERSION_COMMENT HINT_COMMENT.
%token OPERATOR PUNCTUATION LEXICAL_ERROR.

identifier(A) ::= IDENT(A).
identifier(A) ::= QUOTED_IDENT(A).
identifier(A) ::= KEYWORD(A).          // only in parser-approved contexts

literal(A) ::= STRING(A).
literal(A) ::= NATIONAL_STRING(A).
literal(A) ::= HEX_LITERAL(A).
literal(A) ::= BIT_LITERAL(A).
literal(A) ::= NUMBER(A).

variable(A) ::= USER_VARIABLE(A).
variable(A) ::= SYSTEM_VARIABLE(A).
```

## Query Corpus

The initial lexer corpus lives at
`packages/libmylite/tests/corpus/mysql_lexer_success.sql`. It is a new custom
corpus created for MyLite and organized from common application statements to
more exotic MySQL syntax. The corpus is for successful tokenization only; it
does not imply that every statement is currently parsed or executed by MyLite.

The corpus covers:

- common `SELECT`, `INSERT`, `UPDATE`, `DELETE`, and `REPLACE`
- table, index, view, trigger, function, procedure, event, and schema DDL
- transactions, locks, savepoints, prepared statements, and utility statements
- `SHOW`, `DESCRIBE`, `EXPLAIN`, and `SET`
- CTEs, joins, subqueries, set operations, windows, and locking clauses
- string, numeric, hex, bit, boolean, null, JSON path, and user-variable forms
- identifier quoting, digit-leading identifiers, dollar identifiers, and
  keyword-like identifiers
- comments, executable comments, optimizer hints, and symbolic operators

## MySQL Runtime Verification

The following lexical expectations were verified against MySQL 8.4.9 on
2026-05-01:

| Probe | Observed behavior |
| --- | --- |
| `X'01AF'`, `x'01af'`, and `0x01af` | Accepted as hexadecimal literals. |
| `X'0G'` | Rejected as syntax error. |
| `X'FFF'` | Rejected as syntax error because quoted hex has odd digit count. |
| `b'01'`, `B'01'`, `0b01`, and `b''` | Accepted as bit-value literals; `b'' + 0` evaluates numerically to `0`. |
| `b'2'` | Rejected as syntax error. |
| `0X01AF` and `0B01` | Treated as identifier-like tokens, not prefixed hex/bit literals. |
| `0x1G` and `0b102` | Treated as identifier-like tokens, not partial prefixed literals. |
| `1e+3`, `1e3`, `.25`, and `3.` | Accepted as numeric forms. |
| `1e + 3` | `1e` is treated as an identifier-like token before the `+`. |
| `'a' 'b'` | Accepted and concatenated by MySQL after lexing. |
| `_utf8mb4'c' COLLATE utf8mb4_0900_ai_ci` and `N'national'` | Accepted string introducer forms. |
| `@'dash-var' := 42` and ``@`dash-var` `` | Accepted quoted user-variable names. |
| ``@@`default`.key_buffer_size`` | Accepted as a system-variable reference with a quoted component. |
| `SELECT 1\v+2` and `SELECT 1--\vcomment\n+2` | Vertical tab is accepted as whitespace and as the required control byte after `--`. |

## Performance And Ownership

The lexer must be allocation-free for ordinary tokenization. Tokens borrow from
the input SQL buffer. Keyword classification should use a compact static table
and bounded stack storage for ASCII case folding.

No SQLite state, MyLite database handle, global mutable state, locale-sensitive
classification, or heap ownership should be required.

## Known Gaps

- Optimizer hint comments remain no-op comments; hint payloads are not parsed.
- Unicode identifier validation is deliberately permissive at the lexer layer.
- The lexer classifies keywords but does not resolve context-sensitive keyword
  use.
- The lexer does not validate character set introducer names or collation
  names.
- The lexer does not unescape or normalize string, identifier, or variable
  contents.
