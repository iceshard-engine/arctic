#include <ice/arctic_word_matcher.hxx>
#include <cassert>
#include <cctype>

namespace ice::arctic
{

    auto word_match_unknown(
        ice::utf8 const* it,
        ice::utf8 const*& out_end_it,
        ice::u32& characters_out
    ) noexcept -> ice::arctic::WordCategory;

    auto word_match_alphanum(
        ice::utf8 const* it,
        ice::utf8 const*& out_end_it,
        ice::u32& characters_out
    ) noexcept -> ice::arctic::WordCategory;

    auto word_match_punctuation(
        ice::utf8 const* it,
        ice::utf8 const*& out_end_it,
        ice::u32& characters_out
    ) noexcept -> ice::arctic::WordCategory;

    auto word_match_whitespace(
        ice::utf8 const* it,
        ice::utf8 const*& out_end_it,
        ice::u32& characters_out
    ) noexcept -> ice::arctic::WordCategory;

    auto word_match_tabs(
        ice::utf8 const* it,
        ice::utf8 const*& out_end_it,
        ice::u32& characters_out
    ) noexcept -> ice::arctic::WordCategory;

    auto word_match_endofline(
        ice::utf8 const* it,
        ice::utf8 const*& out_end_it,
        ice::u32& characters_out
    ) noexcept -> ice::arctic::WordCategory;

    auto word_matcher_dispatch(
        ice::arctic::WordMatcher const* matcher,
        ice::utf8 u8char
    ) noexcept -> ice::arctic::WordMatcher::MatcherFn*
    {
        if ((u8char & 0x80) != 0)
        {
            return word_match_alphanum;
        }
        else
        {
            return matcher->_dispatch_table[u8char];
        }
    }

    void initialize_ascii_matcher(ice::arctic::WordMatcher* matcher) noexcept
    {
        assert(matcher->_dispatch_table == nullptr);
        assert(matcher->_dispatch_fn == nullptr);

        static ice::arctic::WordMatcher::MatcherFn* const available_matchers[]{
            word_match_endofline,
            word_match_alphanum,
            //word_match_punctuation,
            //word_match_tabs,
            word_match_whitespace,
            word_match_unknown,
        };

        matcher->_dispatch_fn = word_matcher_dispatch;
        matcher->_dispatch_table = new WordMatcher::MatcherFn*[128];

        ice::utf8 test_str[3]{ u8'\0', u8'\0', u8'\0' };

        for (ice::utf8 u8char = 0; u8char < 128; ++u8char)
        {
            // Replace the mid value, so we can check result iterators.
            test_str[1] = u8char;

            ice::utf8 const* const beg = test_str;

            ice::u32 matcher_idx = 0;
            ice::arctic::WordMatcher::MatcherFn* matcher_fn = nullptr;
            while (matcher_fn == nullptr)
            {
                ice::arctic::WordMatcher::MatcherFn* matcher_candidate = available_matchers[matcher_idx];

                ice::u32 temp_unused;
                ice::utf8 const* out = beg;
                ice::arctic::WordCategory const matched_category = matcher_candidate(beg, out, temp_unused);

                if (matched_category == WordCategory::Unknown)
                {
                    if (std::ispunct(u8char))
                    {
                        matcher_fn = word_match_punctuation;
                    }
                    else
                    {
                        matcher_fn = matcher_candidate;
                    }
                }
                else if ((out - beg) == 2)
                {
                    matcher_fn = matcher_candidate;
                }

                matcher_idx += 1;
            }

            matcher->_dispatch_table[u8char] = matcher_fn;
        }
    }

    void shutdown_matcher(ice::arctic::WordMatcher* matcher) noexcept
    {
        delete[] matcher->_dispatch_table;

        matcher->_dispatch_table = nullptr;
        matcher->_dispatch_fn = nullptr;
    }

    auto word_match_unknown(
        ice::utf8 const* it,
        ice::utf8 const*& out_end_it,
        ice::u32& characters_out
    ) noexcept -> ice::arctic::WordCategory
    {
        out_end_it = it;
        characters_out = 0;
        return WordCategory::Unknown;
    }

    auto word_match_alphanum(
        ice::utf8 const* it,
        ice::utf8 const*& out_end_it,
        ice::u32& characters_out
    ) noexcept -> ice::arctic::WordCategory
    {
        out_end_it = it;
        characters_out = 0;

        do
        {
            ice::utf8 u8char = *out_end_it;
            ice::u32 utf8_child_count = 0;

            while ((u8char & 0xC0) == 0xC0)
            {
                u8char <<= 1;
                utf8_child_count += 1;
            }

            out_end_it += 1 + utf8_child_count;
            characters_out += 1;

        } while (((*out_end_it & 0xC0) == 0xC0) || std::isalnum(*out_end_it) || *out_end_it == '_');

        return WordCategory::AlphaNum;
    }

    auto word_match_punctuation(
        ice::utf8 const* it,
        ice::utf8 const*& out_end_it,
        ice::u32& characters_out
    ) noexcept -> ice::arctic::WordCategory
    {
        out_end_it += 1;
        characters_out = 1;

        return WordCategory::Punctuation;
    }

    auto word_match_whitespace(
        ice::utf8 const* it,
        ice::utf8 const*& out_end_it,
        ice::u32& characters_out
    ) noexcept -> ice::arctic::WordCategory
    {
        out_end_it = it + 1;

        while (std::isspace(*out_end_it))
        {
            out_end_it += 1;
        }

        characters_out = ice::u32(out_end_it - it);
        return WordCategory::Whitespace;
    }

    auto word_match_tabs(
        ice::utf8 const* it,
        ice::utf8 const*& out_end_it,
        ice::u32& characters_out
    ) noexcept -> ice::arctic::WordCategory
    {
        out_end_it = it + 1;

        while (*out_end_it == '\t')
        {
            out_end_it += 1;
        }

        characters_out = ice::u32(out_end_it - it);
        return WordCategory::Whitespace;
    }

    auto word_match_endofline(
        ice::utf8 const* it,
        ice::utf8 const*& out_end_it,
        ice::u32& characters_out
    ) noexcept -> ice::arctic::WordCategory
    {
        out_end_it = it + 1;
        characters_out = ice::u32(*out_end_it == '\n');

        while (*out_end_it == '\n' || *out_end_it == '\r')
        {
            out_end_it += 1;
            characters_out += ice::u32(*out_end_it == '\n');
        }

        return WordCategory::EndOfLine;
    }

} // namespace ice::arctic
