# Parser Corpus Charset and Collation Surfaces

This slice reduces MySQL server-test parser-corpus failures where character-set
introducers, postfix `COLLATE`, unary `BINARY`, and character column shorthand
attributes appear in otherwise recognizable MySQL syntax. The compatibility goal
is targeted admission for high-volume corpus forms without claiming full MySQL
character-set or collation semantics.

Primary MySQL references:

- https://dev.mysql.com/doc/refman/8.4/en/charset-introducer.html
- https://dev.mysql.com/doc/refman/8.4/en/charset-literal.html
- https://dev.mysql.com/doc/refman/8.4/en/charset-column.html

## MySQL 8.4.9 Observations

The focused MySQL probes used a `mysql:8.4.9` runtime. Representative accepted
forms:

```sql
SELECT _latin1'B';
SELECT _utf8mb4 0x4142 COLLATE utf8mb4_0900_ai_ci;

CREATE TABLE shorthand (
    a CHAR(8) ASCII,
    b VARCHAR(8) BINARY ASCII
);

CREATE TABLE enum_hex (c ENUM(0xc3a6, 0xc3b8, b'01100001')
    CHARACTER SET utf8mb3);
CREATE TABLE set_hex (c SET('b', 0xc3a6, b'01100001')
    CHARACTER SET utf8mb3);
```

Representative predicate forms are accepted when the target column charset is
compatible with the introduced literal collation:

```sql
CREATE TABLE p_latin (c VARCHAR(32) CHARACTER SET latin1);
CREATE TABLE p_utf16 (c VARCHAR(255) CHARACTER SET utf16);

SELECT c FROM p_latin WHERE c > _latin1'B' COLLATE latin1_bin;
SELECT c FROM p_utf16 WHERE c LIKE _utf16 0x039C0025 COLLATE utf16_general_ci;
SELECT c FROM p_latin WHERE c IN (_latin1'a', _latin1'b' COLLATE latin1_bin);
```

Observed behavior:

- Character-set introducers may prefix string, hexadecimal, and bit literals,
  and may be followed by `COLLATE`.
- The introducer assigns literal charset metadata but does not by itself
  transcode the literal bytes.
- Introduced literals are accepted in MySQL predicate comparison, `BETWEEN`,
  `IN`, and `LIKE` value positions. MySQL also supports broader expression and
  `ESCAPE` positions; those remain outside this MyLite slice.
- `ASCII` and `UNICODE` are accepted as character column shorthand attributes.
  MySQL renders `ASCII` as a latin1 shorthand in `SHOW CREATE TABLE`; MyLite
  keeps its existing explicit `ascii` descriptor surface for now.
- MySQL accepts hexadecimal and bit literal tokens in `ENUM(...)` and `SET(...)`
  value lists. Character-set-introduced enum/set labels such as
  `_utf8mb3'abc'` are syntax errors in MySQL 8.4.9.
- Unary `BINARY` is accepted before literals in predicate value positions and
  requests binary string comparison semantics in MySQL.

## Scope

### Character-set Introduced Literals

MyLite already tokenizes `_charset` introducers and accepts introduced string,
hex, and bit literals in some scalar contexts. This slice admits the same
introduced literal shapes, with optional postfix `COLLATE`, in these targeted
positions:

- descriptor-backed DML constant scalar values;
- column default literal values;
- comparison right operands where the left side is a qualified identifier;
- `BETWEEN` bounds where the compared expression is a qualified identifier;
- `IN` list values where the left side is a qualified identifier;
- `LIKE` patterns where the left side is a qualified identifier.

Runtime execution preserves the existing reduced-fidelity rule: the literal
payload is treated like the underlying string, hex, or bit token. MyLite does
not transcode bytes, validate arbitrary charset/collation compatibility, or
implement general collation coercibility in this slice. Optional `COLLATE` on
introduced DML/default and targeted predicate literals is parser-admitted and
ignored by the reduced-fidelity execution paths. Existing ordinary postfix
`COLLATE` expression handling is reused for descriptor-backed DML constant
scalar values.

This slice intentionally does not add general scalar-expression introduced
literal propagation, `LIKE ... ESCAPE` introduced literals, scalar-expression
`BETWEEN`, arbitrary expression operands, or full charset/collation runtime
semantics.

### Unary BINARY Predicate Values

MySQL admits `BINARY literal` as a unary expression that requests binary string
comparison semantics. MyLite parses the high-volume literal forms in targeted
comparison and `BETWEEN` predicate value positions and retains a unary-binary
AST node. Existing runtime planning executes only the currently supported
descriptor predicate subset; unsupported envelopes must fail with deterministic
runtime diagnostics rather than raw syntax errors.

### Column Shorthand Attributes

MySQL accepts `ASCII` and `UNICODE` as shorthand character column attributes in
the same slot as `CHARACTER SET` / `COLLATE`. MyLite parses both in column
definitions and `ALTER TABLE ... MODIFY/CHANGE` definitions.

`ASCII` uses MyLite's existing explicit `ascii` metadata path. `BINARY ASCII`
is accepted as the MySQL-observed attribute order and selects `ascii_bin`.
`UNICODE` is admitted as a parser-corpus surface and may still be rejected by
the current descriptor charset admission policy until full MySQL aliasing is
implemented.

### ENUM and SET Literal Lists

MySQL accepts string, hexadecimal, and bit literal tokens in `ENUM(...)` and
`SET(...)` lists. MyLite now keeps those literal AST kinds and decodes hex and
bit labels through the descriptor label path. The resulting bytes must be valid
non-`NUL` UTF-8 after decoding and then follow the existing enum/set duplicate,
normalization, metadata, and rendering rules.

