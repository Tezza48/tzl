#pragma once
#include <string.h>
#include "array.h"
#include <ctype.h>

typedef struct
{
    char *data;
    size_t len;
} str;

#define STR_FMT "%.*s"
#define STR_ARGS(s) (s).len, (s).data

str to_str(char *cstr)
{
    return (str){cstr, strlen(cstr)};
}

str str_split(str *s, size_t len)
{
    assert(len <= s->len);
    str r;
    r.data = s->data;
    r.len = len;

    s->data += len;
    s->len -= len;

    return r;
}

typedef enum
{
    tk_invalid,
    tk_array_start,
    tk_array_end,
    tk_obj_start,
    tk_obj_end,
    tk_string,
    tk_number,
    tk_literal,
} tk;

char *tk_to_cstr(tk t)
{
    switch (t)
    {
    case tk_invalid:
        return "tk_invalid";
    case tk_array_start:
        return "tk_array_start";
    case tk_array_end:
        return "tk_array_end";
    case tk_obj_start:
        return "tk_obj_start";
    case tk_obj_end:
        return "tk_obj_end";
    case tk_string:
        return "tk_string";
    case tk_number:
        return "tk_number";
    case tk_literal:
        return "tk_literal";
        // case tk_null:
        //     return "tk_null";
    }
    return "";
}

typedef struct
{
    tk kind;
    str t;
} token;

typedef struct
{
    token *data;
    size_t len;
    size_t cap;
} tzl_j_ts;

token token_next(str *json)
{
    if (json->len == 0)
        return (token){.kind = tk_invalid, .t = *json};

    while ((isspace(*json->data) || ',' == *json->data || ':' == *json->data) && 0 < json->len)
    {
        str_split(json, 1);
    }

    if (*json->data == '[')
    {
        token t = {.kind = tk_array_start, .t = str_split(json, 1)};
        return t;
    }

    if (*json->data == ']')
    {
        token t = {.kind = tk_array_end, .t = str_split(json, 1)};
        return t;
    }

    if (*json->data == '{')
    {
        token t = {.kind = tk_obj_start, .t = str_split(json, 1)};
        return t;
    }

    if (*json->data == '}')
    {
        token t = {.kind = tk_obj_end, .t = str_split(json, 1)};
        return t;
    }

    if (*json->data == '"')
    {
        // Snip off the starting '"'
        str_split(json, 1);

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

        token t = {.kind = tk_string, .t = str_split(json, len)};
        // Snip off the final '"'
        str_split(json, 1);

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

        token t = {.kind = tk_number, .t = str_split(json, len)};
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

        token t = {.kind = tk_literal, .t = str_split(json, len)};
        return t;
    }

    return (token){.kind = tk_invalid, .t = str_split(json, 1)};

    // TODO WT: ultimately shouldnt reach here when we're done.
}

#ifdef TZL_JSON_TEST
char *tzl_json_test(void)
{
    str json_str = to_str("{\"key\": 25, \"other key\": \"Some string value\", \"an array\": [0, 1, 2, 3, 420.69, null, \"hello\"]}");
    tzl_j_ts tokens = {0};

    token t;
    while ((t = token_next(&json_str), t.kind != tk_invalid))
    {
        tzl_arr_push(&tokens, t);
    }

    for (size_t i = 0; i < tokens.len; i++)
    {
        token t = tokens.data[i];
        printf("    Token: %-15s -> " STR_FMT "\n", tk_to_cstr(t.kind), STR_ARGS(t.t));
    }

    return 0;
}
#endif