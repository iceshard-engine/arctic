#include "arctic_word_processor.hxx"
#include "arctic_word_matcher.hxx"

#include <iostream>

namespace ice::arctic
{

    auto create_word_processor(
        ice::String script_data,
        ice::arctic::WordMatcher const* matcher
    ) noexcept -> ice::arctic::WordProcessor
    {
        assert(matcher->_dispatch_table != nullptr);
        assert(matcher->_dispatch_fn != nullptr);

        ice::utf8 const* it = script_data.data();
        ice::utf8 const* end = it;

        ice::arctic::WordLocation current_location{
            .line = 0,
            .character = 0
        };

        bool stop = false;
        while (stop == false)
        {
            ice::arctic::WordMatcher::MatcherFn* matcher_fn = matcher->_dispatch_fn(matcher, *it);

            ice::u32 characters_matched;
            ice::arctic::WordCategory const category = matcher_fn(it, end, characters_matched);

            co_yield Word{
                .value = ice::String{ it, size_t(end - it) },
                .category = category,
                .location = current_location
            };

            if (category == WordCategory::EndOfLine)
            {
                current_location.line += characters_matched;
                current_location.character = 0;
            }
            else
            {
                current_location.character += characters_matched;
            }

            it = end;
            stop = (*end == u8'\0');
        }

        current_location.line += 0;
        current_location.character = 0;

        co_return Word{ .value = u8"\0", .category = WordCategory::EndOfFile, .location = current_location };
    }

} // namespace ice::arctic
