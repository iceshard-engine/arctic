#pragma once
#include <ice/arctic_token.hxx>
#include <ice/arctic_word_processor.hxx>

namespace ice::arctic
{

    using Lexer = ice::arctic::detail::Generator<ice::arctic::Token>;

    enum class LexerRules
    {
        Provided,
        Script,
        Shader,
    };

    struct LexerOptions
    {
        ice::arctic::LexerRules rules = LexerRules::Provided;

        //! \brief Sets the size of tab characters when calculating the token column.
        //! \note This will ensure error messages will have a proper column value.
        ice::u32 tab_size = 4;
    };

    auto create_lexer(
        ice::arctic::WordProcessor words,
        ice::arctic::LexerOptions options = { }
    ) noexcept -> ice::arctic::Lexer;

} // namespace ice::arctic
