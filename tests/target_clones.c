/* This will only be available for GCC with glibc for the foreseeable future */

#include <stdlib.h>

 __attribute__((target_clones("default,avx2"))) int f(int x)
{
    return x + 3;
}

int main(void)
{
    const int y = f(39);
    return y == 42 ? EXIT_SUCCESS : EXIT_FAILURE;
}
