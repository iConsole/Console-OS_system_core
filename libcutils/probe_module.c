/*
 * Copyright (C) 2012 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <cutils/misc.h>

#define LOG_TAG "ProbeModule"
#include <cutils/log.h>


#define LDM_DEFAULT_DEP_FILE "/system/lib/modules/modules.dep"
#define LDM_DEFAULT_MOD_PATH "/system/lib/modules/"
#define LDM_INIT_DEP_NUM 10

extern int init_module(void *, unsigned long, const char *);
extern int delete_module(const char *, unsigned int);

static void dump_dep(char **dep)
{
    int d;

    for (d = 0; dep[d]; d++)
        ALOGD("DUMP DEP: %s\n", dep[d]);
}

static char * strip_path(const char * const str)
{
    char *ptr;
    int i;

    /* initialize pos to terminator */
    for (i = strlen(str); i > 0; i--)
        if (str[i - 1] == '/')
            break;

    return (char *)&str[i];
}

static void hyphen_to_underscore(char *str)
{
    while (str && *str != '\0') {
        if (*str == '-')
            *str = '_';
        str++;
    }
}

/* Compare module names, but don't differentiate '_' and '-'.
 * return: 0 when s1 is matched to s2 or size is zero.
 *         non-zero in any other cases.
 */
static int match_name(const char *s1, const char *s2, const size_t size)
{
    size_t i;

    if (!size)
        return 0;

    for (i = 0; i < size; i++, s1++, s2++) {

        if ((*s1 == '_' || *s1 == '-') && (*s2 == '_' || *s2 == '-'))
            continue;

        if (*s1 != *s2)
            return -1;

        if (*s1 == '\0')
            return 0;
    }

    return 0;
}

/* check if a line in dep file is target module's dependency.
 * return 1 when it is, otherwise 0 in any other cases.
 */
static int is_target_module(char *line, const char *target)
{
    char *token;
    char *name;
    size_t name_len;
    const char *suffix = ".ko";
    const char *delimiter = ":";
    int ret = 0;

    /* search token */
    token = strstr(line, delimiter);

    if (!token) {
        ALOGE("invalid line: no token\n");
        return 0;
    }

    /* only take stuff before the token */
    *token = '\0';

    /* use "module.ko" in comparision */
    name_len = strlen(suffix) + strlen(target) + 1;

    name = malloc(sizeof(char) * name_len);

    if (!name) {
        ALOGE("cannot alloc ram for comparision\n");
        return 0;
    }

    snprintf(name, name_len, "%s%s", target, suffix);

    ret = !match_name(strip_path(line), name, name_len);

    /* restore [single] token, keep line unchanged until we parse it later */
    *token = *delimiter;

    free(name);

    return ret;

}

/* turn a single string into an array of dependency.
 *
 * return: dependency array's address if it succeeded. Caller
 *         is responsible to free the array's memory.
 *         NULL when any error happens.
 */
static char** setup_dep(char *line)
{
    char *tmp;
    char *brk;
    int dep_num = LDM_INIT_DEP_NUM;
    char **new;
    int i;
    char **dep = NULL;

    dep = malloc(sizeof(char *) * dep_num);

    if (!dep) {
        ALOGE("cannot alloc dep array\n");
        return dep;
    }

    for (i = 0, tmp = strtok_r(line, ": ", &brk);
            tmp;
            tmp = strtok_r(NULL, ": ", &brk), i++) {

        /* check if we need enlarge dep array */
        if (!(i < dep_num - 1)) {

            dep_num += LDM_INIT_DEP_NUM;

            new = realloc(dep, dep_num);

            if (!new) {
                ALOGE("failed to enlarge dep buffer\n");
                free(dep);
                return NULL;
            }
            else
                dep = new;
        }

        dep[i] = tmp;

    }
    /* terminate array with a null pointer */
    dep[i] = NULL;

    return dep;
}

static int insmod(const char *path_name, const char *args)
{
    void *data;
    unsigned int len;
    int ret;

    data = load_file(path_name, &len);

    if (!data) {
        ALOGE("%s: Failed to load module file [%s]\n", __FUNCTION__, path_name);
        return -1;
    }

    ret = init_module(data, len, args);

    if (ret != 0 && errno != EEXIST) {
        ALOGE("%s: Failed to insmod [%s] with args [%s] error: %s ret: %d\n",
                __FUNCTION__, path_name, args, strerror(errno), ret);
        ret = -1;
    }
    else
        ret = 0;    /* if module is already in kernel, return success. */

    free(data);

    return ret;
}

/* install all modules in the dependency chain
 * deps    : A array of module file names, must be terminated by a NULL pointer
 * args    : The module parameters for target module.
 * strip   : Non-zero to strip out path info in the file name;
 *           0 to keep path info when loading modules.
 * base    : a prefix to module path, it will NOT be affected by strip flag.
 * return  : 0 for success or nothing to do; non-zero when any error occurs.
 */
