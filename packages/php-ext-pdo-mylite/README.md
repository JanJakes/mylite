# php-ext-pdo-mylite

`php-ext-pdo-mylite` builds the `pdo_mylite` PHP module and registers the PDO
driver named `mylite`.

Build it with the PHP preset:

```sh
cmake --preset php-dev
cmake --build --preset php-dev --target mylite_php_pdo_extension
```
