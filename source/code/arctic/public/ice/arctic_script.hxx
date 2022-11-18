#pragma once
#include <ice/arctic_lexer.hxx>
#include <ice/arctic_syntax_node.hxx>
#include <memory>

namespace ice::arctic
{

    class Compiler;

    class Script
    {
    public:
        virtual ~Script() noexcept = default;

        virtual auto count_usertypes() const noexcept -> ice::u32 = 0;
        virtual auto count_functions() const noexcept -> ice::u32 = 0;
        virtual auto count_constants() const noexcept -> ice::u32 = 0;
        virtual auto count_context_variables() const noexcept -> ice::u32 = 0;

        virtual auto usertypes() const noexcept -> ice::Span<ice::arctic::SyntaxNode const* const> = 0;
        virtual auto functions() const noexcept -> ice::Span<ice::arctic::SyntaxNode_Function const* const> = 0;
        virtual auto constants() const noexcept -> ice::Span<ice::arctic::SyntaxNode const* const> = 0;
        virtual auto context_variables() const noexcept -> ice::Span<ice::arctic::SyntaxNode const* const> = 0;
    };

    auto load_script(
        ice::arctic::Lexer& lexer,
        ice::arctic::SyntaxNodeAllocator* parent_allocator = nullptr
    ) noexcept -> std::unique_ptr<ice::arctic::Script>;

    auto compile_script(
        ice::arctic::Lexer& lexer,
        ice::arctic::Compiler& compiler,
        ice::arctic::SyntaxNodeAllocator* parent_allocator = nullptr
    ) noexcept -> std::unique_ptr<ice::arctic::Script>;

} // namespace ice::arctic
