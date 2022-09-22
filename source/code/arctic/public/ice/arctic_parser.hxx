#pragma once
#include <ice/arctic_syntax_visitor.hxx>
#include <ice/arctic_lexer.hxx>
#include <memory>

namespace ice::arctic
{

    class Parser
    {
    public:
        virtual ~Parser() noexcept = default;

        virtual void parse(
            ice::arctic::Lexer& lexer,
            ice::arctic::SyntaxNodeAllocator& node_alloc,
            ice::Span<ice::arctic::SyntaxVisitorBase*> visitors
        ) noexcept = 0;
    };

    auto create_default_parser() noexcept -> std::unique_ptr<ice::arctic::Parser>;
    auto create_simple_allocator() noexcept -> std::unique_ptr<ice::arctic::SyntaxNodeAllocator>;

} // namespace ice::arctic
