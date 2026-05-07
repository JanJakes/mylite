# Character set and collation foundation

## Scope

This feature adds the first MyLite character set and collation foundation:

- an internal registry for `utf8mb4`, `utf8mb3`, `latin1`, and `binary`,
  with `utf8` accepted as a warning-emitting alias for `utf8mb3`
- default collation lookup for those character sets
- charset/collation compatibility validation for schema defaults and connection
  character-set statements
- session connection state for `character_set_client`,
  `character_set_connection`, `character_set_results`, and
  `collation_connection`
- `SET NAMES charset [COLLATE collation]`, `SET NAMES DEFAULT`,
  `SET CHARACTER SET charset`, and `SET CHARACTER SET DEFAULT`
- schema default charset/collation normalization and validation for
  `CREATE DATABASE` and `ALTER DATABASE`

This is a foundation, not a full string semantics feature. String literal
conversion, expression collation derivation, table/column charset metadata,
`SHOW CHARACTER SET`, `SHOW COLLATION`, `SHOW VARIABLES`, and protocol-level
metadata are later tasks unless explicitly listed here.

## Sources

- MySQL 8.4 Reference Manual, Character Sets, Collations, Unicode:
  https://dev.mysql.com/doc/refman/8.4/en/charset.html
- MySQL 8.4 Reference Manual, Connection Character Sets and Collations:
  https://dev.mysql.com/doc/refman/8.4/en/charset-connection.html
- MySQL 8.4 Reference Manual, SET CHARACTER SET Statement:
  https://dev.mysql.com/doc/refman/8.4/en/set-character-set.html
- MySQL 8.4 Reference Manual, SET NAMES Statement:
  https://dev.mysql.com/doc/refman/8.4/en/set-names.html
- MySQL 8.4 Reference Manual, Character Set and Collation Compatibility:
  https://dev.mysql.com/doc/refman/8.4/en/charset-collation-compatibility.html
- MySQL 8.4 Reference Manual, Unicode Character Sets:
  https://dev.mysql.com/doc/refman/8.4/en/charset-unicode-sets.html
- MySQL 8.4 Reference Manual, West European Character Sets:
  https://dev.mysql.com/doc/refman/8.4/en/charset-we-sets.html
- MySQL 8.4 Reference Manual, The Binary Character Set:
  https://dev.mysql.com/doc/refman/8.4/en/charset-binary-set.html
- MySQL 8.4 Reference Manual, CREATE DATABASE Statement:
  https://dev.mysql.com/doc/refman/8.4/en/create-database.html
- Observed MySQL 8.4.9 runtime behavior from Docker container
  `mylite-mysql-849`.

This specification is independently authored from official documentation and
observed MySQL runtime behavior. It does not copy MySQL grammar or
implementation sources.

## MySQL 8.4.9 behavior summary

MySQL character sets have one or more collations, while each collation belongs
to exactly one character set. A collation used with the wrong character set is
rejected with an invalid-collation-for-charset error. The default server
character set and collation are `utf8mb4` and `utf8mb4_0900_ai_ci`.

The verified CLI session initially used:

- `character_set_client=latin1`
- `character_set_connection=latin1`
- `character_set_results=latin1`
- `collation_connection=latin1_swedish_ci`
- `character_set_server=utf8mb4`
- `collation_server=utf8mb4_0900_ai_ci`
- `character_set_database=utf8mb4`
- `collation_database=utf8mb4_0900_ai_ci`

MyLite intentionally uses the server/database default values as its initial
connection defaults, because MyLite does not yet model client-library locale
autodetection.

Verified character sets for this feature:

| Character set | Default collation | Maxlen |
| --- | --- | --- |
| `binary` | `binary` | 1 |
| `latin1` | `latin1_swedish_ci` | 1 |
| `utf8mb3` | `utf8mb3_general_ci` | 3 |
| `utf8mb4` | `utf8mb4_0900_ai_ci` | 4 |

MySQL 8.4 also accepts `utf8` as a compatibility alias for `utf8mb3` and
emits warning `3719`. MyLite normalizes `utf8` to `utf8mb3` for supported
connection, schema, table-option, and CAST-family charset validation paths.

Verified collations for this feature:

