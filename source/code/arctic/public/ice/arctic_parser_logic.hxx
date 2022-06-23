#pragma once
#include "arctic_lexer.hxx"
#include "arctic_syntax_node.hxx"
#include "arctic_parser_result.hxx"

#pragma once
#include "arctic_parser_result.hxx"

namespace ice::arctic
{

    auto parse_definition(
        ice::arctic::SyntaxNodeAllocator& alloc,
        ice::arctic::Token& token,
        ice::arctic::Lexer& lexer
    ) noexcept -> ice::arctic::ParseResult<ice::arctic::SyntaxNode*>;

    auto parse_context_block(
        ice::arctic::SyntaxNodeAllocator& alloc,
        ice::arctic::Token& token,
        ice::arctic::Lexer& lexer
    ) noexcept -> ice::arctic::ParseResult<ice::arctic::SyntaxNode*>;

} // namespace ice::arctic
