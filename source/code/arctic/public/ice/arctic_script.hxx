#pragma once
#include <ice/arctic_lexer.hxx>
#include <memory>

namespace ice::arctic
{

    class Script
    {
    public:
        virtual ~Script() noexcept = default;

        virtual auto count_functions() const noexcept -> ice::u32 = 0;
    };

    auto load_script(
        ice::arctic::Lexer& lexer,
        ice::arctic::SyntaxNodeAllocator* parent_allocator = nullptr
    ) noexcept -> std::unique_ptr<ice::arctic::Script>;

} // namespace ice::arctic
