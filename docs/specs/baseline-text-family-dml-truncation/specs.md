# Baseline Text-Family DML Truncation

## Purpose

Extend the existing descriptor-owned `TINYTEXT`, `TEXT`, `MEDIUMTEXT`, and `LONGTEXT`
row-value support so overlength values in admitted DML paths follow the verified
MySQL 8.4.9 strict, non-strict, and `IGNORE` truncation behavior. This is a
MyLite conversion-layer feature; it does not add grammar, new public API, SQLite
schema changes, catalog mutations, or SQLite fork patches.

Normative references:

- MySQL 8.4 Reference Manual, `BLOB` and `TEXT` type family:
  <https://dev.mysql.com/doc/refman/8.4/en/blob.html>
- MySQL 8.4 Reference Manual, string storage requirements:
  <https://dev.mysql.com/doc/refman/8.4/en/storage-requirements.html>
- MySQL 8.4 Reference Manual, SQL modes and strict mode:
  <https://dev.mysql.com/doc/refman/8.4/en/sql-mode.html>
- MySQL 8.4 Reference Manual, `INSERT` and `IGNORE` behavior:
  <https://dev.mysql.com/doc/refman/8.4/en/insert.html>

All behavioral expectations below were verified against MySQL 8.4.9 with:

```sh
MYLITE_MYSQL_BIN=/opt/homebrew/opt/mysql@8.4/bin/mysql
MYLITE_MYSQL_SOCKET=/tmp/mylite-mysql-849.jsgoZE/mysql.sock
```

## Scope

Supported in this slice:

- Descriptor-backed `TINYTEXT`, `TEXT`, `MEDIUMTEXT`, and `LONGTEXT` target
  columns, including descriptors produced by `TEXT(M)` normalization.
- String literal text-family values in:
  - `INSERT ... VALUES`
  - `INSERT ... SET`
  - `REPLACE ... VALUES`
  - `REPLACE ... SET`
  - `UPDATE ... SET column = 'literal'`
- Selected text values in supported `INSERT ... SELECT` table-backed and
  row-scalar paths when the target column is a text-family descriptor.
- `INSERT IGNORE` and non-strict SQL mode warning demotion for nonspace
  overlength values.
- Strict-mode trailing-space truncation notes for literal value positions that
  MySQL accepts.
- MySQL-compatible changed-row reporting for `UPDATE` after truncation.

Still out of scope:

- BLOB-family truncation beyond the already documented binary-string behavior.
- Character set conversion. MyLite continues to require valid UTF-8, non-`NUL`
  text for nonbinary `TEXT` descriptors.
- General expression evaluation in DML values. The admitted assignment and row
  value expression set remains whatever the existing DML lifecycle supports.
- Server protocol information strings such as `Rows matched: ...`.
- Streaming large value conversion. MyLite continues to work with in-memory
  statement values and the existing build-size guard.

## Grammar

No new grammar is introduced. The feature applies after existing parsing to the
currently admitted DML shapes. The relevant independently authored MyLite
grammar subset is unchanged:

```lemon
dml_value ::= STRING.
dml_value ::= NULL.
dml_value ::= DEFAULT.

insert_statement ::= INSERT ignore_opt INTO table_name insert_values.
insert_statement ::= INSERT ignore_opt INTO table_name SET insert_assignment_list.
replace_statement ::= REPLACE INTO table_name insert_values.
replace_statement ::= REPLACE INTO table_name SET insert_assignment_list.
update_statement ::= UPDATE table_name SET update_assignment_list where_opt order_limit_opt.
insert_select_statement ::= INSERT ignore_opt INTO table_name insert_target_opt select_statement.
```

## Conversion Semantics

Text-family maximum lengths are byte limits:

| Descriptor | Maximum bytes |
| --- | ---: |
| `TINYTEXT` | 255 |
| `TEXT` | 65535 |
| `MEDIUMTEXT` | 16777215 |
| `LONGTEXT` | 4294967295 |

The conversion layer validates that admitted text is valid UTF-8 and contains no
embedded `NUL` bytes before applying length rules. If a value exceeds the target
byte limit and must be stored, MyLite truncates to the longest valid UTF-8
prefix whose byte length is at most the descriptor limit. It must not split a
multi-byte UTF-8 sequence.

### Literal DML Values

For `INSERT`, `REPLACE`, and `UPDATE` literal values:

- In strict mode, nonspace overlength text fails with error 1406 / SQLSTATE
  `22001`, message containing `Data too long for column 'name' at row N`, and no
  row mutation for the failed statement.
