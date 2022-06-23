#include <ice/arctic_lexer_rules.hxx>
#include <iostream>
#include <cctype>

namespace ice::arctic
{

    void lexer_rules_shader_alphanum(
        ice::arctic::Word const& word,
        ice::arctic::WordProcessor& /*processor*/,
        ice::arctic::Token& out_result
    ) noexcept
    {
        ice::u32 const word_len = ice::u32(word.value.length());

        switch (word_len)
        {
        case 2:
            if (word.value == u8"fn")
            {
                out_result.type = TokenType::KW_Fn;
                out_result.value = word.value;
            }
            break;
        case 3:
            if (word.value == u8"ctx")
            {
                out_result.type = TokenType::KW_Ctx;
                out_result.value = word.value;
            }
            else if (word.value == u8"def")
            {
                out_result.type = TokenType::KW_Def;
                out_result.value = word.value;
            }
            else if (word.value == u8"let")
            {
                out_result.type = TokenType::KW_Let;
                out_result.value = word.value;
            }
            else if (word.value == u8"mut")
            {
                out_result.type = TokenType::KW_Mut;
                out_result.value = word.value;
            }
            break;
        case 4:
            if (word.value == u8"true")
            {
                out_result.type = TokenType::KW_True;
                out_result.value = word.value;
            }
            break;
        case 5:
            if (word.value == u8"alias")
            {
                out_result.type = TokenType::KW_Alias;
                out_result.value = word.value;
            }
            else if (word.value == u8"const")
            {
                out_result.type = TokenType::KW_Const;
                out_result.value = word.value;
            }
            else if (word.value == u8"false")
            {
                out_result.type = TokenType::KW_False;
                out_result.value = word.value;
            }
            break;
        case 6:
            if (word.value == u8"struct")
            {
                out_result.type = TokenType::KW_Struct;
                out_result.value = word.value;
            }
            else if (word.value == u8"typeof")
            {
                out_result.type = TokenType::KW_TypeOf;
                out_result.value = word.value;
            }
            break;
        case 7:
            if (word.value == u8"context")
            {
                assert(false);
            }
            break;
        default: break;
        }
    }

    void lexer_rules_shader_punctuation(
        ice::arctic::Word& word,
        ice::arctic::WordProcessor& processor,
        ice::arctic::Token& out_result,
        bool& out_skip_step_processor
    ) noexcept
    {
        ice::utf8 const first_char = word.value.front();

        out_result.value = word.value;
        switch (first_char)
        {
        case u8'+':
            out_result.type = TokenType::OP_Plus;
            break;
        case u8'-':
            out_result.type = TokenType::OP_Minus;
            break;
        case u8'*':
            out_result.type = TokenType::OP_Mul;
            break;
        case u8'/':
            out_result.type = TokenType::OP_Div;
            break;
        case u8'=':
            out_result.type = TokenType::OP_Assign;
            break;
        case u8'[':
            out_result.type = TokenType::CT_SquareBracketOpen;
            break;
        case u8']':
            out_result.type = TokenType::CT_SquareBracketClose;
            break;
        case u8'(':
            out_result.type = TokenType::CT_ParenOpen;
            break;
        case u8')':
            out_result.type = TokenType::CT_ParenClose;
            break;
        case u8'{':
            out_result.type = TokenType::CT_BracketOpen;
            break;
        case u8'}':
            out_result.type = TokenType::CT_BracketClose;
            break;
        case u8':':
            out_result.type = TokenType::CT_Colon;
            break;
        case u8',':
            out_result.type = TokenType::CT_Comma;
            break;
        case u8'.':
            out_result.type = TokenType::CT_Dot;
            break;
        case u8'#':
            out_result.type = TokenType::CT_Hash;
            break;
        case u8'\'': // handled later
        case u8'"':
            break;
        default:
            assert(false);
            break;
        }
    }

