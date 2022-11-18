#pragma once
#include <ice/arctic_script_symbol.hxx>
#include <ice/arctic_script_typeinfo.hxx>

namespace ice::arctic
{

    class ArcticBytecodeSymbol : public ice::arctic::ScriptSymbol
    {
    public:
        ArcticBytecodeSymbol(std::vector<ice::u32> symbol_bytes) noexcept;
        ~ArcticBytecodeSymbol() noexcept override;

        void dbg_print() const noexcept override;

        bool is_native() const noexcept override { return false; }
        bool is_mutable() const noexcept override { return false; }
        bool is_function() const noexcept override { return false; }
        bool is_context() const noexcept override { return false; }

        bool is_equal(ice::arctic::ScriptSymbol const& other) const noexcept override;
        bool is_compatible(ice::arctic::ScriptSymbol const& other) const noexcept override;
        bool is_convertible(ice::arctic::ScriptSymbol const& other) const noexcept override { return false; }

        auto hash() const noexcept -> ice::u64 override;

    private:
        std::vector<ice::u32> _symbol_bytes;
    };

    void ArcticBytecodeSymbol::dbg_print() const noexcept
    {
        fmt::print("Symbol: #0");
        for (ice::u32 const byte : _symbol_bytes)
        {
            fmt::print("|{:0>8X}", byte);
        }
        fmt::print("\n");
    }

} // namespace ice::arctic
