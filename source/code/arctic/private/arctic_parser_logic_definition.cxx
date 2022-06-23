#include "arctic_parser_logic.hxx"
#include "arctic_parser_utils.hxx"

namespace ice::arctic
{

    namespace rules
    {

        extern auto parse_expression(
            ice::arctic::SyntaxNodeAllocator& alloc,
            ice::arctic::SyntaxNode* parent_node,
            ice::arctic::Token& token,
            ice::arctic::Lexer& lexer
        ) noexcept -> ice::arctic::ParseResult<ice::arctic::SyntaxNode*>;

        struct TempNode : SyntaxNode
        {
            ice::arctic::Token matched_token;
        };

        namespace typeof
        {

            static ice::arctic::TokenRule constexpr MatchRules_Definition_TypeOfBaseType[]{
                TokenRule_MatchType<TokenType::CT_SquareBracketOpen>{}
                    .fail_with(ParseState::Error_TypeOf_MissingBracketOpen),
                TokenRule_MatchType<TokenType::CT_Symbol, TokenRule_StoreToken<&SyntaxNode_TypeDef::base_type>>{}
                    .fail_with(ParseState::Error_TypeOf_MissingTypeName),
                TokenRule_MatchType<TokenType::CT_SquareBracketClose>{}
                    .fail_with(ParseState::Error_TypeOf_MissingBracketClose)
            };

            static ice::arctic::TokenRule constexpr MatchRules_Definition_TypeOfSubType[]{
                TokenRule_MatchType<TokenType::KW_Alias, TokenRule_StoreBool<&SyntaxNode_TypeDef::is_alias, true>>{},
                TokenRule_MatchType<TokenType::KW_TypeOf, TokenRule_StoreBool<&SyntaxNode_TypeDef::is_alias, false>>{},
            };

            static ice::arctic::TokenRule constexpr MatchRules_Definition_TypeOf[]{
                TokenGroup_MatchFirst{ MatchRules_Definition_TypeOfSubType },
                TokenGroup_MatchAll{ MatchRules_Definition_TypeOfBaseType }
            };

            auto parse_node(
                ice::arctic::SyntaxNode_TypeDef* variable,
                ice::arctic::Token& token,
                ice::arctic::Lexer& lexer
            ) noexcept -> ice::arctic::ParseResult<ice::arctic::SyntaxNode*>
            {
                static TokenRule const match_typeof = TokenGroup_MatchAll{ MatchRules_Definition_TypeOf };
                variable->entity = SyntaxEntity::DEF_TypeDef;

                ice::arctic::ParseResult<ice::arctic::SyntaxNode*> result = match_typeof(nullptr, variable, token, lexer);
                if (result.has_error() == false)
                {
                    return variable;
                }
                return result;
            }

        } // namespace typeof

        namespace struct_type
        {

            static ice::arctic::TokenRule constexpr MatchRules_Definition_StructMember[]{
                TokenRule_MatchType<TokenType::CT_Symbol, TokenRule_StoreToken<&SyntaxNode_StructMember::name>>{},
                TokenRule_MatchType<TokenType::CT_Colon>{},
                TokenRule_MatchType<TokenType::CT_Symbol, TokenRule_StoreToken<&SyntaxNode_StructMember::type>>{},
                TokenRule_MatchType<TokenType::ST_EndOfLine>{}
            };

            static ice::arctic::TokenRule constexpr MatchRules_Definition_Struct[]{
                TokenRule_MatchType<TokenType::KW_Struct>{},
                TokenRule_MatchType<TokenType::CT_SquareBracketOpen>{}
                    .fail_with(ParseState::Error_TypeOf_MissingBracketOpen),
                TokenRule_MatchType<TokenType::ST_EndOfLine>{},
                TokenGroup_MatchChild{ SyntaxNode_StructMember{}, MatchRules_Definition_StructMember, true, true },
                TokenRule_MatchType<TokenType::CT_SquareBracketClose>{}
                    .fail_with(ParseState::Error_TypeOf_MissingBracketClose)
            };

