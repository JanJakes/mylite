# Parser Corpus String Literal Surfaces

This slice reduces parser-corpus failures for MySQL string literal forms that
appear in DML and scalar-expression surfaces:

- adjacent ordinary string literal concatenation, such as `'ab' 'cd'`;
- standalone national string literals, such as `N'abc'`, in currently admitted
  string-literal value positions.

Primary MySQL references:

- https://dev.mysql.com/doc/refman/8.4/en/string-literals.html
- https://dev.mysql.com/doc/refman/8.4/en/charset-national.html

## MySQL 8.4.9 Observations

Runtime probes against MySQL 8.4.9 show:

- adjacent ordinary quoted strings are concatenated before expression
  evaluation; `SELECT 'a' 'b'` returns `ab`;
- `INSERT INTO t VALUES ('ab' 'cd')` stores `abcd`;
- standalone `N'xy'` stores the string text and reports the national character
  set (`utf8mb3` in the current MySQL 8.4.9 runtime);
- `N'\\'` decodes the backslash escape using the active SQL mode;
- mixed adjacent ordinary and national tokens, such as `'a' N'b'`, are not part
  of this slice.

## Scope

In scope:

- parse adjacent ordinary `STRING STRING ...` groups as one string-literal
  expression in general literal, `INSERT ... VALUES`, `VALUES ROW(...)`, and
  collated string-literal positions;
- parse standalone `NATIONAL_STRING` in DML/value positions that already accept
  ordinary strings;
- decode adjacent ordinary string-literal groups by decoding each segment with
  the active string escape policy and concatenating the resulting bytes;
- decode standalone national strings with the same byte-preserving policy as
  ordinary string literals for admitted MyLite text contexts;
- preserve current `NO_BACKSLASH_ESCAPES` behavior for both ordinary and
  national string segments;
- keep national charset conversion, collation derivation, introducer mixing,
  and mixed adjacent ordinary/national concatenation out of scope.

Out of scope:

- implementing national character set storage metadata or transcoding;
- arbitrary charset introducer concatenation such as `_utf8mb4'a' 'b'`;
- adjacent hex or bit literal concatenation;
- accepting syntactically invalid mixed adjacent ordinary/national forms;
- expanding table-backed scalar expression execution beyond the existing
  admitted string-literal contexts.

## MyLite Parser Direction

The parser keeps ordinary string concatenation as an AST literal so existing
runtime gates that check for `MYLITE_SQL_AST_LITERAL_STRING` continue to work.
The concatenated literal carries its segment literal nodes as children, allowing
runtime decoders to concatenate decoded segment bytes instead of treating the
joined source span as one quoted token.

Representative Lemon-shape grammar:

```lemon
ordinary_string_literal ::= STRING.
ordinary_string_literal ::= ordinary_string_literal STRING.

string_text_literal ::= ordinary_string_literal.
string_text_literal ::= NATIONAL_STRING.

literal ::= string_text_literal.
insert_value ::= string_text_literal.
values_value ::= string_text_literal.
collated_literal_expression ::= ordinary_string_literal COLLATE option_name.
```

## Runtime Behavior

For admitted ordinary adjacent literals:

- decode each segment independently using the active SQL mode;
- concatenate bytes in source order;
- surface the same unsupported-NUL diagnostics as ordinary string literals in
  contexts that do not allow NUL bytes.

For standalone national strings:

- decode from the quote after the leading `N`/`n`;
- preserve bytes without charset conversion;
- use ordinary string metadata in MyLite result/value contexts until broader
  charset semantics are implemented.

## Tests

Tests cover:

- parser acceptance for adjacent ordinary strings in `SELECT`, `INSERT`, and
  `VALUES` contexts;
- parser acceptance for standalone `N'...'` in DML/value contexts;
- parser rejection for mixed adjacent ordinary/national literals;
- runtime storage and scalar projection for adjacent ordinary strings;
- runtime storage for standalone national strings;
- MySQL 8.4.9 expectation probes for representative ordinary concatenation and
  national string decoding;
- parser-corpus benchmark movement over
  `build/perf-data/mysql-server-tests-queries.csv`.

## Compatibility Status

This is a limited string-literal surface improvement. MyLite still does not
implement full charset introducer or national character set semantics.
