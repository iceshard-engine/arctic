#pragma once
#include <ice/arctic_lexer.hxx>
#include <ice/arctic_syntax_node.hxx>
#include <ice/arctic_parser_result.hxx>

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
