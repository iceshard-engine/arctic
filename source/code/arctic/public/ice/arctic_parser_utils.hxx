#pragma once
#include <ice/arctic_types.hxx>
#include <ice/arctic_token.hxx>
#include <ice/arctic_lexer.hxx>
#include <ice/arctic_parser_result.hxx>

namespace ice::arctic
{

    struct TokenRule;

    using TokenRuleFn = auto(
        void const*,
        ice::arctic::ParseState,
        ice::arctic::SyntaxNodeAllocator*,
        ice::arctic::SyntaxNode*,
        ice::arctic::Token&,
        ice::arctic::Lexer&
    ) noexcept -> ice::arctic::ParseState;

    using TokenGroupFn = auto(
        ice::Span<TokenRule const>,
        ice::arctic::SyntaxNodeAllocator*,
        ice::arctic::SyntaxNode*,
        ice::arctic::Token&,
        ice::arctic::Lexer&
    ) noexcept -> ice::arctic::ParseState;

    //using TokenRuleKeepFn = void(
    //    ice::arctic::SyntaxNode*,
    //    ice::arctic::Token const&
    //) noexcept;

    //void TokenRule_SkipKeep(SyntaxNode*, Token const&) noexcept
    //{
    //}

    //template<typename NodeType, auto Member>
    //void TokenRule_StoreToken(SyntaxNode* node, Token const& token) noexcept
    //{
    //    (static_cast<NodeType*>(node)->*Member) = token;
    //}

    //template<typename NodeType, auto Member>
    //void TokenRule_StoreTrue(SyntaxNode* node, Token const& token) noexcept
    //{
    //    (static_cast<NodeType*>(node)->*Member) = true;
    //}

    //template<typename NodeType, auto Member>
    //void TokenRule_StoreFalse(SyntaxNode* node, Token const& token) noexcept
    //{
    //    (static_cast<NodeType*>(node)->*Member) = false;
    //}


    struct TokenRule
    {
        bool optional = false;
        bool repeat = false;

        ice::arctic::ParseState fail_state = ParseState::Error;

        void const* userdata = nullptr;
        ice::arctic::TokenRuleFn* func;

        auto operator()(
            ice::arctic::SyntaxNodeAllocator* alloc,
            ice::arctic::SyntaxNode* node,
            ice::arctic::Token& token,
            ice::arctic::Lexer& lexer
        ) const noexcept -> ice::arctic::ParseState
        {
            return func(userdata, fail_state, alloc, node, token, lexer);
        }
    };

    template<bool ParentPtr = false>
    struct TokenRuleContext
    {
        ice::arctic::SyntaxNodeAllocator* alloc;
        ice::arctic::SyntaxNode* parent;
        ice::arctic::Token& token;
        ice::arctic::Lexer& lexer;
    };

    template<>
    struct TokenRuleContext<false>
    {
        ice::arctic::SyntaxNodeAllocator* alloc;
        ice::arctic::SyntaxNode parent;
        ice::arctic::Token& token;
        ice::arctic::Lexer& lexer;
    };

    template<bool ParentPtr>
    inline static auto operator<<(
        ice::arctic::TokenRuleContext<ParentPtr>& ctx,
        ice::arctic::TokenRule const& rule
    ) noexcept -> ice::arctic::ParseResult<ice::arctic::SyntaxNode*>
    {
        ice::arctic::ParseState result;

        if constexpr (ParentPtr == false)
        {
            result = rule(ctx.alloc, &ctx.parent, ctx.token, ctx.lexer);
        }
        else
        {
            result = rule(ctx.alloc, ctx.parent, ctx.token, ctx.lexer);
        }

        if (result == ParseState::Success)
        {
            if constexpr (ParentPtr == false)
            {
                return ctx.parent.child;
            }
            else
            {
                return ctx.parent;
            }
        }
        else
        {
            return result;
        }
    }

    static auto GroupFn_MatchAll(
        ice::Span<TokenRule const> rules,
        ice::arctic::SyntaxNodeAllocator* alloc,
        ice::arctic::SyntaxNode* node,
        ice::arctic::Token& token,
        ice::arctic::Lexer& lexer
    ) noexcept -> ice::arctic::ParseState
    {
        auto it = rules.begin();
        auto const end = rules.end();

        bool matched_once = false;
        ice::arctic::ParseState result_state = ParseState::Success;

        bool matching = true;
        while (matching && it != end)
        {
            ice::utf8 const* previous = token.value.data();

            result_state = (*it)(alloc, node, token, lexer);

            // Repeat match (if not failing)
            while (result_state == ParseState::Success && it->repeat)
            {
                matched_once = true;
                previous = token.value.data();
                result_state = (*it)(alloc, node, token, lexer);
            }

            // Apply optionality
            matching &= (result_state == ParseState::Success) | ((it->optional || matched_once) && previous == token.value.data());

            // Move to next rule
            it += 1;
        }

        return matching ? ParseState::Success : result_state;
    }

