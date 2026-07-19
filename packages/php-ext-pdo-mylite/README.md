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
