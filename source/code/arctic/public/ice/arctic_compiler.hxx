#pragma once
#include <ice/arctic_types.hxx>
#include <ice/arctic_bytecode.hxx>

namespace ice::arctic
{

    class Compiler
    {
    public:
        virtual ~Compiler() noexcept = default;

        virtual auto compile() noexcept -> ice::arctic::CompiledScript* = 0;
    };

} // namespace ice::arctic
