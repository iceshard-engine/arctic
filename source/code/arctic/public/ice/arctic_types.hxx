#pragma once
#include <inttypes.h>
#include <string_view>
#include <span>

namespace ice
{

    using utf8 = char8_t;

    using u8 = uint8_t;
    using u16 = uint16_t;
    using u32 = uint32_t;
    using u64 = uint64_t;

    using i8 = int8_t;
    using i16 = int16_t;
    using i32 = int32_t;
    using i64 = int64_t;

    using f32 = float;
    using f64 = double;

    using uptr = uintptr_t;

    using String = std::u8string_view;

    template<typename T>
    using Span = std::span<T>;

} // namespace ice