    template<typename ChildNode>
    static auto GroupFn_MatchAsChild(
        ice::Span<TokenRule const> rules,
        ice::arctic::SyntaxNodeAllocator* alloc,
        ice::arctic::SyntaxNode* node,
        ice::arctic::Token& token,
        ice::arctic::Lexer& lexer
    ) noexcept -> ice::arctic::ParseState
    {
        auto it = rules.begin();
        auto const end = rules.end();

        bool matched_once = false;
        ice::arctic::ParseState result_state = ParseState::Success;

        ChildNode* child = alloc->create<ChildNode>();

        bool matching = true;
        while (matching && it != end)
        {
            ice::utf8 const* previous = token.value.data();

            result_state = (*it)(alloc, child, token, lexer);

            // Repeat match (if not failing)
            while (result_state == ParseState::Success && it->repeat)
            {
                matched_once = true;
                previous = token.value.data();
                result_state = (*it)(alloc, child, token, lexer);
            }

            // Apply optionality
            matching &= (result_state == ParseState::Success) | ((it->optional || matched_once) && previous == token.value.data());

            if (matching == false) // && result_state != ParseState::Success)
            {
                alloc->destroy(child);
                child = nullptr;
            }
            else
            {
                result_state = ParseState::Success;
            }

            // Move to next rule
            it += 1;
        }

        if (node->child == nullptr)
        {
            node->child = child;
        }
        else
        {
            auto last_it = node->child;
            while (last_it->sibling != nullptr)
            {
                last_it = last_it->sibling;
            }

            last_it->sibling = child;
        }

        return result_state;
    }

    template<typename ChildNode>
    static auto GroupFn_MatchAsSibling(
        ice::Span<TokenRule const> rules,
        ice::arctic::SyntaxNodeAllocator* alloc,
        ice::arctic::SyntaxNode* node,
        ice::arctic::Token& token,
        ice::arctic::Lexer& lexer
    ) noexcept -> ice::arctic::ParseState
    {
        auto it = rules.begin();
        auto const end = rules.end();

        bool matched_once = false;
        ice::arctic::ParseState result_state = ParseState::Success;

        ChildNode* sibling = alloc->create<ChildNode>();

        bool matching = true;
        while (matching && it != end)
        {
            ice::utf8 const* previous = token.value.data();

            result_state = (*it)(alloc, sibling, token, lexer);

            // Repeat match (if not failing)
            while (result_state == ParseState::Success && it->repeat)
            {
                matched_once = true;
                previous = token.value.data();
                result_state = (*it)(alloc, sibling, token, lexer);
            }

            // Apply optionality
            matching &= (result_state == ParseState::Success) | ((it->optional || matched_once) && previous == token.value.data());

            if (matching == false)
            //if (result_state != ParseState::Success)
            {
                alloc->destroy(sibling);
                sibling = nullptr;
            }
            else
            {
                result_state = ParseState::Success;
            }

            // Move to next rule
            it += 1;
        }

        if (node->sibling == nullptr)
        {
            node->sibling = sibling;
        }
        else
        {
            SyntaxNode* last_it = node->sibling;
            while (last_it->sibling != nullptr)
            {
                last_it = last_it->sibling;
            }

            last_it->sibling = sibling;
        }

        return result_state;
    }

    static auto GroupFn_MatchFirst(
        ice::Span<ice::arctic::TokenRule const> rules,
        ice::arctic::SyntaxNodeAllocator* alloc,
        ice::arctic::SyntaxNode* node,
        ice::arctic::Token& token,
        ice::arctic::Lexer& lexer
    ) noexcept -> ice::arctic::ParseState
    {
        auto it = rules.begin();
        auto const end = rules.end();

        ice::utf8 const* const previous = token.value.data();
        ice::arctic::ParseState result_state = ParseState::Success;

        bool matching = false;
        while (!matching && it != end && previous == token.value.data())
        {
            result_state = (*it)(alloc, node, token, lexer);

            // Repeat match (if not failing)
            while (result_state == ParseState::Success && it->repeat)
            {
                result_state = (*it)(alloc, node, token, lexer);
            }

            // Apply optionality
            matching |= (result_state == ParseState::Success);

            // Move to next rule
            it += 1;
        }

        return result_state;
    }

