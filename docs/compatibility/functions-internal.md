# Internal functions

Metadata, data dictionary, Performance Schema, sys helper, and statement digest
functions.

| Function | Status | Notes |
| --- | --- | --- |
| `CAN_ACCESS_COLUMN()` | ❌ | Internal helper |
| `CAN_ACCESS_DATABASE()` | ❌ | Internal helper |
| `CAN_ACCESS_TABLE()` | ❌ | Internal helper |
| `CAN_ACCESS_USER()` | ❌ | Internal helper |
| `CAN_ACCESS_VIEW()` | ❌ | Internal helper |
| `FORMAT_BYTES()` | ✅ | MySQL-runtime-verified native byte formatter in scalar and row-backed contexts |
| `FORMAT_PICO_TIME()` | ✅ | MySQL-runtime-verified native picosecond formatter in scalar and row-backed contexts |
| `GET_DD_COLUMN_PRIVILEGES()` | ❌ | Internal helper |
| `GET_DD_CREATE_OPTIONS()` | ❌ | Internal helper |
| `GET_DD_INDEX_SUB_PART_LENGTH()` | ❌ | Internal helper |
| `INTERNAL_AUTO_INCREMENT()` | ❌ | Internal helper |
| `INTERNAL_AVG_ROW_LENGTH()` | ❌ | Internal helper |
| `INTERNAL_CHECK_TIME()` | ❌ | Internal helper |
| `INTERNAL_CHECKSUM()` | ❌ | Internal helper |
| `INTERNAL_DATA_FREE()` | ❌ | Internal helper |
| `INTERNAL_DATA_LENGTH()` | ❌ | Internal helper |
| `INTERNAL_DD_CHAR_LENGTH()` | ❌ | Internal helper |
| `INTERNAL_GET_COMMENT_OR_ERROR()` | ❌ | Internal helper |
| `INTERNAL_GET_ENABLED_ROLE_JSON()` | ❌ | Internal helper |
| `INTERNAL_GET_HOSTNAME()` | ❌ | Internal helper |
| `INTERNAL_GET_USERNAME()` | ❌ | Internal helper |
| `INTERNAL_GET_VIEW_WARNING_OR_ERROR()` | ❌ | Internal helper |
| `INTERNAL_INDEX_COLUMN_CARDINALITY()` | ❌ | Internal helper |
| `INTERNAL_INDEX_LENGTH()` | ❌ | Internal helper |
| `INTERNAL_IS_ENABLED_ROLE()` | ❌ | Internal helper |
| `INTERNAL_IS_MANDATORY_ROLE()` | ❌ | Internal helper |
| `INTERNAL_KEYS_DISABLED()` | ❌ | Internal helper |
| `INTERNAL_MAX_DATA_LENGTH()` | ❌ | Internal helper |
| `INTERNAL_TABLE_ROWS()` | ❌ | Internal helper |
| `INTERNAL_UPDATE_TIME()` | ❌ | Internal helper |
| `PS_CURRENT_THREAD_ID()` | ✅ | MySQL-runtime-verified current synthetic Performance Schema thread id |
| `PS_THREAD_ID()` | ✅ | MySQL-runtime-verified connection-id to synthetic thread-id mapping |
| `ROLES_GRAPHML()` | ❌ | Return a GraphML document representing memory role subgraphs |
| `STATEMENT_DIGEST()` | ❌ | Compute statement digest hash value |
| `STATEMENT_DIGEST_TEXT()` | ❌ | Compute normalized statement digest |

[Back to compatibility overview](../../COMPATIBILITY.md)
