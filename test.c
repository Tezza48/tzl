#include <stdio.h>

#define TZL_ARRAY_TEST
#include "array.h"

#define TZL_JSON
#define TZL_JSON_TEST
#include "json.h"

#define TEST(x)                          \
    printf("Testing " #x "\n");          \
    if (err = x())                       \
        printf("    FAILED(%s)\n", err); \
    else                                 \
        printf("    PASSED\n");

int main(void)
{
    char *err = 0;
    TEST(tzl_arr_test);
    TEST(tzl_json_test);

    return 0;
}