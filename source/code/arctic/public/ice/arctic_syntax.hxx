#pragma once
#include "arctic_types.hxx"

namespace ice::arctic
{

    enum class SyntaxEntity : ice::u32
    {
        ROOT,

        DEF_TypeDef,
        DEF_Struct,
        DEF_StructMember,
        DEF_Variable,
        DEF_ContextVariable,
        DEF_Function,
        DEF_FunctionArgument,
        DEF_FunctionBody,
        DEF_ExplicitScope,
        DEF_Annotation,
        DEF_AnnotationAttribute,

        EXP_Value,
        EXP_GetMember,
        EXP_Call,
        EXP_CallArg,
        EXP_Variable,
        EXP_Assignment,
        EXP_Expression,
        EXP_UnaryOperation,
        EXP_BinaryOperation,
        EXP_ExplicitScope,
        EXP_Condition,
        EXP_Branch,
        EXP_Loop,
    };

    constexpr auto to_string(ice::arctic::SyntaxEntity entity) noexcept
    {
        switch (entity)
        {
        case SyntaxEntity::ROOT: return "ROOT";
        case SyntaxEntity::DEF_TypeDef: return "DEF_TypeDef";
        case SyntaxEntity::DEF_Struct: return "DEF_Struct";
        case SyntaxEntity::DEF_StructMember: return "DEF_StructMember";
        case SyntaxEntity::DEF_Variable: return "DEF_Variable";
        case SyntaxEntity::DEF_ContextVariable: return "DEF_ContextVariable";
        case SyntaxEntity::DEF_Function: return "DEF_Function";
        case SyntaxEntity::DEF_FunctionArgument: return "DEF_FunctionArgument";
        case SyntaxEntity::DEF_FunctionBody: return "DEF_FunctionBody";
        case SyntaxEntity::DEF_ExplicitScope: return "DEF_ExplicitScope";
        case SyntaxEntity::DEF_Annotation: return "DEF_Annotation";
        case SyntaxEntity::DEF_AnnotationAttribute: return "DEF_AnnotationAttribute";

        case SyntaxEntity::EXP_Value: return "EXP_Value";
        case SyntaxEntity::EXP_Call: return "EXP_Call";
        case SyntaxEntity::EXP_CallArg: return "EXP_CallArg";
        case SyntaxEntity::EXP_Variable: return "EXP_Variable";
        case SyntaxEntity::EXP_Assignment: return "EXP_Assignment";
        case SyntaxEntity::EXP_Expression: return "EXP_Expression";
        case SyntaxEntity::EXP_UnaryOperation: return "EXP_UnaryOperation";
        case SyntaxEntity::EXP_BinaryOperation: return "EXP_BinaryOperation";
        case SyntaxEntity::EXP_ExplicitScope: return "EXP_ExplicitScope";
        case SyntaxEntity::EXP_Condition: return "EXP_Condition";
        case SyntaxEntity::EXP_Branch: return "EXP_Branch";
        case SyntaxEntity::EXP_Loop: return "EXP_Loop";
        }
        return "???";
    }

} // namespace ice::arctic
