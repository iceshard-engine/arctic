#pragma once
#include <ice/arctic_types.hxx>
#include <ice/arctic_syntax_node.hxx>

namespace ice::arctic
{

    enum class ParseState : ice::u32
    {
        Success = 0x0,
        Warning = 0x4000'0000,
        Error = 0x8000'0000,

        Error_UnexpectedToken = Error | 0x0001,

        Error_Definition_UnknownToken = Error | 0x0101,
        Error_Definition_MissingAssignmentOperator = Error | 0x0102,

        Error_TypeOf_MissingTypeName = Error | 0x0110,
        Error_TypeOf_MissingBracketOpen = Error | 0x0111,
        Error_TypeOf_MissingBracketClose = Error | 0x0112,
    };

    auto to_string(ice::arctic::ParseState state) noexcept -> std::string_view;

    template<typename Value>
    struct ParseResult
    {
        constexpr ParseResult(
            ice::arctic::ParseState state
        ) noexcept
            : _value{ }
            , _state{ state }
        {
        }

        constexpr ParseResult(
            Value value,
            ice::arctic::ParseState state = ParseState::Success
        ) noexcept
            : _value{ value }
            , _state{ state }
        {
        }

        template<typename OtherValue>
        constexpr ParseResult(
            ice::arctic::ParseResult<OtherValue> const& other_result
        ) noexcept
            : _value{ other_result._value }
            , _state{ other_result._state }
        {
        }

        template<typename Fn>
        auto operator<<(Fn&& fn) noexcept -> ParseResult
        {
            if (has_error() == false)
            {
                auto const result = std::forward<Fn>(fn)();
                if (result.has_error())
                {
                    *this = result;
                }
            }

            return *this;
        }

        constexpr bool has_error() const noexcept
        {
            return (static_cast<ice::u32>(_state) & static_cast<ice::u32>(ParseState::Error)) != 0;
        }

        constexpr operator Value() const noexcept
        {
            return _value;
        }

        auto error_string() const noexcept -> std::string_view
        {
            return to_string(_state);
        }

        Value _value;
        ice::arctic::ParseState _state;
    };

} // namespace ice::arctic
