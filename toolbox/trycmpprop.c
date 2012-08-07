#include <stdio.h>
#include <stdlib.h>

#include <cutils/properties.h>

#include <sys/system_properties.h>
#include "dynarray.h"

static void record_prop(const char* key, const char* name, void* opaque)
{
    strlist_t* list = opaque;
    char* temp = NULL;
    if (asprintf(&temp, "[%s]: [%s]", key, name)) {
        strlist_append_dup(list, temp);
        free(temp);
    }
}

static void list_properties(void)
{
    strlist_t  list[1] = { STRLIST_INITIALIZER };

    /* Record properties in the string list */
    (void)property_list(record_prop, list);

    /* Sort everything */
    strlist_sort(list);

    /* print everything */
    STRLIST_FOREACH(list, str, printf("%s\n", str));

    /* voila */
    strlist_done(list);
}

int trycmpprop_main(int argc, char *argv[])
{
    int n = 0;

    if (argc == 1) {
        list_properties();
    } else if (argc < 3) {
        printf("usage: trycmpprop name cmp_to_value [timeout]\n");
        printf("       - if no argument is given, all properties will be printed.\n");
    } else {
        char* value;
        int timeout = 0;

        /* enforce lenght of value */
        value = strndup(argv[2], PROPERTY_VALUE_MAX);
        if (!value) {
            printf("error allocating memory space, quitting...\n");
            return -1;
        }

        if (argc > 3) {
            timeout = atoi(argv[3]);
        } else {
            timeout = 0;
        }

        n = property_try_cmp(argv[1], value, timeout);
        printf("%d\n", n);

        free(value);
    }
    return 0;
}