            auto parse_node(
                ice::arctic::SyntaxNodeAllocator& alloc,
                ice::arctic::SyntaxNode_Struct* node,
                ice::arctic::Token& token,
                ice::arctic::Lexer& lexer
            ) noexcept -> ice::arctic::ParseResult<ice::arctic::SyntaxNode*>
            {
                static TokenRule const match_struct = TokenGroup_MatchAll{ MatchRules_Definition_Struct };

                ice::arctic::ParseResult<ice::arctic::SyntaxNode*> result = match_struct(&alloc, node, token, lexer);
                if (result.has_error() == false)
                {
                    return node;
                }
                return result;

                //static TokenRule const match_struct_member = TokenGroup_MatchAll{ MatchRules_Definition_StructMember };

                //node->entity = SyntaxEntity::DEF_Struct;

                //ice::arctic::SyntaxNode* last_child = nullptr;

                //ice::arctic::ParseResult<ice::arctic::SyntaxNode*> result = match_struct(nullptr, node, token, lexer);
                //while (result.has_error() == false && token.type != TokenType::CT_SquareBracketClose)
                //{
                //	SyntaxNode_StructMember* member = alloc.create<SyntaxNode_StructMember>();

                //	if (last_child == nullptr)
                //	{
                //		node->child = member;
                //		last_child = member;
                //	}
                //	else
                //	{
                //		last_child->sibling = member;
                //		last_child = member;
                //	}

                //	result = match_struct_member(nullptr, member, token, lexer);
                //}

                //if (result.has_error() == false)
                //{
                //	result = node;
                //}
                //return result;
            }

        } // namespace struct_type

        static ice::arctic::TokenRule constexpr MatchRules_DefinitionTypes[]{
            TokenRule_MatchType<TokenType::KW_TypeOf>{},
            TokenRule_MatchType<TokenType::KW_Struct>{},
            TokenRule_MatchType<TokenType::KW_Alias>{},
        };

        static ice::arctic::TokenRule constexpr MatchRules_Definition[]{
            TokenRule_MatchType<TokenType::CT_Symbol, TokenRule_StoreToken<&TempNode::matched_token>>{},
            TokenRule_MatchType<TokenType::OP_Assign>{},
        };

        auto parse_node_definition(
            ice::arctic::SyntaxNodeAllocator& alloc,
            ice::arctic::Lexer& lexer
        ) noexcept -> ice::arctic::ParseResult<ice::arctic::SyntaxNode*>
        {
            static TokenRule const match_definition = TokenGroup_MatchAll{ MatchRules_Definition };

            ice::arctic::rules::TempNode temp_node;
            ice::arctic::Token token = lexer.next();
            ice::arctic::ParseResult<ice::arctic::SyntaxNode*> result = match_definition(&alloc, &temp_node, token, lexer);

            if (result.has_error() == false)
            {
                if (token.type == TokenType::KW_Struct)
                {
                    ice::arctic::SyntaxNode_Struct* node = alloc.create<SyntaxNode_Struct>();
                    node->name = temp_node.matched_token;

                    result = struct_type::parse_node(alloc, node, token, lexer);
                }
                else if (token.type == TokenType::KW_TypeOf)
                {
                    ice::arctic::SyntaxNode_TypeDef* node = alloc.create<SyntaxNode_TypeDef>();
                    node->name = temp_node.matched_token;

                    result = typeof::parse_node(node, token, lexer);
                }
                else if (token.type == TokenType::KW_Alias)
                {
                    ice::arctic::SyntaxNode_TypeDef* node = alloc.create<SyntaxNode_TypeDef>();
                    node->name = temp_node.matched_token;

                    result = typeof::parse_node(node, token, lexer);
                }
                else
                {
                    result = ParseState::Error_Definition_UnknownToken;
                }
            }

            return result;
        }

        namespace func
        {

            static ice::arctic::TokenRule constexpr MatchRules_FunctionArg[]{
                TokenRule_MatchType<TokenType::ST_EndOfLine>{.optional = true},
                TokenRule_MatchType<TokenType::CT_Symbol, TokenRule_StoreToken<&SyntaxNode_FunctionArgument::name>>{},
                TokenRule_MatchType<TokenType::CT_Colon>{},
                TokenRule_MatchType<TokenType::CT_Symbol, TokenRule_StoreToken<&SyntaxNode_FunctionArgument::type>>{},
                TokenRule_MatchType<TokenType::ST_EndOfLine>{.optional = true},
                TokenRule_MatchType<TokenType::CT_Comma>{ .optional = true }
            };

