#include "bfc.hpp"

int main()
{
    constexpr auto src =
#include "example/mandelbrot.bf"
;

    bfc::parse(src, bfc::jit_compiler {})();
}