| Collation | Character set | ID | Default |
| --- | --- | --- | --- |
| `binary` | `binary` | 63 | yes |
| `latin1_swedish_ci` | `latin1` | 8 | yes |
| `latin1_bin` | `latin1` | 47 | no |
| `utf8mb3_general_ci` | `utf8mb3` | 33 | yes |
| `utf8mb3_bin` | `utf8mb3` | 83 | no |
| `utf8mb4_0900_ai_ci` | `utf8mb4` | 255 | yes |
| `utf8mb4_bin` | `utf8mb4` | 46 | no |
| `utf8mb4_unicode_520_ci` | `utf8mb4` | 246 | no |
| `utf8mb4_unicode_ci` | `utf8mb4` | 224 | no |

`SET NAMES charset` sets `character_set_client`,
`character_set_connection`, and `character_set_results` to the normalized
charset name and sets `collation_connection` to the charset default collation.
`SET NAMES charset COLLATE collation` uses the explicit collation after
checking that the collation belongs to the charset. `SET NAMES DEFAULT` restores
the default mapping. In the verified runtime, that default mapping is
`utf8mb4` for all three charset variables and `utf8mb4_0900_ai_ci` for
`collation_connection`.

`SET CHARACTER SET charset` sets `character_set_client` and
`character_set_results` to the requested charset. `DEFAULT` uses the default
server charset as that requested charset. It sets
`collation_connection` to the current default database collation; that also
sets `character_set_connection` to the charset associated with the default
database collation. With no selected database in the verified runtime, this is
the server default `utf8mb4` / `utf8mb4_0900_ai_ci`. With a selected database
whose defaults are `latin1` / `latin1_bin`, `SET CHARACTER SET utf8mb4` leaves
client/results as `utf8mb4` but makes connection/collation
`latin1` / `latin1_bin`.

Charset and collation names are accepted quoted or unquoted and are resolved
case-insensitively. MySQL stores normalized lowercase names in session variables
and `INFORMATION_SCHEMA.SCHEMATA`.

For schema DDL, `CHARACTER SET` without `COLLATE` stores the charset and its
default collation. `COLLATE` without `CHARACTER SET` stores the collation and
the collation's associated charset. Supplying both requires compatibility. The
verified MySQL runtime rejects an unknown charset with error `1115`, an unknown
collation with error `1273`, and an incompatible charset/collation pair with
error `1253`.

MyLite currently exposes only coarse status codes and an error message through
the public C API. Until the general diagnostics area lands, this feature
matches the error category and message class but does not expose MySQL numeric
error codes or SQLSTATE values.

## MyLite behavior

### Registry

MyLite keeps a small internal static registry for the character sets and
collations listed above. It supports:

- case-insensitive charset lookup
- `utf8` alias lookup that returns the normalized `utf8mb3` registry entry and
  emits warning `3719` in user-facing statement paths
- case-insensitive collation lookup
- normalized charset and collation names
- default collation lookup by charset
- charset lookup by collation
- compatibility checks for explicit charset/collation pairs

The registry is internal. It is not a public ABI, and it does not expose the
full MySQL character set catalog.

Unsupported character sets fail when used by this feature, including valid
MySQL character sets outside the current registry. That is a documented
coverage boundary, not a claim that those MySQL character sets are invalid.

### Session state

Each `mylite_db` handle stores these connection variables:

- `character_set_client`
- `character_set_connection`
- `character_set_results`
- `collation_connection`

Opening a handle initializes all three charset variables to `utf8mb4` and
`collation_connection` to `utf8mb4_0900_ai_ci`.

`SHOW VARIABLES`, `SELECT @@character_set_client`, and general system-variable
assignment are deferred. Tests may use narrow internal test helpers to inspect
this handle-owned state until the general variable surface is implemented.

### Schema default validation

`CREATE DATABASE` and `ALTER DATABASE` use the registry for charset and
collation validation.

Behavior:

- Omitted charset and collation default to `utf8mb4` and
  `utf8mb4_0900_ai_ci` on create.
- Explicit charset without explicit collation uses that charset's default
  collation.
- Explicit collation without explicit charset uses the collation's charset.
- Explicit charset and collation must be compatible.
- Stored schema defaults are normalized to registry names.
- `ALTER DATABASE` changes only the provided options. When both charset and
  collation are provided in the same statement, they are validated together.
  When only one is provided, the other value is derived as described above,
  matching observed MySQL behavior.