            static ice::arctic::TokenRule constexpr MatchRules_Function[]{
                TokenRule_MatchType<TokenType::CT_Symbol, TokenRule_StoreToken<&SyntaxNode_Function::name>>{},
                TokenRule_MatchType<TokenType::CT_ParenOpen>{}.fail_with(ParseState::Error_UnexpectedToken),
                TokenGroup_MatchChild{ SyntaxNode_FunctionArgument{}, MatchRules_FunctionArg, true, true },
                TokenRule_MatchType<TokenType::CT_ParenClose>{}.fail_with(ParseState::Error_UnexpectedToken),
                TokenRule_MatchType<TokenType::CT_Colon>{},
                TokenRule_MatchType<TokenType::CT_Symbol, TokenRule_StoreToken<&SyntaxNode_Function::result_type>>{},
                TokenRule_MatchType<TokenType::ST_EndOfLine>{}
            };

        } // namespace func

        auto parse_node_function(
            ice::arctic::SyntaxNodeAllocator& alloc,
            ice::arctic::Lexer& lexer
        ) noexcept -> ice::arctic::ParseResult<ice::arctic::SyntaxNode*>
        {
            static TokenRule const match_function = TokenGroup_MatchAll{ func::MatchRules_Function };

            ice::arctic::SyntaxNode_Function* node = alloc.create<SyntaxNode_Function>();

            ice::arctic::Token token = lexer.next();
            ice::arctic::ParseResult<ice::arctic::SyntaxNode*> result = match_function(&alloc, node, token, lexer);
            if (result.has_error())
            {
                return result;
            }

            if (token.type != TokenType::CT_BracketOpen)
            {
                return ParseState::Error_UnexpectedToken;
            }

            return node;
        }

        namespace attribs
        {

            static ice::arctic::TokenRule constexpr MatchRules_AttributeValue[]{
                TokenRule_MatchType<TokenType::CT_Number, TokenRule_StoreToken<&SyntaxNode_AnnotationAttribute::value>>{},
                TokenRule_MatchType<TokenType::CT_NumberBin, TokenRule_StoreToken<&SyntaxNode_AnnotationAttribute::value>>{},
                TokenRule_MatchType<TokenType::CT_NumberFloat, TokenRule_StoreToken<&SyntaxNode_AnnotationAttribute::value>>{},
                TokenRule_MatchType<TokenType::CT_NumberHex, TokenRule_StoreToken<&SyntaxNode_AnnotationAttribute::value>>{},
                TokenRule_MatchType<TokenType::CT_NumberOct, TokenRule_StoreToken<&SyntaxNode_AnnotationAttribute::value>>{},
                TokenRule_MatchType<TokenType::CT_Literal, TokenRule_StoreToken<&SyntaxNode_AnnotationAttribute::value>>{},
                TokenRule_MatchType<TokenType::CT_String, TokenRule_StoreToken<&SyntaxNode_AnnotationAttribute::value>>{},
                TokenRule_MatchType<TokenType::KW_True, TokenRule_StoreToken<&SyntaxNode_AnnotationAttribute::value>>{},
                TokenRule_MatchType<TokenType::KW_False, TokenRule_StoreToken<&SyntaxNode_AnnotationAttribute::value>>{},
                TokenRule_MatchType<TokenType::CT_Symbol, TokenRule_StoreToken<&SyntaxNode_AnnotationAttribute::value>>{},
            };

            static ice::arctic::TokenRule constexpr MatchRules_AttributeName[]{
                TokenRule_MatchType<TokenType::CT_Colon, TokenRule_MergeToken<&SyntaxNode_AnnotationAttribute::name>>{.repeat = true },
                TokenRule_MatchType<TokenType::CT_Symbol, TokenRule_MergeToken<&SyntaxNode_AnnotationAttribute::name>>{ },
            };

            static ice::arctic::TokenRule constexpr MatchRules_AttributeAssignValue[]{
                TokenRule_MatchType<TokenType::OP_Assign>{},
                TokenGroup_MatchFirst{ MatchRules_AttributeValue }
            };

            static ice::arctic::TokenRule constexpr MatchRules_AttributeFirst[]{
                TokenRule_MatchType<TokenType::CT_Symbol, TokenRule_StoreToken<&SyntaxNode_AnnotationAttribute::name>>{ },
                TokenGroup_MatchAll{ MatchRules_AttributeAssignValue, true }
            };

