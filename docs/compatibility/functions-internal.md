# Internal functions

Metadata, data dictionary, Performance Schema, sys helper, and statement digest
functions.

| Function | Status | Notes |
| --- | --- | --- |
| `CAN_ACCESS_COLUMN()` | ✅ | MySQL-runtime-verified native access rejection |
| `CAN_ACCESS_DATABASE()` | ✅ | MySQL-runtime-verified native access rejection |
| `CAN_ACCESS_TABLE()` | ✅ | MySQL-runtime-verified native access rejection |
| `CAN_ACCESS_USER()` | ✅ | MySQL-runtime-verified native access rejection |
| `CAN_ACCESS_VIEW()` | ✅ | MySQL-runtime-verified native access rejection |
| `FORMAT_BYTES()` | ✅ | MySQL-runtime-verified native byte formatter in scalar and row-backed contexts |
| `FORMAT_PICO_TIME()` | ✅ | MySQL-runtime-verified native picosecond formatter in scalar and row-backed contexts |
| `GET_DD_COLUMN_PRIVILEGES()` | ✅ | MySQL-runtime-verified native access rejection |
| `GET_DD_CREATE_OPTIONS()` | ✅ | MySQL-runtime-verified native access rejection |
| `GET_DD_INDEX_SUB_PART_LENGTH()` | ✅ | MySQL-runtime-verified native access rejection |
| `INTERNAL_AUTO_INCREMENT()` | ✅ | MySQL-runtime-verified native access rejection |
| `INTERNAL_AVG_ROW_LENGTH()` | ✅ | MySQL-runtime-verified native access rejection |
| `INTERNAL_CHECK_TIME()` | ✅ | MySQL-runtime-verified native access rejection |
| `INTERNAL_CHECKSUM()` | ✅ | MySQL-runtime-verified native access rejection |
| `INTERNAL_DATA_FREE()` | ✅ | MySQL-runtime-verified native access rejection |
| `INTERNAL_DATA_LENGTH()` | ✅ | MySQL-runtime-verified native access rejection |
| `INTERNAL_DD_CHAR_LENGTH()` | ✅ | MySQL-runtime-verified native access rejection |
| `INTERNAL_GET_COMMENT_OR_ERROR()` | ✅ | MySQL-runtime-verified native access rejection |
| `INTERNAL_GET_ENABLED_ROLE_JSON()` | ✅ | MySQL-runtime-verified native access rejection |
| `INTERNAL_GET_HOSTNAME()` | ✅ | MySQL-runtime-verified native access rejection |
| `INTERNAL_GET_USERNAME()` | ✅ | MySQL-runtime-verified native access rejection |
| `INTERNAL_GET_VIEW_WARNING_OR_ERROR()` | ✅ | MySQL-runtime-verified native access rejection |
| `INTERNAL_INDEX_COLUMN_CARDINALITY()` | ✅ | MySQL-runtime-verified native access rejection |
| `INTERNAL_INDEX_LENGTH()` | ✅ | MySQL-runtime-verified native access rejection |
| `INTERNAL_IS_ENABLED_ROLE()` | ✅ | MySQL-runtime-verified native access rejection |
| `INTERNAL_IS_MANDATORY_ROLE()` | ✅ | MySQL-runtime-verified native access rejection |
| `INTERNAL_KEYS_DISABLED()` | ✅ | MySQL-runtime-verified native access rejection |
| `INTERNAL_MAX_DATA_LENGTH()` | ✅ | MySQL-runtime-verified native access rejection |
| `INTERNAL_TABLE_ROWS()` | ✅ | MySQL-runtime-verified native access rejection |
| `INTERNAL_UPDATE_TIME()` | ✅ | MySQL-runtime-verified native access rejection |
| `PS_CURRENT_THREAD_ID()` | ✅ | MySQL-runtime-verified current synthetic Performance Schema thread id |
| `PS_THREAD_ID()` | ✅ | MySQL-runtime-verified connection-id to synthetic thread-id mapping |
| `ROLES_GRAPHML()` | ✅ | MySQL-runtime-verified embedded-account GraphML placeholder, scalar/DUAL/DO/row projection, charset/collation metadata, and native arity diagnostics; no persisted role graph |
| `STATEMENT_DIGEST()` | ❌ | Compute statement digest hash value |
| `STATEMENT_DIGEST_TEXT()` | ❌ | Compute normalized statement digest |

[Back to compatibility overview](../../COMPATIBILITY.md)
