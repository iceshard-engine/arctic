#pragma once
#include <ice/arctic_parser_result.hxx>

namespace ice::arctic
{

    auto to_string(ice::arctic::ParseState state) noexcept -> std::string_view
    {
        switch(state)
#define CASE(value, ...) case ParseState::value: return #value #__VA_ARGS__ ""
        {
            CASE(Success);
            CASE(Warning, ": Unknown");
            CASE(Error, ": Unknown");
            CASE(Error_Definition_UnknownToken);
            CASE(Error_Definition_MissingAssignmentOperator);
            CASE(Error_TypeOf_MissingTypeName);
            CASE(Error_TypeOf_MissingBracketOpen);
            CASE(Error_TypeOf_MissingBracketClose);
#undef CASE
        }
        return "<?>";
    }

} // namespace ice::arctic
