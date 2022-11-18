#pragma once
#include <ice/arctic_syntax_node.hxx>
#include <ice/arctic_script_symbol.hxx>

namespace ice::arctic
{

    class ScriptFunctionInfo;

    auto create_type_symbol(ice::String type_string) noexcept -> std::unique_ptr<ice::arctic::ScriptSymbol>;

    auto gather_function_info(ice::arctic::SyntaxNode_Function const& func) noexcept -> ice::arctic::ScriptFunctionInfo*;

} // namespace ice::arctic
