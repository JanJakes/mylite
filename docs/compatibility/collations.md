# Collations and comparison behavior

The collation catalog below is the MySQL Community Server 8.4.9 default
catalog from `INFORMATION_SCHEMA.COLLATIONS`. It is the MyLite target catalog
and should not vary by host system. Custom MySQL builds can add or change
collations; those are out of scope until tracked explicitly. All listed rows
are compiled in MySQL 8.4.9.

| Feature | Status | Notes |
| --- | --- | --- |
| Collation catalog entries | 🟡 | Metadata-only MySQL 8.4.9 `SHOW COLLATION` rows, `INFORMATION_SCHEMA.COLLATIONS` metadata, and `INFORMATION_SCHEMA.COLLATION_CHARACTER_SET_APPLICABILITY` mappings for all 286 target collation rows; catalog visibility does not imply DDL/default admission, conversion, storage, comparison semantics, or `mysql.collations` |
| Default collation selection | 🟡 | Limited `CREATE DATABASE` / `ALTER DATABASE` descriptor-owned schema default collations for admitted `utf8mb4`, `ascii`, and `binary` defaults; limited `CREATE TABLE`, `CREATE TABLE ... LIKE`, `ALTER TABLE ... [DEFAULT] COLLATE`, same-`utf8mb4` `ALTER TABLE ... CONVERT TO CHARACTER SET`, `SHOW CREATE TABLE`, `SHOW TABLE STATUS`, and `INFORMATION_SCHEMA.TABLES` preservation for admitted `utf8mb4` and `ascii` table default collations; limited `CREATE TABLE` / `CREATE TEMPORARY TABLE` inherited `COLLATE binary` defaults from table or schema metadata normalize inherited non-national `CHAR`, `VARCHAR`, and bare `TEXT` family descriptors to binary string/BLOB descriptors and store table collation `binary`; limited explicit column `COLLATE` metadata for admitted `utf8mb4` and `ascii` collations on `CHAR`, `VARCHAR`, and bare `TEXT` family descriptors where the host DDL action admits that type; limited explicit column `COLLATE binary` normalization for admitted `CHAR`, `VARCHAR`, and bare `TEXT` family definitions to existing binary string/BLOB descriptors; limited `SET NAMES utf8mb4 COLLATE admitted_collation` updates session readback; scalar `@@collation_server` remains fixed while session/local `@@collation_database` follows the selected schema descriptor; admitted `CHAR` / `VARCHAR` primary and unique key enforcement still uses MyLite's fixed ASCII subset of `utf8mb4_0900_ai_ci`; no cross-character-set conversion, general collation comparison/order/group/distinct semantics, full Unicode weights, mutable server-global state, or DDL/default admission for catalog-only collations |
| Function result introspection | 🟡 | Limited `CHARSET()`, `COLLATION()`, and `COERCIBILITY()` support for admitted scalar arguments, descriptor-backed column metadata, scalar and single-table row-scalar `CONVERT(value USING utf8mb4\|utf8\|utf8mb3\|latin1)` metadata, scalar postfix `COLLATE` metadata for admitted `utf8mb4`, `utf8mb3`, and `latin1` collations, and current explicit-collation scalar `CONCAT()` metadata/conflict shapes in no-source, `DUAL`, `DO`, and single-table row-scalar `SELECT` projection contexts; no full expression metadata, general conversion, non-ASCII `latin1` transcoding, or comparison semantics |
| Unicode Collation Algorithm families | ❌ | UCA families and sensitivity |
| Binary collations | 🟡 | Full metadata catalog visibility plus descriptor-owned schema defaults through supported `CREATE DATABASE` / `ALTER DATABASE` option forms, explicit column-level `COLLATE binary` normalization to binary string/BLOB descriptors, and limited `CREATE TABLE` / `CREATE TEMPORARY TABLE` inherited table-default `COLLATE binary` behavior for admitted string descriptors, plus metadata admission and session/table/column default preservation for `utf8mb4_bin` and `utf8mb4_0900_bin`; no binary comparison, `ALTER TABLE ... DEFAULT COLLATE binary`, `NO PAD` trailing-space comparison, or ordering semantics |
| PAD SPACE and NO PAD collations | 🟡 | Limited default-mode `CHAR` storage/readback trims trailing spaces as verified for MySQL 8.4.9, full metadata catalog rows report MySQL 8.4.9 PAD attributes, and admitted ASCII `CHAR` / `VARCHAR` primary and unique keys use MyLite's fixed ASCII key-collation subset; no general comparison semantics, non-ASCII string-key collation weights, or `PAD_CHAR_TO_FULL_LENGTH` mode |
| Collation coercibility rules | 🟡 | Limited `COERCIBILITY()` metadata values for admitted scalar expressions, scalar postfix `COLLATE`, scalar and single-table row-scalar `CONVERT(... USING ...)`, and descriptor-backed columns; no coercibility-driven comparison, ordering, grouping, distinct, set-operation, conversion, or full expression collation resolution |

## MySQL 8.4.9 default collation catalog

