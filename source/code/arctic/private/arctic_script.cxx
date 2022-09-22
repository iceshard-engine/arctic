#include <ice/arctic_script.hxx>
#include <ice/arctic_parser.hxx>
#include <ice/arctic_syntax_node.hxx>
#include <ice/arctic_syntax_visitor.hxx>
#include <unordered_set>

namespace ice::arctic
{

    class LoadedScript final : public ice::arctic::Script
    {
    public:
        LoadedScript(ice::arctic::Lexer& lexer) noexcept;
        LoadedScript(ice::arctic::Lexer& lexer, ice::arctic::SyntaxNodeAllocator& parent_alloc) noexcept;
        ~LoadedScript() noexcept override;

        auto count_functions() const noexcept -> ice::u32 override { return (ice::u32) _def_functions.size(); }

    public:
        std::vector<ice::arctic::SyntaxNode_Function const*> _def_functions;

    private:
        class AllocationTracker;
        class SyntaxNodeTracker;

        std::unique_ptr<ice::arctic::SyntaxNodeAllocator> _default_parent_alloc;

        AllocationTracker* _alloc_tracker;
        SyntaxNodeTracker* _nodes_tracker;
    };

    class LoadedScript::AllocationTracker final : public ice::arctic::SyntaxNodeAllocator
    {
    public:
        AllocationTracker(ice::arctic::SyntaxNodeAllocator& parent_alloc) noexcept
            : _parent_alloc{ parent_alloc }
        {
        }

        ~AllocationTracker() noexcept override
        {
            for (void* allocated_ptr : _allocations)
            {
                _parent_alloc.deallocate(allocated_ptr);
            }
        }

        auto allocate(ice::u64 size, ice::u64 align) noexcept -> void* override
        {
            void* result = _parent_alloc.allocate(size, align);
            _allocations.insert(result);
            return result;
        }

        void deallocate(void* ptr) noexcept override
        {
            _allocations.erase(ptr);
            _parent_alloc.deallocate(ptr);
        }

    private:
        ice::arctic::SyntaxNodeAllocator& _parent_alloc;
        std::unordered_set<void*> _allocations;
    };

    class LoadedScript::SyntaxNodeTracker final : public ice::arctic::SyntaxVisitorBase
    {
    public:
        SyntaxNodeTracker(LoadedScript& script) noexcept
            : _script{ script }
        {
        }

        void visit(ice::arctic::SyntaxNode const* node) noexcept override
        {
            if (node->entity == SyntaxEntity::DEF_Function)
            {
                _script._def_functions.push_back(static_cast<ice::arctic::SyntaxNode_Function const*>(node));
            }
        }

    private:
        LoadedScript& _script;
    };

    auto load_script(
        ice::arctic::Lexer& lexer,
        ice::arctic::SyntaxNodeAllocator* parent_allocator /*= nullptr*/
    ) noexcept -> std::unique_ptr<ice::arctic::Script>
    {
        if (parent_allocator == nullptr)
        {
            return std::make_unique<LoadedScript>(lexer);
        }
        else
        {
            return std::make_unique<LoadedScript>(lexer, *parent_allocator);
        }
    }

    LoadedScript::LoadedScript(
        ice::arctic::Lexer& lexer
    ) noexcept
        : _default_parent_alloc{ ice::arctic::create_simple_allocator() }
        , _alloc_tracker{ new AllocationTracker{ *_default_parent_alloc.get() } }
        , _nodes_tracker{ new SyntaxNodeTracker{ *this } }
    {
        auto parser = ice::arctic::create_default_parser();
        ice::arctic::SyntaxVisitorBase* visitors[] { _nodes_tracker };
        parser->parse(lexer, *_alloc_tracker, visitors);
    }

    LoadedScript::LoadedScript(
        ice::arctic::Lexer& lexer,
        ice::arctic::SyntaxNodeAllocator& parent_allocator
    ) noexcept
        : _default_parent_alloc{ nullptr }
        , _alloc_tracker{ new AllocationTracker{ parent_allocator } }
        , _nodes_tracker{ new SyntaxNodeTracker{ *this } }
    {
        auto parser = ice::arctic::create_default_parser();
        ice::arctic::SyntaxVisitorBase* visitors[] { _nodes_tracker };
        parser->parse(lexer, *_alloc_tracker, visitors);
    }

    LoadedScript::~LoadedScript() noexcept
    {
        delete _alloc_tracker;
        delete _nodes_tracker;
    }

} // namespace ice::arctic
