#pragma once
#include <assert.h>
#include <coroutine>

#include "arctic_word.hxx"
#include "arctic_generator.hxx"

namespace ice::arctic
{

    struct WordMatcher;

    using WordProcessor = ice::arctic::detail::Generator<ice::arctic::Word>;

    auto create_word_processor(
        ice::String script_data,
        ice::arctic::WordMatcher const* matcher
    ) noexcept -> ice::arctic::WordProcessor;

} // namespace ice::arctic
