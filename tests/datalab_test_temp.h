#ifndef DATALAB_TEST_TEMP_H
#define DATALAB_TEST_TEMP_H

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

/* Respect the caller's contained fixture root, including preparation sandboxes. */
static inline const char *datalab_test_temp_root(void) {
    const char *root = getenv("TMPDIR");
    return root && root[0] ? root : "/tmp";
}

static inline char *datalab_test_temp_dir(char *out, size_t capacity, const char *name) {
    char *root = realpath(datalab_test_temp_root(), NULL);
    int length;
    if (!root) return NULL;
    length = snprintf(out, capacity, "%s/%s.XXXXXX", root, name);
    free(root);
    if (length < 0 || (size_t)length >= capacity) return NULL;
    return mkdtemp(out);
}

#endif