| Collation | Status | Notes |
| --- | --- | --- |
| `armscii8_general_ci` | 🟡 | Metadata-only catalog row; armscii8; id 32; default; sortlen 1; PAD SPACE; no DDL or default admission, conversion, storage, or comparison semantics |
| `armscii8_bin` | 🟡 | Metadata-only catalog row; armscii8; id 64; sortlen 1; PAD SPACE; no DDL or default admission, conversion, storage, or comparison semantics |
| `ascii_general_ci` | 🟡 | Limited static catalog row, schema/table/column default preservation for admitted string descriptors, and result-column collation metadata; ascii; id 11; default; sortlen 1; PAD SPACE; no full ascii comparison semantics |
| `ascii_bin` | 🟡 | Limited static catalog row, schema/table/column default preservation for admitted string descriptors, and result-column collation metadata; ascii; id 65; sortlen 1; PAD SPACE; no full ascii binary comparison semantics |
| `big5_chinese_ci` | 🟡 | Metadata-only catalog row; big5; id 1; default; sortlen 1; PAD SPACE; no DDL or default admission, conversion, storage, or comparison semantics |
| `big5_bin` | 🟡 | Metadata-only catalog row; big5; id 84; sortlen 1; PAD SPACE; no DDL or default admission, conversion, storage, or comparison semantics |
| `binary` | 🟡 | Limited static catalog row, schema-default metadata through supported `CREATE DATABASE` / `ALTER DATABASE` options, explicit column-level normalization for admitted `CHAR`, `VARCHAR`, and bare `TEXT` family definitions, and limited `CREATE TABLE` / `CREATE TEMPORARY TABLE` inherited default behavior for inherited non-national string descriptors; binary; id 63; default; sortlen 1; NO PAD; no `ALTER TABLE` binary default or binary comparison semantics |
| `cp1250_general_ci` | 🟡 | Metadata-only catalog row; cp1250; id 26; default; sortlen 1; PAD SPACE; no DDL or default admission, conversion, storage, or comparison semantics |
| `cp1250_czech_cs` | 🟡 | Metadata-only catalog row; cp1250; id 34; sortlen 2; PAD SPACE; no DDL or default admission, conversion, storage, or comparison semantics |
| `cp1250_croatian_ci` | 🟡 | Metadata-only catalog row; cp1250; id 44; sortlen 1; PAD SPACE; no DDL or default admission, conversion, storage, or comparison semantics |
| `cp1250_bin` | 🟡 | Metadata-only catalog row; cp1250; id 66; sortlen 1; PAD SPACE; no DDL or default admission, conversion, storage, or comparison semantics |
| `cp1250_polish_ci` | 🟡 | Metadata-only catalog row; cp1250; id 99; sortlen 1; PAD SPACE; no DDL or default admission, conversion, storage, or comparison semantics |
| `cp1251_bulgarian_ci` | 🟡 | Metadata-only catalog row; cp1251; id 14; sortlen 1; PAD SPACE; no DDL or default admission, conversion, storage, or comparison semantics |
| `cp1251_ukrainian_ci` | 🟡 | Metadata-only catalog row; cp1251; id 23; sortlen 1; PAD SPACE; no DDL or default admission, conversion, storage, or comparison semantics |
| `cp1251_bin` | 🟡 | Metadata-only catalog row; cp1251; id 50; sortlen 1; PAD SPACE; no DDL or default admission, conversion, storage, or comparison semantics |
| `cp1251_general_ci` | 🟡 | Metadata-only catalog row; cp1251; id 51; default; sortlen 1; PAD SPACE; no DDL or default admission, conversion, storage, or comparison semantics |
| `cp1251_general_cs` | 🟡 | Metadata-only catalog row; cp1251; id 52; sortlen 1; PAD SPACE; no DDL or default admission, conversion, storage, or comparison semantics |
| `cp1256_general_ci` | 🟡 | Metadata-only catalog row; cp1256; id 57; default; sortlen 1; PAD SPACE; no DDL or default admission, conversion, storage, or comparison semantics |
| `cp1256_bin` | 🟡 | Metadata-only catalog row; cp1256; id 67; sortlen 1; PAD SPACE; no DDL or default admission, conversion, storage, or comparison semantics |
| `cp1257_lithuanian_ci` | 🟡 | Metadata-only catalog row; cp1257; id 29; sortlen 1; PAD SPACE; no DDL or default admission, conversion, storage, or comparison semantics |
| `cp1257_bin` | 🟡 | Metadata-only catalog row; cp1257; id 58; sortlen 1; PAD SPACE; no DDL or default admission, conversion, storage, or comparison semantics |
| `cp1257_general_ci` | 🟡 | Metadata-only catalog row; cp1257; id 59; default; sortlen 1; PAD SPACE; no DDL or default admission, conversion, storage, or comparison semantics |
| `cp850_general_ci` | 🟡 | Metadata-only catalog row; cp850; id 4; default; sortlen 1; PAD SPACE; no DDL or default admission, conversion, storage, or comparison semantics |
| `cp850_bin` | 🟡 | Metadata-only catalog row; cp850; id 80; sortlen 1; PAD SPACE; no DDL or default admission, conversion, storage, or comparison semantics |
| `cp852_general_ci` | 🟡 | Metadata-only catalog row; cp852; id 40; default; sortlen 1; PAD SPACE; no DDL or default admission, conversion, storage, or comparison semantics |
| `cp852_bin` | 🟡 | Metadata-only catalog row; cp852; id 81; sortlen 1; PAD SPACE; no DDL or default admission, conversion, storage, or comparison semantics |
| `cp866_general_ci` | 🟡 | Metadata-only catalog row; cp866; id 36; default; sortlen 1; PAD SPACE; no DDL or default admission, conversion, storage, or comparison semantics |
| `cp866_bin` | 🟡 | Metadata-only catalog row; cp866; id 68; sortlen 1; PAD SPACE; no DDL or default admission, conversion, storage, or comparison semantics |
| `cp932_japanese_ci` | 🟡 | Metadata-only catalog row; cp932; id 95; default; sortlen 1; PAD SPACE; no DDL or default admission, conversion, storage, or comparison semantics |
| `cp932_bin` | 🟡 | Metadata-only catalog row; cp932; id 96; sortlen 1; PAD SPACE; no DDL or default admission, conversion, storage, or comparison semantics |
| `dec8_swedish_ci` | 🟡 | Metadata-only catalog row; dec8; id 3; default; sortlen 1; PAD SPACE; no DDL or default admission, conversion, storage, or comparison semantics |
| `dec8_bin` | 🟡 | Metadata-only catalog row; dec8; id 69; sortlen 1; PAD SPACE; no DDL or default admission, conversion, storage, or comparison semantics |
| `eucjpms_japanese_ci` | 🟡 | Metadata-only catalog row; eucjpms; id 97; default; sortlen 1; PAD SPACE; no DDL or default admission, conversion, storage, or comparison semantics |
| `eucjpms_bin` | 🟡 | Metadata-only catalog row; eucjpms; id 98; sortlen 1; PAD SPACE; no DDL or default admission, conversion, storage, or comparison semantics |
| `euckr_korean_ci` | 🟡 | Metadata-only catalog row; euckr; id 19; default; sortlen 1; PAD SPACE; no DDL or default admission, conversion, storage, or comparison semantics |
| `euckr_bin` | 🟡 | Metadata-only catalog row; euckr; id 85; sortlen 1; PAD SPACE; no DDL or default admission, conversion, storage, or comparison semantics |
| `gb18030_chinese_ci` | 🟡 | Metadata-only catalog row; gb18030; id 248; default; sortlen 2; PAD SPACE; no DDL or default admission, conversion, storage, or comparison semantics |
| `gb18030_bin` | 🟡 | Metadata-only catalog row; gb18030; id 249; sortlen 1; PAD SPACE; no DDL or default admission, conversion, storage, or comparison semantics |
| `gb18030_unicode_520_ci` | 🟡 | Metadata-only catalog row; gb18030; id 250; sortlen 8; PAD SPACE; no DDL or default admission, conversion, storage, or comparison semantics |
| `gb2312_chinese_ci` | 🟡 | Metadata-only catalog row; gb2312; id 24; default; sortlen 1; PAD SPACE; no DDL or default admission, conversion, storage, or comparison semantics |
| `gb2312_bin` | 🟡 | Metadata-only catalog row; gb2312; id 86; sortlen 1; PAD SPACE; no DDL or default admission, conversion, storage, or comparison semantics |
| `gbk_chinese_ci` | 🟡 | Metadata-only catalog row; gbk; id 28; default; sortlen 1; PAD SPACE; no DDL or default admission, conversion, storage, or comparison semantics |
| `gbk_bin` | 🟡 | Metadata-only catalog row; gbk; id 87; sortlen 1; PAD SPACE; no DDL or default admission, conversion, storage, or comparison semantics |
| `geostd8_general_ci` | 🟡 | Metadata-only catalog row; geostd8; id 92; default; sortlen 1; PAD SPACE; no DDL or default admission, conversion, storage, or comparison semantics |
| `geostd8_bin` | 🟡 | Metadata-only catalog row; geostd8; id 93; sortlen 1; PAD SPACE; no DDL or default admission, conversion, storage, or comparison semantics |
| `greek_general_ci` | 🟡 | Metadata-only catalog row; greek; id 25; default; sortlen 1; PAD SPACE; no DDL or default admission, conversion, storage, or comparison semantics |
| `greek_bin` | 🟡 | Metadata-only catalog row; greek; id 70; sortlen 1; PAD SPACE; no DDL or default admission, conversion, storage, or comparison semantics |
| `hebrew_general_ci` | 🟡 | Metadata-only catalog row; hebrew; id 16; default; sortlen 1; PAD SPACE; no DDL or default admission, conversion, storage, or comparison semantics |
| `hebrew_bin` | 🟡 | Metadata-only catalog row; hebrew; id 71; sortlen 1; PAD SPACE; no DDL or default admission, conversion, storage, or comparison semantics |
| `hp8_english_ci` | 🟡 | Metadata-only catalog row; hp8; id 6; default; sortlen 1; PAD SPACE; no DDL or default admission, conversion, storage, or comparison semantics |
| `hp8_bin` | 🟡 | Metadata-only catalog row; hp8; id 72; sortlen 1; PAD SPACE; no DDL or default admission, conversion, storage, or comparison semantics |
| `keybcs2_general_ci` | 🟡 | Metadata-only catalog row; keybcs2; id 37; default; sortlen 1; PAD SPACE; no DDL or default admission, conversion, storage, or comparison semantics |
| `keybcs2_bin` | 🟡 | Metadata-only catalog row; keybcs2; id 73; sortlen 1; PAD SPACE; no DDL or default admission, conversion, storage, or comparison semantics |
| `koi8r_general_ci` | 🟡 | Metadata-only catalog row; koi8r; id 7; default; sortlen 1; PAD SPACE; no DDL or default admission, conversion, storage, or comparison semantics |
| `koi8r_bin` | 🟡 | Metadata-only catalog row; koi8r; id 74; sortlen 1; PAD SPACE; no DDL or default admission, conversion, storage, or comparison semantics |
| `koi8u_general_ci` | 🟡 | Metadata-only catalog row; koi8u; id 22; default; sortlen 1; PAD SPACE; no DDL or default admission, conversion, storage, or comparison semantics |
| `koi8u_bin` | 🟡 | Metadata-only catalog row; koi8u; id 75; sortlen 1; PAD SPACE; no DDL or default admission, conversion, storage, or comparison semantics |
| `latin1_german1_ci` | 🟡 | Metadata-only catalog row; latin1; id 5; sortlen 1; PAD SPACE; no DDL or default admission, conversion, storage, or comparison semantics |
| `latin1_swedish_ci` | 🟡 | Metadata catalog row plus limited scalar `CONVERT(value USING latin1)` default metadata and scalar postfix `COLLATE latin1_swedish_ci` validation for ASCII scalar values; latin1; id 8; default; sortlen 1; PAD SPACE; no DDL/default admission, non-ASCII transcoding, storage, or comparison semantics |
| `latin1_danish_ci` | 🟡 | Metadata-only catalog row; latin1; id 15; sortlen 1; PAD SPACE; no DDL or default admission, conversion, storage, or comparison semantics |
| `latin1_german2_ci` | 🟡 | Metadata-only catalog row; latin1; id 31; sortlen 2; PAD SPACE; no DDL or default admission, conversion, storage, or comparison semantics |
| `latin1_bin` | 🟡 | Metadata catalog row plus limited scalar postfix `COLLATE latin1_bin` metadata for ASCII `latin1` conversion results; latin1; id 47; sortlen 1; PAD SPACE; no DDL/default admission, non-ASCII transcoding, storage, or comparison semantics |
| `latin1_general_ci` | 🟡 | Metadata-only catalog row; latin1; id 48; sortlen 1; PAD SPACE; no DDL or default admission, conversion, storage, or comparison semantics |
| `latin1_general_cs` | 🟡 | Metadata-only catalog row; latin1; id 49; sortlen 1; PAD SPACE; no DDL or default admission, conversion, storage, or comparison semantics |
| `latin1_spanish_ci` | 🟡 | Metadata-only catalog row; latin1; id 94; sortlen 1; PAD SPACE; no DDL or default admission, conversion, storage, or comparison semantics |
| `latin2_czech_cs` | 🟡 | Metadata-only catalog row; latin2; id 2; sortlen 4; PAD SPACE; no DDL or default admission, conversion, storage, or comparison semantics |
| `latin2_general_ci` | 🟡 | Metadata-only catalog row; latin2; id 9; default; sortlen 1; PAD SPACE; no DDL or default admission, conversion, storage, or comparison semantics |
| `latin2_hungarian_ci` | 🟡 | Metadata-only catalog row; latin2; id 21; sortlen 1; PAD SPACE; no DDL or default admission, conversion, storage, or comparison semantics |
| `latin2_croatian_ci` | 🟡 | Metadata-only catalog row; latin2; id 27; sortlen 1; PAD SPACE; no DDL or default admission, conversion, storage, or comparison semantics |
| `latin2_bin` | 🟡 | Metadata-only catalog row; latin2; id 77; sortlen 1; PAD SPACE; no DDL or default admission, conversion, storage, or comparison semantics |
| `latin5_turkish_ci` | 🟡 | Metadata-only catalog row; latin5; id 30; default; sortlen 1; PAD SPACE; no DDL or default admission, conversion, storage, or comparison semantics |
| `latin5_bin` | 🟡 | Metadata-only catalog row; latin5; id 78; sortlen 1; PAD SPACE; no DDL or default admission, conversion, storage, or comparison semantics |
| `latin7_estonian_cs` | 🟡 | Metadata-only catalog row; latin7; id 20; sortlen 1; PAD SPACE; no DDL or default admission, conversion, storage, or comparison semantics |
| `latin7_general_ci` | 🟡 | Metadata-only catalog row; latin7; id 41; default; sortlen 1; PAD SPACE; no DDL or default admission, conversion, storage, or comparison semantics |
| `latin7_general_cs` | 🟡 | Metadata-only catalog row; latin7; id 42; sortlen 1; PAD SPACE; no DDL or default admission, conversion, storage, or comparison semantics |
| `latin7_bin` | 🟡 | Metadata-only catalog row; latin7; id 79; sortlen 1; PAD SPACE; no DDL or default admission, conversion, storage, or comparison semantics |
| `macce_general_ci` | 🟡 | Metadata-only catalog row; macce; id 38; default; sortlen 1; PAD SPACE; no DDL or default admission, conversion, storage, or comparison semantics |
| `macce_bin` | 🟡 | Metadata-only catalog row; macce; id 43; sortlen 1; PAD SPACE; no DDL or default admission, conversion, storage, or comparison semantics |
| `macroman_general_ci` | 🟡 | Metadata-only catalog row; macroman; id 39; default; sortlen 1; PAD SPACE; no DDL or default admission, conversion, storage, or comparison semantics |
| `macroman_bin` | 🟡 | Metadata-only catalog row; macroman; id 53; sortlen 1; PAD SPACE; no DDL or default admission, conversion, storage, or comparison semantics |
| `sjis_japanese_ci` | 🟡 | Metadata-only catalog row; sjis; id 13; default; sortlen 1; PAD SPACE; no DDL or default admission, conversion, storage, or comparison semantics |
| `sjis_bin` | 🟡 | Metadata-only catalog row; sjis; id 88; sortlen 1; PAD SPACE; no DDL or default admission, conversion, storage, or comparison semantics |
| `swe7_swedish_ci` | 🟡 | Metadata-only catalog row; swe7; id 10; default; sortlen 1; PAD SPACE; no DDL or default admission, conversion, storage, or comparison semantics |
| `swe7_bin` | 🟡 | Metadata-only catalog row; swe7; id 82; sortlen 1; PAD SPACE; no DDL or default admission, conversion, storage, or comparison semantics |
| `tis620_thai_ci` | 🟡 | Metadata-only catalog row; tis620; id 18; default; sortlen 4; PAD SPACE; no DDL or default admission, conversion, storage, or comparison semantics |
| `tis620_bin` | 🟡 | Metadata-only catalog row; tis620; id 89; sortlen 1; PAD SPACE; no DDL or default admission, conversion, storage, or comparison semantics |
| `ucs2_general_ci` | 🟡 | Metadata-only catalog row; ucs2; id 35; default; sortlen 1; PAD SPACE; no DDL or default admission, conversion, storage, or comparison semantics |
| `ucs2_bin` | 🟡 | Metadata-only catalog row; ucs2; id 90; sortlen 1; PAD SPACE; no DDL or default admission, conversion, storage, or comparison semantics |
| `ucs2_unicode_ci` | 🟡 | Metadata-only catalog row; ucs2; id 128; sortlen 8; PAD SPACE; no DDL or default admission, conversion, storage, or comparison semantics |
| `ucs2_icelandic_ci` | 🟡 | Metadata-only catalog row; ucs2; id 129; sortlen 8; PAD SPACE; no DDL or default admission, conversion, storage, or comparison semantics |
| `ucs2_latvian_ci` | 🟡 | Metadata-only catalog row; ucs2; id 130; sortlen 8; PAD SPACE; no DDL or default admission, conversion, storage, or comparison semantics |
| `ucs2_romanian_ci` | 🟡 | Metadata-only catalog row; ucs2; id 131; sortlen 8; PAD SPACE; no DDL or default admission, conversion, storage, or comparison semantics |
| `ucs2_slovenian_ci` | 🟡 | Metadata-only catalog row; ucs2; id 132; sortlen 8; PAD SPACE; no DDL or default admission, conversion, storage, or comparison semantics |
| `ucs2_polish_ci` | 🟡 | Metadata-only catalog row; ucs2; id 133; sortlen 8; PAD SPACE; no DDL or default admission, conversion, storage, or comparison semantics |
| `ucs2_estonian_ci` | 🟡 | Metadata-only catalog row; ucs2; id 134; sortlen 8; PAD SPACE; no DDL or default admission, conversion, storage, or comparison semantics |
| `ucs2_spanish_ci` | 🟡 | Metadata-only catalog row; ucs2; id 135; sortlen 8; PAD SPACE; no DDL or default admission, conversion, storage, or comparison semantics |
| `ucs2_swedish_ci` | 🟡 | Metadata-only catalog row; ucs2; id 136; sortlen 8; PAD SPACE; no DDL or default admission, conversion, storage, or comparison semantics |
| `ucs2_turkish_ci` | 🟡 | Metadata-only catalog row; ucs2; id 137; sortlen 8; PAD SPACE; no DDL or default admission, conversion, storage, or comparison semantics |
| `ucs2_czech_ci` | 🟡 | Metadata-only catalog row; ucs2; id 138; sortlen 8; PAD SPACE; no DDL or default admission, conversion, storage, or comparison semantics |
| `ucs2_danish_ci` | 🟡 | Metadata-only catalog row; ucs2; id 139; sortlen 8; PAD SPACE; no DDL or default admission, conversion, storage, or comparison semantics |
| `ucs2_lithuanian_ci` | 🟡 | Metadata-only catalog row; ucs2; id 140; sortlen 8; PAD SPACE; no DDL or default admission, conversion, storage, or comparison semantics |
| `ucs2_slovak_ci` | 🟡 | Metadata-only catalog row; ucs2; id 141; sortlen 8; PAD SPACE; no DDL or default admission, conversion, storage, or comparison semantics |
| `ucs2_spanish2_ci` | 🟡 | Metadata-only catalog row; ucs2; id 142; sortlen 8; PAD SPACE; no DDL or default admission, conversion, storage, or comparison semantics |
| `ucs2_roman_ci` | 🟡 | Metadata-only catalog row; ucs2; id 143; sortlen 8; PAD SPACE; no DDL or default admission, conversion, storage, or comparison semantics |
| `ucs2_persian_ci` | 🟡 | Metadata-only catalog row; ucs2; id 144; sortlen 8; PAD SPACE; no DDL or default admission, conversion, storage, or comparison semantics |
| `ucs2_esperanto_ci` | 🟡 | Metadata-only catalog row; ucs2; id 145; sortlen 8; PAD SPACE; no DDL or default admission, conversion, storage, or comparison semantics |
| `ucs2_hungarian_ci` | 🟡 | Metadata-only catalog row; ucs2; id 146; sortlen 8; PAD SPACE; no DDL or default admission, conversion, storage, or comparison semantics |
| `ucs2_sinhala_ci` | 🟡 | Metadata-only catalog row; ucs2; id 147; sortlen 8; PAD SPACE; no DDL or default admission, conversion, storage, or comparison semantics |
| `ucs2_german2_ci` | 🟡 | Metadata-only catalog row; ucs2; id 148; sortlen 8; PAD SPACE; no DDL or default admission, conversion, storage, or comparison semantics |
| `ucs2_croatian_ci` | 🟡 | Metadata-only catalog row; ucs2; id 149; sortlen 8; PAD SPACE; no DDL or default admission, conversion, storage, or comparison semantics |
| `ucs2_unicode_520_ci` | 🟡 | Metadata-only catalog row; ucs2; id 150; sortlen 8; PAD SPACE; no DDL or default admission, conversion, storage, or comparison semantics |
| `ucs2_vietnamese_ci` | 🟡 | Metadata-only catalog row; ucs2; id 151; sortlen 8; PAD SPACE; no DDL or default admission, conversion, storage, or comparison semantics |
| `ucs2_general_mysql500_ci` | 🟡 | Metadata-only catalog row; ucs2; id 159; sortlen 1; PAD SPACE; no DDL or default admission, conversion, storage, or comparison semantics |
| `ujis_japanese_ci` | 🟡 | Metadata-only catalog row; ujis; id 12; default; sortlen 1; PAD SPACE; no DDL or default admission, conversion, storage, or comparison semantics |
| `ujis_bin` | 🟡 | Metadata-only catalog row; ujis; id 91; sortlen 1; PAD SPACE; no DDL or default admission, conversion, storage, or comparison semantics |
| `utf16_general_ci` | 🟡 | Metadata-only catalog row; utf16; id 54; default; sortlen 1; PAD SPACE; no DDL or default admission, conversion, storage, or comparison semantics |
| `utf16_bin` | 🟡 | Metadata-only catalog row; utf16; id 55; sortlen 1; PAD SPACE; no DDL or default admission, conversion, storage, or comparison semantics |
| `utf16_unicode_ci` | 🟡 | Metadata-only catalog row; utf16; id 101; sortlen 8; PAD SPACE; no DDL or default admission, conversion, storage, or comparison semantics |
| `utf16_icelandic_ci` | 🟡 | Metadata-only catalog row; utf16; id 102; sortlen 8; PAD SPACE; no DDL or default admission, conversion, storage, or comparison semantics |
| `utf16_latvian_ci` | 🟡 | Metadata-only catalog row; utf16; id 103; sortlen 8; PAD SPACE; no DDL or default admission, conversion, storage, or comparison semantics |
| `utf16_romanian_ci` | 🟡 | Metadata-only catalog row; utf16; id 104; sortlen 8; PAD SPACE; no DDL or default admission, conversion, storage, or comparison semantics |
| `utf16_slovenian_ci` | 🟡 | Metadata-only catalog row; utf16; id 105; sortlen 8; PAD SPACE; no DDL or default admission, conversion, storage, or comparison semantics |
| `utf16_polish_ci` | 🟡 | Metadata-only catalog row; utf16; id 106; sortlen 8; PAD SPACE; no DDL or default admission, conversion, storage, or comparison semantics |
| `utf16_estonian_ci` | 🟡 | Metadata-only catalog row; utf16; id 107; sortlen 8; PAD SPACE; no DDL or default admission, conversion, storage, or comparison semantics |
| `utf16_spanish_ci` | 🟡 | Metadata-only catalog row; utf16; id 108; sortlen 8; PAD SPACE; no DDL or default admission, conversion, storage, or comparison semantics |
| `utf16_swedish_ci` | 🟡 | Metadata-only catalog row; utf16; id 109; sortlen 8; PAD SPACE; no DDL or default admission, conversion, storage, or comparison semantics |
| `utf16_turkish_ci` | 🟡 | Metadata-only catalog row; utf16; id 110; sortlen 8; PAD SPACE; no DDL or default admission, conversion, storage, or comparison semantics |
| `utf16_czech_ci` | 🟡 | Metadata-only catalog row; utf16; id 111; sortlen 8; PAD SPACE; no DDL or default admission, conversion, storage, or comparison semantics |
| `utf16_danish_ci` | 🟡 | Metadata-only catalog row; utf16; id 112; sortlen 8; PAD SPACE; no DDL or default admission, conversion, storage, or comparison semantics |
| `utf16_lithuanian_ci` | 🟡 | Metadata-only catalog row; utf16; id 113; sortlen 8; PAD SPACE; no DDL or default admission, conversion, storage, or comparison semantics |
| `utf16_slovak_ci` | 🟡 | Metadata-only catalog row; utf16; id 114; sortlen 8; PAD SPACE; no DDL or default admission, conversion, storage, or comparison semantics |
| `utf16_spanish2_ci` | 🟡 | Metadata-only catalog row; utf16; id 115; sortlen 8; PAD SPACE; no DDL or default admission, conversion, storage, or comparison semantics |
| `utf16_roman_ci` | 🟡 | Metadata-only catalog row; utf16; id 116; sortlen 8; PAD SPACE; no DDL or default admission, conversion, storage, or comparison semantics |
| `utf16_persian_ci` | 🟡 | Metadata-only catalog row; utf16; id 117; sortlen 8; PAD SPACE; no DDL or default admission, conversion, storage, or comparison semantics |
| `utf16_esperanto_ci` | 🟡 | Metadata-only catalog row; utf16; id 118; sortlen 8; PAD SPACE; no DDL or default admission, conversion, storage, or comparison semantics |
| `utf16_hungarian_ci` | 🟡 | Metadata-only catalog row; utf16; id 119; sortlen 8; PAD SPACE; no DDL or default admission, conversion, storage, or comparison semantics |
| `utf16_sinhala_ci` | 🟡 | Metadata-only catalog row; utf16; id 120; sortlen 8; PAD SPACE; no DDL or default admission, conversion, storage, or comparison semantics |
| `utf16_german2_ci` | 🟡 | Metadata-only catalog row; utf16; id 121; sortlen 8; PAD SPACE; no DDL or default admission, conversion, storage, or comparison semantics |
| `utf16_croatian_ci` | 🟡 | Metadata-only catalog row; utf16; id 122; sortlen 8; PAD SPACE; no DDL or default admission, conversion, storage, or comparison semantics |
| `utf16_unicode_520_ci` | 🟡 | Metadata-only catalog row; utf16; id 123; sortlen 8; PAD SPACE; no DDL or default admission, conversion, storage, or comparison semantics |
| `utf16_vietnamese_ci` | 🟡 | Metadata-only catalog row; utf16; id 124; sortlen 8; PAD SPACE; no DDL or default admission, conversion, storage, or comparison semantics |
| `utf16le_general_ci` | 🟡 | Metadata-only catalog row; utf16le; id 56; default; sortlen 1; PAD SPACE; no DDL or default admission, conversion, storage, or comparison semantics |
| `utf16le_bin` | 🟡 | Metadata-only catalog row; utf16le; id 62; sortlen 1; PAD SPACE; no DDL or default admission, conversion, storage, or comparison semantics |
| `utf32_general_ci` | 🟡 | Metadata-only catalog row; utf32; id 60; default; sortlen 1; PAD SPACE; no DDL or default admission, conversion, storage, or comparison semantics |
| `utf32_bin` | 🟡 | Metadata-only catalog row; utf32; id 61; sortlen 1; PAD SPACE; no DDL or default admission, conversion, storage, or comparison semantics |
| `utf32_unicode_ci` | 🟡 | Metadata-only catalog row; utf32; id 160; sortlen 8; PAD SPACE; no DDL or default admission, conversion, storage, or comparison semantics |
| `utf32_icelandic_ci` | 🟡 | Metadata-only catalog row; utf32; id 161; sortlen 8; PAD SPACE; no DDL or default admission, conversion, storage, or comparison semantics |
| `utf32_latvian_ci` | 🟡 | Metadata-only catalog row; utf32; id 162; sortlen 8; PAD SPACE; no DDL or default admission, conversion, storage, or comparison semantics |
| `utf32_romanian_ci` | 🟡 | Metadata-only catalog row; utf32; id 163; sortlen 8; PAD SPACE; no DDL or default admission, conversion, storage, or comparison semantics |
| `utf32_slovenian_ci` | 🟡 | Metadata-only catalog row; utf32; id 164; sortlen 8; PAD SPACE; no DDL or default admission, conversion, storage, or comparison semantics |
| `utf32_polish_ci` | 🟡 | Metadata-only catalog row; utf32; id 165; sortlen 8; PAD SPACE; no DDL or default admission, conversion, storage, or comparison semantics |
| `utf32_estonian_ci` | 🟡 | Metadata-only catalog row; utf32; id 166; sortlen 8; PAD SPACE; no DDL or default admission, conversion, storage, or comparison semantics |
| `utf32_spanish_ci` | 🟡 | Metadata-only catalog row; utf32; id 167; sortlen 8; PAD SPACE; no DDL or default admission, conversion, storage, or comparison semantics |
| `utf32_swedish_ci` | 🟡 | Metadata-only catalog row; utf32; id 168; sortlen 8; PAD SPACE; no DDL or default admission, conversion, storage, or comparison semantics |
| `utf32_turkish_ci` | 🟡 | Metadata-only catalog row; utf32; id 169; sortlen 8; PAD SPACE; no DDL or default admission, conversion, storage, or comparison semantics |
| `utf32_czech_ci` | 🟡 | Metadata-only catalog row; utf32; id 170; sortlen 8; PAD SPACE; no DDL or default admission, conversion, storage, or comparison semantics |
| `utf32_danish_ci` | 🟡 | Metadata-only catalog row; utf32; id 171; sortlen 8; PAD SPACE; no DDL or default admission, conversion, storage, or comparison semantics |
| `utf32_lithuanian_ci` | 🟡 | Metadata-only catalog row; utf32; id 172; sortlen 8; PAD SPACE; no DDL or default admission, conversion, storage, or comparison semantics |
| `utf32_slovak_ci` | 🟡 | Metadata-only catalog row; utf32; id 173; sortlen 8; PAD SPACE; no DDL or default admission, conversion, storage, or comparison semantics |
| `utf32_spanish2_ci` | 🟡 | Metadata-only catalog row; utf32; id 174; sortlen 8; PAD SPACE; no DDL or default admission, conversion, storage, or comparison semantics |
| `utf32_roman_ci` | 🟡 | Metadata-only catalog row; utf32; id 175; sortlen 8; PAD SPACE; no DDL or default admission, conversion, storage, or comparison semantics |
| `utf32_persian_ci` | 🟡 | Metadata-only catalog row; utf32; id 176; sortlen 8; PAD SPACE; no DDL or default admission, conversion, storage, or comparison semantics |
| `utf32_esperanto_ci` | 🟡 | Metadata-only catalog row; utf32; id 177; sortlen 8; PAD SPACE; no DDL or default admission, conversion, storage, or comparison semantics |
| `utf32_hungarian_ci` | 🟡 | Metadata-only catalog row; utf32; id 178; sortlen 8; PAD SPACE; no DDL or default admission, conversion, storage, or comparison semantics |
| `utf32_sinhala_ci` | 🟡 | Metadata-only catalog row; utf32; id 179; sortlen 8; PAD SPACE; no DDL or default admission, conversion, storage, or comparison semantics |
| `utf32_german2_ci` | 🟡 | Metadata-only catalog row; utf32; id 180; sortlen 8; PAD SPACE; no DDL or default admission, conversion, storage, or comparison semantics |
| `utf32_croatian_ci` | 🟡 | Metadata-only catalog row; utf32; id 181; sortlen 8; PAD SPACE; no DDL or default admission, conversion, storage, or comparison semantics |
| `utf32_unicode_520_ci` | 🟡 | Metadata-only catalog row; utf32; id 182; sortlen 8; PAD SPACE; no DDL or default admission, conversion, storage, or comparison semantics |
| `utf32_vietnamese_ci` | 🟡 | Metadata-only catalog row; utf32; id 183; sortlen 8; PAD SPACE; no DDL or default admission, conversion, storage, or comparison semantics |
| `utf8mb3_general_ci` | 🟡 | Metadata catalog row plus limited metadata collation for national `CHAR` / `VARCHAR` aliases in `SHOW CREATE TABLE`, `SHOW FULL COLUMNS`, `INFORMATION_SCHEMA.COLUMNS`, and result-column metadata plus scalar `CONVERT(value USING utf8\|utf8mb3)` default metadata; no table/default admission outside national aliases, storage conversion, or general comparison/order/group/distinct semantics |
| `utf8mb3_tolower_ci` | 🟡 | Metadata-only catalog row; utf8mb3; id 76; sortlen 1; PAD SPACE; no DDL or default admission, conversion, storage, or comparison semantics |
| `utf8mb3_bin` | 🟡 | Metadata catalog row plus limited scalar postfix `COLLATE utf8mb3_bin` metadata for scalar `utf8` / `utf8mb3` conversion results; utf8mb3; id 83; sortlen 1; PAD SPACE; no DDL/default admission, storage conversion, or comparison semantics |
| `utf8mb3_unicode_ci` | 🟡 | Metadata-only catalog row; utf8mb3; id 192; sortlen 8; PAD SPACE; no DDL or default admission, conversion, storage, or comparison semantics |
| `utf8mb3_icelandic_ci` | 🟡 | Metadata-only catalog row; utf8mb3; id 193; sortlen 8; PAD SPACE; no DDL or default admission, conversion, storage, or comparison semantics |
| `utf8mb3_latvian_ci` | 🟡 | Metadata-only catalog row; utf8mb3; id 194; sortlen 8; PAD SPACE; no DDL or default admission, conversion, storage, or comparison semantics |
| `utf8mb3_romanian_ci` | 🟡 | Metadata-only catalog row; utf8mb3; id 195; sortlen 8; PAD SPACE; no DDL or default admission, conversion, storage, or comparison semantics |
| `utf8mb3_slovenian_ci` | 🟡 | Metadata-only catalog row; utf8mb3; id 196; sortlen 8; PAD SPACE; no DDL or default admission, conversion, storage, or comparison semantics |
| `utf8mb3_polish_ci` | 🟡 | Metadata-only catalog row; utf8mb3; id 197; sortlen 8; PAD SPACE; no DDL or default admission, conversion, storage, or comparison semantics |
| `utf8mb3_estonian_ci` | 🟡 | Metadata-only catalog row; utf8mb3; id 198; sortlen 8; PAD SPACE; no DDL or default admission, conversion, storage, or comparison semantics |
| `utf8mb3_spanish_ci` | 🟡 | Metadata-only catalog row; utf8mb3; id 199; sortlen 8; PAD SPACE; no DDL or default admission, conversion, storage, or comparison semantics |
| `utf8mb3_swedish_ci` | 🟡 | Metadata-only catalog row; utf8mb3; id 200; sortlen 8; PAD SPACE; no DDL or default admission, conversion, storage, or comparison semantics |
| `utf8mb3_turkish_ci` | 🟡 | Metadata-only catalog row; utf8mb3; id 201; sortlen 8; PAD SPACE; no DDL or default admission, conversion, storage, or comparison semantics |
| `utf8mb3_czech_ci` | 🟡 | Metadata-only catalog row; utf8mb3; id 202; sortlen 8; PAD SPACE; no DDL or default admission, conversion, storage, or comparison semantics |
| `utf8mb3_danish_ci` | 🟡 | Metadata-only catalog row; utf8mb3; id 203; sortlen 8; PAD SPACE; no DDL or default admission, conversion, storage, or comparison semantics |
| `utf8mb3_lithuanian_ci` | 🟡 | Metadata-only catalog row; utf8mb3; id 204; sortlen 8; PAD SPACE; no DDL or default admission, conversion, storage, or comparison semantics |
| `utf8mb3_slovak_ci` | 🟡 | Metadata-only catalog row; utf8mb3; id 205; sortlen 8; PAD SPACE; no DDL or default admission, conversion, storage, or comparison semantics |
| `utf8mb3_spanish2_ci` | 🟡 | Metadata-only catalog row; utf8mb3; id 206; sortlen 8; PAD SPACE; no DDL or default admission, conversion, storage, or comparison semantics |
| `utf8mb3_roman_ci` | 🟡 | Metadata-only catalog row; utf8mb3; id 207; sortlen 8; PAD SPACE; no DDL or default admission, conversion, storage, or comparison semantics |
| `utf8mb3_persian_ci` | 🟡 | Metadata-only catalog row; utf8mb3; id 208; sortlen 8; PAD SPACE; no DDL or default admission, conversion, storage, or comparison semantics |
| `utf8mb3_esperanto_ci` | 🟡 | Metadata-only catalog row; utf8mb3; id 209; sortlen 8; PAD SPACE; no DDL or default admission, conversion, storage, or comparison semantics |
| `utf8mb3_hungarian_ci` | 🟡 | Metadata-only catalog row; utf8mb3; id 210; sortlen 8; PAD SPACE; no DDL or default admission, conversion, storage, or comparison semantics |
| `utf8mb3_sinhala_ci` | 🟡 | Metadata-only catalog row; utf8mb3; id 211; sortlen 8; PAD SPACE; no DDL or default admission, conversion, storage, or comparison semantics |
| `utf8mb3_german2_ci` | 🟡 | Metadata-only catalog row; utf8mb3; id 212; sortlen 8; PAD SPACE; no DDL or default admission, conversion, storage, or comparison semantics |
| `utf8mb3_croatian_ci` | 🟡 | Metadata-only catalog row; utf8mb3; id 213; sortlen 8; PAD SPACE; no DDL or default admission, conversion, storage, or comparison semantics |
| `utf8mb3_unicode_520_ci` | 🟡 | Metadata-only catalog row; utf8mb3; id 214; sortlen 8; PAD SPACE; no DDL or default admission, conversion, storage, or comparison semantics |
| `utf8mb3_vietnamese_ci` | 🟡 | Metadata-only catalog row; utf8mb3; id 215; sortlen 8; PAD SPACE; no DDL or default admission, conversion, storage, or comparison semantics |
| `utf8mb3_general_mysql500_ci` | 🟡 | Metadata-only catalog row; utf8mb3; id 223; sortlen 1; PAD SPACE; no DDL or default admission, conversion, storage, or comparison semantics |
| `utf8mb4_general_ci` | 🟡 | Limited static catalog row, table/column default metadata preservation, `SET NAMES` session readback, and no MySQL collation comparison semantics; utf8mb4; id 45; sortlen 1; PAD SPACE |
| `utf8mb4_bin` | 🟡 | Limited static catalog row, table/column default metadata preservation, `SET NAMES` session readback, and no MySQL binary comparison semantics; utf8mb4; id 46; sortlen 1; PAD SPACE |
| `utf8mb4_unicode_ci` | 🟡 | Limited static catalog row, table/column default metadata preservation, `SET NAMES` session readback, and no MySQL collation comparison semantics; utf8mb4; id 224; sortlen 8; PAD SPACE |
| `utf8mb4_icelandic_ci` | 🟡 | Metadata-only catalog row; utf8mb4; id 225; sortlen 8; PAD SPACE; no DDL or default admission, conversion, storage, or comparison semantics |
| `utf8mb4_latvian_ci` | 🟡 | Metadata-only catalog row; utf8mb4; id 226; sortlen 8; PAD SPACE; no DDL or default admission, conversion, storage, or comparison semantics |
| `utf8mb4_romanian_ci` | 🟡 | Metadata-only catalog row; utf8mb4; id 227; sortlen 8; PAD SPACE; no DDL or default admission, conversion, storage, or comparison semantics |
| `utf8mb4_slovenian_ci` | 🟡 | Metadata-only catalog row; utf8mb4; id 228; sortlen 8; PAD SPACE; no DDL or default admission, conversion, storage, or comparison semantics |
| `utf8mb4_polish_ci` | 🟡 | Metadata-only catalog row; utf8mb4; id 229; sortlen 8; PAD SPACE; no DDL or default admission, conversion, storage, or comparison semantics |
| `utf8mb4_estonian_ci` | 🟡 | Metadata-only catalog row; utf8mb4; id 230; sortlen 8; PAD SPACE; no DDL or default admission, conversion, storage, or comparison semantics |
| `utf8mb4_spanish_ci` | 🟡 | Metadata-only catalog row; utf8mb4; id 231; sortlen 8; PAD SPACE; no DDL or default admission, conversion, storage, or comparison semantics |
| `utf8mb4_swedish_ci` | 🟡 | Metadata-only catalog row; utf8mb4; id 232; sortlen 8; PAD SPACE; no DDL or default admission, conversion, storage, or comparison semantics |
| `utf8mb4_turkish_ci` | 🟡 | Metadata-only catalog row; utf8mb4; id 233; sortlen 8; PAD SPACE; no DDL or default admission, conversion, storage, or comparison semantics |
| `utf8mb4_czech_ci` | 🟡 | Metadata-only catalog row; utf8mb4; id 234; sortlen 8; PAD SPACE; no DDL or default admission, conversion, storage, or comparison semantics |
| `utf8mb4_danish_ci` | 🟡 | Metadata-only catalog row; utf8mb4; id 235; sortlen 8; PAD SPACE; no DDL or default admission, conversion, storage, or comparison semantics |
| `utf8mb4_lithuanian_ci` | 🟡 | Metadata-only catalog row; utf8mb4; id 236; sortlen 8; PAD SPACE; no DDL or default admission, conversion, storage, or comparison semantics |
| `utf8mb4_slovak_ci` | 🟡 | Metadata-only catalog row; utf8mb4; id 237; sortlen 8; PAD SPACE; no DDL or default admission, conversion, storage, or comparison semantics |
| `utf8mb4_spanish2_ci` | 🟡 | Metadata-only catalog row; utf8mb4; id 238; sortlen 8; PAD SPACE; no DDL or default admission, conversion, storage, or comparison semantics |
| `utf8mb4_roman_ci` | 🟡 | Metadata-only catalog row; utf8mb4; id 239; sortlen 8; PAD SPACE; no DDL or default admission, conversion, storage, or comparison semantics |
| `utf8mb4_persian_ci` | 🟡 | Metadata-only catalog row; utf8mb4; id 240; sortlen 8; PAD SPACE; no DDL or default admission, conversion, storage, or comparison semantics |
| `utf8mb4_esperanto_ci` | 🟡 | Metadata-only catalog row; utf8mb4; id 241; sortlen 8; PAD SPACE; no DDL or default admission, conversion, storage, or comparison semantics |
| `utf8mb4_hungarian_ci` | 🟡 | Metadata-only catalog row; utf8mb4; id 242; sortlen 8; PAD SPACE; no DDL or default admission, conversion, storage, or comparison semantics |
| `utf8mb4_sinhala_ci` | 🟡 | Metadata-only catalog row; utf8mb4; id 243; sortlen 8; PAD SPACE; no DDL or default admission, conversion, storage, or comparison semantics |
| `utf8mb4_german2_ci` | 🟡 | Metadata-only catalog row; utf8mb4; id 244; sortlen 8; PAD SPACE; no DDL or default admission, conversion, storage, or comparison semantics |
| `utf8mb4_croatian_ci` | 🟡 | Metadata-only catalog row; utf8mb4; id 245; sortlen 8; PAD SPACE; no DDL or default admission, conversion, storage, or comparison semantics |
| `utf8mb4_unicode_520_ci` | 🟡 | Limited static catalog row, table/column default metadata preservation, `SET NAMES` session readback, and no MySQL collation comparison semantics; utf8mb4; id 246; sortlen 8; PAD SPACE |
| `utf8mb4_vietnamese_ci` | 🟡 | Metadata-only catalog row; utf8mb4; id 247; sortlen 8; PAD SPACE; no DDL or default admission, conversion, storage, or comparison semantics |
| `utf8mb4_0900_ai_ci` | 🟡 | Limited static catalog row, schema/table/column default metadata preservation, `SET NAMES` session readback, fixed scalar `@@collation_server`, descriptor-backed scalar `@@collation_database`, and MyLite-owned ASCII equality subset for admitted `CHAR` / `VARCHAR` primary and unique key enforcement; utf8mb4; id 255; default; sortlen 0; NO PAD; no general comparison semantics |
| `utf8mb4_de_pb_0900_ai_ci` | 🟡 | Metadata-only catalog row; utf8mb4; id 256; sortlen 0; NO PAD; no DDL or default admission, conversion, storage, or comparison semantics |
| `utf8mb4_is_0900_ai_ci` | 🟡 | Metadata-only catalog row; utf8mb4; id 257; sortlen 0; NO PAD; no DDL or default admission, conversion, storage, or comparison semantics |
| `utf8mb4_lv_0900_ai_ci` | 🟡 | Metadata-only catalog row; utf8mb4; id 258; sortlen 0; NO PAD; no DDL or default admission, conversion, storage, or comparison semantics |
| `utf8mb4_ro_0900_ai_ci` | 🟡 | Metadata-only catalog row; utf8mb4; id 259; sortlen 0; NO PAD; no DDL or default admission, conversion, storage, or comparison semantics |
| `utf8mb4_sl_0900_ai_ci` | 🟡 | Metadata-only catalog row; utf8mb4; id 260; sortlen 0; NO PAD; no DDL or default admission, conversion, storage, or comparison semantics |
| `utf8mb4_pl_0900_ai_ci` | 🟡 | Metadata-only catalog row; utf8mb4; id 261; sortlen 0; NO PAD; no DDL or default admission, conversion, storage, or comparison semantics |
| `utf8mb4_et_0900_ai_ci` | 🟡 | Metadata-only catalog row; utf8mb4; id 262; sortlen 0; NO PAD; no DDL or default admission, conversion, storage, or comparison semantics |
| `utf8mb4_es_0900_ai_ci` | 🟡 | Metadata-only catalog row; utf8mb4; id 263; sortlen 0; NO PAD; no DDL or default admission, conversion, storage, or comparison semantics |
| `utf8mb4_sv_0900_ai_ci` | 🟡 | Metadata-only catalog row; utf8mb4; id 264; sortlen 0; NO PAD; no DDL or default admission, conversion, storage, or comparison semantics |
| `utf8mb4_tr_0900_ai_ci` | 🟡 | Metadata-only catalog row; utf8mb4; id 265; sortlen 0; NO PAD; no DDL or default admission, conversion, storage, or comparison semantics |
| `utf8mb4_cs_0900_ai_ci` | 🟡 | Metadata-only catalog row; utf8mb4; id 266; sortlen 0; NO PAD; no DDL or default admission, conversion, storage, or comparison semantics |
| `utf8mb4_da_0900_ai_ci` | 🟡 | Metadata-only catalog row; utf8mb4; id 267; sortlen 0; NO PAD; no DDL or default admission, conversion, storage, or comparison semantics |
| `utf8mb4_lt_0900_ai_ci` | 🟡 | Metadata-only catalog row; utf8mb4; id 268; sortlen 0; NO PAD; no DDL or default admission, conversion, storage, or comparison semantics |
| `utf8mb4_sk_0900_ai_ci` | 🟡 | Metadata-only catalog row; utf8mb4; id 269; sortlen 0; NO PAD; no DDL or default admission, conversion, storage, or comparison semantics |
| `utf8mb4_es_trad_0900_ai_ci` | 🟡 | Metadata-only catalog row; utf8mb4; id 270; sortlen 0; NO PAD; no DDL or default admission, conversion, storage, or comparison semantics |
| `utf8mb4_la_0900_ai_ci` | 🟡 | Metadata-only catalog row; utf8mb4; id 271; sortlen 0; NO PAD; no DDL or default admission, conversion, storage, or comparison semantics |
| `utf8mb4_eo_0900_ai_ci` | 🟡 | Metadata-only catalog row; utf8mb4; id 273; sortlen 0; NO PAD; no DDL or default admission, conversion, storage, or comparison semantics |
| `utf8mb4_hu_0900_ai_ci` | 🟡 | Metadata-only catalog row; utf8mb4; id 274; sortlen 0; NO PAD; no DDL or default admission, conversion, storage, or comparison semantics |
| `utf8mb4_hr_0900_ai_ci` | 🟡 | Metadata-only catalog row; utf8mb4; id 275; sortlen 0; NO PAD; no DDL or default admission, conversion, storage, or comparison semantics |
| `utf8mb4_vi_0900_ai_ci` | 🟡 | Metadata-only catalog row; utf8mb4; id 277; sortlen 0; NO PAD; no DDL or default admission, conversion, storage, or comparison semantics |
| `utf8mb4_0900_as_cs` | 🟡 | Metadata-only catalog row; utf8mb4; id 278; sortlen 0; NO PAD; no DDL or default admission, conversion, storage, or comparison semantics |
| `utf8mb4_de_pb_0900_as_cs` | 🟡 | Metadata-only catalog row; utf8mb4; id 279; sortlen 0; NO PAD; no DDL or default admission, conversion, storage, or comparison semantics |
| `utf8mb4_is_0900_as_cs` | 🟡 | Metadata-only catalog row; utf8mb4; id 280; sortlen 0; NO PAD; no DDL or default admission, conversion, storage, or comparison semantics |
| `utf8mb4_lv_0900_as_cs` | 🟡 | Metadata-only catalog row; utf8mb4; id 281; sortlen 0; NO PAD; no DDL or default admission, conversion, storage, or comparison semantics |
| `utf8mb4_ro_0900_as_cs` | 🟡 | Metadata-only catalog row; utf8mb4; id 282; sortlen 0; NO PAD; no DDL or default admission, conversion, storage, or comparison semantics |
| `utf8mb4_sl_0900_as_cs` | 🟡 | Metadata-only catalog row; utf8mb4; id 283; sortlen 0; NO PAD; no DDL or default admission, conversion, storage, or comparison semantics |
| `utf8mb4_pl_0900_as_cs` | 🟡 | Metadata-only catalog row; utf8mb4; id 284; sortlen 0; NO PAD; no DDL or default admission, conversion, storage, or comparison semantics |
| `utf8mb4_et_0900_as_cs` | 🟡 | Metadata-only catalog row; utf8mb4; id 285; sortlen 0; NO PAD; no DDL or default admission, conversion, storage, or comparison semantics |
| `utf8mb4_es_0900_as_cs` | 🟡 | Metadata-only catalog row; utf8mb4; id 286; sortlen 0; NO PAD; no DDL or default admission, conversion, storage, or comparison semantics |
| `utf8mb4_sv_0900_as_cs` | 🟡 | Metadata-only catalog row; utf8mb4; id 287; sortlen 0; NO PAD; no DDL or default admission, conversion, storage, or comparison semantics |
| `utf8mb4_tr_0900_as_cs` | 🟡 | Metadata-only catalog row; utf8mb4; id 288; sortlen 0; NO PAD; no DDL or default admission, conversion, storage, or comparison semantics |
| `utf8mb4_cs_0900_as_cs` | 🟡 | Metadata-only catalog row; utf8mb4; id 289; sortlen 0; NO PAD; no DDL or default admission, conversion, storage, or comparison semantics |
| `utf8mb4_da_0900_as_cs` | 🟡 | Metadata-only catalog row; utf8mb4; id 290; sortlen 0; NO PAD; no DDL or default admission, conversion, storage, or comparison semantics |
| `utf8mb4_lt_0900_as_cs` | 🟡 | Metadata-only catalog row; utf8mb4; id 291; sortlen 0; NO PAD; no DDL or default admission, conversion, storage, or comparison semantics |
| `utf8mb4_sk_0900_as_cs` | 🟡 | Metadata-only catalog row; utf8mb4; id 292; sortlen 0; NO PAD; no DDL or default admission, conversion, storage, or comparison semantics |
| `utf8mb4_es_trad_0900_as_cs` | 🟡 | Metadata-only catalog row; utf8mb4; id 293; sortlen 0; NO PAD; no DDL or default admission, conversion, storage, or comparison semantics |
| `utf8mb4_la_0900_as_cs` | 🟡 | Metadata-only catalog row; utf8mb4; id 294; sortlen 0; NO PAD; no DDL or default admission, conversion, storage, or comparison semantics |
| `utf8mb4_eo_0900_as_cs` | 🟡 | Metadata-only catalog row; utf8mb4; id 296; sortlen 0; NO PAD; no DDL or default admission, conversion, storage, or comparison semantics |
| `utf8mb4_hu_0900_as_cs` | 🟡 | Metadata-only catalog row; utf8mb4; id 297; sortlen 0; NO PAD; no DDL or default admission, conversion, storage, or comparison semantics |
| `utf8mb4_hr_0900_as_cs` | 🟡 | Metadata-only catalog row; utf8mb4; id 298; sortlen 0; NO PAD; no DDL or default admission, conversion, storage, or comparison semantics |
| `utf8mb4_vi_0900_as_cs` | 🟡 | Metadata-only catalog row; utf8mb4; id 300; sortlen 0; NO PAD; no DDL or default admission, conversion, storage, or comparison semantics |
| `utf8mb4_ja_0900_as_cs` | 🟡 | Metadata-only catalog row; utf8mb4; id 303; sortlen 0; NO PAD; no DDL or default admission, conversion, storage, or comparison semantics |
| `utf8mb4_ja_0900_as_cs_ks` | 🟡 | Metadata-only catalog row; utf8mb4; id 304; sortlen 24; NO PAD; no DDL or default admission, conversion, storage, or comparison semantics |
| `utf8mb4_0900_as_ci` | 🟡 | Metadata-only catalog row; utf8mb4; id 305; sortlen 0; NO PAD; no DDL or default admission, conversion, storage, or comparison semantics |
| `utf8mb4_ru_0900_ai_ci` | 🟡 | Metadata-only catalog row; utf8mb4; id 306; sortlen 0; NO PAD; no DDL or default admission, conversion, storage, or comparison semantics |
| `utf8mb4_ru_0900_as_cs` | 🟡 | Metadata-only catalog row; utf8mb4; id 307; sortlen 0; NO PAD; no DDL or default admission, conversion, storage, or comparison semantics |
| `utf8mb4_zh_0900_as_cs` | 🟡 | Metadata-only catalog row; utf8mb4; id 308; sortlen 0; NO PAD; no DDL or default admission, conversion, storage, or comparison semantics |
| `utf8mb4_0900_bin` | 🟡 | Limited static catalog row, table/column default metadata preservation, `SET NAMES` session readback, and result-column collation metadata; utf8mb4; id 309; sortlen 1; NO PAD; no MySQL binary `NO PAD` comparison semantics |
| `utf8mb4_nb_0900_ai_ci` | 🟡 | Metadata-only catalog row; utf8mb4; id 310; sortlen 0; NO PAD; no DDL or default admission, conversion, storage, or comparison semantics |
| `utf8mb4_nb_0900_as_cs` | 🟡 | Metadata-only catalog row; utf8mb4; id 311; sortlen 0; NO PAD; no DDL or default admission, conversion, storage, or comparison semantics |
| `utf8mb4_nn_0900_ai_ci` | 🟡 | Metadata-only catalog row; utf8mb4; id 312; sortlen 0; NO PAD; no DDL or default admission, conversion, storage, or comparison semantics |
| `utf8mb4_nn_0900_as_cs` | 🟡 | Metadata-only catalog row; utf8mb4; id 313; sortlen 0; NO PAD; no DDL or default admission, conversion, storage, or comparison semantics |
| `utf8mb4_sr_latn_0900_ai_ci` | 🟡 | Metadata-only catalog row; utf8mb4; id 314; sortlen 0; NO PAD; no DDL or default admission, conversion, storage, or comparison semantics |
| `utf8mb4_sr_latn_0900_as_cs` | 🟡 | Metadata-only catalog row; utf8mb4; id 315; sortlen 0; NO PAD; no DDL or default admission, conversion, storage, or comparison semantics |
| `utf8mb4_bs_0900_ai_ci` | 🟡 | Metadata-only catalog row; utf8mb4; id 316; sortlen 0; NO PAD; no DDL or default admission, conversion, storage, or comparison semantics |
| `utf8mb4_bs_0900_as_cs` | 🟡 | Metadata-only catalog row; utf8mb4; id 317; sortlen 0; NO PAD; no DDL or default admission, conversion, storage, or comparison semantics |
| `utf8mb4_bg_0900_ai_ci` | 🟡 | Metadata-only catalog row; utf8mb4; id 318; sortlen 0; NO PAD; no DDL or default admission, conversion, storage, or comparison semantics |
| `utf8mb4_bg_0900_as_cs` | 🟡 | Metadata-only catalog row; utf8mb4; id 319; sortlen 0; NO PAD; no DDL or default admission, conversion, storage, or comparison semantics |
| `utf8mb4_gl_0900_ai_ci` | 🟡 | Metadata-only catalog row; utf8mb4; id 320; sortlen 0; NO PAD; no DDL or default admission, conversion, storage, or comparison semantics |
| `utf8mb4_gl_0900_as_cs` | 🟡 | Metadata-only catalog row; utf8mb4; id 321; sortlen 0; NO PAD; no DDL or default admission, conversion, storage, or comparison semantics |
| `utf8mb4_mn_cyrl_0900_ai_ci` | 🟡 | Metadata-only catalog row; utf8mb4; id 322; sortlen 0; NO PAD; no DDL or default admission, conversion, storage, or comparison semantics |
| `utf8mb4_mn_cyrl_0900_as_cs` | 🟡 | Metadata-only catalog row; utf8mb4; id 323; sortlen 0; NO PAD; no DDL or default admission, conversion, storage, or comparison semantics |

[Back to compatibility overview](../../COMPATIBILITY.md)
