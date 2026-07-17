#include <mylite/mylite.h>

#include <stdio.h>

int main(int argc, char **argv) {
    mylite_db *database = NULL;
    int rc = MYLITE_MISUSE;

    if (argc != 2) {
        fprintf(stderr, "usage: catalog-version-probe FILE\n");
        return 2;
    }
    rc = mylite_open(argv[1], &database);
    if (rc != MYLITE_OK) {
        fprintf(stderr, "mylite_open returned %d\n", rc);
        return 1;
    }
    mylite_close(database);
    return 0;
}
