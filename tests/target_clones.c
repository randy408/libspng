/* This will only be available for GCC with glibc for the foreseeable future */

#include <stdlib.h> /* EXIT_*, exit */

 __attribute__((target_clones("default,avx2"))) int f(int x)
{
    return x + 3;
}

int main(void)
{
    const int y = f(39);
    if (y != 42) {
        exit(EXIT_FAILURE);
    }

    return EXIT_SUCCESS;
}
