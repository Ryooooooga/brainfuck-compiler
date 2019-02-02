#ifndef INCLUDE_bfc_hpp
#define INCLUDE_bfc_hpp

#include <cstdint>
#include <ostream>
#include <string_view>
#include <vector>

namespace bfc
{
    template <typename Driver>
    auto parse(std::string_view src, Driver&& driver)
    {
        driver.start();

        for (char c : src)
        {
            switch (c)
            {
                case '<':
                    driver.emit_backward();
                    break;
                case '>':
                    driver.emit_forward();
                    break;
                case '+':
                    driver.emit_inc();
                    break;
                case '-':
                    driver.emit_dec();
                    break;
                case '[':
                    driver.emit_loop_begin();
                    break;
                case ']':
                    driver.emit_loop_end();
                    break;
                case '.':
                    driver.emit_write();
                    break;
                case ',':
                    driver.emit_read();
                    break;
                default:
                    driver.emit_comment(c);
                    break;
            }
        }

        return driver.finish();
    }

    class assembler
    {
    public:
        explicit assembler(std::ostream& stream)
            : stream_(stream)
            , loops_()
            , label_()
        {
        }

        void start()
        {
            loops_.clear();
            label_ = 0;

            stream_ <<
                "    .intel_syntax noprefix\n"
                "    .global _main\n"
                "_main:\n"
                "    push rbp\n"
                "    mov rbp, rsp\n"

                "    mov esi, 1\n"
                "    mov edi, 0x10000\n"
                "    call _calloc\n"
                "    mov r12, rax\n"
                "    mov rbx, rax\n";
        }

        void finish()
        {
            if (!loops_.empty())
            {
                throw std::invalid_argument {"unterminated loop"};
            }

            stream_ <<
                "    mov rdi, r12\n"
                "    call _free\n"

                "    mov rax, 0\n"
                "    mov rsp, rbp\n"
                "    pop rbp\n"
                "    ret\n";

            stream_.flush();
        }

        void emit_backward()
        {
            stream_ <<
                "    dec rbx\n";
        }

        void emit_forward()
        {
            stream_ <<
                "    inc rbx\n";
        }

        void emit_inc()
        {
            stream_ <<
                "    inc byte ptr [rbx]\n";
        }

        void emit_dec()
        {
            stream_ <<
                "    dec byte ptr [rbx]\n";
        }

        void emit_loop_begin()
        {
            const auto top = label_++;
            const auto end = label_++;

            loops_.emplace_back(top, end);

            stream_ <<
                ".L" << top << ":\n"
                "    cmp byte ptr [rbx], 0\n"
                "    jz .L" << end << "\n";
        }

        void emit_loop_end()
        {
            if (loops_.empty())
            {
                throw std::invalid_argument {"unresolved loop"};
            }

            const auto [top, end] = loops_.back();
            loops_.pop_back();

            stream_ <<
                "    jmp .L" << top << "\n"
                ".L" << end << ":\n";
        }

        void emit_write()
        {
            stream_ <<
                "    mov al, [rbx]\n"
                "    movsx edi, al\n"
                "    call _putchar\n";
        }

        void emit_read()
        {
        }

        void emit_comment([[maybe_unused]] char c)
        {
        }

    private:
        std::ostream& stream_;
        std::vector<std::pair<std::size_t, std::size_t>> loops_;
        std::size_t label_;
    };
}

#endif // INCLUDE_bfc_hpp
