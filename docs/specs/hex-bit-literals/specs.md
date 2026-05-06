# Hex and bit literals

## Scope

This feature implements runtime evaluation for MySQL hex and bit literals that
the MyLite lexer and parser already accept:

- `0x...`, `x'...'`, and `X'...'`
- `0b...`, `b'...'`, and `B'...'`
- no-table scalar `SELECT`
- table-backed projection, predicate, and order expression contexts that use
  the shared scalar evaluator
- scalar functions over those literal values, such as `HEX()` and `LENGTH()`

Out of scope:

- `BIT` column storage semantics
- bit-literal numeric coercion in every MySQL type-conversion context
- exact literal metadata for every prepared-statement and protocol surface
- character-set introducer and SQL-mode interactions outside existing string
  literal handling

## Sources

- MySQL 8.4 Reference Manual, Hexadecimal Literals:
  https://dev.mysql.com/doc/refman/8.4/en/hexadecimal-literals.html
- MySQL 8.4 Reference Manual, Bit-Value Literals:
  https://dev.mysql.com/doc/refman/8.4/en/bit-value-literals.html
- MySQL 8.4 Reference Manual, String Functions and Operators:
  https://dev.mysql.com/doc/refman/8.4/en/string-functions.html
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

## Runtime Design

The parser continues to produce `MYLITE_SQL_AST_LITERAL_HEX` and
`MYLITE_SQL_AST_LITERAL_BIT`. Runtime support lives in the shared scalar
expression evaluator so literal bytes are available consistently to scalar
`SELECT`, table-backed projections, and existing scalar functions.

The evaluator:

- identifies the literal spelling to find the digit span
- decodes hex pairs into bytes, left-padding odd digit counts
- packs bit digits into bytes after calculating the required left padding
- returns a `MYLITE_EXPRESSION_VALUE_TEXT` with an explicit byte length so
  embedded NUL bytes survive nested functions such as `HEX()` and `LENGTH()`

No storage or file-format changes are required.

## Compatibility Status

Hex and bit literal runtime evaluation is supported for the current shared
scalar expression contexts. The status remains partial until MyLite implements
full MySQL numeric/string coercion, `BIT` columns, and exact binary-string
metadata across all protocol and prepared-statement surfaces.
