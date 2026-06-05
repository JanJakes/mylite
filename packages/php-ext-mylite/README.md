# php-ext-mylite

`php-ext-mylite` builds the core PHP module named `mylite`.

Build it with the PHP preset:

```sh
cmake --preset php-dev
cmake --build --preset php-dev --target mylite_php_extension
```

Run a script with only the core extension loaded:

```sh
php -n -d extension=build/php-dev/packages/php-ext-mylite/mylite.so script.php
```
