# String and binary column types

## Scope

This feature extends MyLite's parse-only `CREATE TABLE` column type foundation
with MySQL string and binary declarations:

- `CHAR`, `CHARACTER`, `VARCHAR`, `CHAR VARYING`, and `CHARACTER VARYING`
- `TEXT`, `TINYTEXT`, `MEDIUMTEXT`, and `LONGTEXT`
- `BINARY`, `VARBINARY`, `BLOB`, `TINYBLOB`, `MEDIUMBLOB`, and `LONGBLOB`
- MySQL aliases `BYTE`, `LONG VARCHAR`, and `LONG VARBINARY`
- string and binary length validation and metadata descriptors
- `CHARACTER SET`, `CHARSET`, `COLLATE`, `BINARY`, and `BYTE` type attributes
  where MySQL accepts them for these types
- parse-only `CREATE TABLE` column definitions for the covered declarations

Full table DDL execution, catalog writes, storage enforcement, warning records,
row-size diagnostics, default values, constraints, and information-schema row
creation remain later roadmap work. Valid declarations prepare as
`MYLITE_UNSUPPORTED`; malformed declarations fail during parsing.

## Sources

- MySQL 8.4 Reference Manual, String Data Type Syntax:
  https://dev.mysql.com/doc/refman/8.4/en/string-type-syntax.html
- MySQL 8.4 Reference Manual, The CHAR and VARCHAR Types:
  https://dev.mysql.com/doc/refman/8.4/en/char.html
- MySQL 8.4 Reference Manual, The BINARY and VARBINARY Types:
  https://dev.mysql.com/doc/refman/8.4/en/binary-varbinary.html
- MySQL 8.4 Reference Manual, The BLOB and TEXT Types:
  https://dev.mysql.com/doc/refman/8.4/en/blob.html
- MySQL 8.4 Reference Manual, `INFORMATION_SCHEMA.COLUMNS` table:
  https://dev.mysql.com/doc/refman/8.4/en/information-schema-columns-table.html
- Observed MySQL 8.4.9 runtime behavior from Docker container
  `mylite-mysql-849`.

This specification is independently authored from official documentation and
observed runtime behavior. It does not copy MySQL grammar or implementation
sources.

## MySQL 8.4.9 behavior summary

In a schema whose default character set is `utf8mb4` and default collation is
`utf8mb4_0900_ai_ci`, MySQL exposes the following base metadata:

| Declaration | `DATA_TYPE` | `COLUMN_TYPE` | Character length | Octet length | Charset | Collation |
| --- | --- | --- | ---: | ---: | --- | --- |
| `CHAR` | `char` | `char(1)` | 1 | 4 | `utf8mb4` | `utf8mb4_0900_ai_ci` |
| `CHAR(4)` | `char` | `char(4)` | 4 | 16 | `utf8mb4` | `utf8mb4_0900_ai_ci` |
| `CHARACTER(4)` | `char` | `char(4)` | 4 | 16 | `utf8mb4` | `utf8mb4_0900_ai_ci` |
| `VARCHAR(4)` | `varchar` | `varchar(4)` | 4 | 16 | `utf8mb4` | `utf8mb4_0900_ai_ci` |
| `BINARY` | `binary` | `binary(1)` | 1 | 1 | `NULL` | `NULL` |
| `BINARY(4)` | `binary` | `binary(4)` | 4 | 4 | `NULL` | `NULL` |
| `VARBINARY(4)` | `varbinary` | `varbinary(4)` | 4 | 4 | `NULL` | `NULL` |
| `TINYTEXT` | `tinytext` | `tinytext` | 255 | 255 | `utf8mb4` | `utf8mb4_0900_ai_ci` |
| `TEXT` | `text` | `text` | 65535 | 65535 | `utf8mb4` | `utf8mb4_0900_ai_ci` |
| `MEDIUMTEXT` | `mediumtext` | `mediumtext` | 16777215 | 16777215 | `utf8mb4` | `utf8mb4_0900_ai_ci` |
| `LONGTEXT` | `longtext` | `longtext` | 4294967295 | 4294967295 | `utf8mb4` | `utf8mb4_0900_ai_ci` |
| `TINYBLOB` | `tinyblob` | `tinyblob` | 255 | 255 | `NULL` | `NULL` |
| `BLOB` | `blob` | `blob` | 65535 | 65535 | `NULL` | `NULL` |
| `MEDIUMBLOB` | `mediumblob` | `mediumblob` | 16777215 | 16777215 | `NULL` | `NULL` |
| `LONGBLOB` | `longblob` | `longblob` | 4294967295 | 4294967295 | `NULL` | `NULL` |

