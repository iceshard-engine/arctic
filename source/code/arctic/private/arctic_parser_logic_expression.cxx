#include <ice/arctic_parser_logic.hxx>
#include <ice/arctic_parser_utils.hxx>

namespace ice::arctic::rules
{

    // struct EnsureTokenOnExit final
    // {
    //     ice::arctic::ParseState const fail_state = ParseState::Error;
    //     ice::arctic::ParseResult<ice::arctic::SyntaxNode*>& result;
    //     ice::arctic::TokenType const expected_type;
    //     ice::arctic::Token const& token_ref;

    //     inline ~EnsureTokenOnExit() noexcept
    //     {
    //         if (token_ref.type != expected_type)
    //         {
    //             result = fail_state;
    //         }
    //     }
    // };

    static ice::arctic::TokenRule const MatchRules_ExpValues[]{
        TokenRule_MatchType<TokenType::CT_Number, TokenRule_StoreToken<&SyntaxNode_ExpressionValue::value>>{},
        TokenRule_MatchType<TokenType::CT_NumberBin, TokenRule_StoreToken<&SyntaxNode_ExpressionValue::value>>{},
        TokenRule_MatchType<TokenType::CT_NumberFloat, TokenRule_StoreToken<&SyntaxNode_ExpressionValue::value>>{},
        TokenRule_MatchType<TokenType::CT_NumberHex, TokenRule_StoreToken<&SyntaxNode_ExpressionValue::value>>{},
        TokenRule_MatchType<TokenType::CT_NumberOct, TokenRule_StoreToken<&SyntaxNode_ExpressionValue::value>>{},
        TokenRule_MatchType<TokenType::CT_Literal, TokenRule_StoreToken<&SyntaxNode_ExpressionValue::value>>{},
        TokenRule_MatchType<TokenType::CT_String, TokenRule_StoreToken<&SyntaxNode_ExpressionValue::value>>{},
        TokenRule_MatchType<TokenType::KW_True, TokenRule_StoreToken<&SyntaxNode_ExpressionValue::value>>{},
        TokenRule_MatchType<TokenType::KW_False, TokenRule_StoreToken<&SyntaxNode_ExpressionValue::value>>{},
        TokenRule_MatchType<TokenType::CT_Symbol, TokenRule_StoreToken<&SyntaxNode_ExpressionValue::value>>{},
    };

    static ice::arctic::TokenRule const MatchRules_ExpValue[]{
        TokenGroup_MatchFirst{ MatchRules_ExpValues }
    };

    static ice::arctic::TokenRule const MatchRules_ExpBinaryOperations[]{
        TokenRule_MatchType<TokenType::OP_Assign, TokenRule_StoreToken<&SyntaxNode_ExpressionBinaryOperation::operation>>{},
        TokenRule_MatchType<TokenType::OP_Plus, TokenRule_StoreToken<&SyntaxNode_ExpressionBinaryOperation::operation>>{},
        TokenRule_MatchType<TokenType::OP_Minus, TokenRule_StoreToken<&SyntaxNode_ExpressionBinaryOperation::operation>>{},
        TokenRule_MatchType<TokenType::OP_Mul, TokenRule_StoreToken<&SyntaxNode_ExpressionBinaryOperation::operation>>{},
        TokenRule_MatchType<TokenType::OP_Div, TokenRule_StoreToken<&SyntaxNode_ExpressionBinaryOperation::operation>>{},
        TokenRule_MatchType<TokenType::OP_And, TokenRule_StoreToken<&SyntaxNode_ExpressionBinaryOperation::operation>>{},
        TokenRule_MatchType<TokenType::OP_Or, TokenRule_StoreToken<&SyntaxNode_ExpressionBinaryOperation::operation>>{}
    };

    static ice::arctic::TokenRule const MatchRules_ExpBinaryOperation[]{
        TokenGroup_MatchFirst{ MatchRules_ExpBinaryOperations }
    };

    static ice::arctic::TokenRule const MatchRules_ExpBinaryLeftAndOperation[]{
        TokenGroup_MatchSibling{ SyntaxNode_ExpressionValue{ }, MatchRules_ExpValue },
        TokenGroup_MatchSibling{ SyntaxNode_ExpressionBinaryOperation{}, MatchRules_ExpBinaryOperation, true },
    };

    static ice::arctic::TokenRule const MatchRules_ExpBinary[]{
        TokenGroup_MatchAll{ MatchRules_ExpBinaryLeftAndOperation, false, true }
    };