Character-set-introduced enum/set labels remain syntax errors, matching the
observed MySQL 8.4.9 behavior.

## MyLite Grammar Snippets

These snippets describe the intended MyLite-owned Lemon grammar shape and do
not copy MySQL grammar.

```lemon
introduced_predicate_literal ::= charset_introducer STRING.
introduced_predicate_literal ::= charset_introducer STRING COLLATE option_name.
introduced_predicate_literal ::= charset_introducer HEX_LITERAL.
introduced_predicate_literal ::= charset_introducer HEX_LITERAL COLLATE option_name.
introduced_predicate_literal ::= charset_introducer BIT_LITERAL.
introduced_predicate_literal ::= charset_introducer BIT_LITERAL COLLATE option_name.

binary_predicate_literal ::= BINARY STRING.
binary_predicate_literal ::= BINARY HEX_LITERAL.
binary_predicate_literal ::= BINARY BIT_LITERAL.
binary_predicate_literal ::= BINARY introduced_predicate_literal.

predicate_atom ::= qualified_identifier predicate_comparison_operator
                   introduced_predicate_literal.
predicate_atom ::= qualified_identifier predicate_comparison_operator
                   binary_predicate_literal.
predicate_atom ::= qualified_identifier BETWEEN introduced_predicate_literal
                   AND introduced_predicate_literal.
predicate_atom ::= qualified_identifier BETWEEN binary_predicate_literal
                   AND binary_predicate_literal.
predicate_atom ::= qualified_identifier IN LP introduced_predicate_literal_list RP.
predicate_atom ::= qualified_identifier LIKE introduced_predicate_literal.

dml_constant_scalar_value ::= charset_introducer STRING.
dml_constant_scalar_value ::= charset_introducer STRING COLLATE option_name.
dml_constant_scalar_value ::= charset_introducer HEX_LITERAL.
dml_constant_scalar_value ::= charset_introducer HEX_LITERAL COLLATE option_name.
dml_constant_scalar_value ::= charset_introducer BIT_LITERAL.
dml_constant_scalar_value ::= charset_introducer BIT_LITERAL COLLATE option_name.
dml_constant_scalar_value ::= STRING COLLATE option_name.
dml_constant_scalar_value ::= HEX_LITERAL COLLATE option_name.
dml_constant_scalar_value ::= BIT_LITERAL COLLATE option_name.

column_default_value ::= charset_introducer STRING.
column_default_value ::= charset_introducer STRING COLLATE option_name.
column_default_value ::= charset_introducer HEX_LITERAL.
column_default_value ::= charset_introducer HEX_LITERAL COLLATE option_name.
column_default_value ::= charset_introducer BIT_LITERAL.
column_default_value ::= charset_introducer BIT_LITERAL COLLATE option_name.

column_attribute ::= ASCII.
column_attribute ::= UNICODE.
column_attribute ::= BINARY.
column_attribute_list ::= BINARY column_charset_shorthand_attribute.

enum_label ::= STRING.
enum_label ::= HEX_LITERAL.
enum_label ::= BIT_LITERAL.
set_member ::= STRING.
set_member ::= HEX_LITERAL.
set_member ::= BIT_LITERAL.
```

The implementation deliberately uses targeted productions instead of a broad
generic expression-value admission rule. The broad form materially increased
Lemon state-generation cost in this grammar; the targeted rules keep parser
generation in the same practical range while improving corpus coverage.

## Runtime Behavior

This slice uses MyLite parser/runtime code only and does not need a SQLite fork
hook.

- Introduced DML/default literal values execute through the existing descriptor
  conversion path as their underlying literal kind.
- Introduced predicate literals execute only where the current descriptor
  predicate conversion subset supports the resulting AST.
- Optional `COLLATE` on introduced DML/default and targeted predicate literals
  does not add new runtime collation semantics.
- Unary `BINARY` predicate values retain the unary-binary AST; unsupported
  runtime envelopes remain explicit runtime errors.
- Hex and bit enum/set labels decode to bytes, reject embedded `NUL`, validate
  as UTF-8 for nonbinary enum/set descriptors, and render through the existing
  descriptor metadata path.

## Tests

Focused tests cover:

- parser acceptance for introduced literals with optional `COLLATE` in DML,
  default, and targeted comparison, `LIKE`, `BETWEEN`, and `IN` positions;
- parser acceptance for unary `BINARY` literal predicate values;
- parser acceptance for `ASCII` / `UNICODE` column shorthand attributes in
  create and alter column definitions;
- parser rejection for character-set-introduced enum/set labels;
- runtime preservation of introduced DML literal values and existing explicit
  ASCII column charset metadata;
- runtime execution of representative ordinary postfix `COLLATE` DML values and
  introduced predicate comparison, `LIKE`, `BETWEEN`, and `IN` paths;
- runtime decoding and metadata rendering for executable hex and bit enum/set
  labels;
- MySQL 8.4.9 expectation script output for representative accepted and
  rejected forms.

The parser corpus benchmark over the WordPress mysql-on-sqlite
`mysql-server-tests-queries.csv` must be rerun before commit to measure accepted
query movement.

## Compatibility Status

This slice broadens parser and limited runtime admission for existing charset
and collation surfaces. It does not implement full character-set conversion,
non-ASCII collation weights, MySQL's `ASCII` to `latin1` shorthand rendering,
full executable `UNICODE` aliasing, introduced literal `LIKE ... ESCAPE`, broad
expression-level introduced literal semantics, or charset-introduced
`ENUM` / `SET` labels.