`CHAR(4) CHARACTER SET binary` normalizes to `binary(4)`.
`VARCHAR(4) CHARACTER SET binary` normalizes to `varbinary(4)`. Binary string
and blob types have no character set or collation metadata.

Length rules verified against MySQL 8.4.9:

- `CHAR`, `BINARY`, `CHAR(0)`, `CHAR(255)`, `BINARY(0)`, and `BINARY(255)` are
  accepted. `CHAR(256)` and `BINARY(256)` error with maximum `255`.
- `VARCHAR` and `VARBINARY` require a length.
- `VARCHAR(0)` and `VARCHAR(16383)` are accepted in default `utf8mb4`.
  `VARCHAR(16384)` errors because the maximum character count under the
  default four-byte character set is `16383`.
- In a `latin1` default schema, `VARCHAR(65532)` is accepted and reports octet
  length `65532`; `VARCHAR(65533)` and `VARCHAR(65535)` reach row-size error
  1118. `VARCHAR(21844) CHARACTER SET utf8mb3` is accepted with octet length
  `65532`, while `VARCHAR(21845) CHARACTER SET utf8mb3` reaches row-size error
  1118.
- `VARBINARY(0)` is accepted. `VARBINARY(65536)` errors because the type
  maximum is `65535`. `VARBINARY(65535)` reaches MySQL row-size diagnostics,
  which this parse-only task does not implement.
- `TEXT(M)` selects the smallest text family whose byte capacity can hold `M`
  characters in the effective character set. Under `utf8mb4`, `TEXT(10)` and
  `TEXT(63)` map to `tinytext`, `TEXT(64)`, `TEXT(255)`, and `TEXT(256)` map
  to `text`, `TEXT(65535)` and `TEXT(65536)` map to `mediumtext`, and
  `TEXT(16777215)` and `TEXT(16777216)` map to `longtext`.
- Under `latin1`, `TEXT(63)` and `TEXT(255)` map to `tinytext`.
- `BLOB(M)` is byte-based: `BLOB(10)` and `BLOB(255)` map to `tinyblob`;
  `BLOB(256)` and `BLOB(65535)` map to `blob`; `BLOB(65536)` and
  `BLOB(16777215)` map to `mediumblob`; `BLOB(16777216)` maps to `longblob`.
- `TEXT(4294967296)` and `BLOB(4294967296)` error because the maximum is
  `4294967295`.
- Length tokens larger than the unsigned 64-bit parser representation, such as
  `TEXT(18446744073709551616)`, `BLOB(18446744073709551616)`, and
  `VARCHAR(18446744073709551616)`, fail parsing instead of wrapping.
- Length suffixes on explicit `TINYTEXT`, `MEDIUMTEXT`, `LONGTEXT`,
  `TINYBLOB`, `MEDIUMBLOB`, and `LONGBLOB` are syntax errors.

Character set, collation, and alias behavior:

- `CHARACTER SET` and `CHARSET` are equivalent in type attributes.
- `COLLATE latin1_swedish_ci` without an explicit character set implies
  `latin1`.
- `COLLATE binary` without an explicit character set implies the `binary`
  character set and normalizes character string declarations to binary string
  declarations, and text declarations to blob declarations.
- `CHARACTER SET utf8mb4 COLLATE latin1_swedish_ci` errors because the
  collation does not belong to the character set.
- Unknown character sets and collations are errors.
- `CHAR(4) BINARY`, `VARCHAR(4) BINARY`, and `TEXT BINARY` keep their character
  type and default character set, but use the default binary collation for that
  character set. MySQL emits warning 1287 for the deprecated attribute; MyLite
  warning storage is deferred.
- `CHARACTER SET binary` converts character string declarations to binary
  string declarations: `CHAR` becomes `binary(1)`, `CHAR(4)` becomes
  `binary(4)`, `VARCHAR(4)` becomes `varbinary(4)`, plain `TEXT` becomes
  `blob`, `TEXT(255)` becomes `tinyblob`, and `TEXT(256)` becomes `blob`.
- `CHAR(4) BYTE`, `CHAR BYTE`, and `VARCHAR(4) BYTE` normalize to `binary(4)`,
  `binary(1)`, and `varbinary(4)`.
- `CHAR VARYING(4)` and `CHARACTER VARYING(4)` normalize to `varchar(4)`.
- `NATIONAL CHAR(4)` and `NCHAR(4)` normalize to `char(4)` with
  `utf8mb3`/`utf8mb3_general_ci`.
- `NATIONAL VARCHAR(4)` and `NVARCHAR(4)` normalize to `varchar(4)` with
  `utf8mb3`/`utf8mb3_general_ci` and warning 3720.
- `LONG VARCHAR` normalizes to `mediumtext`; `LONG VARBINARY` normalizes to
  `mediumblob`. Adding a length to either form is a syntax error.
