#include <ice/arctic_script_typeinfo.hxx>

namespace ice::arctic
{

    auto native_type_from_string(ice::String type_str) noexcept -> ice::arctic::ScriptNativeType
    {
        if (type_str.empty())
        {
            return ScriptNativeType::Invalid;
        }

        auto test_str = type_str;
        auto const ws_char = type_str.find_first_of(u8' ');
        if (ws_char != std::string::npos)
        {
            test_str = test_str.substr(0, ws_char);
        }

        ScriptNativeType result = ScriptNativeType::Usertype;
        switch(test_str[0])
        {
        case u8'i':
            if (test_str == u8"i8") result = ScriptNativeType::Int8;
            else if (test_str == u8"i16") result = ScriptNativeType::Int16;
            else if (test_str == u8"i32") result = ScriptNativeType::Int32;
            else if (test_str == u8"i64") result = ScriptNativeType::Int64;
            break;
        case u8'u':
            if (test_str == u8"u8") result = ScriptNativeType::UInt8;
            else if (test_str == u8"u16") result = ScriptNativeType::UInt16;
            else if (test_str == u8"u32") result = ScriptNativeType::UInt32;
            else if (test_str == u8"u64") result = ScriptNativeType::UInt64;
            else if (test_str == u8"utf8") result = ScriptNativeType::Utf8;
            break;
        case u8'f':
            if (test_str == u8"f32") result = ScriptNativeType::Float32;
            else if (test_str == u8"f64") result = ScriptNativeType::Float64;
            break;
        case u8'v':
            if (test_str == u8"void") result = ScriptNativeType::Void;
            break;
        case u8'b':
            if (test_str == u8"bool") result = ScriptNativeType::Bool;
            break;
        default:
            break;
        }

        if (result == ScriptNativeType::Usertype)
        {
            if (auto const open_bracket = type_str.find_first_of('('); open_bracket != std::string::npos)
            {
                auto const close_bracket = type_str.find_first_of(')');
                assert(open_bracket < close_bracket && close_bracket != std::string::npos);
                result = ScriptNativeType::Function;
            }
        }

        return result;
    }

} // namespace ice::arctic