- In strict mode, if every byte beyond the stored UTF-8 prefix is an ASCII space,
  the statement succeeds, stores the prefix, and records note 1265
  `Data truncated for column 'name' at row N`.
- In non-strict mode, nonspace overlength text succeeds, stores the prefix, and
  records warning 1265.
- In non-strict mode, trailing-space overlength text follows the same note
  behavior as strict literal insertion.
- `INSERT IGNORE` demotes nonspace overlength literal text to warning 1265 even
  when strict mode is active.

`UPDATE` diagnostics are per matched row, not per changed row. If the truncated
value equals an existing value, MySQL reports zero affected rows but still
records the truncation diagnostic for the matched row.

### `INSERT ... SELECT`

For table-backed selected text-family source columns inserted into a smaller
text-family target descriptor:

- Strict mode treats both nonspace and trailing-space overlength selected values
  as error 1406 / SQLSTATE `22001`.
- Non-strict mode truncates, stores the valid prefix, and records warning 1265
  for each adjusted selected row.
- `INSERT IGNORE ... SELECT` uses the same adjustment path as non-strict mode for
  admitted selected values.

For row-scalar selected string expressions without a source descriptor,
trailing-space overlength values follow the literal note behavior. Nonspace
overlength values follow strict/non-strict/`IGNORE` behavior as literal values.

## Architecture

- Public API: unchanged. Successful statements return through the existing
  non-row `mylite_result` conventions with affected-row and warning-count data.
- Statement context and diagnostics: the DML conversion layer records existing
  MySQL-compatible warning/note/error conditions. No new diagnostics API is
  added.
- Parser/AST: unchanged.
- Analyzer/planner: target columns continue to resolve from MyLite catalog
  descriptors, not SQLite metadata.
- Catalog: unchanged. This feature must not mutate descriptors, descriptor
  versions, catalog generation, or SQLite schema generation.
- Runtime conversion: text-family conversion gains the same strict/non-strict
  adjustment policy shape already used by `CHAR`/`VARCHAR`, but with byte-limit
  semantics and valid UTF-8 prefix truncation.
- SQLite physical storage: generated MyLite user tables remain SQLite `TEXT`
  columns. MyLite binds the converted value; SQLite does not enforce MySQL text
  limits.
- Storage/VFS: unchanged. The `.mylite` preamble and shifted SQLite payload
  invariants must remain untouched.

## Diagnostics

Use the existing MyLite diagnostics where possible:

- Strict nonspace overlength literal or selected value: error 1406 / `22001`.
- Strict literal trailing-space overlength: note 1265.
- Non-strict nonspace overlength: warning 1265.
- Non-strict table-backed selected overlength: warning 1265.
- `INSERT IGNORE` adjusted overlength: warning 1265.
- Invalid UTF-8, embedded `NUL`, unsupported expression kind, and unsupported
  implicit conversion retain the existing MyLite-specific diagnostics.
- Allocation failures retain the existing `MYLITE_NOMEM` path.

## Verification Notes

Representative MySQL 8.4.9 observations:

```sql
CREATE TABLE dml_text (id INT NOT NULL, tt TINYTEXT, t TEXT, nn TINYTEXT NOT NULL);

SET sql_mode = 'STRICT_TRANS_TABLES';
INSERT INTO dml_text (id, tt, nn) VALUES (1, CONCAT(REPEAT('x',255),'y'), 'ok');
-- ERROR 1406 (22001): Data too long for column 'tt' at row 1

INSERT INTO dml_text (id, tt, nn) VALUES (1, CONCAT(REPEAT('x',255),' '), 'ok');
-- ROW_COUNT() = 1, @@warning_count = 1, SHOW WARNINGS has Note 1265.

SET sql_mode = '';
INSERT INTO dml_text (id, tt, t, nn)
VALUES (1, REPEAT('a',256), REPEAT('b',65536), REPEAT('c',256));
-- ROW_COUNT() = 1, @@warning_count = 3, stored lengths 255, 65535, 255.

UPDATE dml_text SET tt = CONCAT(REPEAT('q',255),'z');
-- Non-strict mode records one warning per matched row.

INSERT INTO dst_text SELECT id, t FROM src_text;
-- Strict table-backed overlength selected TEXT fails with 1406.
-- Non-strict table-backed overlength selected TEXT truncates with warnings.
```

## Compatibility Documentation

After implementation, update:

- `COMPATIBILITY.md`
- `docs/compatibility/sql-table-dml.md`
- `docs/compatibility/type-system-literals-conversion.md`

The docs must claim only text-family DML truncation for the admitted DML shapes
above. They must not imply full text expression coercion, BLOB-family parity,
character-set conversion, protocol information strings, or general expression
support.