            static ice::arctic::TokenRule constexpr MatchRules_Attribute[]{
                TokenRule_MatchType<TokenType::CT_Comma>{},
                TokenRule_MatchType<TokenType::CT_Symbol, TokenRule_StoreToken<&SyntaxNode_AnnotationAttribute::name>>{ },
                TokenGroup_MatchAll{ MatchRules_AttributeAssignValue, true }
            };

            static ice::arctic::TokenRule constexpr MatchRules_Attributes[]{
                TokenGroup_MatchChild{ SyntaxNode_AnnotationAttribute{}, MatchRules_AttributeFirst },
                TokenGroup_MatchChild{ SyntaxNode_AnnotationAttribute{}, MatchRules_Attribute, /*.optional =*/ true, /*.repeat =*/ true },
            };

            static ice::arctic::TokenRule constexpr MatchRules_Annotation[]{
                TokenRule_MatchType<TokenType::CT_SquareBracketOpen>{},
                TokenGroup_MatchAll{ MatchRules_Attributes, false, false },
                TokenRule_MatchType<TokenType::CT_SquareBracketClose>{}
            };

            auto parse_node_annotation(
                ice::arctic::SyntaxNodeAllocator& alloc,
                ice::arctic::Token& token,
                ice::arctic::Lexer& lexer
            ) noexcept -> ice::arctic::ParseResult<ice::arctic::SyntaxNode*>
            {
                static TokenRule const match_annotation = TokenGroup_MatchAll{ MatchRules_Annotation, false, false };

                ice::arctic::SyntaxNode_Annotation* node = alloc.create<SyntaxNode_Annotation>();
                ice::arctic::ParseResult<ice::arctic::SyntaxNode*> result = match_annotation(&alloc, node, token, lexer);
                if (result.has_error())
                {
                    return result;
                }
                return node;
            }

        } // namespace attribs

        namespace variable
        {

            static ice::arctic::TokenRule constexpr MatchRules_Variable[]{
                TokenRule_MatchType<TokenType::KW_Let>{},
                TokenRule_MatchType<TokenType::CT_Symbol, TokenRule_StoreToken<&SyntaxNode_Variable::name>>{},
                TokenRule_MatchType<TokenType::CT_Colon>{},
                TokenRule_MatchType<TokenType::CT_Symbol, TokenRule_StoreToken<&SyntaxNode_Variable::type>>{},
            };

            template<typename VariableType>
            auto parse_variable_definition(
                ice::arctic::SyntaxNodeAllocator& alloc,
                ice::arctic::Token& token,
                ice::arctic::Lexer& lexer
            ) noexcept -> ice::arctic::ParseResult<ice::arctic::SyntaxNode*>
            {
                static TokenRule const match_variable = TokenGroup_MatchAll{ MatchRules_Variable, false, false };

                VariableType* node = alloc.create<VariableType>();
                ice::arctic::ParseResult<ice::arctic::SyntaxNode*> result = match_variable(&alloc, node, token, lexer);
                if (result.has_error())
                {
                    return result;
                }

                if (token.type == TokenType::OP_Assign)
                {
                    ice::arctic::Token const saved_token = token;
                    token = lexer.next();

                    ice::arctic::SyntaxNode temp{ };
                    if (auto expr_result = ice::arctic::rules::parse_expression(alloc, &temp, token, lexer); expr_result.has_error())
                    {
                        return expr_result;
                    }

                    SyntaxNode_ExpressionBinaryOperation* operation = alloc.create<SyntaxNode_ExpressionBinaryOperation>();
                    operation->operation = saved_token;
                    operation->sibling = temp.child;

                    SyntaxNode_Expression* expression = alloc.create<SyntaxNode_Expression>();
                    expression->child = operation;

                    ice::arctic::append_child(node, expression);
                }

                return node;
            }

        } // namespace variable

