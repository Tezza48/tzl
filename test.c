#include <stdio.h>

#define TZL_STR_IMPLEMENTATION
#define TZL_STR_TEST
#include "./str.h"

#define TZL_ARRAY_TEST
#include "./array.h"

#define TZL_JSON_TEST
#define TZL_JSON_IMPLEMENTATION
#include "./json.h"

#define TEST(x)                          \
    printf("Testing " #x "\n");          \
    if (err = x())                       \
        printf("    FAILED(%s)\n", err); \
    else                                 \
        printf("    PASSED\n");

int main(void)
{
    char *err = 0;

    TEST(tzl_str_test);
    TEST(tzl_arr_test);
    TEST(tzl_json_test);

    return 0;
}