# Hex, bit, and binary string literals

## Scope

This feature implements runtime evaluation for MySQL hex and bit literals that
the MyLite lexer and parser already accept:

- `0x...`, `x'...'`, and `X'...'`
- `0b...`, `b'...'`, and `B'...'`
- `_binary '...'` and `_binary "..."` character-set introducer string
  literals
- no-table scalar `SELECT`
- table-backed projection, predicate, and order expression contexts that use
  the shared scalar evaluator
- scalar functions over those literal values, such as `HEX()` and `LENGTH()`
- target-aware DML assignment for supported `INSERT ... VALUES`,
  `INSERT ... SET`, `ON DUPLICATE KEY UPDATE`, single-table `UPDATE`, and
  joined `UPDATE` paths

Out of scope:

- `BIT` column storage semantics
- hex/bit literal numeric coercion outside the covered explicit signed and
  unsigned integer `CAST` / `CONVERT` targets and target-aware DML assignment
  paths
- broader prepared-statement and protocol metadata outside the current result
  column descriptor path
- character-set introducers other than `_binary`
- complete introducer/collation grammar such as `_utf8mb4 'x' COLLATE ...`

## Sources

- MySQL 8.4 Reference Manual, Hexadecimal Literals:
  https://dev.mysql.com/doc/refman/8.4/en/hexadecimal-literals.html
- MySQL 8.4 Reference Manual, Bit-Value Literals:
  https://dev.mysql.com/doc/refman/8.4/en/bit-value-literals.html
- MySQL 8.4 Reference Manual, String Functions and Operators:
  https://dev.mysql.com/doc/refman/8.4/en/string-functions.html
- MySQL 8.4 Reference Manual, Character Set Introducers:
  https://dev.mysql.com/doc/refman/8.4/en/charset-introducer.html
- Observed MySQL 8.4.9 runtime behavior from Docker container
  `mylite-mysql-849`.

This specification is independently authored from official documentation and
observed runtime behavior. It does not copy MySQL grammar, documentation prose,
or implementation sources.

## Semantics

Hex literals evaluate to binary string bytes. Quoted and `0x` forms share the
same byte decoding for MyLite's supported expression paths. Odd numbers of hex
digits are interpreted as if the first digit has a leading zero nibble.

Bit literals also evaluate to binary string bytes for the supported expression
paths. The bit string is left-padded to a whole number of bytes before packing
bits from most significant to least significant bit.

Representative results:

| Expression | Result bytes |
| --- | --- |
| `0x417a` | `Az` |
| `x'417a'` | `Az` |
| `X'417a'` | `Az` |
| `0b0100000101111010` | `Az` |
| `b'0100000101111010'` | `Az` |
| `B'0100000101111010'` | `Az` |
| `HEX(0b1)` | `01` |
| `HEX(0b01)` | `01` |
| `HEX(0b001)` | `01` |
| `HEX(0b00000001)` | `01` |
| `LENGTH(0b000000001)` | `2` |
| `HEX(0b000000001)` | `0001` |
| `CAST(0x3132 AS SIGNED)` | `12594` |
| `CAST(X'3132' AS UNSIGNED)` | `12594` |
| `CAST(0b1010 AS SIGNED)` | `10` |
| `CAST(B'1010' AS UNSIGNED)` | `10` |
| `X'3132' + 0` | `12594` |
| `X'3132' = 12` | `0` |
| `B'1010' + 0` | `10` |
| `B'1010' = 10` | `1` |
| `B'1111111111111111111111111111111111111111111111111111111111111111' + 0` | `-1` |
| `HEX(_binary 'a\0b')` | `610062` |
| `LENGTH(_binary 'a\0b')` | `3` |
| `CHARSET(_binary 'abc')` | `binary` |
| `COLLATION(_binary 'abc')` | `binary` |
| `COERCIBILITY(_binary 'abc')` | `4` |

