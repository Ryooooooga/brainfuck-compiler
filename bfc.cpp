#include "bfc.hpp"

#include <iostream>

int main()
{
    bfc::parse(R"(+++++++++[>++++++++>+++++++++++>+++++<<<-]>.>++.+++++++..+++.>-------------.<<+++++++++++++++.>.+++.------.--------.>+.)", bfc::assembler {std::cout});
}
