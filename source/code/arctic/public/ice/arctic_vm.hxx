#pragma once
#include <ice/arctic_bytecode.hxx>
#include <ice/arctic_context.hxx>

namespace ice::arctic
{

    struct ExecutionState
    {
        static constexpr ice::u32 Constant_RegisterCount = ice::arctic::ByteCode::OpReg::OR_VOID + 1;

        ice::u32 registers[Constant_RegisterCount];
        ice::u32 stack_size;
        char* stack_pointer;
    };

    class VirtualMachine
    {
    public:
        virtual void execute(
            ice::arctic::ByteCode const* scriptcode,
            std::span<ice::arctic::ByteCode const> const& bytecode,
            ice::arctic::ExecutionContext const& context,
            ice::arctic::ExecutionState& state
        ) noexcept = 0;
    };

} // namespace ice::arctic
