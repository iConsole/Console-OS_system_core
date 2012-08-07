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

int trygetprop_main(int argc, char *argv[])
{
    int n = 0;

    /* usage: trygetprop property_name [timeout [default_value]] */

    if (argc == 1) {
        list_properties();
    } else {
        char value[PROPERTY_VALUE_MAX];
        char *default_value;
        int timeout = 0;
        if(argc > 3) {
            timeout = atoi(argv[2]);
            default_value = argv[3];
        } else if(argc > 2) {
            timeout = atoi(argv[2]);
            default_value = "";
        } else {
            timeout = 0;
            default_value = "";
        }

        property_try_get(argv[1], value, default_value, timeout);
        printf("%s\n", value);
    }
    return 0;
}