    template<TokenGroupFn* Fn, size_t Count>
    struct TokenGroupRules
    {
        bool optional;
        bool repeat;

        ice::arctic::TokenRule const (&rules)[Count];

        constexpr TokenGroupRules(TokenRule const(&rules)[Count], bool optional, bool repeat) noexcept
            : optional{ optional }
            , repeat{ repeat }
            , rules{ rules }
        {
        }

        static auto rule_fn(
            void const* ud,
            ice::arctic::ParseState fail_state,
            ice::arctic::SyntaxNodeAllocator* alloc,
            ice::arctic::SyntaxNode* node,
            ice::arctic::Token& token,
            ice::arctic::Lexer& lexer
        ) noexcept -> ice::arctic::ParseState
        {
            TokenRule const* rules = reinterpret_cast<TokenRule const*>(ud);
            ParseState const result = Fn({ rules, Count }, alloc, node, token, lexer);;

            if (fail_state == ParseState::Success)
            {
                return result;
            }
            else
            {
                return result == ParseState::Success ? result : fail_state;
            }
        }

        constexpr operator TokenRule() const noexcept
        {
            return TokenRule{
                .optional = optional,
                .repeat = repeat,
                .userdata = &rules[0],
                .func = rule_fn
            };
        }

        constexpr auto fail_with(ice::arctic::ParseState state) noexcept -> ice::arctic::TokenRule
        {
            TokenRule result = *this;
            result.fail_state = state;
            return result;
        }
    };

    template<typename ChildType, size_t Count>
    struct TokenGroup_MatchChild : TokenGroupRules<GroupFn_MatchAsChild<ChildType>, Count>
    {
        constexpr TokenGroup_MatchChild(
            ChildType,
            TokenRule const(&rules)[Count],
            bool optional = false,
            bool repeat = false
        ) noexcept
            : TokenGroupRules<GroupFn_MatchAsChild<ChildType>, Count>{ rules, optional, repeat }
        {
        }
    };

    template<typename ChildType, size_t Count>
    TokenGroup_MatchChild(ChildType, TokenRule const(&)[Count]) -> TokenGroup_MatchChild<ChildType, Count>;

    template<typename ChildType, size_t Count>
    struct TokenGroup_MatchSibling : TokenGroupRules<GroupFn_MatchAsSibling<ChildType>, Count>
    {
        constexpr TokenGroup_MatchSibling(
            ChildType,
            TokenRule const(&rules)[Count],
            bool optional = false,
            bool repeat = false
        ) noexcept
            : TokenGroupRules<GroupFn_MatchAsSibling<ChildType>, Count>{ rules, optional, repeat }
        {
        }
    };

    template<typename ChildType, size_t Count>
    TokenGroup_MatchSibling(ChildType, TokenRule const(&)[Count]) -> TokenGroup_MatchSibling<ChildType, Count>;

    template<size_t Count>
    struct TokenGroup_MatchAll : TokenGroupRules<GroupFn_MatchAll, Count>
    {
        constexpr TokenGroup_MatchAll(
            TokenRule const(&rules)[Count],
            bool optional = false,
            bool repeat = false
        ) noexcept
            : TokenGroupRules<GroupFn_MatchAll, Count>{ rules, optional, repeat }
        {
        }
    };

    template<size_t Size>
    TokenGroup_MatchAll(TokenRule const(&)[Size]) -> TokenGroup_MatchAll<Size>;

    template<size_t Count>
    struct TokenGroup_MatchFirst : TokenGroupRules<GroupFn_MatchFirst, Count>
    {
        constexpr TokenGroup_MatchFirst(
            TokenRule const(&rules)[Count],
            bool optional = false,
            bool repeat = false
        ) noexcept
            : TokenGroupRules<GroupFn_MatchFirst, Count>{ rules, optional, repeat }
        {
        }
    };

    template<size_t Size>
    TokenGroup_MatchFirst(TokenRule const(&)[Size]) -> TokenGroup_MatchFirst<Size>;

    template<typename T>
    struct FieldInfo
    {
    };

