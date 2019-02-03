#include "bfc.hpp"

#include <iostream>

int main()
{
    constexpr auto src =
#include "example/mandelbrot.bf"
;

    bfc::parse(src, bfc::jit_compiler {})();
}
