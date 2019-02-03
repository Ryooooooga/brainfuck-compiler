#ifndef INCLUDE_bfc_hpp
#define INCLUDE_bfc_hpp

#include <cstdint>
#include <memory>
#include <ostream>
#include <string_view>
#include <vector>

#include <sys/mman.h>

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

        assembler(const assembler&) =delete;
        assembler(assembler&&) =delete;

        assembler& operator =(const assembler&) =delete;
        assembler& operator =(assembler&&) =delete;

        ~assembler() noexcept =default;

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
                "    mov rbx, rax\n"
                "    mov r12, rax\n";
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
            stream_ <<
                "    call _getchar\n"
                "    mov [rbx], al\n";
        }

        void emit_comment([[maybe_unused]] char c)
        {
        }

    private:
        std::ostream& stream_;
        std::vector<std::pair<std::size_t, std::size_t>> loops_;
        std::size_t label_;
    };

    class function
    {
    public:
        explicit function(const std::uint8_t* data, std::size_t size)
            : data_(::mmap(nullptr, size, PROT_READ | PROT_WRITE | PROT_EXEC, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0))
            , size_(size)
        {
            if (data_ == MAP_FAILED)
            {
                throw std::bad_alloc {};
            }

            ::memcpy(data_, data, size_);
        }

        function(const function&) =delete;
        function(function&&) =delete;

        function& operator =(const function&) =delete;
        function& operator =(function&&) =delete;

        ~function() noexcept
        {
            ::munmap(data_, size_);
        }

        void operator ()() const
        {
            reinterpret_cast<void(*)()>(data_)();
        }

    private:
        void* data_;
        std::size_t size_;
    };

    class jit_compiler
    {
    public:
        explicit jit_compiler() =default;

        jit_compiler(const jit_compiler&) =delete;
        jit_compiler(jit_compiler&&) =delete;

        jit_compiler& operator =(const jit_compiler&) =delete;
        jit_compiler& operator =(jit_compiler&&) =delete;

        ~jit_compiler() noexcept =default;

        void start()
        {
            code_.clear();
            loops_.clear();

            code_.insert(std::end(code_), {
                0x55,                                                       // push rbp
                0x48, 0x89, 0xe5,                                           // mov rbp, rsp
                0xbe, 0x01, 0x00, 0x00, 0x00,                               // mov esi, 1
                0xbf, 0x00, 0x00, 0x01, 0x00,                               // mov edi, 0x10000
                0x48, 0xb8, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // mov rax, calloc
                0xff, 0xd0,                                                 // call rax
                0x48, 0x89, 0xc3,                                           // mov rbx, rax
                0x49, 0x89, 0xc4,                                           // mov r12, rax
                0x49, 0xbd, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // mov r13, putchar
                0x49, 0xbe, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // mov r14, getchar
            });

            const auto calloc_ptr = reinterpret_cast<std::uintptr_t>(calloc);
            const auto putchar_ptr = reinterpret_cast<std::uintptr_t>(putchar);
            const auto getchar_ptr = reinterpret_cast<std::uintptr_t>(getchar);

            code_[0x10] = calloc_ptr >>  0;
            code_[0x11] = calloc_ptr >>  8;
            code_[0x12] = calloc_ptr >> 16;
            code_[0x13] = calloc_ptr >> 24;
            code_[0x14] = calloc_ptr >> 32;
            code_[0x15] = calloc_ptr >> 40;
            code_[0x16] = calloc_ptr >> 48;
            code_[0x17] = calloc_ptr >> 56;

            code_[0x22] = putchar_ptr >>  0;
            code_[0x23] = putchar_ptr >>  8;
            code_[0x24] = putchar_ptr >> 16;
            code_[0x25] = putchar_ptr >> 24;
            code_[0x26] = putchar_ptr >> 32;
            code_[0x27] = putchar_ptr >> 40;
            code_[0x28] = putchar_ptr >> 48;
            code_[0x29] = putchar_ptr >> 56;

            code_[0x2c] = getchar_ptr >>  0;
            code_[0x2d] = getchar_ptr >>  8;
            code_[0x2e] = getchar_ptr >> 16;
            code_[0x2f] = getchar_ptr >> 24;
            code_[0x30] = getchar_ptr >> 32;
            code_[0x31] = getchar_ptr >> 40;
            code_[0x32] = getchar_ptr >> 48;
            code_[0x33] = getchar_ptr >> 56;
        }

        function finish()
        {
            if (!loops_.empty())
            {
                throw std::invalid_argument {"unterminated loop"};
            }

            code_.insert(std::end(code_), {
                0x4c, 0x89, 0xe7,                                           // mov rdi, r12
                0x48, 0xb8, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // mov rax, free
                0xff, 0xd0,                                                 // call rax
                0x48, 0x89, 0xec,                                           // mov rsp, rbp
                0x5d,                                                       // pop rbp
                0xc3,                                                       // ret
            });

            const auto free_ptr = reinterpret_cast<std::uintptr_t>(free);

            code_[code_.size() - 0x0f] = free_ptr >>  0;
            code_[code_.size() - 0x0e] = free_ptr >>  8;
            code_[code_.size() - 0x0d] = free_ptr >> 16;
            code_[code_.size() - 0x0c] = free_ptr >> 24;
            code_[code_.size() - 0x0b] = free_ptr >> 32;
            code_[code_.size() - 0x0a] = free_ptr >> 40;
            code_[code_.size() - 0x09] = free_ptr >> 48;
            code_[code_.size() - 0x08] = free_ptr >> 56;

            return function {code_.data(), code_.size()};
        }

        void emit_backward()
        {
            code_.insert(std::end(code_), {
                0x48, 0xff, 0xc3, // inc [rbx]
            });
        }

        void emit_forward()
        {
            code_.insert(std::end(code_), {
                0x48, 0xff, 0xcb, // dec [rbx]
            });
        }

        void emit_inc()
        {
            code_.insert(std::end(code_), {
                0xfe, 0x03,       // inc byte ptr [rbx]
            });
        }

        void emit_dec()
        {
            code_.insert(std::end(code_), {
                0xfe, 0x0b,       // dec byte ptr [rbx]
            });
        }

        void emit_loop_begin()
        {
            loops_.emplace_back(code_.size());

            code_.insert(std::end(code_), {
                0x80, 0x3b, 0x00,                   // cmp byte ptr [rbx], 0
                0x0f, 0x84, 0x00, 0x00, 0x00, 0x00, // jz .end
            });
        }

        void emit_loop_end()
        {
            if (loops_.empty())
            {
                throw std::invalid_argument {"unresolved loop"};
            }

            code_.insert(std::end(code_), {
                0xe9, 0x00, 0x00, 0x00, 0x00, // jmp .top
            });

            const auto loop = loops_.back();
            const auto top = loop - code_.size();
            const auto end = code_.size() - loop - 9;

            loops_.pop_back();

            code_[loop + 5] = end >>  0;
            code_[loop + 6] = end >>  8;
            code_[loop + 7] = end >> 16;
            code_[loop + 8] = end >> 24;

            code_[code_.size() - 4] = top >>  0;
            code_[code_.size() - 3] = top >>  8;
            code_[code_.size() - 2] = top >> 16;
            code_[code_.size() - 1] = top >> 24;
        }

        void emit_write()
        {
            code_.insert(std::end(code_), {
                0x8a, 0x03,       // mov al, [rbx]
                0x0f, 0xbe, 0xf8, // movsb edi, al
                0x41, 0xff, 0xd5, // call r13
            });
        }

        void emit_read()
        {
            code_.insert(std::end(code_), {
                0x41, 0xff, 0xd6, // call r14
                0x88, 0x03,       // mov [rbx], al
            });
        }

        void emit_comment([[maybe_unused]] char c)
        {
        }

    private:
        std::vector<uint8_t> code_;
        std::vector<std::size_t> loops_;
    };
}

#endif // INCLUDE_bfc_hpp
