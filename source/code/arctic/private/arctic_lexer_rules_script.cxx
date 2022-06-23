#include <ice/arctic_lexer_rules.hxx>

namespace ice::arctic
{

    auto lexer_rules_script_tokenizer(
        ice::arctic::Word& word,
        ice::arctic::WordProcessor& processor,
        ice::arctic::TokenLocation location
    ) noexcept -> ice::arctic::Token
    {
        ice::arctic::Token result{
            .value = word.value,
            .type = TokenType::CT_Dot,
            .location = location
        };

        word = processor.next();
        return result;
    }

} // namespace ice::arctic
