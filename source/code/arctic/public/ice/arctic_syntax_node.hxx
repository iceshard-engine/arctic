#pragma once
#include <ice/arctic_syntax.hxx>
#include <ice/arctic_token.hxx>

namespace ice::arctic
{

    struct SyntaxNode
    {
        ice::arctic::SyntaxEntity entity;

        ice::arctic::SyntaxNode* child;
        ice::arctic::SyntaxNode* sibling;
        ice::arctic::SyntaxNode* annotation;

        //ice::arctic::SyntaxNode const* custom_child;
        //ice::arctic::SyntaxNode const* custom_sibling;
    };

    struct SyntaxNode_Variable : SyntaxNode
    {
        static constexpr ice::arctic::SyntaxEntity RepresentedSyntaxEntity = SyntaxEntity::DEF_Variable;

        ice::arctic::Token name;
        ice::arctic::Token type;
    };

    struct SyntaxNode_ContextVariable : SyntaxNode
    {
        static constexpr ice::arctic::SyntaxEntity RepresentedSyntaxEntity = SyntaxEntity::DEF_ContextVariable;

        ice::arctic::Token name;
        ice::arctic::Token type;
    };

    struct SyntaxNode_TypeDef : SyntaxNode
    {
        static constexpr ice::arctic::SyntaxEntity RepresentedSyntaxEntity = SyntaxEntity::DEF_TypeDef;

        ice::arctic::Token name;
        ice::arctic::Token base_type;
        bool is_alias;
    };

    struct SyntaxNode_Struct : SyntaxNode
    {
        static constexpr ice::arctic::SyntaxEntity RepresentedSyntaxEntity = SyntaxEntity::DEF_Struct;

        ice::arctic::Token name;
    };

    struct SyntaxNode_StructMember : SyntaxNode
    {
        static constexpr ice::arctic::SyntaxEntity RepresentedSyntaxEntity = SyntaxEntity::DEF_StructMember;

        ice::arctic::Token name;
        ice::arctic::Token type;
    };

    struct SyntaxNode_Function : SyntaxNode
    {
        static constexpr ice::arctic::SyntaxEntity RepresentedSyntaxEntity = SyntaxEntity::DEF_Function;

        ice::arctic::Token name;
        ice::arctic::Token result_type;
    };

    struct SyntaxNode_FunctionArgument : SyntaxNode
    {
        static constexpr ice::arctic::SyntaxEntity RepresentedSyntaxEntity = SyntaxEntity::DEF_FunctionArgument;

        ice::arctic::Token name;
        ice::arctic::Token type;
    };

    struct SyntaxNode_Scope : SyntaxNode
    {
        static constexpr ice::arctic::SyntaxEntity RepresentedSyntaxEntity = SyntaxEntity::DEF_ExplicitScope;
    };

    struct SyntaxNode_Annotation : SyntaxNode
    {
        static constexpr ice::arctic::SyntaxEntity RepresentedSyntaxEntity = SyntaxEntity::DEF_Annotation;
    };

    struct SyntaxNode_AnnotationAttribute : SyntaxNode
    {
        static constexpr ice::arctic::SyntaxEntity RepresentedSyntaxEntity = SyntaxEntity::DEF_AnnotationAttribute;

        ice::arctic::Token name;
        ice::arctic::Token value;
    };

    struct SyntaxNode_Expression : SyntaxNode
    {
        static constexpr ice::arctic::SyntaxEntity RepresentedSyntaxEntity = SyntaxEntity::EXP_Expression;
    };

    struct SyntaxNode_ExpressionBranch : SyntaxNode
    {
        static constexpr ice::arctic::SyntaxEntity RepresentedSyntaxEntity = SyntaxEntity::EXP_Branch;
    };

    struct SyntaxNode_ExplicitScope : SyntaxNode
    {
        static constexpr ice::arctic::SyntaxEntity RepresentedSyntaxEntity = SyntaxEntity::EXP_ExplicitScope;
    };

    struct SyntaxNode_FunctionBody : SyntaxNode
    {
        static constexpr ice::arctic::SyntaxEntity RepresentedSyntaxEntity = SyntaxEntity::DEF_FunctionBody;
    };

    struct SyntaxNode_ExpressionCall : SyntaxNode
    {
        static constexpr ice::arctic::SyntaxEntity RepresentedSyntaxEntity = SyntaxEntity::EXP_Call;

        ice::arctic::Token function;
    };

    struct SyntaxNode_ExpressionCallArg : SyntaxNode
    {
        static constexpr ice::arctic::SyntaxEntity RepresentedSyntaxEntity = SyntaxEntity::EXP_CallArg;
    };

    struct SyntaxNode_ExpressionValue : SyntaxNode
    {
        static constexpr ice::arctic::SyntaxEntity RepresentedSyntaxEntity = SyntaxEntity::EXP_Value;

        ice::arctic::Token value;
    };

    struct SyntaxNode_ExpressionGetMember : SyntaxNode
    {
        static constexpr ice::arctic::SyntaxEntity RepresentedSyntaxEntity = SyntaxEntity::EXP_GetMember;

        ice::arctic::Token member;
    };

    struct SyntaxNode_ExpressionUnaryOperation : SyntaxNode
    {
        static constexpr ice::arctic::SyntaxEntity RepresentedSyntaxEntity = SyntaxEntity::EXP_UnaryOperation;

        ice::arctic::Token operation;
    };

    struct SyntaxNode_ExpressionBinaryOperation : SyntaxNode
    {
        static constexpr ice::arctic::SyntaxEntity RepresentedSyntaxEntity = SyntaxEntity::EXP_BinaryOperation;

        ice::arctic::Token operation;
    };

    struct SyntaxNode_ExpressionAssignment : SyntaxNode
    {
        static constexpr ice::arctic::SyntaxEntity RepresentedSyntaxEntity = SyntaxEntity::EXP_Assignment;
    };

    struct SyntaxNodeAllocator
    {
        virtual ~SyntaxNodeAllocator() noexcept = default;

        virtual auto allocate(ice::u64 size, ice::u64 align) noexcept -> void* = 0;
        virtual void deallocate(void* ptr) noexcept = 0;

        template<typename T, typename... Args>
        auto create(Args&&... args) noexcept -> T*
        {
            T* result = new (allocate(sizeof(T), alignof(T))) T{ std::forward<Args>(args)... };
            result->entity = T::RepresentedSyntaxEntity;
            return result;
        }

        template<typename T>
        void destroy(T* ptr) noexcept
        {
            ptr->~T();
            deallocate(ptr);
        }
    };

} // namespace ice::arctic
