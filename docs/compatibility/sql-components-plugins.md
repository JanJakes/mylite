# SQL components and plugins

Component and plugin installation statement compatibility.

| Feature | Status | Notes |
| --- | --- | --- |
| Plugin metadata introspection | 🟡 | Limited `SHOW PLUGINS` and `INFORMATION_SCHEMA.PLUGINS` one-row synthetic `InnoDB` storage-engine plugin surface; no plugin lifecycle or complete server plugin inventory |
| `INSTALL COMPONENT` | ❌ | Component installation syntax, diagnostics |
| `UNINSTALL COMPONENT` | ❌ | Component uninstallation syntax, diagnostics |
| `INSTALL PLUGIN` | ❌ | Plugin installation syntax, diagnostics |
| `UNINSTALL PLUGIN` | ❌ | Plugin uninstallation syntax, diagnostics |

[Back to compatibility overview](../../COMPATIBILITY.md)
