#pragma once
#include <ice/arctic_syntax_visitor.hxx>
#include <ice/arctic_lexer.hxx>

#include <vector>

namespace ice::arctic
{

    class Parser : public ice::arctic::SyntaxNodeAllocator
    {
    public:
        void parse(ice::arctic::Lexer& lexer) noexcept;

        void add_visitor(ice::arctic::SyntaxVisitorBase& visitor) noexcept
        {
            _visitors.push_back(&visitor);
        }

        auto allocate(ice::u64 size, ice::u64 align) noexcept -> void*;
        void deallocate(void* ptr) noexcept;

    private:
        std::vector<ice::arctic::SyntaxVisitorBase*> _visitors;
    };

} // namespace ice::arctic
