# Baseline WordPress Legacy Character Sets

## Status

This slice admits the legacy character-set surface that WordPress uses in
`wpdb::strip_invalid_text()` and charset metadata tests. It is a compatibility
bridge over MyLite's existing descriptor and scalar conversion layers, not a
general transcoding implementation.

Supported legacy character sets:

- `big5`
- `cp1251`
- `hebrew`
- `koi8r`
- `latin1`
- `tis620`
- `ujis`
- `utf8` as an alias for `utf8mb3`
- `utf8mb3`

Supported default collations:

- `big5_chinese_ci`
- `cp1251_general_ci`
- `hebrew_general_ci`
- `koi8r_general_ci`
- `latin1_swedish_ci`
- `tis620_thai_ci`
- `ujis_japanese_ci`
- `utf8mb3_general_ci`

Supported utf8 alias collations:

- `utf8_bin` canonicalizes to `utf8mb3_bin`
- `utf8_general_ci` canonicalizes to `utf8mb3_general_ci`
- `utf8_unicode_ci` canonicalizes to `utf8mb3_unicode_ci`

## Sources And Evidence

- MyLite architecture and engineering standards:
  - `README.md`
  - `AGENTS.md`
  - `docs/architecture/engineering-standards.md`
- Existing MyLite specifications:
  - `docs/specs/baseline-convert-using-charsets/specs.md`
  - `docs/specs/baseline-column-charset-collation-attributes/specs.md`
  - `docs/specs/baseline-table-charset-collation-surface/specs.md`
  - `docs/specs/wordpress-phpunit-mysqli-harness/specs.md`
- Official MySQL 8.4 Reference Manual:
  - Character sets and collations:
    <https://dev.mysql.com/doc/refman/8.4/en/charset.html>
  - Supported character sets and collations:
    <https://dev.mysql.com/doc/refman/8.4/en/charset-charsets.html>
  - `CONVERT()` and `CAST()`:
    <https://dev.mysql.com/doc/refman/8.4/en/cast-functions.html>
  - Connection character sets:
    <https://dev.mysql.com/doc/refman/8.4/en/charset-connection.html>
- Observed MySQL 8.4.9 runtime behavior from `mylite-mysql-849`.
- Focused WordPress PHPUnit verification through
  `tools/wordpress-phpunit-mysqli-mylite test --filter Tests_DB_Charset`.

This specification is independently authored from project documentation,
official MySQL 8.4 documentation, observed MySQL 8.4.9 runtime behavior,
WordPress test expectations, public SQLite APIs, and existing MyLite code. It
does not copy MySQL, MariaDB, Percona, SQLite, or WordPress implementation
source.

## Supported Scope

Metadata admission:

- `SHOW CHARACTER SET`, `SHOW COLLATION`, `INFORMATION_SCHEMA.CHARACTER_SETS`,
  `INFORMATION_SCHEMA.COLLATIONS`, and
  `INFORMATION_SCHEMA.COLLATION_CHARACTER_SET_APPLICABILITY` expose the
  admitted charset/collation rows.
- Table and column DDL may record admitted legacy charset/collation metadata for
  existing string descriptor paths where MyLite already accepts charset and
  collation attributes.
- `SHOW CREATE TABLE`, `SHOW FULL COLUMNS`, `INFORMATION_SCHEMA.COLUMNS`, and
  public result metadata report the canonical charset/collation names.
- `SET NAMES charset [COLLATE collation]` and `SET CHARACTER SET charset`
  update MyLite's session charset/collation readback for the admitted names.

Scalar and row-scalar conversion:

- `CONVERT(value USING charset)` accepts the admitted legacy charset names in
  no-source scalar contexts and single-table row-scalar projection contexts.
- `CONVERT(CONVERT(value USING charset) USING charset)` preserves the original
  bytes for admitted single-byte WordPress validation inputs.
- `CONVERT(LEFT(CONVERT(value USING charset), n) USING charset)` preserves
  single-byte legacy text for `cp1251`, `hebrew`, `koi8r`, and `tis620`.
- `CONVERT(LEFT(CONVERT(value USING big5), n) USING big5)` counts a Big5
  lead/trail byte pair as one character for the focused WordPress character
  truncation path.
- `CONVERT(LEFT(CONVERT(value USING binary), n) USING big5)` trims a trailing
  partial Big5 lead byte for the focused WordPress byte truncation path.
- `ujis` is admitted as a pass-through surface for WordPress's UTF-8 fixture
  values; MyLite does not transcode EUC-JP bytes in this slice.
- `latin1` remains ASCII-only in MyLite scalar conversion paths.

## Runtime Semantics

MyLite stores and returns raw byte buffers internally for admitted legacy
conversion results that are not valid UTF-8. The public result metadata still
uses MySQL var-string metadata and the requested MySQL charset/collation ids.
This avoids asking SQLite to validate legacy bytes as UTF-8 text while keeping
the compatibility behavior inside MyLite-owned scalar functions.

For `big5`, this slice implements only the WordPress truncation rules needed by
the verified test suite:

- bytes in the `0x81..0xfe` range are treated as Big5 lead bytes when followed
  by another byte;
- character-length `LEFT()` over a Big5 conversion counts a lead/trail pair as
  one character;
- final Big5 conversion of a byte-truncated value drops one trailing lead byte
  when the byte slice ends in an incomplete character.

These rules are not a full Big5 decoder and do not validate all legal or
illegal Big5 trail-byte ranges.

## Diagnostics

Unknown charset and collation names keep the existing MySQL-compatible
diagnostics:

- unknown charset: `1115 / 42000`
- unknown collation: `1273 / HY000`
- collation not valid for character set: `1253 / 42000`

Unsupported scalar inputs keep the deterministic MyLite capability diagnostics
from the existing conversion slices.

## Non-Goals

This slice does not implement:

- general character-set transcoding;
- full legacy charset validation;
- legacy charset storage conversion;
- collation-aware comparison, ordering, grouping, distinct, or index weights;
- literal introducers;
- protocol-level charset negotiation;
- mutable server-global charset or collation defaults;
- complete Big5, EUC-JP, or latin1 conversion semantics;
- SQLite fork changes.

## MySQL 8.4.9 Runtime Observations

Observed against `mylite-mysql-849`:

- `CONVERT(CONVERT(0xD86F7264D072657373 USING cp1251) USING cp1251)` returns
  bytes `D86F7264D072657373`, charset `cp1251`, and collation
  `cp1251_general_ci`.
- `CONVERT(LEFT(CONVERT(0x61AA406261AA406261AA406261AA4062 USING big5), 10)
  USING big5)` returns bytes `61AA406261AA406261AA406261`.
- `CONVERT(LEFT(CONVERT(_utf8mb4 text USING ujis), 4) USING utf8mb4)` returns
  the first four UTF-8 characters for the verified WordPress fixture shape.

The WordPress `big5` byte-length fixture uses the connection/literal path from
`wpdb::strip_invalid_text()` and expects incomplete trailing Big5 byte
sequences to be removed rather than returned as partial characters. MyLite
implements that focused bridge behavior without claiming full MySQL Big5
transcoding.

## Tests

Fast C coverage:

- legacy charset/collation catalog and descriptor metadata admission;
- `SET NAMES` readback for admitted legacy charsets;
- row-scalar nested charset conversion with invalid single-byte values;
- scalar nested cp1251 conversion without a `FROM` clause;
- Big5 character truncation and byte truncation partial-character cleanup.

Integration coverage:

- `tools/wordpress-phpunit-mysqli-mylite test --filter Tests_DB_Charset`
  passes with the upstream WordPress skipped test.