MyLite does not yet apply schema defaults to table or column metadata because
table DDL is a later feature.

### `SET NAMES`

Supported syntax:

```sql
SET NAMES charset_name [ COLLATE collation_name ]
SET NAMES DEFAULT
```

`charset_name` and `collation_name` may be identifiers or quoted strings.

Behavior:

- `SET NAMES charset` sets client, connection, and results charset variables
  to the normalized charset name.
- Without `COLLATE`, `collation_connection` becomes the charset default
  collation.
- With `COLLATE`, the explicit collation is normalized and stored after
  compatibility validation.
- `SET NAMES DEFAULT` restores `utf8mb4` and `utf8mb4_0900_ai_ci`.

Unsupported or rejected forms:

- Unknown charset: execution error.
- Unknown collation: execution error.
- Collation from another charset: execution error.
- General `SET` assignments remain out of scope and should continue to return
  parse or unsupported diagnostics through the existing SQL pipeline.

### `SET CHARACTER SET`

Supported syntax:

```sql
SET CHARACTER SET charset_name
SET CHARACTER SET DEFAULT
SET CHARSET charset_name
SET CHARSET DEFAULT
```

`charset_name` may be an identifier or a quoted string.

Behavior:

- `character_set_client` and `character_set_results` become the normalized
  requested charset.
- `collation_connection` becomes the current default database collation.
- `character_set_connection` becomes the charset associated with that
  collation.
- If no schema is selected, MyLite uses the server default
  `utf8mb4` / `utf8mb4_0900_ai_ci`, matching the verified no-selected-database
  behavior.
- `DEFAULT` uses `utf8mb4` for `character_set_client` and
  `character_set_results`; `character_set_connection` and
  `collation_connection` still follow the selected schema defaults, or the
  server defaults when no schema is selected.

When a selected schema has defaults unsupported by the current registry,
`SET CHARACTER SET charset` fails with a clear execution error. This should not
occur for schemas created through this feature because schema defaults are
validated on write.

## Lemon grammar snippets

These snippets describe MyLite's intended grammar for this feature:

```lemon
statement ::= set_names_statement.
statement ::= set_character_set_statement.

set_names_statement ::= SET NAMES charset_value opt_set_names_collation.
set_names_statement ::= SET NAMES DEFAULT.

opt_set_names_collation ::= .
opt_set_names_collation ::= COLLATE charset_value.

set_character_set_statement ::= SET CHARACTER SET charset_value.
set_character_set_statement ::= SET CHARACTER SET DEFAULT.
set_character_set_statement ::= SET CHARSET charset_value.
set_character_set_statement ::= SET CHARSET DEFAULT.

charset_value ::= identifier.
charset_value ::= STRING.
charset_value ::= BINARY.
```

The existing schema lifecycle grammar continues to use `schema_option_value`
for `CHARACTER SET`, `CHARSET`, and `COLLATE` options; this feature extends
runtime validation rather than broadening schema DDL syntax.

## Runtime and storage impact

- The registry is immutable static data.
- The `mylite_db` handle owns connection-state strings.
- Schema catalog rows continue to store normalized text values.
- No file format change is required.
- No SQLite collation implementation is added in this feature. String
  comparison and ordering semantics are later work.
- All lookups are linear over a deliberately tiny registry.

## MySQL 8.4.9 verified expectations

The following observations were verified against `mylite-mysql-849`:

