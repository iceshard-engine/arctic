#include <ice/arctic_syntax_node.hxx>
#include <fmt/format.h>

namespace ice::arctic
{

    auto dbg_to_string(ice::arctic::SyntaxNode const* node) noexcept -> std::string
    {
        if (node == nullptr)
        {
            return "[<null>]";
        }

        if (node->entity == SyntaxEntity::EXP_BinaryOperation)
        {
            auto& exp = *static_cast<ice::arctic::SyntaxNode_ExpressionBinaryOperation const*>(node);
            return fmt::format("[{}( {} )]", to_string(node->entity), std::string_view{ (char const*)exp.operation.value.data(), exp.operation.value.size()});
        }
        else if (node->entity == SyntaxEntity::EXP_Value)
        {
            auto& exp = *static_cast<ice::arctic::SyntaxNode_ExpressionValue const*>(node);
            return fmt::format("[{}( {} )]", to_string(node->entity), std::string_view{ (char const*)exp.value.value.data(), exp.value.value.size()});
        }

        return { };
    }

} // namespace ice::arctic
