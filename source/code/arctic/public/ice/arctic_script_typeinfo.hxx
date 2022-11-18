#pragma once
#include <ice/arctic_types.hxx>
#include <ice/arctic_script_symbol.hxx>

namespace ice::arctic
{

    enum class ScriptNativeType : ice::u32
    {
        Invalid = 0x0000'0000,
        Usertype = 0x0001'0000,
        Function = 0x0002'0000,

        NativeType = 0x0004'0000,
        NativeType_Signed = NativeType | 0x0100,
        NativeType_Unsigned = NativeType | 0x0200,
        NativeType_FloatingPoint = NativeType | 0x0400,
        // NativeType_Pointer = NativeType | 0x1000,

        Void = NativeType,
        Bool = NativeType | 0x0001,
        Utf8 = NativeType | 0x0002,

        Float32 = NativeType_FloatingPoint | 0x0040,
        Float64 = NativeType_FloatingPoint | 0x0080,

        Int8 = NativeType_Signed | 0x0010,
        Int16 = NativeType_Signed | 0x0020,
        Int32 = NativeType_Signed | 0x0040,
        Int64 = NativeType_Signed | 0x0080,
        UInt8 = NativeType_Unsigned | 0x0010,
        UInt16 = NativeType_Unsigned | 0x0020,
        UInt32 = NativeType_Unsigned | 0x0040,
        UInt64 = NativeType_Unsigned | 0x0080,
    };

    class ScriptTypeInfo
    {
    public:
        virtual ~ScriptTypeInfo() noexcept = default;

        virtual auto name() noexcept -> ice::String = 0;
        virtual auto native_type() noexcept -> ice::arctic::ScriptNativeType = 0;

        virtual auto symbol() noexcept -> ice::arctic::ScriptSymbol const& = 0;
    };

    auto native_type_from_string(ice::String type_str) noexcept -> ice::arctic::ScriptNativeType;

} // namespace ice::arctic
