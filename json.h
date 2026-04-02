#pragma once
#include <string.h>
#include "array.h"
#include <ctype.h>
#include "str.h"
#include <stdint.h>
#include <stdbool.h>

typedef enum
{
    tzlj_tk_invalid,
    tzlj_tk_array_start,
    tzlj_tk_array_end,
    tzlj_tk_obj_start,
    tzlj_tk_obj_end,
    tzlj_tk_string,
    tzlj_tk_number,
    tzlj_tk_literal,
} tzlj_tk;

typedef enum
{
    tzlj_vk_invalid,
    tzlj_vk_array,
    tzlj_vk_object,
    tzlj_vk_string,
    tzlj_vk_number,
    tzlj_vk_bool,
    tzlj_vk_null,
} tzlj_vk;

typedef struct tzlj_value tzlj_value;

struct tzlj_value
{
    tzlj_vk kind;
    union
    {
        struct
        {
            tzlj_value **data;
            size_t len;
            size_t cap;
        } array;
        struct
        {
            struct tzlj_object_kv
            {
                tzl_str key;
                tzlj_value *val;
            } *data;
            size_t len;
            size_t cap;
        } object;
        tzl_str string;
        struct
        {
            double number;
            tzl_str raw;
        } number;
        bool boolean;
    };
};

typedef struct
{
    tzlj_tk kind;
    tzl_str data;
} tzlj_token;

typedef struct
{
    tzlj_token *data;
    size_t len;
    size_t cap;
} tzl_j_ts;

#ifdef TZL_JSON_IMPLEMENTATION
tzlj_token tzlj_token_next(tzl_str *json)
{
    if (json->len == 0)
        return (tzlj_token){.kind = tzlj_tk_invalid, .data = *json};

    while ((isspace(*json->data) || ',' == *json->data || ':' == *json->data) && 0 < json->len)
    {
        tzl_str_split(json, 1);
    }

    if (*json->data == '[')
    {
        tzlj_token t = {.kind = tzlj_tk_array_start, .data = tzl_str_split(json, 1)};
        return t;
    }

    if (*json->data == ']')
    {
        tzlj_token t = {.kind = tzlj_tk_array_end, .data = tzl_str_split(json, 1)};
        return t;
    }

    if (*json->data == '{')
    {
        tzlj_token t = {.kind = tzlj_tk_obj_start, .data = tzl_str_split(json, 1)};
        return t;
    }

    if (*json->data == '}')
    {
        tzlj_token t = {.kind = tzlj_tk_obj_end, .data = tzl_str_split(json, 1)};
        return t;
    }

    if (*json->data == '"')
    {
        // Snip off the starting '"'
        tzl_str_split(json, 1);

        // Loop to the end of the string
        size_t len = 0;
        while (json->len > len)
        {
            if ('\\' == json->data[len])
            {
                len += 2;
                continue;
            }

            if ('"' == json->data[len])
            {
                break;
            }

            len++;
        }

        tzlj_token t = {.kind = tzlj_tk_string, .data = tzl_str_split(json, len)};
        // Snip off the final '"'
        tzl_str_split(json, 1);

        return t;
    }

    if (isdigit(*json->data))
    {
        size_t len = 0;
        while (json->len >= len)
        {
            if ('.' == json->data[len])
            {
                len++;
                continue;
            }

            if (!isdigit(json->data[len]))
            {
                break;
            }

            len++;
        }

        tzlj_token t = {.kind = tzlj_tk_number, .data = tzl_str_split(json, len)};
        return t;
    }

    // TODO WT: Its worth actually handling null, true and false directly, this is lazy
    if (isalnum(*json->data))
    {
        size_t len = 0;
        while (json->len >= len)
        {
            if (!isalnum(json->data[len]))
            {
                break;
            }

            len++;
        }

        tzlj_token t = {.kind = tzlj_tk_literal, .data = tzl_str_split(json, len)};
        return t;
    }

    return (tzlj_token){.kind = tzlj_tk_invalid, .data = tzl_str_split(json, 1)};
}

