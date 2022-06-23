#pragma once
#include "arctic_token.hxx"
#include "arctic_word_processor.hxx"

namespace ice::arctic
{

    auto lexer_rules_script_tokenizer(
        ice::arctic::Word& word,
        ice::arctic::WordProcessor& processor,
        ice::arctic::TokenLocation location
    ) noexcept -> ice::arctic::Token;

    auto lexer_rules_shader_tokenizer(
        ice::arctic::Word& word,
        ice::arctic::WordProcessor& processor,
        ice::arctic::TokenLocation location
    ) noexcept -> ice::arctic::Token;

} // namespace ice::arctic
