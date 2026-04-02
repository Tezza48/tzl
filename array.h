#pragma once
#include <stdlib.h>
#include <assert.h>

#define tzl_arr_push(p_arr, rvalue)                                                        \
    do                                                                                     \
    {                                                                                      \
        if (0 == (p_arr)->data)                                                            \
        {                                                                                  \
            (p_arr)->data = calloc(64, sizeof(*(p_arr)->data));                            \
            assert((p_arr)->data && "Could not allocate initial array data");              \
            (p_arr)->cap = 64;                                                             \
        }                                                                                  \
        if ((p_arr)->len == (p_arr)->cap)                                                  \
        {                                                                                  \
            (p_arr)->cap *= 2;                                                             \
            (p_arr)->data = realloc((p_arr)->data, (p_arr)->cap * sizeof(*(p_arr)->data)); \
            assert((p_arr)->data && "Could not reallocate array data");                    \
        }                                                                                  \
        (p_arr)->data[(p_arr)->len++] = (rvalue);                                          \
    } while (0)

#define tzl_arr_pop(p_arr) (assert((p_arr)->len > 0), (p_arr)->data[--(p_arr)->len])

#define tzl_arr_free(p_arr) (free((p_arr)->data), (p_arr)->len = 0, (p_arr)->cap = 0, 0)

#ifdef TZL_ARRAY_TEST

char *tzl_arr_test()
{
    struct
    {
        int *data;
        size_t len;
        size_t cap;
    } arr = {0};

    for (int i = 0; i < 5; i++)
    {
        tzl_arr_push(&arr, i);
        if (i != arr.data[i])
            return "Couldn't push to the array";
    }

    if (5 != arr.len)
        return "Array len wasn't 5";
    if (64 != arr.cap)
        return "Array cap wasn't 64";

    while (arr.len)
    {
        int x = tzl_arr_pop(&arr);
    }

    if (0 != arr.len)
        return "Array len wasn't 0 after popping all elements";
    tzl_arr_free(&arr);

    return 0;
}
#endif