        auto parse_expression_block(
            ice::arctic::SyntaxNodeAllocator& alloc,
            ice::arctic::SyntaxNode* parent_node,
            ice::arctic::Lexer& lexer
        ) noexcept -> ice::arctic::ParseResult<ice::arctic::SyntaxNode*>
        {
            ice::arctic::SyntaxNode* child = nullptr;

            const auto append_child = [&parent_node, &child](ice::arctic::SyntaxNode* new_child) noexcept -> void
            {
                if (child == nullptr)
                {
                    parent_node->child = new_child;
                    child = new_child;
                }
                else
                {
                    child->sibling = new_child;
                    child = new_child;
                }
            };

            ice::arctic::Token token = lexer.next();

            do
            {
                token = lexer.next();

                switch (token.type)
                {
                case TokenType::KW_Let:
                {
                    auto var_result = rules::variable::parse_variable_definition<SyntaxNode_Variable>(alloc, token, lexer);
                    if (var_result.has_error())
                    {
                        return var_result;
                    }

                    append_child(var_result);
                    break;
                }
                case TokenType::CT_Symbol:
                {
                    ice::arctic::SyntaxNode temp{ };
                    if (auto expr_result = parse_expression(alloc, &temp, token, lexer); expr_result.has_error())
                    {
                        return expr_result;
                    }

                    SyntaxNode_Expression* expression = alloc.create<SyntaxNode_Expression>();
                    expression->child = temp.child;
                    append_child(expression);
                    break;
                }
                case TokenType::CT_BracketOpen:
                {
                    ice::arctic::ParseResult<ice::arctic::SyntaxNode*> const result = parse_expression_block(
                        alloc,
                        alloc.create<ice::arctic::SyntaxNode_Scope>(),
                        lexer
                    );
                    if (result.has_error())
                    {
                        return result;
                    }

                    append_child(result);
                    break;
                }
                case TokenType::CT_BracketClose:
                case TokenType::ST_EndOfLine:
                    break;
                case TokenType::ST_EndOfFile:
                    return ParseState::Error; // TODO;
                default:
                    assert(false);
                    break;
                }

            } while (token.type != TokenType::CT_BracketClose);

            return parent_node;
        }

    } // namespace rules

    auto parse_definition(
        ice::arctic::SyntaxNodeAllocator& alloc,
        ice::arctic::Token& token,
        ice::arctic::Lexer& lexer
    ) noexcept -> ice::arctic::ParseResult<ice::arctic::SyntaxNode*>
    {
        ice::arctic::ParseResult<ice::arctic::SyntaxNode*> result = ParseState::Success;

        switch (token.type)
        {
        case TokenType::KW_Fn:
            result = rules::parse_node_function(alloc, lexer);
            if (result.has_error() == false)
            {
                ice::arctic::ParseResult<ice::arctic::SyntaxNode*> exp_result = rules::parse_expression_block(
                    alloc,
                    alloc.create<ice::arctic::SyntaxNode_FunctionBody>(),
                    lexer
                );
                if (exp_result.has_error())
                {
                    return exp_result;
                }

                ice::arctic::SyntaxNode* fn_node = result;
                fn_node->sibling = exp_result;
            }
            break;
        case TokenType::KW_Def:
            result = rules::parse_node_definition(alloc, lexer);
            break;
        case TokenType::KW_Let:
            result = rules::variable::parse_variable_definition<SyntaxNode_Variable>(alloc, token, lexer);
            break;
        case TokenType::CT_SquareBracketOpen:
            result = rules::attribs::parse_node_annotation(alloc, token, lexer);
            break;
        default:
            return ParseState::Error_Definition_UnknownToken;
            break;
        }

        if (result.has_error())
        {
            alloc.destroy(result._value);
        }

        return result;
    }

    auto parse_context_block(
        ice::arctic::SyntaxNodeAllocator& alloc,
        ice::arctic::Token& token,
        ice::arctic::Lexer& lexer
    ) noexcept -> ice::arctic::ParseResult<ice::arctic::SyntaxNode*>
    {
        ice::arctic::ParseResult<ice::arctic::SyntaxNode*> result = ParseState::Success;

        switch (token.type)
        {
        case TokenType::KW_Fn:
            result = rules::parse_node_function(alloc, lexer);
            break;
        case TokenType::KW_Let:
            result = rules::variable::parse_variable_definition<SyntaxNode_ContextVariable>(alloc, token, lexer);
            break;
        case TokenType::CT_SquareBracketOpen:
            result = rules::attribs::parse_node_annotation(alloc, token, lexer);
            break;
        default:
            return ParseState::Error_Definition_UnknownToken;
            break;
        }

        if (result.has_error())
        {
            alloc.destroy(result._value);
        }

        return result;
    }

} // namespace ice::arctic
