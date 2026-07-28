# php-ext-pdo-mylite

`php-ext-pdo-mylite` builds the `pdo_mylite` PHP module and registers the PDO
driver named `mylite`.

Build it with the PHP preset:

```sh
cmake --preset php-dev
cmake --build --preset php-dev --target mylite_php_pdo_extension
```

## Cursor contract

PDO MyLite statements use forward-only cursors. Scrollable cursor preparation,
non-next fetch orientations, and nonzero fetch offsets fail explicitly. As with
PDO MySQL, the optional `PDO::lastInsertId()` name argument is accepted and
ignored; MyLite returns the connection's generated insert ID.

PDO fetches return representable integral and BIT values as PHP integers and
FLOAT/DOUBLE values as PHP floats. DECIMAL, overflowing exact integers, text,
binary, temporal, JSON, geometry, and unknown values remain strings, and SQL
NULL remains `null`. `PDO::ATTR_STRINGIFY_FETCHES` stringifies numeric fetches
when enabled.

`PDO::ATTR_CLIENT_VERSION` identifies the MyLite library.
`PDO::ATTR_SERVER_VERSION` exposes the MySQL compatibility identity and equals
`SELECT VERSION()`.

`PDOStatement::getColumnMeta()` publishes the native type, PDO parameter type,
MySQL PDO flags, table and column names, display length, and decimal precision
from MyLite's result descriptor. Metadata remains available for empty result
sets. Current native expression and aggregate descriptor differences remain
visible rather than being hidden in the adapter.

PDO MyLite buffers row-producing statements. Direct and prepared SELECT
`rowCount()` therefore report the complete selected-row count immediately
after execution and retain it across fetching. Non-row statements report
affected rows.
