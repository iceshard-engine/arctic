#include <ice/arctic_lexer_rules.hxx>

namespace ice::arctic
{

    extern auto lexer_rules_shader_tokenizer(
        ice::arctic::Word& word,
        ice::arctic::WordProcessor& processor,
        ice::arctic::TokenLocation location
    ) noexcept -> ice::arctic::Token;

    auto lexer_rules_script_tokenizer(
        ice::arctic::Word& word,
        ice::arctic::WordProcessor& processor,
        ice::arctic::TokenLocation location
    ) noexcept -> ice::arctic::Token
    {
        return lexer_rules_shader_tokenizer(word, processor, location);
    }

} // namespace ice::arctic