In supported DML assignment paths, numeric target columns receive the literal's
numeric value, while character and binary string targets receive decoded bytes.
For example, `INSERT INTO t(n, vb) VALUES (0x41, X'4100')` stores numeric `65`
in `n` and bytes `41 00` in `vb`.

The `_binary` introducer keeps ordinary MySQL string-literal escape handling,
including embedded NUL escapes, but assigns the resulting value the binary
character set and binary collation. Other character-set introducers remain
deferred until MyLite implements the broader introducer and explicit collation
grammar.

## Metadata

MySQL reports hex and bit literals as binary `VAR_STRING` result columns even
when a table-backed query returns zero rows:

| Expression | Type | Length | Decimals | Charset | Flags |
| --- | --- | --- | --- | --- | --- |
| `0x417a` | `VAR_STRING` | `2` | `0` | `binary` | `NOT_NULL UNSIGNED BINARY` |
| `x''` | `VAR_STRING` | `0` | `0` | `binary` | `NOT_NULL UNSIGNED BINARY` |
| `0b0100000101111010` | `VAR_STRING` | `2` | `0` | `binary` | `NOT_NULL BINARY` |
| `0b000000001` | `VAR_STRING` | `2` | `0` | `binary` | `NOT_NULL BINARY` |
| `_binary 'abc'` | `VAR_STRING` | `3` | `31` | `binary` | `NOT_NULL BINARY` |
| `_binary 'a\0b'` | `VAR_STRING` | `3` | `31` | `binary` | `NOT_NULL BINARY` |

## Grammar

The first character-set-introducer slice adds only the binary introducer token:

```lemon
literal(A) ::= BINARY_STRING_INTRODUCER(B) STRING(T). {
    A = mylite_sql_parser_make_binary_string_literal(state, B, T);
}
```

The lexer continues to report `_binary` as an unquoted identifier token; the
parser maps that spelling to `BINARY_STRING_INTRODUCER` before Lemon receives
it. This keeps ordinary `IDENTIFIER STRING` alias syntax out of the literal
production.

## Runtime Design

The parser continues to produce `MYLITE_SQL_AST_LITERAL_HEX` and
`MYLITE_SQL_AST_LITERAL_BIT` for hex and bit literals, and produces
`MYLITE_SQL_AST_LITERAL_BINARY_STRING` for `_binary` string literals. Runtime
support lives in the shared scalar expression evaluator so literal bytes are
available consistently to scalar `SELECT`, table-backed projections, and
existing scalar functions.

The evaluator:

- identifies the literal spelling to find the digit span
- decodes hex pairs into bytes, left-padding odd digit counts
- packs bit digits into bytes after calculating the required left padding
- returns a `MYLITE_EXPRESSION_VALUE_TEXT` with an explicit byte length so
  embedded NUL bytes survive nested functions such as `HEX()` and `LENGTH()`
- exposes the byte length through `mylite_column_bytes()` so clients can read
  binary strings that begin with `0x00`
- derives result-column metadata from the literal spelling so zero-row
  table-backed result sets do not need a runtime value to expose the MySQL
  binary-string descriptor
- uses the original literal AST for explicit signed and unsigned integer casts
  so those targets receive the literal's numeric value rather than the decoded
  binary string bytes
- uses a target-aware DML literal resolver for covered write paths so numeric
  columns receive unsigned literal numeric values and text/binary columns
  receive byte-decoded values before normal column coercion
- decodes `_binary` string literals with the same escape rules as ordinary
  string literals, but marks their expression values and metadata as binary
  string results

No storage or file-format changes are required.

## Compatibility Status

Hex, bit, and `_binary` string literal runtime evaluation is supported for the
current shared scalar expression contexts, explicit signed and unsigned integer
casts, and the covered DML assignment paths. The status remains partial until
MyLite implements full MySQL numeric/string coercion, additional character-set
introducers, `BIT` columns, and exact binary-string metadata across all protocol
and prepared-statement surfaces.