    template<typename Struct, typename FieldType>
    struct FieldInfo<FieldType Struct::*>
    {
        using Type = Struct;
        using Field = FieldType;
    };

    struct TokenRule_StoreSkip
    {
        static void Execute(SyntaxNode*, Token const&) noexcept
        {
        }
    };

    template<auto Member>
    struct TokenRule_StoreToken
    {
        using ParentType = typename FieldInfo<decltype(Member)>::Type;
        using FieldType = typename FieldInfo<decltype(Member)>::Field;

        static void Execute(SyntaxNode* node, Token const& token) noexcept
        {
            (static_cast<ParentType*>(node)->*Member) = token;
        }
    };

    template<auto Member>
    struct TokenRule_MergeToken
    {
        using ParentType = typename FieldInfo<decltype(Member)>::Type;
        using FieldType = typename FieldInfo<decltype(Member)>::Field;

        static void Execute(SyntaxNode* node, Token const& token) noexcept
        {
            Token& target = (static_cast<ParentType*>(node)->*Member);

            target.value = ice::String{
                target.value.data(),
                (token.value.data() - target.value.data()) + token.value.size()
            };
        }
    };

    template<auto Member, bool Value>
    struct TokenRule_StoreBool
    {
        using ParentType = typename FieldInfo<decltype(Member)>::Type;
        using FieldType = typename FieldInfo<decltype(Member)>::Field;

        static void Execute(SyntaxNode* node, Token const&) noexcept
        {
            (static_cast<ParentType*>(node)->*Member) = Value;
        }
    };

    template<typename T>
    concept TokenRule_StoreOp = requires(T) {
        { T::Execute } -> std::convertible_to<void(*)(SyntaxNode*, Token const&) noexcept>;
    };

    template<TokenType Type, TokenRule_StoreOp SuccessFn = TokenRule_StoreSkip>
    struct TokenRule_MatchType
    {
        using ExtendedFn = bool(Token const&) noexcept;

        static bool TypeCheck(Token const& token) noexcept
        {
            return token.type == Type;
        }

        ExtendedFn* func = &TypeCheck;

        bool optional = false;
        bool repeat = false;

        static auto rule_fn(
            void const* ud,
            ice::arctic::ParseState fail_state,
            ice::arctic::SyntaxNodeAllocator* alloc,
            ice::arctic::SyntaxNode* node,
            ice::arctic::Token& token,
            ice::arctic::Lexer& lexer
        ) noexcept -> ice::arctic::ParseState
        {
            ExtendedFn* fn = reinterpret_cast<ExtendedFn*>(ud);

            if (fn(token))
            {
                SuccessFn::Execute(node, token);
                token = lexer.next();
                return ParseState::Success;
            }
            return fail_state;
        }

        constexpr operator TokenRule() const noexcept
        {
            return TokenRule{
                .optional = optional,
                .repeat = repeat,
                .userdata = func,
                .func = rule_fn
            };
        }

        constexpr auto fail_with(ice::arctic::ParseState state) noexcept -> ice::arctic::TokenRule
        {
            TokenRule result = *this;
            result.fail_state = state;
            return result;
        }
    };

    inline constexpr void append_child(
        ice::arctic::SyntaxNode* parent,
        ice::arctic::SyntaxNode* child
    ) noexcept
    {
        if (parent->child == nullptr)
        {
            parent->child = child;
        }
        else
        {
            // Save the sub expression in the parent as a child.
            SyntaxNode* children = parent->child;
            while (children->sibling != nullptr)
            {
                children = children->sibling;
            }

            children->sibling = child;
        }
    }

    inline constexpr void append_sibling(
        ice::arctic::SyntaxNode* node,
        ice::arctic::SyntaxNode* sibling
    ) noexcept
    {
        // Save the sub expression in the parent as a child.
        SyntaxNode* children = node->sibling;
        while (children->sibling != nullptr)
        {
            children = children->sibling;
        }

        children->sibling = sibling;
    }

    inline constexpr void append_sibling_or_assign(
        ice::arctic::SyntaxNode*& node,
        ice::arctic::SyntaxNode* sibling
    ) noexcept
    {
        if (node == nullptr)
        {
            node = sibling;
        }
        else
        {
            // Save the sub expression in the parent as a child.
            SyntaxNode* previous_sibling = node;
            while (previous_sibling->sibling != nullptr)
            {
                previous_sibling = previous_sibling->sibling;
            }

            previous_sibling->sibling = sibling;
        }
    }

} // namespace ice::arctic
