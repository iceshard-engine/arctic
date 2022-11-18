#pragma once
#include <ice/arctic_script_typeinfo.hxx>

namespace ice::arctic
{

    class ScriptFunctionArgumentInfo : public ice::arctic::ScriptTypeInfo
    {
    public:
        virtual ~ScriptFunctionArgumentInfo() noexcept = 0;

        virtual auto name() noexcept -> ice::String = 0;
        virtual auto type_info() noexcept -> ice::arctic::ScriptTypeInfo const& = 0;
    };

    class ScriptFunctionInfo : public ice::arctic::ScriptTypeInfo
    {
    public:
        virtual ~ScriptFunctionInfo() noexcept = default;

        virtual auto return_type() noexcept -> ice::arctic::ScriptTypeInfo const& = 0;

        virtual auto count_arguments() noexcept -> ice::u32 = 0;
        virtual auto arguments() noexcept -> ice::Span<ice::arctic::ScriptFunctionArgumentInfo const*> = 0;
    };

} // namespace ice::arctic