    static ice::arctic::TokenRule const MatchRules_ExpBinaryAll =
        TokenGroup_MatchAll{ MatchRules_ExpBinary };


    auto match_subexpression_recursive(
        void const* ud,
        ice::arctic::ParseState fail_state,
        ice::arctic::SyntaxNodeAllocator* alloc,
        ice::arctic::SyntaxNode* node,
        ice::arctic::Token& token,
        ice::arctic::Lexer& lexer
    ) noexcept -> ice::arctic::ParseState;

    static ice::arctic::TokenRule const MatchRules_RecursiveExpression[]{
        ice::arctic::TokenRule{
            .optional = false,
            .repeat = false,
            .fail_state = ParseState::Error,
            .userdata = nullptr,
            .func = match_subexpression_recursive
        }
    };

    static ice::arctic::TokenRule const MatchRules_RecursiveExpressionRepeat[]{
        ice::arctic::TokenRule{
            .optional = false,
            .repeat = true,
            .fail_state = ParseState::Error,
            .userdata = nullptr,
            .func = match_subexpression_recursive
        }
    };

    auto match_subexpression_recursive(
        void const* ud,
        ice::arctic::ParseState fail_state,
        ice::arctic::SyntaxNodeAllocator* alloc,
        ice::arctic::SyntaxNode* node,
        ice::arctic::Token& token,
        ice::arctic::Lexer& lexer
    ) noexcept -> ice::arctic::ParseState
    {
        static ice::arctic::TokenRule const match_rec = TokenGroup_MatchAll{ MatchRules_RecursiveExpression };
        static ice::arctic::TokenRule const match_post_binary = TokenGroup_MatchSibling{ SyntaxNode_ExpressionBinaryOperation{}, MatchRules_ExpBinaryOperation, true };

        ice::arctic::ParseState result = ParseState::Success;

        if (token.type == TokenType::ST_EndOfLine)
        {
            return ParseState::Error;
        }
        else if (token.type == TokenType::CT_ParenOpen)
        {
            SyntaxNode_ExplicitScope* sub_expression = alloc->create<SyntaxNode_ExplicitScope>();
            token = lexer.next();

            while (result == ParseState::Success && token.type != TokenType::CT_ParenClose)
            {
                result = match_subexpression_recursive(ud, fail_state, alloc, sub_expression, token, lexer);
            }

            if (token.type != TokenType::CT_ParenClose)
            {
                result = ParseState::Error_TypeOf_MissingBracketClose;
            }
            else
            {
                result = ParseState::Success;
                token = lexer.next();
            }

            if (result == ParseState::Success)
            {
                if (node->child == nullptr)
                {
                    node->child = sub_expression;
                }
                else
                {
                    // Save the sub expression in the parent as a child.
                    SyntaxNode* children = node->child;
                    while (children->sibling != nullptr)
                    {
                        children = children->sibling;
                    }

                    children->sibling = sub_expression;
                }

                if (token.type != TokenType::ST_EndOfLine)
                {
                    result = match_post_binary(alloc, sub_expression, token, lexer);
                }
            }
        }
        else if (token.type == TokenType::CT_Symbol)
        {
            ice::arctic::Token second = lexer.next();
            if (second.type == TokenType::CT_ParenOpen)
            {
                SyntaxNode_ExpressionCall* call = alloc->create<SyntaxNode_ExpressionCall>();
                call->function = token;
                token = lexer.next();

                SyntaxNode* last_call_arg = nullptr;
                while (result == ParseState::Success && token.type != TokenType::CT_ParenClose)
                {
                    SyntaxNode_ExpressionCallArg* call_arg = alloc->create<SyntaxNode_ExpressionCallArg>();

                    while (result == ParseState::Success
                        && token.type != TokenType::CT_Comma
                        && token.type != TokenType::CT_ParenClose)
                    {
                        result = match_subexpression_recursive(ud, fail_state, alloc, call_arg, token, lexer);

                        if (token.type == TokenType::ST_EndOfLine)
                        {
                            token = lexer.next();
                            result = ParseState::Success;
                        }
                    }

                    if (last_call_arg == nullptr)
                    {
                        call->child = call_arg;
                        last_call_arg = call_arg;
                    }
                    else
                    {
                        last_call_arg->sibling = call_arg;
                        last_call_arg = call_arg;
                    }

                    if (token.type == TokenType::CT_Comma)
                    {
                        token = lexer.next();
                        result = ParseState::Success;
                    }
                }

                if (token.type != TokenType::CT_ParenClose)
                {
                    result = ParseState::Error_TypeOf_MissingBracketClose;
                }

                token = lexer.next();

                if (result == ParseState::Success)
                {
                    if (node->child == nullptr)
                    {
                        node->child = call;
                    }
                    else
                    {
                        // Save the sub expression in the parent as a child.
                        SyntaxNode* children = node->child;
                        while (children->sibling != nullptr)
                        {
                            children = children->sibling;
                        }

                        children->sibling = call;
                    }
                }
            }
            else if (second.type == TokenType::CT_Dot)
            {
                SyntaxNode_ExpressionValue* value = alloc->create<SyntaxNode_ExpressionValue>();
                value->value = token;

                SyntaxNode_ExpressionGetMember* member = alloc->create<SyntaxNode_ExpressionGetMember>();
                member->member = lexer.next();
                value->child = member;
                token = lexer.next();

                while (token.type == TokenType::CT_Dot)
                {
                    member = alloc->create<SyntaxNode_ExpressionGetMember>();
                    member->member = lexer.next();

                    ice::arctic::append_sibling_or_assign(value->child, member);
                    token = lexer.next();
                }

                if (node->child == nullptr)
                {
                    node->child = value;
                }
                else
                {
                    ice::arctic::append_child(node, value);
                }

                if (token.type != TokenType::ST_EndOfLine)
                {
                    result = match_post_binary(alloc, value, token, lexer);
                }
            }
            else
            {
                SyntaxNode_ExpressionValue* value = alloc->create<SyntaxNode_ExpressionValue>();
                value->value = token;
                token = second;

                if (token.type != TokenType::CT_ParenClose
                    && token.type != TokenType::CT_Comma
                    && token.type != TokenType::ST_EndOfLine)
                {
                    result = match_post_binary(alloc, value, token, lexer);
                }

                if (result == ParseState::Success)
                {
                    if (node->child == nullptr)
                    {
                        node->child = value;
                    }
                    else
                    {
                        ice::arctic::append_child(node, value);
                    }
                }
            }
        }
        else
        {
            if (token.type == TokenType::OP_Minus)
            {
                SyntaxNode_ExpressionUnaryOperation* unary = alloc->create<SyntaxNode_ExpressionUnaryOperation>();
                unary->operation = token;

                token = lexer.next();
                if (token.type == TokenType::CT_ParenOpen)
                {
                    SyntaxNode temp{ };
                    result = match_subexpression_recursive(ud, fail_state, alloc, &temp, token, lexer);

                    unary->child = temp.child;
                    unary->sibling = temp.child->sibling;
                    temp.child->sibling = nullptr;
                }
                else
                {
                    result = match_rec(alloc, unary, token, lexer);
                }

                if (result == ParseState::Success)
                {
                    if (node->child == nullptr)
                    {
                        node->child = unary;
                    }
                    else
                    {
                        // Save the sub expression in the parent as a child.
                        SyntaxNode* children = node->child;
                        while (children->sibling != nullptr)
                        {
                            children = children->sibling;
                        }

                        children->sibling = unary;
                    }
                }
            }
            else
            {
                ice::arctic::SyntaxNode temp{ };
                result = MatchRules_ExpBinaryAll(alloc, &temp, token, lexer);

                if (result == ParseState::Success)
                {
                    if (node->child == nullptr)
                    {
                        node->child = temp.sibling;
                    }
                    else
                    {
                        // Save the sub expression in the parent as a child.
                        SyntaxNode* children = node->child;
                        while (children->sibling != nullptr)
                        {
                            children = children->sibling;
                        }

                        children->sibling = temp.sibling;
                    }
                }
            }
        }

        return result;
    }

    auto parse_expression(
        ice::arctic::SyntaxNodeAllocator& alloc,
        ice::arctic::SyntaxNode* parent_node,
        ice::arctic::Token& token,
        ice::arctic::Lexer& lexer
    ) noexcept -> ice::arctic::ParseResult<ice::arctic::SyntaxNode*>
    {
        static ice::arctic::TokenRule const match_exp = TokenGroup_MatchAll{ MatchRules_RecursiveExpressionRepeat };

        return match_exp(&alloc, parent_node, token, lexer);
    }

    auto parse_expression(
        ice::arctic::SyntaxNodeAllocator& alloc,
        ice::arctic::Token const& token,
        ice::arctic::Lexer& lexer
    ) noexcept -> ice::arctic::ParseResult<ice::arctic::SyntaxNode*>
    {
        return ice::arctic::ParseState::Error;
    }

} // namespace ice::arctic
