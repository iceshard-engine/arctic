#pragma once
#include <ice/arctic_word.hxx>

namespace ice::arctic
{

    //! \brief A word matcher using only basic ascii values for punctuation and whitespaces cases.
    struct WordMatcher
    {
        using MatcherFn = auto(ice::utf8 const*, ice::utf8 const*&, ice::u32&) noexcept -> ice::arctic::WordCategory;
        using DispatchFn = auto(ice::arctic::WordMatcher const*, ice::utf8) noexcept -> MatcherFn*;

        MatcherFn** _dispatch_table;
        DispatchFn* _dispatch_fn;
    };

    void initialize_ascii_matcher(
        ice::arctic::WordMatcher* matcher
    ) noexcept;

    void shutdown_matcher(
        ice::arctic::WordMatcher* matcher
    ) noexcept;

} // namespace ice::arctic