- Character set clauses on `BINARY`, `VARBINARY`, and blob-family declarations
  are syntax errors.

## MyLite behavior

### Type descriptor

MyLite extends the internal column type descriptor with a string/binary domain.
Given a type keyword, optional length, and type attributes, it returns:

- canonical family and lowercase `DATA_TYPE`
- MySQL-compatible `COLUMN_TYPE`
- character maximum length and octet length where MySQL reports them
- effective character set and collation names, or `NULL` for binary data
- whether the type is character or binary data
- whether the declaration used a compatibility alias such as `BYTE`,
  `CHAR VARYING`, `NCHAR`, `NVARCHAR`, `LONG VARCHAR`, or `LONG VARBINARY`
- whether a deprecated `BINARY` type attribute was present

The initial descriptor recognizes the character sets and collations already in
MyLite's foundation: `utf8mb4`, `utf8mb3`, `latin1`, and `binary`, plus the
verified collations used by this feature. It rejects unknown values and
charset/collation mismatches.

### Parser and AST

MyLite accepts the following narrow `CREATE TABLE` shape for parser and AST
coverage:

```sql
CREATE TABLE table_name (
    column_name column_type
    [, column_name column_type ...]
)
```

Column attributes such as `NULL`, `NOT NULL`, `DEFAULT`, comments, generated
columns, inline keys, table constraints, table options, temporary tables,
`CREATE TABLE ... LIKE`, and `CREATE TABLE ... SELECT` remain out of scope.

Accepted type syntax for this task:

```sql
character_type ::= CHAR opt_length character_type_attributes
character_type ::= CHARACTER opt_length character_type_attributes
character_type ::= CHAR VARYING length character_type_attributes
character_type ::= CHARACTER VARYING length character_type_attributes
character_type ::= VARCHAR length character_type_attributes
character_type ::= NATIONAL CHAR opt_length
character_type ::= NCHAR opt_length
character_type ::= NATIONAL VARCHAR length
character_type ::= NVARCHAR length

text_type ::= TINYTEXT text_type_attributes
text_type ::= TEXT opt_length text_type_attributes
text_type ::= MEDIUMTEXT text_type_attributes
text_type ::= LONGTEXT text_type_attributes
text_type ::= LONG VARCHAR

binary_type ::= BINARY opt_length
binary_type ::= VARBINARY length
binary_type ::= CHAR opt_length BYTE
binary_type ::= CHARACTER opt_length BYTE
binary_type ::= VARCHAR length BYTE

blob_type ::= TINYBLOB
blob_type ::= BLOB opt_length
blob_type ::= MEDIUMBLOB
blob_type ::= LONGBLOB
blob_type ::= LONG VARBINARY

character_type_attributes ::= [character_type_attribute ...]
text_type_attributes ::= [text_type_attribute ...]
character_type_attribute ::= CHARACTER SET charset_name
character_type_attribute ::= CHARSET charset_name
character_type_attribute ::= COLLATE collation_name
character_type_attribute ::= BINARY
text_type_attribute ::= character_type_attribute
```

`length` and `opt_length` use unsigned integer tokens only. Negative lengths
therefore fail as syntax errors. MyLite performs parser-time range checks for
the declaration-level maxima that do not require full row-size analysis.

### Runtime boundary

Preparing a parse-only `CREATE TABLE` statement covered by this feature returns
`MYLITE_UNSUPPORTED`, not `MYLITE_PARSE_ERROR`. No SQLite table is created and
no MyLite catalog rows are written. Task 11 owns execution, metadata writes,
implicit commit semantics, warnings, and statement side effects.

## Lemon grammar snippets

These snippets describe MyLite's intended grammar for this feature:

