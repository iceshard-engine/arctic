#pragma once
#include <ice/arctic_types.hxx>

namespace ice::arctic
{

    struct ByteCode
    {
        enum OpCode : ice::u16
        {
            OC_NOOP,
            OC_META,

            OC_EXEC,

            OC_MOVR,
            OC_MOVA,
            OC_MOVS,

            OC_ADD32,
            OC_SUB32,
            OC_MUL32,
            OC_DIV32,

            OC_ADD64,
            OC_SUB64,
            OC_MUL64,
            OC_DIV64,

            OC_CALL0_VOID,

            OC_END,
        };
        enum OpExt : ice::u8
        {
            OE_NONE,
            OE_REG,
            OE_STACK,
            OE_ADDR,
            OE_VALUE,
            OE_VALUE_SP,
            OE_FUNC,

            OE_META_SYMBOL,
            OE_META_END,
        };
        enum OpReg : ice::u8
        {
            OR_R0, OR_R1, OR_R2, OR_R3,
            OR_R4, OR_R5, OR_R6, OR_R7,
            OR_R8, OR_R9, OR_R10, OR_R11,
            OR_R12, OR_R13, OR_R14, OR_R15,

            OR_PTR, OR_TP, OR_SP, OR_VOID,
        };
        struct OpAddress
        {
            ice::u32 loc : 28;
            ice::u32 ext : 4;
        };

        union
        {
            struct
            {
                OpCode operation;
                OpExt extension;
                OpReg registr;
            } op;
            OpAddress addr;
            ice::u32 byte;
        };

        struct Op
        {
            OpCode operation;
            OpExt extension = OE_NONE;
            OpReg registr = OR_VOID;

            constexpr operator ByteCode() const noexcept;
        };
        struct Value
        {
            ice::u32 value;

            constexpr operator ByteCode() const noexcept;
        };
        struct Addr
        {
            ice::u32 loc;

            constexpr operator ByteCode() const noexcept;
        };
    };

    constexpr ByteCode::Op::operator ByteCode() const noexcept
    {
        ByteCode bc;
        bc.op.operation = operation;
        bc.op.extension = extension;
        bc.op.registr = registr;
        return bc;
    }

    constexpr ByteCode::Value::operator ByteCode() const noexcept
    {
        ByteCode bc;
        bc.byte = value;
        return bc;
    }

    constexpr ByteCode::Addr::operator ByteCode() const noexcept
    {
        ByteCode bc;
        bc.addr.loc = loc;
        return bc;
    }

    static constexpr auto to_string(ByteCode::OpCode code) noexcept -> std::string_view
    {
        switch(code)
        {
            case ByteCode::OpCode::OC_NOOP: return "NOOP";
            case ByteCode::OpCode::OC_META: return "META";

            case ByteCode::OpCode::OC_EXEC: return "EXEC";

            case ByteCode::OpCode::OC_MOVR: return "MOV[R]";
            case ByteCode::OpCode::OC_MOVA: return "MOV[A]";
            case ByteCode::OpCode::OC_MOVS: return "MOV[S]";

            case ByteCode::OpCode::OC_ADD32: return "ADD[32]";
            case ByteCode::OpCode::OC_SUB32: return "SUB[32]";
            case ByteCode::OpCode::OC_MUL32: return "MUL[32]";
            case ByteCode::OpCode::OC_DIV32: return "DIV[32]";

            case ByteCode::OpCode::OC_ADD64: return "ADD[64]";
            case ByteCode::OpCode::OC_SUB64: return "SUB[64]";
            case ByteCode::OpCode::OC_MUL64: return "MUL[64]";
            case ByteCode::OpCode::OC_DIV64: return "DIV[64]";

            case ByteCode::OpCode::OC_CALL0_VOID: return "CALL[0,VOID]";

            case ByteCode::OpCode::OC_END: return "END";
        }
        return "<unknown>";
    }

    static constexpr auto to_string(ByteCode::OpExt code) noexcept -> std::string_view
    {
        switch(code)
        {
            case ByteCode::OpExt::OE_NONE: return "NONE";
            case ByteCode::OpExt::OE_REG: return "REGI";
            case ByteCode::OpExt::OE_STACK: return "STAK";
            case ByteCode::OpExt::OE_ADDR: return "ADDR";
            case ByteCode::OpExt::OE_VALUE: return "VALU";
            case ByteCode::OpExt::OE_VALUE_SP: return "SPTR";
            case ByteCode::OpExt::OE_META_SYMBOL: return "MSYM";
        }
        return "<unknown>";
    }

} // namespace ice::arctic
