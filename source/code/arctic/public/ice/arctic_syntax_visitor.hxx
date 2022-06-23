#pragma once
#include <ice/arctic_syntax_node.hxx>
#include <utility>

namespace ice::arctic
{

    struct SyntaxVisitorBase
    {
        virtual ~SyntaxVisitorBase() noexcept = default;

        virtual void visit(ice::arctic::SyntaxNode const* node) noexcept = 0;
    };

    template<typename NodeType>
    struct SyntaxVisitor
    {
        virtual ~SyntaxVisitor() noexcept = default;

        virtual void visit(NodeType const* node) noexcept = 0;
    };

    template<typename... NodeTypes>
    struct SyntaxVisitorGroup : SyntaxVisitorBase, SyntaxVisitor<NodeTypes>...
    {
        virtual ~SyntaxVisitorGroup() noexcept = default;

        template<typename NodeType>
        static void visit_fn(
            ice::arctic::SyntaxNode const* node,
            ice::arctic::SyntaxVisitorGroup<NodeTypes...>* visitor_group
        ) noexcept
        {
            static_cast<SyntaxVisitor<NodeType>*>(visitor_group)->visit(
                reinterpret_cast<NodeType const*>(node)
            );
        }

        void visit(ice::arctic::SyntaxNode const* node) noexcept override
        {
            using VisitFn = void(
                ice::arctic::SyntaxNode const*,
                ice::arctic::SyntaxVisitorGroup<NodeTypes...>*
            ) noexcept;

            static ice::arctic::SyntaxEntity const group_entities[]{
                NodeTypes::RepresentedSyntaxEntity...
            };

            static VisitFn* const group_funcs[]{
                &visit_fn<NodeTypes>...
            };

            for (ice::u32 idx = 0; idx < sizeof...(NodeTypes); ++idx)
            {
                if (group_entities[idx] == node->entity)
                {
                    group_funcs[idx](node, this);
                }
            }
        }
    };

} // namespace ice::arctic