/// @brief
///     Parse a string as json, the json value tree is valid for the lifetime of the string
///
///     By using string views we don't need to allocate and deal with strings everywhere.
/// @param src
/// @return
tzlj_value *tzlj_parse(tzl_str src)
{
    struct
    {
        tzlj_value **data;
        size_t len;
        size_t cap;
    } stack = {0};

    tzl_str lexer = src;

    // Root array just to conveniently store any top level values that might have been submitted
    tzlj_value root = {
        .kind = tzlj_vk_array,
        .array = {0},
    };

    // The most recently popped value is top
    // when we start a new child, push this to the stack
    // when a child closes, pop the stack back into top
    tzlj_value *top = &root;

    tzlj_token t;
    while ((t = tzlj_token_next(&lexer), t.kind != tzlj_tk_invalid))
    {
        // Array and object mutate the top so we have to store it
        tzlj_value *parent = top;

        // Deal with anything ending so we can dip out and parse the next element pronto
        if (t.kind == tzlj_tk_array_end || t.kind == tzlj_tk_obj_end)
        {
            top = tzl_arr_pop(&stack);
            continue;
        }

        tzl_str key = {0};
        if (top->kind == tzlj_vk_object)
        {
            assert(t.kind == tzlj_tk_string);
            key = t.data;
            t = tzlj_token_next(&lexer);
        }

        tzlj_value *v = calloc(1, sizeof(*v));

        if (t.kind == tzlj_tk_array_start)
        {
            tzl_arr_push(&stack, top);
            v->kind = tzlj_vk_array;
            top = v;
        }

        if (t.kind == tzlj_tk_obj_start)
        {
            tzl_arr_push(&stack, top);
            v->kind = tzlj_vk_object;
            top = v;
        }

        if (t.kind == tzlj_tk_string)
        {
            v->kind = tzlj_vk_string;
            v->string = t.data;
        }

        if (t.kind == tzlj_tk_number)
        {
            v->kind = tzlj_vk_number;
            v->number.raw = t.data;
            char buf[128];
            memset(buf, 0, 128);
            memcpy(buf, t.data.data, t.data.len);
            v->number.number = atof(buf);
        }

        if (t.kind == tzlj_tk_literal)
        {
            char *lit_null = "null";
            char *lit_true = "true";
            char *lit_false = "false";

            if (memcmp(lit_null, t.data.data, t.data.len) == 0)
            {
                v->kind = tzlj_vk_null;
            }
            else if (memcmp(lit_true, t.data.data, t.data.len) == 0)
            {
                v->kind = tzlj_vk_bool;
                v->boolean = true;
            }
            else if (memcmp(lit_false, t.data.data, t.data.len) == 0)
            {
                v->kind = tzlj_vk_bool;
                v->boolean = false;
            }
            else
            {
                v->kind = tzlj_vk_invalid;
            }
        }

        if (parent->kind == tzlj_vk_array)
        {
            tzl_arr_push(&parent->array, v);
        }
        else if (parent->kind == tzlj_vk_object)
        {
            tzl_arr_push(&parent->object, ((struct tzlj_object_kv){.key = key, .val = v}));
        }
        else
        {
            assert(0);
        }
    }

    return root.array.data[0];
}

void tzlj_value_free(tzlj_value *value)
{
    // TODO WT: Recurse children and free
}

#endif

#ifdef TZL_JSON_TEST

char *tzlj_tk_to_cstr(tzlj_tk t)
{
    switch (t)
    {
    case tzlj_tk_invalid:
        return "tk_invalid";
    case tzlj_tk_array_start:
        return "tk_array_start";
    case tzlj_tk_array_end:
        return "tk_array_end";
    case tzlj_tk_obj_start:
        return "tk_obj_start";
    case tzlj_tk_obj_end:
        return "tk_obj_end";
    case tzlj_tk_string:
        return "tk_string";
    case tzlj_tk_number:
        return "tk_number";
    case tzlj_tk_literal:
        return "tk_literal";
        // case tk_null:
        //     return "tk_null";
    }
    return "";
}

char *tzl_json_test(void)
{
    tzl_str json_str = tzl_str_from_cstr("{\"keyname\": \"a string value\", \"anarray\": [0, 1, 2, 3, 4]}");
    // tzl_str json_str = tzl_str_from_cstr("\"Hello, World!\"");
    tzlj_value *val = tzlj_parse(json_str);

    // TODO WT: create a result object, store all values in an arena that is returned in the ret object
    // then implement a free function that will free the arena
    // We can also copy the input json into the arena so that referenced strings exist in the arena's lifetime

    return 0;
}
#endif