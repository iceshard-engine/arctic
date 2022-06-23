#pragma once
#include <assert.h>
#include <coroutine>

#include <ice/arctic_word.hxx>
#include <ice/arctic_generator.hxx>

namespace ice::arctic
{

    struct WordMatcher;

    using WordProcessor = ice::arctic::detail::Generator<ice::arctic::Word>;

    auto create_word_processor(
        ice::String script_data,
        ice::arctic::WordMatcher const* matcher
    ) noexcept -> ice::arctic::WordProcessor;

} // namespace ice::arctic
