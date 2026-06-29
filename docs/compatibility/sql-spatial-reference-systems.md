# SQL spatial reference systems

Spatial reference system catalog DDL compatibility.

| Feature | Status | Notes |
| --- | --- | --- |
| `CREATE SPATIAL REFERENCE SYSTEM` | ⚪ | Accepted as an embedded utility no-op with warning `1105`; no mutable SRS catalog, WKT validation, dependency checks, or custom SRID behavior; see [baseline SRS DDL placeholders](../specs/baseline-spatial-reference-system-ddl-placeholders/specs.md) |
| `DROP SPATIAL REFERENCE SYSTEM` | ⚪ | Accepted as an embedded utility no-op with warning `1105`; no mutable SRS catalog deletion, `IF EXISTS` warning parity, dependency checks, or custom SRID behavior; see [baseline SRS DDL placeholders](../specs/baseline-spatial-reference-system-ddl-placeholders/specs.md) |

[Back to compatibility overview](../../COMPATIBILITY.md)