| SQL | Expected behavior |
| --- | --- |
| `SET NAMES utf8mb4` | Sets client/connection/results to `utf8mb4`, collation to `utf8mb4_0900_ai_ci`. |
| `SET NAMES latin1 COLLATE latin1_bin` | Sets client/connection/results to `latin1`, collation to `latin1_bin`. |
| `SET NAMES binary` | Sets client/connection/results to `binary`, collation to `binary`. |
| `SET NAMES UTF8MB4 COLLATE UTF8MB4_BIN` | Succeeds and stores normalized lowercase names. |
| `SET NAMES utf8mb3 COLLATE utf8mb3_bin` | Sets client/connection/results to `utf8mb3`, collation to `utf8mb3_bin`. |
| `SET NAMES utf8` | Sets client/connection/results to `utf8mb3`, collation to `utf8mb3_general_ci`, with warning `3719`. |
| `SET NAMES DEFAULT` | Restores `utf8mb4` and `utf8mb4_0900_ai_ci` in this container. |
| `SET CHARACTER SET utf8mb3` with no selected database | Sets client/results to `utf8mb3`; connection/collation remain the default database values `utf8mb4` / `utf8mb4_0900_ai_ci`. |
| `SET CHARACTER SET utf8` with no selected database | Sets client/results to `utf8mb3`; connection/collation remain the default database values, with warning `3719`. |
| `SET CHARACTER SET binary` with no selected database | Sets client/results to `binary`; connection/collation remain `utf8mb4` / `utf8mb4_0900_ai_ci`. |
| `SET CHARACTER SET DEFAULT` with no selected database | Restores all four connection variables to `utf8mb4` / `utf8mb4_0900_ai_ci`. |
| `SET CHARACTER SET utf8mb4` after selecting a database whose defaults are `latin1` / `latin1_bin` | Sets client/results to `utf8mb4`; sets connection/collation to `latin1` / `latin1_bin`. |
| `SET CHARACTER SET DEFAULT` after selecting a database whose defaults are `latin1` / `latin1_bin` | Sets client/results to `utf8mb4`; sets connection/collation to `latin1` / `latin1_bin`. |
| `CREATE DATABASE ... DEFAULT CHARACTER SET UTF8MB4 COLLATE UTF8MB4_BIN` | Stores `utf8mb4` / `utf8mb4_bin` in `INFORMATION_SCHEMA.SCHEMATA`. |
| `CREATE DATABASE ... COLLATE latin1_bin` | Stores `latin1` / `latin1_bin`. |
| `CREATE DATABASE ... DEFAULT CHARACTER SET latin1` | Stores `latin1` / `latin1_swedish_ci`. |
| `ALTER DATABASE ... DEFAULT CHARACTER SET latin1 COLLATE latin1_bin` | Stores `latin1` / `latin1_bin`. |
| `ALTER DATABASE ... DEFAULT CHARACTER SET utf8mb3` | Stores `utf8mb3` / `utf8mb3_general_ci`. |
| `CREATE DATABASE ... DEFAULT CHARACTER SET utf8` | Stores `utf8mb3` / `utf8mb3_general_ci`, with warning `3719`. |
| `SET NAMES nosuchcharset` | Fails with unknown charset error `1115`. |
| `SET NAMES utf8mb4 COLLATE latin1_bin` | Fails with invalid collation/charset combination error `1253`. |
| `SET NAMES utf8mb4 COLLATE nosuchcollation` | Fails with unknown collation error `1273`. |
| `SET CHARACTER SET nosuchcharset` | Fails with unknown charset error `1115`. |
| `CREATE DATABASE ... DEFAULT CHARACTER SET nosuchcharset` | Fails with unknown charset error `1115`. |
| `CREATE DATABASE ... DEFAULT CHARACTER SET utf8mb4 COLLATE latin1_bin` | Fails with invalid collation/charset combination error `1253`. |

## Test plan

- Parser tests:
  - parse `SET NAMES utf8mb4`
  - parse `SET NAMES latin1 COLLATE latin1_bin`
  - parse quoted charset and collation names
  - parse `SET NAMES DEFAULT`
  - parse `SET CHARACTER SET utf8mb4`
  - parse `SET CHARSET DEFAULT`
  - reject unsupported `SET NAMES DEFAULT COLLATE ...`
  - keep unrelated general `SET` forms outside this feature
- Runtime tests:
  - verify initial MyLite connection state
  - verify `SET NAMES` default-collation behavior for all supported charsets
  - verify explicit compatible collations and case-insensitive normalization
  - verify `SET NAMES DEFAULT`
  - verify `SET CHARACTER SET` with no selected schema
  - verify `SET CHARACTER SET` with a selected schema default
  - verify unknown charset, unknown collation, and incompatible
    charset/collation errors
  - verify schema DDL stores normalized charset/collation names
  - verify charset-only and collation-only schema default inference
  - verify incompatible schema charset/collation combinations are rejected
  - preserve existing schema lifecycle and metadata catalog tests