static int insmod_s(char *dep[], const char *args, int strip, const char *base)
{
    char *name;
    char *path_name;
    int cnt;
    size_t len;
    int ret = 0;
    const char * base_dir = LDM_DEFAULT_MOD_PATH;

    if (base && strlen(base))
        base_dir = base;

    /* load modules in reversed order */
    for (cnt = 0; dep[cnt]; cnt++)
        ;

    while (cnt--) {

        name = strip ? strip_path(dep[cnt]) : dep[cnt];

        len = strlen(base_dir) + strlen(name) + 1;

        path_name = malloc(sizeof(char) * len);

        if (!path_name) {
            ALOGE("alloc module [%s] path failed\n", path_name);
            return -1;
        }

        snprintf(path_name, len, "%s%s", base_dir, name);

        if (cnt)
            ret = insmod(path_name, "");
        else
            ret = insmod(path_name, args);

        free(path_name);

        if (ret)
            break;
    }

    return ret;
}

static int rmmod(const char *mod_name, unsigned int flags)
{
    return delete_module(mod_name, flags);
}

/* remove all modules in a dependency chain
 * NOTE: We assume module name in kernel is same as the file name without .ko
 */
static int rmmod_s(char *dep[], unsigned int flags)
{
    int i;
    int ret = 0;
    char * mod_name;

    for (i = 0; dep[i]; i++) {
        size_t len;
        mod_name = strip_path(dep[i]);
        len = strlen(mod_name);

        if (len > strlen(".ko")
                && mod_name[len - 1] == 'o'
                        && mod_name[len - 2] == 'k'
                                && mod_name[len - 3] == '.') {

            mod_name[len - 3] = '\0';

            hyphen_to_underscore(mod_name);

            ret = rmmod(mod_name, flags);

            if (ret) {
                ALOGE("%s: Failed to remove module [%s] error (%s)\n",
                        __FUNCTION__, mod_name, strerror(errno));
                break;

            }
        }
    }

    return ret;
}

/* look_up_dep() find and setup target module's dependency in modules.dep
 *
 * dep_file:    a pointer to module's dep file loaded in memory, its content
 *              will be CHANGED during parsing.
 *
 * return:      a pointer to an array which holds the dependency strings and
 *              terminated by a NULL pointer. Caller is responsible to free the
 *              array's memory.
 *
 *              non-zero in any other cases. Content of dep array is invalid.
 */
static char ** look_up_dep(const char *module_name, void *dep_file)
{

    char *line;
    char *saved_pos;
    char *start;
    int ret = -1;
    char **dep = NULL;

    if (!dep_file || !module_name || *module_name == '\0')
        return NULL;

    start = (char *)dep_file;

    /* We expect modules.dep file has a new line char before EOF. */
    while ((line = strtok_r(start, "\n", &saved_pos)) != NULL) {

        start = NULL;

        if (is_target_module(line, module_name)) {

            dep = setup_dep(line);
            /* job done */
            break;
        }
    }

    return dep;
}

/* load_dep_file() load a dep file (usually it is modules.dep)
 * into memory. Caller is responsible to free the memory.
 *
 * file_name:   dep file's name, if it is NULL or an empty string,
 *              This function will try to load a dep file in the
 *              default path defined in LDM_DEFAULT_DEP_FILE
 *
 * return:      a pointer to the allocated mem which holds all
 *              content of the depfile. a zero pointer will be
 *              returned for any errors.
 * */
static void *load_dep_file(const char *file_name)
{
    const char *dep_file_name = LDM_DEFAULT_DEP_FILE;
    unsigned int len;

    if (file_name && *file_name != '\0')
        dep_file_name = file_name;

    return load_file(dep_file_name, &len);
}

/* insmod_by_dep() interface to outside,
 * refer to its description in probe_module.h
 */
int insmod_by_dep(const char *module_name,
        const char *args,
        const char *dep_name,
        int strip,
        const char *base)
{
    void *dep_file;
    char **dep = NULL;
    int ret = -1;

    if (!module_name || *module_name == '\0') {
        ALOGE("need valid module name\n");
        return ret;
    }

    dep_file = load_dep_file(dep_name);

    if (!dep_file) {
        ALOGE("cannot load dep file : %s\n", dep_name);
        return ret;
    }

    dep = look_up_dep(module_name, dep_file);

    if (!dep) {
        ALOGE("%s: cannot load module: [%s]\n", __FUNCTION__, module_name);
        goto free_file;
    }

    ret = insmod_s(dep, args, strip, base);

    free(dep);

free_file:
    free(dep_file);

    return ret;

}

/* rmmod_by_dep() interface to outside,
 * refer to its description in probe_module.h
 */
int rmmod_by_dep(const char *module_name,
        const char *dep_name)
{
    void *dep_file;
    char **dep = NULL;
    int ret = -1;

    if (!module_name || *module_name == '\0') {
        ALOGE("need valid module name\n");
        return ret;
    }

    dep_file = load_dep_file(dep_name);

    if (!dep_file) {
        ALOGE("cannot load dep file : %s\n", dep_name);
        return ret;
    }

    dep = look_up_dep(module_name, dep_file);

    if (!dep) {
        ALOGE("%s: cannot remove module: [%s]\n", __FUNCTION__, module_name);
        goto free_file;
    }

    ret = rmmod_s(dep, O_NONBLOCK);

    free(dep);

free_file:
    free(dep_file);

    return ret;
}

/* end of file */
