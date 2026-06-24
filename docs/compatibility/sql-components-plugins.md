# SQL components and plugins

Component and plugin installation statement compatibility.

| Feature | Status | Notes |
| --- | --- | --- |
| Plugin metadata introspection | ✅ | MySQL-runtime-verified `SHOW PLUGINS`, `INFORMATION_SCHEMA.PLUGINS`, and read-only `mysql.plugin` metadata expose MyLite's embedded synthetic plugin surfaces; plugin loading, lifecycle, mutable plugin registry, and complete server plugin inventory are tracked separately |
| `INSTALL COMPONENT` | ⚪ | Accepted as an embedded no-op with warning `1105`; no component loading, registry mutation, service activation, metadata, or persistence |
| `UNINSTALL COMPONENT` | ⚪ | Accepted as an embedded no-op with warning `1105`; no component unloading, registry mutation, service shutdown, metadata, or persistence |
| `INSTALL PLUGIN` | ⚪ | Accepted as an embedded no-op with warning `1105`; no plugin loading, `mysql.plugin` writes, activation, metadata, or persistence |
| `UNINSTALL PLUGIN` | ⚪ | Accepted as an embedded no-op with warning `1105`; no plugin unloading, `mysql.plugin` writes, deactivation, metadata, or persistence |

[Back to compatibility overview](../../COMPATIBILITY.md)
