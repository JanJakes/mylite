# SQL components and plugins

Component and plugin installation statement compatibility.

| Feature | Status | Notes |
| --- | --- | --- |
| Plugin metadata introspection | 🟡 | Limited `SHOW PLUGINS` and `INFORMATION_SCHEMA.PLUGINS` one-row synthetic `InnoDB` storage-engine plugin surface plus limited read-only `mysql.plugin` connection-control registry rows; no plugin loading, lifecycle, mutable plugin registry, or complete server plugin inventory |
| `INSTALL COMPONENT` | ⚪ | Accepted as an embedded no-op with warning `1105`; no component loading, registry mutation, service activation, metadata, or persistence |
| `UNINSTALL COMPONENT` | ⚪ | Accepted as an embedded no-op with warning `1105`; no component unloading, registry mutation, service shutdown, metadata, or persistence |
| `INSTALL PLUGIN` | ⚪ | Accepted as an embedded no-op with warning `1105`; no plugin loading, `mysql.plugin` writes, activation, metadata, or persistence |
| `UNINSTALL PLUGIN` | ⚪ | Accepted as an embedded no-op with warning `1105`; no plugin unloading, `mysql.plugin` writes, deactivation, metadata, or persistence |

[Back to compatibility overview](../../COMPATIBILITY.md)
