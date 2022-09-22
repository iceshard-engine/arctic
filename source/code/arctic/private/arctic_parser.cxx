#include <ice/arctic_parser.hxx>
#include <ice/arctic_parser_logic.hxx>
#include <ice/arctic_parser_utils.hxx>
#include <ice/arctic_syntax_node.hxx>
#include <fmt/format.h>

#if !defined _WIN32
#include <malloc.h>
#endif

namespace ice::arctic
{

    class DefaultParser final : public ice::arctic::Parser
    {
    public:
        DefaultParser() noexcept = default;

        void parse(
            ice::arctic::Lexer& lexer,
            ice::arctic::SyntaxNodeAllocator& node_alloc,
            ice::Span<ice::arctic::SyntaxVisitorBase*> visitors
        ) noexcept override;
    };

    auto parse_block(
        ice::arctic::SyntaxNodeAllocator& alloc,
        ice::arctic::Token& token,
        ice::arctic::Lexer& lexer,
        ice::Span<ice::arctic::SyntaxVisitorBase*> visitors
    ) noexcept -> ice::arctic::ParseState
    {
        token = (lexer.next(), lexer.next());
        if (token.type != TokenType::CT_BracketOpen)
        {
            return ParseState::Error_TypeOf_MissingBracketOpen;
        }

        ice::arctic::SyntaxNode* annotation = nullptr;

        token = lexer.next();
        while(token.type != TokenType::CT_BracketClose)
        {
            ice::arctic::ParseResult<ice::arctic::SyntaxNode*> result = ParseState::Success;

            if (token.type != TokenType::ST_EndOfLine)
            {
                result = parse_context_block(alloc, token, lexer);
            }

            if (result.has_error())
            {
                return result._state;
            }

            if (result._value != nullptr)
            {
                if (result._value->entity == SyntaxEntity::DEF_Annotation)
                {
                    ice::arctic::append_sibling_or_assign(annotation, result);
                }
                else
                {
                    result._value->annotation = annotation;
                    annotation = nullptr;
                }
            }

            if (result._value != nullptr)
            {
                for (ice::arctic::SyntaxVisitorBase* visitor : visitors)
                {
                    visitor->visit(result);
                }
            }

            token = lexer.next();
        }

        if (token.type != TokenType::CT_BracketClose)
        {
            return ParseState::Error_TypeOf_MissingBracketOpen;
        }

        return ParseState::Success;
    }

    void DefaultParser::parse(
        ice::arctic::Lexer& lexer,
        ice::arctic::SyntaxNodeAllocator& node_alloc,
        ice::Span<ice::arctic::SyntaxVisitorBase*> visitors
    ) noexcept
    {
        ice::arctic::Token token = lexer.next();

        ice::arctic::SyntaxNode* root = node_alloc.create<ice::arctic::SyntaxNode_ROOT>();
        ice::arctic::ParseResult result = root;
        for (ice::arctic::SyntaxVisitorBase* visitor : visitors)
        {
            visitor->visit(result);
        }

        ice::arctic::SyntaxNode* annotation = nullptr;
        while (result.has_error() == false && token.type != ice::arctic::TokenType::ST_EndOfFile)
        {
            switch (token.type)
            {
            case TokenType::KW_Fn:
            case TokenType::KW_Def:
            case TokenType::KW_Let:
                result = parse_definition(node_alloc, token, lexer);
                if (result.has_error() == false)
                {
                    result._value->annotation = annotation;
                    annotation = nullptr;
                }
                break;
            case TokenType::KW_Ctx:
            {
                if (ice::arctic::ParseState state = parse_block(node_alloc, token, lexer, visitors); state != ParseState::Success)
                {
                    result = state;
                }
                break;
            }
            case TokenType::CT_SquareBracketOpen:
                result = parse_definition(node_alloc, token, lexer);
                if (result.has_error() == false)
                {
                    ice::arctic::append_sibling_or_assign(annotation, result);
                }
                break;
            case TokenType::ST_EndOfLine:
                token = lexer.next();
                continue;
            default:
                //result = parse_expression(*this, token, lexer);
                break;
            }

            if (result.has_error() == false && result._value != nullptr)
            {
                for (ice::arctic::SyntaxVisitorBase* visitor : visitors)
                {
                    visitor->visit(result);
                }
            }

            token = lexer.next();
        }

        if (result.has_error())
        {
            printf("Error while parsing: %s\n", result.error_string().data());
        }
    }

    class SimpleHostAllocator final : public ice::arctic::SyntaxNodeAllocator
    {
    public:
        SimpleHostAllocator() noexcept = default;
        ~SimpleHostAllocator() noexcept override;

        auto allocate(ice::u64 size, ice::u64 align) noexcept -> void* override;
        void deallocate(void* ptr) noexcept override;

    private:
        ice::u32 _active_allocation_count = 0;
    };

    SimpleHostAllocator::~SimpleHostAllocator() noexcept
    {
        fmt::print("Active allocations: {}\n", _active_allocation_count);
        assert(_active_allocation_count == 0);
    }

#if defined _WIN32
    auto SimpleHostAllocator::allocate(ice::u64 size, ice::u64 align) noexcept -> void*
    {
        _active_allocation_count += 1;
        return _aligned_malloc(size, align);
    }

    void SimpleHostAllocator::deallocate(void* ptr) noexcept
    {
        _aligned_free(ptr);
        _active_allocation_count -= 1;
    }
#else
    auto SimpleHostAllocator::allocate(ice::u64 size, ice::u64 align) noexcept -> void*
    {
        _active_allocation_count += 1;
        return aligned_alloc(align, size);
    }

    void SimpleHostAllocator::deallocate(void* ptr) noexcept
    {
        free(ptr);
        _active_allocation_count -= 1;
    }
#endif

    auto create_default_parser() noexcept -> std::unique_ptr<ice::arctic::Parser>
    {
        return std::make_unique<DefaultParser>();
    }

    auto create_simple_allocator() noexcept -> std::unique_ptr<ice::arctic::SyntaxNodeAllocator>
    {
        return std::make_unique<SimpleHostAllocator>();
    }

} // namespace ice::arctic
