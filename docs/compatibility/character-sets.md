# Character sets

| Character set | Status | Notes |
| --- | --- | --- |
| `armscii8` | ❌ | Metadata and conversions |
| `ascii` | 🟡 | Limited static `SHOW CHARACTER SET` row, `INFORMATION_SCHEMA.CHARACTER_SETS` metadata, `INFORMATION_SCHEMA.COLLATION_CHARACTER_SET_APPLICABILITY` mappings for `ascii_general_ci` and `ascii_bin`, descriptor-owned schema defaults through supported `CREATE DATABASE` / `ALTER DATABASE` option forms, `CREATE TABLE`, `CREATE TABLE ... LIKE`, and `ALTER TABLE ... [DEFAULT] CHARSET` table default metadata, and explicit column `CHARACTER SET ascii` / `CHARSET ascii` metadata for admitted `CHAR`, `VARCHAR`, and bare `TEXT` family definitions; no bare `ASCII` shorthand, non-ASCII storage rejection, conversions, connection charset state, or full ascii comparison semantics |
| `big5` | ❌ | Metadata and conversions |
| `binary` | 🟡 | Limited static `SHOW CHARACTER SET` row, `INFORMATION_SCHEMA.CHARACTER_SETS` metadata, `INFORMATION_SCHEMA.COLLATION_CHARACTER_SET_APPLICABILITY` mapping for the `binary` collation, scalar `@@character_set_filesystem` placeholder value, descriptor-owned schema defaults through supported `CREATE DATABASE` / `ALTER DATABASE` option forms, explicit column-level `CHARACTER SET binary` / `CHARSET binary` / `COLLATE binary` normalization for admitted `CHAR`, `VARCHAR`, and bare `TEXT` family definitions to existing binary string/BLOB descriptors, and limited `CREATE TABLE` / `CREATE TEMPORARY TABLE` table-default inheritance from table or schema defaults that normalizes inherited non-national `CHAR`, `VARCHAR`, and bare `TEXT` family descriptors while preserving explicit `utf8mb4` column overrides and leaving `ENUM`/`SET` descriptors unchanged; no `ALTER TABLE ... DEFAULT CHARSET=binary`, binary string defaults, full-column binary string indexes, literal introducers, connection charset state, binary comparison semantics, conversions, or file-name conversion semantics |
| `cp1250` | ❌ | Metadata and conversions |
| `cp1251` | ❌ | Metadata and conversions |
| `cp1256` | ❌ | Metadata and conversions |
| `cp1257` | ❌ | Metadata and conversions |
| `cp850` | ❌ | Metadata and conversions |
| `cp852` | ❌ | Metadata and conversions |
| `cp866` | ❌ | Metadata and conversions |
| `cp932` | ❌ | Metadata and conversions |
| `dec8` | ❌ | Metadata and conversions |
| `eucjpms` | ❌ | Metadata and conversions |
| `euckr` | ❌ | Metadata and conversions |
| `gb18030` | ❌ | Metadata and conversions |
| `gb2312` | ❌ | Metadata and conversions |
| `gbk` | ❌ | Metadata and conversions |
| `geostd8` | ❌ | Metadata and conversions |
| `greek` | ❌ | Metadata and conversions |
| `hebrew` | ❌ | Metadata and conversions |
| `hp8` | ❌ | Metadata and conversions |
| `keybcs2` | ❌ | Metadata and conversions |
| `koi8r` | ❌ | Metadata and conversions |
| `koi8u` | ❌ | Metadata and conversions |
| `latin1` | ❌ | Metadata and conversions |
| `latin2` | ❌ | Metadata and conversions |
| `latin5` | ❌ | Metadata and conversions |
| `latin7` | ❌ | Metadata and conversions |
| `macce` | ❌ | Metadata and conversions |
| `macroman` | ❌ | Metadata and conversions |
| `sjis` | ❌ | Metadata and conversions |
| `swe7` | ❌ | Metadata and conversions |
| `tis620` | ❌ | Metadata and conversions |
| `ucs2` | ❌ | Metadata and conversions |
| `ujis` | ❌ | Metadata and conversions |
| `utf8mb3` | 🟡 | Limited scalar `@@character_set_system` placeholder value, metadata charset labels in supported synthetic `INFORMATION_SCHEMA` system-view column rows, and national `CHAR` / `VARCHAR` alias column metadata (`SHOW CREATE TABLE`, `SHOW FULL COLUMNS`, `INFORMATION_SCHEMA.COLUMNS`, and result-column metadata). National alias values still use existing SQLite `TEXT` storage and MyLite string validation and reject explicit column charset/collation attributes; no literal introducers, conversions, collation semantics, or identifier character-set semantics |
| `utf8mb4` | 🟡 | Limited static `SHOW CHARACTER SET` row, `INFORMATION_SCHEMA.CHARACTER_SETS` metadata, `INFORMATION_SCHEMA.COLLATION_CHARACTER_SET_APPLICABILITY` mappings for admitted `utf8mb4` collations, descriptor-owned schema defaults through supported `CREATE DATABASE` / `ALTER DATABASE` option forms, `CREATE TABLE` / `CREATE TABLE ... LIKE` / `ALTER TABLE ... [DEFAULT] CHARSET` table default metadata for `utf8mb4`, limited explicit column `CHARACTER SET` / `CHARSET` metadata for `CHAR`, `VARCHAR`, and bare `TEXT` family descriptors where the host DDL action admits that type, scalar `@@character_set_client` / `@@character_set_connection` / `@@character_set_results` reads updated by admitted `SET NAMES` / `SET CHARACTER SET` forms, fixed scalar `@@character_set_server`, descriptor-backed scalar `@@character_set_database`, and limited UTF-8 validation for `CHAR`, `VARCHAR(0..16383)`, and baseline `TEXT` family storage; no conversions, mutable server-global charset state, alternate non-admitted database charsets, or full catalog semantics |
| `utf16` | ❌ | Metadata and conversions |
| `utf16le` | ❌ | Metadata and conversions |
| `utf32` | ❌ | Metadata and conversions |

[Back to compatibility overview](../../COMPATIBILITY.md)
