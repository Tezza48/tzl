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

typedef struct
{
    void *chunk;
    size_t cap;
    size_t head;
} tzlj_arena;

typedef struct tzlj_value tzlj_value;

typedef struct
{
    tzl_str key;
    tzlj_value *val;
} tzlj_object_kv;

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
            tzlj_object_kv *data;
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

void *tzlj_arena_alloc(tzlj_arena *arena, size_t bytes);
tzlj_arena tzlj_arena_create(size_t bytes);
void tzlj_arena_free(tzlj_arena *arena);

tzlj_token tzlj_token_next(tzl_str *json);
tzlj_value *tzlj_parse(tzl_str src, tzlj_arena *arena);

#ifdef TZL_JSON_IMPLEMENTATION
void *tzlj_arena_alloc(tzlj_arena *arena, size_t bytes)
{
    assert(arena->head + bytes <= arena->cap);

    void *result = (char *)arena->chunk + arena->head;
    arena->head += bytes;
    return result;
}

tzlj_arena tzlj_arena_create(size_t bytes)
{
    tzlj_arena result = {
        .chunk = calloc(bytes, sizeof(char)),
        .head = 0,
        .cap = bytes,
    };
    return result;
}

void tzlj_arena_free(tzlj_arena *arena)
{
    free(arena->chunk);
    arena->chunk = 0;
    arena->cap = 0;
    arena->head = 0;
}

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
        return (tzlj_token){.kind = tzlj_tk_array_start, .data = tzl_str_split(json, 1)};
    }

    if (*json->data == ']')
    {
        return (tzlj_token){.kind = tzlj_tk_array_end, .data = tzl_str_split(json, 1)};
    }

    if (*json->data == '{')
    {
        return (tzlj_token){.kind = tzlj_tk_obj_start, .data = tzl_str_split(json, 1)};
    }

    if (*json->data == '}')
    {
        return (tzlj_token){.kind = tzlj_tk_obj_end, .data = tzl_str_split(json, 1)};
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

tzlj_value *tzlj_parse(tzl_str src, tzlj_arena *arena)
{
    struct
    {
        tzlj_value **data;
        size_t len;
        size_t cap;
    } stack = {0};

    // Copy the string into the arena, so the lifetime of strings is in the arena not the passed in slice
    tzl_str lexer = {
        .data = tzlj_arena_alloc(arena, src.len),
        .len = src.len,
    };
    memcpy(lexer.data, src.data, src.len);

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

        tzlj_value *v = tzlj_arena_alloc(arena, sizeof(*v));

        switch (t.kind)
        {
        case tzlj_tk_array_start:
        {
            tzl_arr_push(&stack, top);
            v->kind = tzlj_vk_array;
            top = v;
            break;
        }
        case tzlj_tk_obj_start:
        {

            tzl_arr_push(&stack, top);
            v->kind = tzlj_vk_object;
            top = v;
            break;
        }
        case tzlj_tk_string:
        {
            v->kind = tzlj_vk_string;
            v->string = t.data;
            break;
        }
        case tzlj_tk_number:
        {
            v->kind = tzlj_vk_number;
            v->number.raw = t.data;
            char buf[128];
            memset(buf, 0, 128);
            memcpy(buf, t.data.data, t.data.len);
            v->number.number = atof(buf);
            break;
        }
        case tzlj_tk_literal:
        {
            const struct
            {
                const char *str;
                tzlj_vk kind;
                bool bool_value;
            } literals[] = {
                {.str = "null", .kind = tzlj_vk_null, .bool_value = false},
                {.str = "true", .kind = tzlj_vk_bool, .bool_value = true},
                {.str = "false", .kind = tzlj_vk_bool, .bool_value = false},
            };
            const size_t num_literals = sizeof(literals) / sizeof(*literals);

            bool is_valid = false;
            for (size_t lit = 0; lit < num_literals; lit++)
            {
                if (memcmp(literals[lit].str, t.data.data, t.data.len) == 0)
                {
                    v->kind = literals[lit].kind;
                    v->boolean = literals[lit].bool_value;
                    is_valid = true;
                    ;
                }
            }

            if (!is_valid)
            {
                v->kind = tzlj_vk_invalid;
            }
            break;
        }
        }

        if (parent->kind == tzlj_vk_array)
        {
            tzl_arr_push(&parent->array, v);
        }
        else if (parent->kind == tzlj_vk_object)
        {
            tzl_arr_push(&parent->object, ((tzlj_object_kv){.key = key, .val = v}));
        }
        else
        {
            assert(0);
        }
    }

    return root.array.data[0];
}
#endif

#ifdef TZL_JSON_TEST

char *tzl_json_test(void)
{
    tzlj_arena arena = tzlj_arena_create(1024);
    char *src = "{\"name\": \"TZL Json Tests\", \"values\": [1, 2, 3], \"flag\": true, \"missing\": null}";
    tzl_str json = tzl_str_from_cstr(src);

    tzlj_value *root = tzlj_parse(json, &arena);
    if (root->kind != tzlj_vk_object)
        return "json parse: root is not object";
    if (root->object.len != 4)
        return "json parse: object length mismatch";
    if (root->object.data[0].key.len != strlen("name") || strncmp(root->object.data[0].key.data, "name", root->object.data[0].key.len) != 0)
        return "json parse: first key mismatch";
    if (root->object.data[0].val->kind != tzlj_vk_string)
        return "json parse: first value kind mismatch";
    if (root->object.data[0].val->string.len != strlen("TZL Json Tests") || strncmp(root->object.data[0].val->string.data, "TZL Json Tests", root->object.data[0].val->string.len) != 0)
        return "json parse: string value mismatch";

    if (root->object.data[1].key.len != strlen("values") || strncmp(root->object.data[1].key.data, "values", root->object.data[1].key.len) != 0)
        return "json parse: second key mismatch";
    if (root->object.data[1].val->kind != tzlj_vk_array)
        return "json parse: values kind mismatch";
    if (root->object.data[1].val->array.len != 3)
        return "json parse: array length mismatch";
    if (root->object.data[1].val->array.data[0]->number.number != 1.0)
        return "json parse: first array number mismatch";

    if (root->object.data[2].key.len != strlen("flag") || strncmp(root->object.data[2].key.data, "flag", root->object.data[2].key.len) != 0)
        return "json parse: third key mismatch";
    if (root->object.data[2].val->kind != tzlj_vk_bool || root->object.data[2].val->boolean != true)
        return "json parse: bool value mismatch";

    if (root->object.data[3].key.len != strlen("missing") || strncmp(root->object.data[3].key.data, "missing", root->object.data[3].key.len) != 0)
        return "json parse: fourth key mismatch";
    if (root->object.data[3].val->kind != tzlj_vk_null)
        return "json parse: null value mismatch";

    tzlj_arena_free(&arena);
    return NULL;
}
#endif