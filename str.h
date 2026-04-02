#pragma once
#include <stddef.h>
#include <string.h>
#include <assert.h>

/// @brief a String view/slice
typedef struct
{
    char *data;
    size_t len;
} tzl_str;

#define TZL_STR_FMT "%.*s"
#define TZL_STR_ARGS(s) (s).len, (s).data

tzl_str tzl_str_from_cstr(char *cstr);
tzl_str tzl_str_split(tzl_str *s, size_t len);

#ifdef TZL_STR_IMPLEMENTATION
tzl_str tzl_str_from_cstr(char *cstr)
{
    return (tzl_str){cstr, strlen(cstr)};
}

tzl_str tzl_str_split(tzl_str *s, size_t len)
{
    assert(len <= s->len);
    tzl_str r;
    r.data = s->data;
    r.len = len;

    s->data += len;
    s->len -= len;

    return r;
}
#endif

#ifdef TZL_STR_TEST
char *tzl_str_test()
{
    // Basic creation from C string
    char *cstr = "Hello, World!";
    tzl_str sut = tzl_str_from_cstr(cstr);
    if (strlen(cstr) != sut.len)
    {
        return "Str's length was not equal to strlen after creation from c string";
    }

    // Empty string handling
    char *empty = "";
    sut = tzl_str_from_cstr(empty);
    if (sut.len != 0)
    {
        return "Empty string's length should be 0";
    }
    if (sut.data != empty)
    {
        return "Empty string should still point to the same char*";
    }

    // Basic split functionality
    sut = tzl_str_from_cstr(cstr);
    tzl_str hello = tzl_str_split(&sut, strlen("Hello"));
    if (hello.len != strlen("Hello"))
    {
        return "Length of a split string should be equal to the length that it was split at";
    }
    if (sut.len != strlen(cstr) - hello.len)
    {
        return "Length of original string after split should be reduced by the amount split off";
    }

    // Additional: UTF-8 handling
    char *utf8_str = "\xF0\x9F\x98\x81";
    tzl_str utf8_sut = tzl_str_from_cstr(utf8_str);
    if (utf8_sut.len != 4)
    {
        return "UTF-8 string length should be 4 bytes";
    }

    // Additional: boundary splits
    char buffer[] = "abcd";
    tzl_str full_s = {buffer, 4};
    tzl_str full_split = tzl_str_split(&full_s, 4);
    if (full_split.len != 4 || strncmp(full_split.data, "abcd", 4) != 0)
    {
        return "Full split should return entire string";
    }
    if (full_s.len != 0)
    {
        return "Remaining string after full split should be empty";
    }

    full_s = (tzl_str){buffer, 4};
    tzl_str zero_split = tzl_str_split(&full_s, 0);
    if (zero_split.len != 0)
    {
        return "Zero split should return empty string";
    }
    if (full_s.len != 4 || strncmp(full_s.data, "abcd", 4) != 0)
    {
        return "Original string should be unchanged after zero split";
    }

    // Additional: sequential splits
    full_s = (tzl_str){buffer, 4};
    tzl_str seq1 = tzl_str_split(&full_s, 1);
    if (seq1.len != 1 || seq1.data[0] != 'a')
    {
        return "First sequential split failed";
    }
    tzl_str seq2 = tzl_str_split(&full_s, 2);
    if (seq2.len != 2 || strncmp(seq2.data, "bc", 2) != 0)
    {
        return "Second sequential split failed";
    }
    tzl_str seq3 = tzl_str_split(&full_s, 1);
    if (seq3.len != 1 || seq3.data[0] != 'd')
    {
        return "Third sequential split failed";
    }
    if (full_s.len != 0)
    {
        return "Remaining string after sequential splits should be empty";
    }

    // Additional: immutability check
    full_s = (tzl_str){buffer, 4};
    tzl_str immut_split = tzl_str_split(&full_s, 2);
    if (immut_split.data != buffer)
    {
        return "Split should not copy data, pointers should match";
    }

    return NULL;
}
#endif