    void lexer_rules_shader_uservalues(
        ice::arctic::Word& word,
        ice::arctic::WordProcessor& processor,
        ice::arctic::Token& out_result,
        bool& out_skip_step_processor
    ) noexcept
    {
        out_result.value = word.value;

        ice::utf8 const first_char = word.value.front();
        if (first_char == u8'\'' || first_char == u8'"')
        {
            bool is_end = false;
            bool is_backslash = false;

            while (is_end == false)
            {
                word = processor.next();

                if (is_backslash)
                {
                    is_backslash = false;
                    continue;
                }

                if (word.category == WordCategory::EndOfFile)
                {
                    return;
                }

                ice::utf8 const initial_char = word.value.front();
                if (initial_char == '\\')
                {
                    is_backslash = true;
                }
                else if (initial_char == first_char)
                {
                    is_end = true;
                }
            }

            out_result.type = first_char == '\'' ? TokenType::CT_Literal : TokenType::CT_String;

            ice::utf8 const* const beg = out_result.value.data();
            ice::utf8 const* const end = word.value.data() + word.value.size();

            out_result.value = ice::String{ beg, size_t(end - beg) };
        }
        else if ((first_char & 0x80) == 0 && std::isdigit(char(first_char)))
        {
            ice::u64 const word_size = word.value.size();

            bool is_number = true;
            bool is_done = false;
            bool is_quote_separator = false;

            bool has_representation_prefix = first_char == u8'0' && word_size > 1;
            bool is_hex = has_representation_prefix && word.value[1] == 'x';
            bool is_binary = has_representation_prefix && word.value[1] == 'b';
            bool is_oct = has_representation_prefix && word.value[1] != 'x';
            bool is_floating_point = false;
            bool is_next_word = false;

            while (is_done == false)
            {
                word = processor.next();

                ice::utf8 const initial_char = word.value.front();
                switch (initial_char)
                {
                case u'\'':
                    is_number = is_quote_separator == false;
                    is_quote_separator = true;
                    break;
                case u'.':
                    is_number = is_floating_point == false;
                    is_next_word = true;
                    is_floating_point = true;
                    break;
                default:
                    is_done = (is_quote_separator == false) && (is_next_word == false);
                    is_next_word = false;
                    is_quote_separator = false;
                    break;
                }

                is_done |= (is_number == false);
            }

            if (is_number)
            {
                ice::utf8 const* const beg = out_result.value.data();
                ice::utf8 const* const end = word.value.data();

                bool const is_float_suffix = *(end - 1) == u8'f';
                bool const is_unsigned_suffix = *(end - 1) == u8'u';

                out_result.value = ice::String{ beg, size_t(end - beg) - ice::u32(is_unsigned_suffix || is_float_suffix) };
                out_skip_step_processor = true;

                if (is_binary)
                {
                    if (out_result.value.find_first_not_of(u8"'01", 2) == ice::String::npos)
                    {
                        out_result.type = TokenType::CT_NumberBin;
                    }
                }
                else if (is_hex)
                {
                    if (out_result.value.find_first_not_of(u8"'0123456789abcdefABCDEF", 2) == ice::String::npos)
                    {
                        out_result.type = TokenType::CT_NumberHex;
                    }
                }
                else if (is_oct)
                {
                    if (out_result.value.find_first_not_of(u8"'01234567", 1) == ice::String::npos)
                    {
                        out_result.type = TokenType::CT_NumberOct;
                    }
                }
                else if (is_floating_point || is_float_suffix)
                {
                    ice::u64 const dot_pos = out_result.value.find_first_not_of(u8"'0123456789", 0);
                    if (dot_pos != ice::String::npos && out_result.value[dot_pos] == '.')
                    {
                        is_number &= out_result.value.find_first_not_of(u8"'0123456789", dot_pos + 1) == ice::String::npos;
                    }

                    if (is_number)
                    {
                        out_result.type = TokenType::CT_NumberFloat;
                    }
                }
                else if (out_result.value.find_first_not_of(u8"'0123456789") == ice::String::npos)
                {
                    out_result.type = TokenType::CT_Number;
                }
            }
        }
        else
        {
            out_result.type = TokenType::CT_Symbol;
        }
    }

    auto lexer_rules_shader_tokenizer(
        ice::arctic::Word& word,
        ice::arctic::WordProcessor& processor,
        ice::arctic::TokenLocation location
    ) noexcept -> ice::arctic::Token
    {
        ice::arctic::Token result{
            .type = TokenType::Invalid,
            .location = location
        };

        bool skip_step_processor = false;
        switch (word.category)
        {
        case WordCategory::AlphaNum:
            lexer_rules_shader_alphanum(word, processor, result);
            break;
        case WordCategory::Punctuation:
            lexer_rules_shader_punctuation(word, processor, result, skip_step_processor);
            break;
        case WordCategory::EndOfLine:
            result.type = TokenType::ST_EndOfLine;
            result.value = word.value;
            break;
        default:
            break;
        }

        if (result.type == TokenType::Invalid)
        {
            lexer_rules_shader_uservalues(word, processor, result, skip_step_processor);
        }

        if (skip_step_processor == false)
        {
            word = processor.next();
        }
        return result;
    }

} // namespace ice::arctic
