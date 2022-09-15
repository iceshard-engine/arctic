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
            OC_ADD32,
            OC_ADD64,
            OC_END,
        };
        enum OpExt : ice::u8
        {
            OE_NONE,
            OE_REG,
            OE_ADDR,
            OE_VALUE,

            OE_META_SYMBOL,
        };
        enum OpReg : ice::u8
        {
            OR_R0, OR_R1, OR_R2, OR_R3,
            OR_VOID,
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

} // namespace ice::arctic
