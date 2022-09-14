#pragma once
#include <ice/arctic_bytecode.hxx>

namespace ice::arctic
{

    struct ExecutionContext { };
    struct ExecutionState { };

    class VirtualMachine
    {
    public:
        virtual void execute(
            std::vector<ice::arctic::ByteCode> const& bytecode,
            ice::arctic::ExecutionContext const& context,
            ice::arctic::ExecutionState& state
        ) noexcept = 0;
    };

} // namespace ice::arctic
