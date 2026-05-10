# Character sets

| Character set | Status | Notes |
| --- | --- | --- |
| `armscii8` | ❌ | Metadata and conversions |
| `ascii` | ❌ | Metadata and conversions |
| `big5` | ❌ | Metadata and conversions |
| `binary` | 🟡 | Limited scalar `@@character_set_filesystem` placeholder value only; no table/column/literal storage, metadata catalogs, conversions, collations, or file-name conversion semantics |
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
| `utf8mb3` | 🟡 | Limited scalar `@@character_set_system` placeholder value only; no table/column/literal storage, metadata catalogs, conversions, collations, or identifier character-set semantics |
| `utf8mb4` | 🟡 | Limited static `SHOW CHARACTER SET` row, fixed `CREATE TABLE` option acceptance, scalar `@@character_set_client` / `@@character_set_connection` / `@@character_set_results` reads, scalar `@@character_set_server`, scalar `@@character_set_database`, and fixed no-op `SET NAMES` / `SET CHARACTER SET` forms that preserve `utf8mb4`; no conversions, string types, mutable connection/server/database state, or full catalog semantics |
| `utf16` | ❌ | Metadata and conversions |
| `utf16le` | ❌ | Metadata and conversions |
| `utf32` | ❌ | Metadata and conversions |

[Back to compatibility overview](../../COMPATIBILITY.md)
