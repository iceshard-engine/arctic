#include "arctic_script_utils.hxx"
#include "arctic_script_bytecode_symbol.hxx"
#include <ice/arctic_script_function.hxx>

namespace ice::arctic
{

    class ArcticFunctionInfo final : public ice::arctic::ScriptFunctionInfo
    {
    public:
        struct Symbol;

        ArcticFunctionInfo(std::unique_ptr<Symbol> symbol) noexcept;

        auto name() noexcept -> ice::String { return {}; }
        auto native_type() noexcept -> ice::arctic::ScriptNativeType { return ScriptNativeType::Function; }

        auto symbol() noexcept -> ice::arctic::ScriptSymbol const& override;
    private:
        std::unique_ptr<Symbol> _function_symbol;
    };

    struct ArcticFunctionInfo::Symbol final : public ice::arctic::ArcticBytecodeSymbol
    {
    public:
    };

    ArcticFunctionInfo::ArcticFunctionInfo(std::unique_ptr<ArcticFunctionInfo::Symbol> symbol) noexcept
        : _function_symbol{ std::move(symbol) }
    {
    }

    auto ArcticFunctionInfo::symbol() noexcept -> ice::arctic::ScriptSymbol const&
    {
        return *_function_symbol;
    }

    auto gather_function_info(ice::arctic::SyntaxNode_Function const& func) noexcept -> ice::arctic::ScriptFunctionInfo*
    {
        return nullptr;
    }

} // namespace ice::arctic
