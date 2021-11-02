#include <stdlib.h>

#include "libmicroarchi.h"

int test_highest_value (int* array, size_t array_size, int expected_value)
{
    printf ("########\n");
    printf ("Array : { ");
    for (int i = 0; i < array_size; i++) {
        printf ("%d, ", array[i]);
    }
    printf ("}\n");

    int highest_value = libmicro_get_highest_value_index (array, array_size);
    printf ("Value returned : %d\n", highest_value);

    if (highest_value == expected_value) {
        OK_NOARG ("Test ok !");
    }
    else {
        ERR ("Test failed !");
    }
}

int main (int argc, char const* argv[])
{
    LOG ("RAM time (average): %lu cycles", libmicro_get_ram_hit_threshold ());
    LOG ("Cache time (average): %lu cycles", libmicro_get_cache_hit_threshold ());

    int array1[] = { 1, 2, 3, 4, 5 };
    test_highest_value (array1, 5, 4);

    int array2[] = { 5, 4, 3, 2, 1 };
    test_highest_value (array2, 5, 0);

    int array3[] = { 0, 0, 0, 0, 0 };
    test_highest_value (array3, 5, -1);

    int array4[] = { 0, 0, 3, 0, 0 };
    test_highest_value (array4, 5, 2);

    int array5[] = { 0, 1, 0, 1, 1 };
    test_highest_value (array5, 5, 1);

    return EXIT_SUCCESS;
}