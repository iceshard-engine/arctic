#pragma once
#include "arctic_types.hxx"

namespace ice::arctic
{

    enum class TokenType : ice::u32
    {
        Invalid = 0,

        // Core tokens
        CT_AlphaNum = 0x0000'0001,
        CT_Symbol = 0x0000'0002,
        CT_Literal = 0x0000'0003,
        CT_String = 0x0000'0004,

        CT_Number = 0x0000'0008,
        CT_NumberHex = 0x0000'0009,
        CT_NumberOct = 0x0000'000A,
        CT_NumberBin = 0x0000'000B,
        CT_NumberFloat = 0x0000'000F,

        CT_Colon = 0x0000'0010,
        CT_Dot = 0x0000'0011,
        CT_Comma = 0x0000'0012,
        CT_ParenOpen = 0x0000'0013,
        CT_ParenClose = 0x0000'0014,
        CT_BracketOpen = 0x0000'0015,
        CT_BracketClose = 0x0000'0016,
        CT_SquareBracketOpen = 0x0000'0017,
        CT_SquareBracketClose = 0x0000'0018,
        CT_Quote = 0x0000'0019,
        CT_DoubleQuote = 0x0000'0020,
        CT_Hash = 0x0000'0021,

        // Keyword tokens
        Keyword = 0x0001'0000,
        KW_Let = Keyword | 0x0001,
        KW_Fn = Keyword | 0x0002,
        KW_Context = Keyword | 0x0003,
        KW_Const = Keyword | 0x0004,
        KW_Ctx = Keyword | 0x0005,
        KW_Mut = Keyword | 0x0006,
        KW_Def = Keyword | 0x0007,
        KW_TypeOf = Keyword | 0x0100,
        KW_Struct = Keyword | 0x0101,
        KW_Alias = Keyword | 0x0102,

        KW_False = Keyword | 0x1000,
        KW_True = Keyword | 0x1001,

        // Operators
        Operator = 0x0002'0000,
        OP_Assign = Operator | 0x0001,
        OP_Plus = Operator | 0x0002,
        OP_Minus = Operator | 0x0003,
        OP_Mul = Operator | 0x0004,
        OP_Div = Operator | 0x0005,
        OP_And = Operator | 0x0006,
        OP_Or = Operator | 0x0007,

        // Native type tokens
        NativeType = 0x0004'0000,
        NativeType_Signed = NativeType | 0x0100,
        NativeType_Unsigned = NativeType | 0x0200,
        NativeType_FloatingPoint = NativeType | 0x0400,
        //NativeType_PointerType = NativeType | 0x1000,

        NT_Void = NativeType,
        NT_Bool = NativeType | 0x0001,
        NT_Utf8 = NativeType | 0x0002,

        NT_f32 = NativeType_FloatingPoint | 0x0001,
        NT_f64 = NativeType_FloatingPoint | 0x0002,

        NT_i8 = NativeType_Signed | 0x0001,
        NT_i16 = NativeType_Signed | 0x0002,
        NT_i32 = NativeType_Signed | 0x0003,
        NT_i64 = NativeType_Signed | 0x0004,
        NT_u8 = NativeType_Unsigned | 0x0005,
        NT_u16 = NativeType_Unsigned | 0x0006,
        NT_u32 = NativeType_Unsigned | 0x0007,
        NT_u64 = NativeType_Unsigned | 0x0008,

        // Special
        ST_Any = 0x7000'0000,
        ST_Whitespace = 0x8000'0000,
        ST_EndOfLine = 0x8000'0001,
        ST_EndOfFile = 0x8000'0002,
    };

    struct TokenLocation
    {
        static constexpr uint32_t Constant_MaxLine = 0x000f'ffff;
        static constexpr uint32_t Constant_MaxColumn = 0x0000'0fff;

        ice::u32 line : 20;
        ice::u32 column : 12;
    };

    struct Token
    {
        ice::String value;
        ice::arctic::TokenType type;
        ice::arctic::TokenLocation location;
    };

} // namespace ice::arctic
