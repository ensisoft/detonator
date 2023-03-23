
// g++ -o test -Wall -std=c++17 test.cpp

#include <cstdio>
#include <cmath>
#include <cstring>

void string_padding()
{
    // %6s to right pad a string to a minimum of 6 chars

    printf("string pad to 20\n");
    printf("------------------------\n");
    printf("%20s\n", "left pad"); // print spaces before the string.
    printf("%-20s\n", "right pad"); // print spaces after the string.
}

void float_precision()
{
    // single precision floating point values have precision loss relative
    // to the magnitude of the value itself.
    // here's some more information
    // https://randomascii.wordpress.com/2012/02/13/dont-store-that-in-a-float/

    printf("\n\n%-10s %20s %20s %20s\n", "Name", "Value", "Next", "Precision");

    const char* const names[] = {"one", "ten", "hundred", "1k", "10k", "100k", "1m", "10m", "100m", "1b"};

    for (unsigned i=0; i<10; ++i)
    {
        const float value = powf(10.0f, i);
        unsigned val;
        memcpy(&val, &value, 4);
        ++val;
        //const float next  = nextafterf(value, value+1);
        float next;
        memcpy(&next, &val, 4);

        const float diff  = next - value;
        printf("%-10s %20.9f %20.9f %20.9f\n", names[i], value, next, diff);
    }

    printf("\n\n");

    // demonstration of some instability that can come around and bite
    // us in the proverbial behind.
    {
        float top = 12.6130676;
        float bottom = 970.467957;
        // the result of top + (bottom-top) doesn't equal bottom
        printf("%f", top + (bottom-top));
    }

    printf("\n\n");
}

int main()
{
    string_padding();
    float_precision();

    return 0;
}