```lemon
column_definition ::= identifier column_type.

column_type ::= integer_column_type.
column_type ::= boolean_column_type.
column_type ::= character_column_type.
column_type ::= text_column_type.
column_type ::= binary_column_type.
column_type ::= blob_column_type.

character_column_type ::= CHAR opt_column_length character_type_attribute_list.
character_column_type ::= CHARACTER opt_column_length character_type_attribute_list.
character_column_type ::= CHAR VARYING column_length character_type_attribute_list.
character_column_type ::= CHARACTER VARYING column_length character_type_attribute_list.
character_column_type ::= VARCHAR column_length character_type_attribute_list.
character_column_type ::= NATIONAL CHAR opt_column_length.
character_column_type ::= NCHAR opt_column_length.
character_column_type ::= NATIONAL VARCHAR column_length.
character_column_type ::= NVARCHAR column_length.

text_column_type ::= TINYTEXT text_type_attribute_list.
text_column_type ::= TEXT opt_column_length text_type_attribute_list.
text_column_type ::= MEDIUMTEXT text_type_attribute_list.
text_column_type ::= LONGTEXT text_type_attribute_list.
text_column_type ::= LONG VARCHAR.

binary_column_type ::= BINARY opt_column_length.
binary_column_type ::= VARBINARY column_length.
binary_column_type ::= CHAR opt_column_length BYTE.
binary_column_type ::= CHARACTER opt_column_length BYTE.
binary_column_type ::= VARCHAR column_length BYTE.

blob_column_type ::= TINYBLOB.
blob_column_type ::= BLOB opt_column_length.
blob_column_type ::= MEDIUMBLOB.
blob_column_type ::= LONGBLOB.
blob_column_type ::= LONG VARBINARY.

character_type_attribute_list ::= .
character_type_attribute_list ::= character_type_attribute_list character_type_attribute.
character_type_attribute ::= CHARACTER SET charset_value.
character_type_attribute ::= CHARSET charset_value.
character_type_attribute ::= COLLATE charset_value.
character_type_attribute ::= BINARY.

text_type_attribute_list ::= .
text_type_attribute_list ::= text_type_attribute_list text_type_attribute.
text_type_attribute ::= character_type_attribute.

opt_column_length ::= .
opt_column_length ::= column_length.
column_length ::= LPAREN INTEGER RPAREN.
```

## MySQL-runtime-verified expectations

Implementation tests should cover these MySQL 8.4.9 expectations:

| SQL or declaration | Expected MySQL-compatible outcome |
| --- | --- |
| `CHAR`, `CHAR(4)`, `CHARACTER(4)` | `DATA_TYPE=char`, `COLUMN_TYPE=char(N)`, default charset/collation |
| `VARCHAR(4)` | `DATA_TYPE=varchar`, `COLUMN_TYPE=varchar(4)`, default charset/collation |
| `CHAR VARYING(4)`, `CHARACTER VARYING(4)` | normalize to `varchar(4)` |
| `BINARY`, `BINARY(4)`, `VARBINARY(4)` | binary metadata, no charset/collation |
| `TINYTEXT`, `TEXT`, `MEDIUMTEXT`, `LONGTEXT` | text-family capacities and default charset/collation |
| `TINYBLOB`, `BLOB`, `MEDIUMBLOB`, `LONGBLOB` | blob-family capacities and no charset/collation |
| `TEXT(M)` | family chosen by effective charset byte capacity |
| `BLOB(M)` | family chosen by byte capacity |
| `CHAR(4) CHARACTER SET binary` | normalizes to `binary(4)` |
| `VARCHAR(4) CHARSET binary` | normalizes to `varbinary(4)` |
| `CHAR(4) COLLATE binary`, `TEXT COLLATE binary` | normalize to `binary(4)` and `blob` |
| `CHAR(4) BINARY`, `VARCHAR(4) BINARY`, `TEXT BINARY` | default charset with binary collation |
| `CHAR(4) BYTE`, `CHAR BYTE`, `VARCHAR(4) BYTE` | normalize to `binary(4)`, `binary(1)`, `varbinary(4)` |
| `CHARACTER SET latin1`, `CHARSET latin1 COLLATE latin1_swedish_ci` | latin1 metadata and one-byte octet lengths |
| `COLLATE latin1_swedish_ci` | implies latin1 |
| `NCHAR(4)`, `NVARCHAR(4)` | utf8mb3 metadata, with deferred warning for `NVARCHAR` |
| `LONG VARCHAR`, `LONG VARBINARY` | normalize to `mediumtext`, `mediumblob` |
| Missing `VARCHAR`/`VARBINARY` length | syntax error |
| `CHAR(256)`, `BINARY(256)`, `VARCHAR(16384)` under default utf8mb4, `VARBINARY(65536)` | parse error for this parse-only feature |
| `TEXT(4294967296)`, `BLOB(4294967296)` | parse error |
| `TEXT(18446744073709551616)`, `BLOB(18446744073709551616)`, `VARCHAR(18446744073709551616)` | parse error without integer wraparound |
| Length on explicit tiny/medium/long text/blob family | syntax error |
| Character-set clauses on binary/blob types | syntax error |

## Compatibility gaps

- Table DDL execution, catalog writes, storage, row-size diagnostics, and
  value conversion are deferred.
- Warning records for deprecated `BINARY` attributes and `NVARCHAR` are
  deferred until diagnostics/warning storage exists.
- Parser-time validation uses the current default `utf8mb4` boundary for
  `VARCHAR`; full session/schema-sensitive validation belongs with executable
  DDL.
- Detailed row-size diagnostics such as `VARCHAR(65533)` in `latin1` and
  `VARCHAR(21845) CHARACTER SET utf8mb3` are deferred to executable DDL.
- Protocol-level column flags and prepared-statement metadata are deferred to
  the result metadata task.
