#include "arctic_lexer.hxx"
#include "arctic_lexer_rules.hxx"

#include <cassert>

namespace ice::arctic
{

    using LexerTokenizer = auto(
        ice::arctic::Word&,
        ice::arctic::WordProcessor&,
        ice::arctic::TokenLocation
    ) noexcept -> ice::arctic::Token;

    auto create_lexer(
        ice::arctic::WordProcessor words,
        ice::arctic::LexerOptions options
    ) noexcept -> ice::arctic::Lexer
    {
        ice::u32 const whitespace_sizes[]{
            1, // non-tabs
            options.tab_size // tabs
        };

        if (options.rules == LexerRules::Provided)
        {
            ice::arctic::Word word = words.next();
            while (word.category != WordCategory::AlphaNum)
            {
                word = words.next();
            }

            // We expect the 'context' keyword followed by a known context name.
            assert(word.value == u8"context");
            word = words.next();
            assert(word.category == WordCategory::Whitespace);
            word = words.next();
            assert(word.category == WordCategory::AlphaNum);

            if (word.value == u8"Script")
            {
                options.rules = LexerRules::Script;
            }
            else if (word.value == u8"Shader")
            {
                options.rules = LexerRules::Shader;
            }

            assert(options.rules != LexerRules::Provided);
        }

        ice::arctic::LexerTokenizer* const tokenizer_funcs[]{
            nullptr,
            ice::arctic::lexer_rules_script_tokenizer,
            ice::arctic::lexer_rules_shader_tokenizer,
        };

        ice::arctic::LexerTokenizer* const tokenizer_nf =
            tokenizer_funcs[static_cast<ice::u32>(options.rules)];

        ice::u32 current_column_offset = 0;
        ice::arctic::TokenLocation current_location{ };

        ice::arctic::Word word = words.next();
        while (word.category != WordCategory::EndOfFile)
        {
            current_location.line = word.location.line + 1;

            if (word.category == WordCategory::Whitespace)
            {
                for (utf8 const u8char : word.value)
                {
                    current_column_offset += whitespace_sizes[ice::u32(u8char == u8'\t')];
                }

                current_column_offset -= ice::u32(word.value.size());
                word = words.next();
            }
            else
            {
                current_location.column = 1 + word.location.character + current_column_offset;

                co_yield tokenizer_nf(
                    word,
                    words,
                    current_location
                );
            }

            // Reset the next column if needed
            current_column_offset *= (word.category != WordCategory::EndOfLine);
        }

        co_return Token{
            .value = { },
            .type = TokenType::ST_EndOfFile,
            .location = { .line = word.location.line + 1, .column = 0 }
        };
    }

} // namespace ice::arctic
