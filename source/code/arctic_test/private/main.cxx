#include <filesystem>
#include <iostream>
#include <string_view>
#include <unordered_map>

#include <fmt/format.h>

#include <ice/arctic_word_matcher.hxx>
#include <ice/arctic_word_processor.hxx>
#include <ice/arctic_lexer.hxx>
#include <ice/arctic_parser.hxx>

#include <ice/arctic_bytecode.hxx>
#include <ice/arctic_vm.hxx>

#if defined _WIN32
#include <Windows.h>
#else
#include <sys/stat.h>

long GetFileSize(std::string filename)
{
    struct stat stat_buf;
    int rc = stat(filename.c_str(), &stat_buf);
    return rc == 0 ? stat_buf.st_size : -1;
}
#endif

static auto str_view(ice::arctic::Token const& tok) noexcept -> std::string_view
{
    return std::string_view{ (const char*)tok.value.data(), tok.value.size() };
}

struct MyVisitors : ice::arctic::SyntaxVisitorGroup<
    ice::arctic::SyntaxNode_TypeDef,
    ice::arctic::SyntaxNode_Function,
    ice::arctic::SyntaxNode_Struct,
    ice::arctic::SyntaxNode_ContextVariable
>
{
    void visit(ice::arctic::SyntaxNode_ContextVariable const* node) noexcept override
    {
        std::cout << "ctx: " << str_view(node->name) << " : " << str_view(node->type) << "\n";
    }

    void visit(ice::arctic::SyntaxNode_TypeDef const* node) noexcept override
    {
        std::string msg = fmt::format(
            "TypeDef: {} == {}, is_alias: {}",
            fmt::string_view{ (const char*)node->name.value.data(), node->name.value.size() },
            fmt::string_view{ (const char*)node->base_type.value.data(), node->base_type.value.size() },
            node->is_alias
        );

        std::cout << msg << "\n";
    }

    static void show_attribs(ice::arctic::SyntaxNode const* attrib) noexcept
    {
        while (attrib != nullptr)
        {
            ice::arctic::SyntaxNode_AnnotationAttribute const* attr = static_cast<ice::arctic::SyntaxNode_AnnotationAttribute const*>(attrib);

            std::cout << "  - [" << str_view(attr->name) << " : " << str_view(attr->value) << "]\n";
            attrib = attrib->sibling;
        }
    }

    static void show_annotations(ice::arctic::SyntaxNode const* node) noexcept
    {
        ice::arctic::SyntaxNode const* annotation = node->annotation;

        while (annotation != nullptr)
        {
            show_attribs(annotation->child);
            annotation = annotation->sibling;
        }
    }

    void visit(ice::arctic::SyntaxNode_Function const* fn) noexcept override
    {
        std::cout << "Function!\n";
        show_annotations(fn);

        ice::arctic::SyntaxNode* exp_sib = fn->sibling;
        while(exp_sib != nullptr)
        {
            std::cout << "Expression!\n";
            //show_exp_tree(exp_sib);
            exp_sib = exp_sib->sibling;
        }
    }

    void visit(ice::arctic::SyntaxNode_Struct const* node) noexcept override
    {
        std::string msg = fmt::format(
            "Struct: {}",
            fmt::string_view{ (const char*)node->name.value.data(), node->name.value.size() }
        );

        std::cout << msg << "\n";

        ice::arctic::SyntaxNode const* child = node->child;
        while (child != nullptr)
        {
            ice::arctic::SyntaxNode_StructMember const* member = static_cast<ice::arctic::SyntaxNode_StructMember const*>(child);

            std::string msg2 = fmt::format(
                "- {} : {}",
                fmt::string_view{ (const char*)member->name.value.data(), member->name.value.size() },
                fmt::string_view{ (const char*)member->type.value.data(), member->type.value.size() }
            );

            std::cout << msg2 << "\n";
            child = child->sibling;
        }
    }
};

template<typename CharT>
struct Buffer
{
    Buffer() noexcept
        :_buffer{ nullptr }
    {
    }

    Buffer(size_t buffer_size) noexcept
        : _buffer{ new CharT[buffer_size] }
    {
    }

    ~Buffer() noexcept
    {
        delete[] _buffer;
    }

    Buffer(Buffer&& other) noexcept
        : _buffer{ std::exchange(other._buffer, nullptr) }
    {
    }

    auto operator=(Buffer&& other) noexcept -> Buffer&
    {
        if (this != &other)
        {
            _buffer = std::exchange(other._buffer, nullptr);
        }
        return *this;
    }

    Buffer(Buffer const&) noexcept = delete;
    auto operator=(Buffer const&) noexcept -> Buffer & = delete;

    CharT* _buffer;
};

struct GLSL_Transpiler : ice::arctic::SyntaxVisitorGroup<
    ice::arctic::SyntaxNode_Struct,
    ice::arctic::SyntaxNode_ContextVariable,
    ice::arctic::SyntaxNode_Function
>
{
    auto symbol_replacer(std::u8string_view from) const noexcept -> std::u8string_view
    {
        if (from == u8"vec3f") return u8"vec3";
        if (from == u8"vec4f") return u8"vec4";
        if (from == u8"mtx4f") return u8"mat4";
        if (from == _vtx_outvar) return u8"gl_Position";
        return from;
    }

    static bool get_next_attrib(
        ice::arctic::SyntaxNode const* node,
        ice::arctic::SyntaxNode_AnnotationAttribute const*& attrib
    ) noexcept
    {
        if (node->annotation == nullptr)
        {
            return false;
        }

        if (attrib == nullptr)
        {
            attrib = static_cast<ice::arctic::SyntaxNode_AnnotationAttribute const*>(node->annotation->child);
        }
        else
        {
            attrib = static_cast<ice::arctic::SyntaxNode_AnnotationAttribute const*>(attrib->sibling);
        }

        return attrib != nullptr;
    }

    void transpile_expression(
        std::u8string& target,
        ice::arctic::SyntaxNode const* node,
        std::u8string indent = u8"  "
    ) const noexcept
    {
        using ice::arctic::SyntaxEntity;

        while (node != nullptr)
        {
            switch (node->entity)
            {
            case SyntaxEntity::DEF_ExplicitScope:
                target.append(indent);
                target.append(u8"{\n");
                transpile_expression(target, node->child, indent + u8"  ");
                target.append(indent);
                target.append(u8"}\n");
                break;
            case SyntaxEntity::DEF_Variable:
            {
                ice::arctic::SyntaxNode_Variable const* var = static_cast<ice::arctic::SyntaxNode_Variable const*>(node);
                target.append(indent);
                target.append(
                    symbol_replacer(
                        var->type.value
                    )
                );
                target.append(u8" ");
                target.append(var->name.value);

                if (var->child->entity == SyntaxEntity::EXP_Expression)
                {
                    transpile_expression(target, node->child, u8"");
                }
                else
                {
                    target.append(u8";\n");
                }
                break;
            }
            case SyntaxEntity::EXP_Expression:
                target.append(indent);
                transpile_expression(target, node->child, indent);
                target.append(u8";\n");
                break;
            case SyntaxEntity::EXP_UnaryOperation:
                target.append(static_cast<ice::arctic::SyntaxNode_ExpressionUnaryOperation const*>(node)->operation.value);
                transpile_expression(target, node->child, indent);
                break;
            case SyntaxEntity::EXP_BinaryOperation:
                target.append(u8" ");
                target.append(static_cast<ice::arctic::SyntaxNode_ExpressionBinaryOperation const*>(node)->operation.value);
                target.append(u8" ");
                break;
            case SyntaxEntity::EXP_Assignment:
                target.append(u8" =");
                transpile_expression(target, node->child, indent);
                break;
            case SyntaxEntity::EXP_ExplicitScope:
                target.append(u8"(");
                transpile_expression(target, node->child, indent);
                target.append(u8")");
                break;
            case SyntaxEntity::EXP_Call:
            {
                target.append(
                    symbol_replacer(
                        static_cast<ice::arctic::SyntaxNode_ExpressionCall const*>(node)->function.value
                    )
                );
                target.append(u8"(");
                ice::arctic::SyntaxNode const* call_arg = node->child;
                while (call_arg != nullptr)
                {
                    transpile_expression(target, call_arg->child, indent);

                    call_arg = call_arg->sibling;
                    if (call_arg != nullptr)
                    {
                        target.append(u8", ");
                    }
                }
                target.append(u8")");
                break;
            }
            case SyntaxEntity::EXP_CallArg:
                transpile_expression(target, node->child, indent);
                break;
            case SyntaxEntity::EXP_Value:
                target.append(
                    symbol_replacer(
                        static_cast<ice::arctic::SyntaxNode_ExpressionValue const*>(node)->value.value
                    )
                );
                transpile_expression(target, node->child, indent);
                break;
            case SyntaxEntity::EXP_GetMember:
                target.append(u8".");
                target.append(
                    symbol_replacer(
                        static_cast<ice::arctic::SyntaxNode_ExpressionGetMember const*>(node)->member.value
                    )
                );
                transpile_expression(target, node->child, indent);
                break;
            }

            node = node->sibling;
        }
    }

    GLSL_Transpiler() noexcept
    {
        _buffer.append(u8"#version 450\n\n");
    }
    ~GLSL_Transpiler() noexcept
    {
        // std::cout << (const char*)_buffer.c_str() << "\n";
    }

    void visit(ice::arctic::SyntaxNode_Struct const* node) noexcept override
    {
        ice::arctic::SyntaxNode_AnnotationAttribute const* attrib = nullptr;
        if (get_next_attrib(node, attrib) && attrib->name.value == u8"uniform")
        {
            ice::String uniform_var = attrib->value.value;

            _buffer.append(u8"\nlayout(std140");
            while(get_next_attrib(node, attrib))
            {
                _buffer.append(u8", ");
                _buffer.append(attrib->name.value);
                _buffer.append(u8"=");
                _buffer.append(attrib->value.value);
            }
            _buffer.append(u8") uniform ");
            _buffer.append(node->name.value);
            _buffer.append(u8"\n{\n");

            ice::arctic::SyntaxNode_StructMember const* member = static_cast<ice::arctic::SyntaxNode_StructMember const*>(node->child);
            while (member != nullptr)
            {
                _buffer.append(u8"  ");
                _buffer.append(member->type.value);
                _buffer.append(u8" ");
                _buffer.append(member->name.value);
                _buffer.append(u8";\n");

                member = static_cast<ice::arctic::SyntaxNode_StructMember const*>(member->sibling);
            }

            _buffer.append(u8"} ");
            _buffer.append(uniform_var);
            _buffer.append(u8";\n");
        }
    }

    void visit(ice::arctic::SyntaxNode_ContextVariable const* node) noexcept override
    {
        ice::arctic::SyntaxNode_AnnotationAttribute const* attrib = nullptr;
        if (get_next_attrib(node, attrib))
        {
            if (attrib->name.value == u8"in")
            {
                _buffer.append(u8"layout(location=");
                _buffer.append(attrib->value.value);
                _buffer.append(u8") in ");
                _buffer.append(symbol_replacer(node->type.value));
                _buffer.append(u8" ");
                _buffer.append(node->name.value);
                _buffer.append(u8";\n");
            }
            else if (attrib->name.value == u8"out")
            {
                if (attrib->value.value == u8"fragment")
                {
                    _vtx_outvar = node->name.value;
                }
                else
                {
                    _buffer.append(u8"layout(location=");
                    _buffer.append(attrib->value.value);
                    _buffer.append(u8") out ");
                    _buffer.append(symbol_replacer(node->type.value));
                    _buffer.append(u8" ");
                    _buffer.append(node->name.value);
                    _buffer.append(u8";\n");
                }
            }
        }

    }

    void visit(ice::arctic::SyntaxNode_Function const* node) noexcept override
    {
        ice::String return_type = node->result_type.value;
        //ice::arctic::SyntaxNode_AnnotationAttribute const* attrib = nullptr;
        //while(get_next_attrib(node, attrib))
        {
            //if (attrib->name.value == u8"vertex_shader")
            if (return_type == u8"VertexShader")
            {
                _vtx_outvar = node->name.value;
                return_type = u8"void";
            }
        }

        _buffer.append(u8"\n");
        _buffer.append(return_type);
        _buffer.append(u8" ");
        _buffer.append(node->name.value);
        _buffer.append(u8"(");

        ice::arctic::SyntaxNode_FunctionArgument const* args = static_cast<ice::arctic::SyntaxNode_FunctionArgument const*>(node->child);
        while (args != nullptr)
        {
            _buffer.append(args->type.value);
            _buffer.append(u8" ");
            _buffer.append(args->name.value);
            args = static_cast<ice::arctic::SyntaxNode_FunctionArgument const*>(args->sibling);

            if (args != nullptr)
            {
                _buffer.append(u8", ");
            }
        }

        _buffer.append(u8")\n");

        if (node->sibling != nullptr)
        {
            _buffer.append(u8"{\n");
            transpile_expression(_buffer, node->sibling->child);
            _buffer.append(u8"}\n");
        }
        else
        {
            _buffer.append(u8";\n");
        }

        _vtx_outvar = u8"";
    }

private:
    std::u8string _buffer;
    std::u8string_view _vtx_outvar;
};


struct HLSL_Transpiler : ice::arctic::SyntaxVisitorGroup<
    ice::arctic::SyntaxNode_Struct,
    ice::arctic::SyntaxNode_ContextVariable,
    ice::arctic::SyntaxNode_Function
>
{
    auto symbol_replacer(std::u8string_view from) const noexcept -> std::u8string_view
    {
        auto const ctx_tokens = _shader_ctx_tokens.begin();
        auto const ctx_tokens_end = _shader_ctx_tokens.end();

        if (from == u8"vec3f") return u8"float3";
        if (from == u8"vec4f") return u8"float4";
        if (from == u8"mtx4f") return u8"float4x4";

        auto const check = [&](TokenReplacement const& tok) noexcept -> bool
        {
            return from == tok.expected;
        };

        if (auto res = std::find_if(ctx_tokens, ctx_tokens_end, check); res != ctx_tokens_end)
        {
            return res->result;
        }

        if (from == _vtx_outvar) return u8"pix_in.out_pos";
        return from;
    }

    static bool get_next_attrib(
        ice::arctic::SyntaxNode const* node,
        ice::arctic::SyntaxNode_AnnotationAttribute const*& attrib
    ) noexcept
    {
        if (node->annotation == nullptr)
        {
            return false;
        }

        if (attrib == nullptr)
        {
            attrib = static_cast<ice::arctic::SyntaxNode_AnnotationAttribute const*>(node->annotation->child);
        }
        else
        {
            attrib = static_cast<ice::arctic::SyntaxNode_AnnotationAttribute const*>(attrib->sibling);
        }

        return attrib != nullptr;
    }

    void transpile_expression(
        std::u8string& target,
        ice::arctic::SyntaxNode const* node,
        std::u8string indent = u8"  "
    ) const noexcept
    {
        using ice::arctic::SyntaxEntity;

        while (node != nullptr)
        {
            switch (node->entity)
            {
            case SyntaxEntity::DEF_ExplicitScope:
                target.append(indent);
                target.append(u8"{\n");
                transpile_expression(target, node->child, indent + u8"  ");
                target.append(indent);
                target.append(u8"}\n");
                break;
            case SyntaxEntity::DEF_Variable:
            {
                ice::arctic::SyntaxNode_Variable const* var = static_cast<ice::arctic::SyntaxNode_Variable const*>(node);
                target.append(indent);
                target.append(
                    symbol_replacer(
                        var->type.value
                    )
                );
                target.append(u8" ");
                target.append(var->name.value);

                if (var->child->entity == SyntaxEntity::EXP_Expression)
                {
                    transpile_expression(target, node->child, u8"");
                }
                else
                {
                    target.append(u8";\n");
                }
                break;
            }
            case SyntaxEntity::EXP_Expression:
                target.append(indent);
                transpile_expression(target, node->child, indent);
                target.append(u8";\n");
                break;
            case SyntaxEntity::EXP_UnaryOperation:
                target.append(static_cast<ice::arctic::SyntaxNode_ExpressionUnaryOperation const*>(node)->operation.value);
                transpile_expression(target, node->child, indent);
                break;
            case SyntaxEntity::EXP_BinaryOperation:
                target.append(u8" ");
                target.append(static_cast<ice::arctic::SyntaxNode_ExpressionBinaryOperation const*>(node)->operation.value);
                target.append(u8" ");
                break;
            case SyntaxEntity::EXP_Assignment:
                target.append(u8" =");
                transpile_expression(target, node->child, indent);
                break;
            case SyntaxEntity::EXP_ExplicitScope:
                target.append(u8"(");
                transpile_expression(target, node->child, indent);
                target.append(u8")");
                break;
            case SyntaxEntity::EXP_Call:
            {
                target.append(
                    symbol_replacer(
                        static_cast<ice::arctic::SyntaxNode_ExpressionCall const*>(node)->function.value
                    )
                );
                target.append(u8"(");
                ice::arctic::SyntaxNode const* call_arg = node->child;
                while (call_arg != nullptr)
                {
                    transpile_expression(target, call_arg->child, indent);

                    call_arg = call_arg->sibling;
                    if (call_arg != nullptr)
                    {
                        target.append(u8", ");
                    }
                }
                target.append(u8")");
                break;
            }
            case SyntaxEntity::EXP_CallArg:
                transpile_expression(target, node->child, indent);
                break;
            case SyntaxEntity::EXP_Value:
                target.append(
                    symbol_replacer(
                        static_cast<ice::arctic::SyntaxNode_ExpressionValue const*>(node)->value.value
                    )
                );
                transpile_expression(target, node->child, indent);
                break;
            case SyntaxEntity::EXP_GetMember:
                target.append(u8".");
                target.append(
                    symbol_replacer(
                        static_cast<ice::arctic::SyntaxNode_ExpressionGetMember const*>(node)->member.value
                    )
                );
                transpile_expression(target, node->child, indent);
                break;
            }

            node = node->sibling;
        }
    }

    HLSL_Transpiler() noexcept
    {
    }
    ~HLSL_Transpiler() noexcept
    {
        // std::cout << "\nstruct VertexShaderInputs\n";
        // std::cout << "{\n";
        // std::cout << (const char*)_input_struct.c_str();
        // std::cout << "}\n";
        // std::cout << "\nstruct PixelShaderInputs\n";
        // std::cout << "{\n";
        // std::cout << (const char*)_output_struct.c_str();
        // std::cout << "  float4 out_pos : SV_Position;\n";
        // std::cout << "}\n";

        // std::cout << (const char*)_buffer.c_str() << "\n";
    }

    void visit(ice::arctic::SyntaxNode_Struct const* node) noexcept override
    {
        ice::arctic::SyntaxNode_AnnotationAttribute const* attrib = nullptr;
        if (get_next_attrib(node, attrib) && attrib->name.value == u8"uniform")
        {
            ice::String uniform_var = attrib->value.value;

            _shader_ctx_tokens.push_back({ uniform_var, std::u8string{ node->name.value } });

            _buffer.append(u8"\ncbuffer ");
            while (get_next_attrib(node, attrib) && attrib->name.value != u8"set")
            {

            }
            assert(attrib != nullptr);

            _buffer.append(node->name.value);
            _buffer.append(u8" : register(b");
            _buffer.append(attrib->value.value);
            _buffer.append(u8")");
            _buffer.append(u8"\n{\n");

            ice::arctic::SyntaxNode_StructMember const* member = static_cast<ice::arctic::SyntaxNode_StructMember const*>(node->child);
            while (member != nullptr)
            {
                _buffer.append(u8"  ");
                _buffer.append(
                    symbol_replacer(member->type.value)
                );
                _buffer.append(u8" ");
                _buffer.append(member->name.value);
                _buffer.append(u8";\n");

                member = static_cast<ice::arctic::SyntaxNode_StructMember const*>(member->sibling);
            }

            _buffer.append(u8"};\n");
        }
    }

    std::u8string _input_struct = u8"";
    std::u8string _output_struct = u8"";

    void visit(ice::arctic::SyntaxNode_ContextVariable const* node) noexcept override
    {
        ice::arctic::SyntaxNode_AnnotationAttribute const* attrib = nullptr;
        if (get_next_attrib(node, attrib))
        {
            if (attrib->name.value == u8"in")
            {
                _input_struct.append(u8"  ");
                _input_struct.append(symbol_replacer(node->type.value));
                _input_struct.append(u8" ");
                _input_struct.append(node->name.value);
                _input_struct.append(u8" : ");

                std::u8string appendix{ node->name.value.substr(3) };
                for (ice::utf8& c : appendix)
                {
                    if (c >= u8'a' && c <= u8'z')
                    {
                        c -= (u8'a' - u8'A');
                    }
                }

                _input_struct.append(appendix); // in_
                _input_struct.append(u8";\n");

                _shader_ctx_tokens.push_back(
                    {
                        node->name.value,
                        std::u8string{u8"vtx_in."} + std::u8string{node->name.value}
                    }
                );
            }
            else if (attrib->name.value == u8"out")
            {
                _output_struct.append(u8"  ");
                _output_struct.append(symbol_replacer(node->type.value));
                _output_struct.append(u8" ");
                _output_struct.append(node->name.value);
                _output_struct.append(u8" : ");

                std::u8string appendix{ node->name.value.substr(4) };
                for (ice::utf8& c : appendix)
                {
                    if (c >= u8'a' && c <= u8'z')
                    {
                        c -= (u8'a' - u8'A');
                    }
                }

                _output_struct.append(appendix); // out_
                _output_struct.append(u8";\n");

                _shader_ctx_tokens.push_back(
                    {
                        node->name.value,
                        std::u8string{u8"pix_in."} + std::u8string{node->name.value}
                    }
                );
            }
        }
    }

    void visit(ice::arctic::SyntaxNode_Function const* node) noexcept override
    {
        bool main_func = false;
        ice::String return_type = node->result_type.value;
        // ice::arctic::SyntaxNode_AnnotationAttribute const* attrib = nullptr;
        //while (get_next_attrib(node, attrib))
        {
            //if (attrib->name.value == u8"vertex_shader")
            if (return_type == u8"VertexShader")
            {
                _vtx_outvar = node->name.value;
                return_type = u8"PixelShaderInput";
                main_func = true;
            }
        }

        _buffer.append(u8"\n");
        _buffer.append(return_type);
        _buffer.append(u8" ");
        _buffer.append(node->name.value);
        _buffer.append(u8"(");

        ice::arctic::SyntaxNode_FunctionArgument const* args = static_cast<ice::arctic::SyntaxNode_FunctionArgument const*>(node->child);

        if (main_func == false)
        {
            while (args != nullptr)
            {
                _buffer.append(args->type.value);
                _buffer.append(u8" ");
                _buffer.append(args->name.value);
                args = static_cast<ice::arctic::SyntaxNode_FunctionArgument const*>(args->sibling);

                if (args != nullptr)
                {
                    _buffer.append(u8", ");
                }
            }
        }
        else
        {
            assert(args == nullptr);
            _buffer.append(u8"VertexShaderInputs vtx_in");
        }

        _buffer.append(u8")\n");

        if (node->sibling != nullptr)
        {
            _buffer.append(u8"{\n");

            if (main_func)
            {
                _buffer.append(u8"  PixelShaderInputs pix_in;\n\n");
            }

            ice::arctic::SyntaxNode const* exp = node->sibling->child;
            if (exp->entity == ice::arctic::SyntaxEntity::EXP_Value)
            {
                ice::arctic::SyntaxNode_ExpressionValue const* val = static_cast<ice::arctic::SyntaxNode_ExpressionValue const*>(exp);
                if (val->value.value == node->name.value)
                {
                    transpile_expression(_buffer, val->sibling);
                }
                else
                {
                    transpile_expression(_buffer, node->sibling->child);
                }
            }
            else
            {
                transpile_expression(_buffer, node->sibling->child);
            }

            if (main_func)
            {
                _buffer.append(u8"\n  return pix_in;\n");
            }

            _buffer.append(u8"}\n");
        }
        else
        {
            _buffer.append(u8";\n");
        }
    }

private:
    std::u8string _buffer;
    std::u8string_view _vtx_outvar;

    struct TokenReplacement
    {
        std::u8string_view expected;
        std::u8string result;
    };

    std::vector<TokenReplacement> _shader_ctx_tokens;
};

using ice::arctic::TokenType;
using ice::arctic::ByteCode;
using ice::arctic::SyntaxEntity;

struct Symbol
{
    std::vector<ice::arctic::ByteCode> bcrep;

    inline bool operator!=(Symbol const& other) const noexcept
    {
        auto const eqfn = [&]() noexcept
        {
            ice::u32 const size =  bcrep.size();
            bool is_same = true;
            for (ice::u32 idx = 0; is_same && idx < size; ++idx)
            {
                is_same &= (other.bcrep[idx].byte == bcrep[idx].byte);
            }
            return is_same;
        };

        return other.bcrep.size() != bcrep.size() || eqfn() == false;
    }

    inline bool operator==(Symbol const& other) const noexcept
    {
        return ((*this) != other) == false;
    }
};

namespace std
{
    template <>
    struct hash<Symbol>
    {
        using argument_type = Symbol;
        using result_type = size_t;

        result_type operator() (const argument_type& symbol) const
        {
            std::string_view const sv{ (char const*) &symbol.bcrep[0], symbol.bcrep.size() * 4 };
            return std::hash<std::string_view>{ }(sv);
        }
    };
}

auto create_symbol(std::u8string_view sv) noexcept -> Symbol
{
    std::vector<ice::arctic::ByteCode> result;

    auto it = sv.begin();
    auto const end = sv.end();

    ice::u32 byte = 0;
    ice::u32 byte_size = 0;
    while(it != end)
    {
        byte <<= 8;
        // std::cout << std::hex << ((ice::u32) *it);
        byte |= static_cast<ice::u8>(*it);
        byte_size += 1;

        if (byte_size == 4)
        {
            // std::cout << "\n";
            result.push_back(ByteCode::Value{ byte });
            byte = 0;
            byte_size = 0;
        }

        it += 1;
    }

    if (byte_size != 0)
    {
        result.push_back(ByteCode::Value{ byte });
    }
    // std::cout << "\n" << result.size() << "\n";
    return { result };
}

struct ByteCodeGenerator : public ice::arctic::SyntaxVisitorBase
{
    std::vector<ByteCode> scriptcode;
    std::vector<std::vector<ByteCode>> codeblocks;

    std::unordered_map<Symbol, ByteCode::Addr> variables;

    void generate_bytecode_expression(
        ice::arctic::SyntaxNode const* node,
        std::vector<ByteCode>& codes
    ) noexcept
    {
        assert(node->entity == SyntaxEntity::EXP_Expression);
        node = node->child;

        // ByteCode::Addr addr{ 0 };
        ice::arctic::SyntaxNode_ExpressionValue const* tar_var = nullptr;

        ice::arctic::SyntaxNode const* prev = nullptr;
        if(node->entity == SyntaxEntity::EXP_Value)
        {
            tar_var = static_cast<ice::arctic::SyntaxNode_ExpressionValue const*>(node);
            assert(tar_var->value.type == TokenType::CT_Symbol);
            prev = node;
            node = node->sibling;

            // auto var_sym = create_symbol(val->value.value);
            // assert(variables.contains(var_sym));

            // addr = variables.at(var_sym);
            // if (addr.loc == 0)
            // {
            //     code = ByteCode::OC_MOVR;
            //     ext = ByteCode::OE_REG;
            //     reg = ByteCode::OR_R0;
            // }
            // else
            // {
            //     code = ByteCode::OC_MOVA;
            //     ext = ByteCode::OE_ADDR;
            // }

            // assert(node->entity == SyntaxEntity::EXP_BinaryOperation);
            // prev = node;
            // node = node->sibling;
        }

        // ByteCode::OpCode code = ByteCode::OC_NOOP;
        // ByteCode::OpExt ext = ByteCode::OE_NONE;
        // ByteCode::OpReg reg = ByteCode::OR_VOID;

        while(node != nullptr)
        {
            if (node->entity == SyntaxEntity::EXP_BinaryOperation)
            {
                auto const* binop = static_cast<ice::arctic::SyntaxNode_ExpressionBinaryOperation const*>(node);
                auto const* src = node->sibling;

                assert(src != nullptr);

                ByteCode srcbc;
                ByteCode::OpExt src_ext = ByteCode::OE_NONE;
                if (src->entity == SyntaxEntity::EXP_Value)
                {
                    auto const srcv = static_cast<ice::arctic::SyntaxNode_ExpressionValue const*>(src);
                    if (srcv->value.type == TokenType::CT_Number)
                    {
                        ice::u32 const num_val = atol((char const*)srcv->value.value.data());
                        src_ext = ByteCode::OE_VALUE;
                        srcbc = ByteCode::Value{ num_val };
                    }
                }

                if (binop->operation.type == TokenType::OP_Assign)
                {
                    assert(prev != nullptr && prev->entity == SyntaxEntity::EXP_Value);
                    auto const destv = static_cast<ice::arctic::SyntaxNode_ExpressionValue const*>(prev);
                    assert(destv->value.type == TokenType::CT_Symbol);
                    auto const destv_sym = create_symbol(destv->value.value);

                    [[maybe_unused]]
                    ByteCode::Addr const addr = variables.at(destv_sym);

                    codes.push_back(ByteCode{ ByteCode::OC_MOVR, src_ext, ByteCode::OR_R0 });
                    codes.push_back(srcbc);
                    codes.push_back(ByteCode{ ByteCode::OC_ADD32, ByteCode::OE_VALUE, ByteCode::OR_R0 });
                    codes.push_back(ByteCode::Value{ 10 });
                    codes.push_back(ByteCode{ ByteCode::OC_MOVA, ByteCode::OE_REG, ByteCode::OR_VOID });// OR_R0
                    codes.push_back(addr);
                    codes.push_back(ByteCode::Value{ ByteCode::OR_R0 });
                }
                else if (binop->operation.type == TokenType::OP_Plus)
                {

                }
            }
            else
            {
                prev = node;
            }

            // if (node->entity == SyntaxEntity::EXP_Value)
            // {
            //     auto const* val = static_cast<ice::arctic::SyntaxNode_ExpressionValue const*>(node);
            //     if (val->value.type == TokenType::CT_Symbol)
            //     {
            //         auto var_sym = create_symbol(val->value.value);
            //         assert(variables.contains(var_sym));
            //     }
            //     else if (val->value.type == TokenType::CT_Number)
            //     {

            //     }
            // }

            node = node->sibling;
        }

        // codes.push_back(ByteCode{ ByteCode::OC_MOVR, ext, reg });
    }

    void visit(ice::arctic::SyntaxNode const* node) noexcept override
    {
        std::cout << "- " << to_string(node->entity) << "\n";

        if (node->entity == SyntaxEntity::DEF_Function)
        {
            ice::arctic::SyntaxNode_Function const* const fn = static_cast<ice::arctic::SyntaxNode_Function const*>(node);

            scriptcode.push_back(ByteCode::Op{ ByteCode::OC_META, ByteCode::OE_META_SYMBOL, ByteCode::OR_VOID });

            auto symbol = create_symbol(fn->name.value);
            {
                scriptcode.push_back(ByteCode::Value{ (ice::u32) symbol.bcrep.size() });
                scriptcode.insert(scriptcode.end(), symbol.bcrep.begin(), symbol.bcrep.end());
                variables.emplace(symbol, ByteCode::Addr{ 0 });
            }

            if (node->sibling && node->sibling->child)
            {
                std::vector<ByteCode> codes;

                ice::arctic::SyntaxNode const* exp = node->sibling->child;

                while(exp != nullptr)
                {
                    codes.push_back(ByteCode::Op{ ByteCode::OC_META, ByteCode::OpExt{ 1 /*ver*/ }, ByteCode::OR_VOID });
                    // codes.push_back(ByteCode::Op{ ByteCode::OC_META, ByteCode::OpExt{ 0 /*result_size*/ }, ByteCode::OR_VOID });
                    codes.push_back(ByteCode::Op{ ByteCode::OC_META, ByteCode::OpExt{ 4 /*stack_size*/ }, ByteCode::OR_VOID });
                    codes.push_back(ByteCode::Op{ ByteCode::OC_EXEC, ByteCode::OpExt{ 1 /*ver*/ }, ByteCode::OR_VOID });

                    if (exp->entity == SyntaxEntity::EXP_Expression)
                    {
                        generate_bytecode_expression(exp, codes);

                        // std::cout << to_string(exp->entity) << "\n";
                        // ice::arctic::SyntaxNode const* exp_child = exp->child;
                        // std::cout << to_string(exp_child->entity) << "\n";

                        // if (exp_child->entity == SyntaxEntity::EXP_Value)
                        // {
                        //     ice::arctic::SyntaxNode_ExpressionValue const* val = static_cast<ice::arctic::SyntaxNode_ExpressionValue const*>(exp_child);

                        //     auto value_symbol = create_symbol(val->value.value);
                        //     assert(val->value.type == TokenType::CT_Symbol);

                        //     if (value_symbol != symbol)
                        //     {
                        //         exp = exp->sibling;
                        //         continue;
                        //     }
                        //     // std::cout << () << std::endl;
                        // }

                        // exp_child = exp_child->sibling;
                        // std::cout << to_string(exp_child->entity) << "\n";
                        // assert(exp_child->entity == SyntaxEntity::EXP_BinaryOperation);

                        // exp_child = exp_child->sibling;
                        // std::cout << to_string(exp_child->entity) << "\n";
                        // assert(exp_child->entity == SyntaxEntity::EXP_Value);

                        // ice::arctic::SyntaxNode_ExpressionValue const* val = static_cast<ice::arctic::SyntaxNode_ExpressionValue const*>(exp_child);
                        // assert(val->value.type == TokenType::CT_Number);

                        // codes.push_back(ByteCode::Op{ ByteCode::OC_MOVR, ByteCode::OE_VALUE, ByteCode::OR_R0 });

                        // ice::u32 const num_val = atol((char const*)val->value.value.data());
                        // codes.push_back(ByteCode::Value{ num_val });
                    }

                    exp = exp->sibling;
                }

                codes.push_back(ByteCode::Op{ ByteCode::OC_END });


                // codes.push_back(ByteCode::Op{ ByteCode::OC_META, ByteCode::OpExt{ 1 /*ver*/ }, ByteCode::OR_VOID });
                // codes.push_back(ByteCode::Op{ ByteCode::OC_META, ByteCode::OpExt{ 64 /*stack_size*/ }, ByteCode::OR_VOID });
                // codes.push_back(ByteCode::Op{ ByteCode::OC_EXEC, ByteCode::OpExt{ 1 /*ver*/ }, ByteCode::OR_VOID });
                // codes.push_back(ByteCode::Op{ ByteCode::OC_ADD32, ByteCode::OE_VALUE, ByteCode::OR_R0 });
                // codes.push_back(ByteCode::Value{ 42 });
                // codes.push_back(ByteCode::Op{ ByteCode::OC_ADD32, ByteCode::OE_VALUE, ByteCode::OR_R1 });
                // codes.push_back(ByteCode::Value{ 42 });
                // codes.push_back(ByteCode::Op{ ByteCode::OC_ADD32, ByteCode::OE_REG, ByteCode::OR_R0 });
                // codes.push_back(ByteCode::Value{ ByteCode::OR_R1 });
                // codes.push_back(ByteCode::Op{ ByteCode::OC_END });

                codeblocks.push_back(std::move(codes));
            }

        }
    }
};



class BasicVirtualMachine : public ice::arctic::VirtualMachine
{
public:
    void execute(
        std::vector<ice::arctic::ByteCode> const& bytecode,
        ice::arctic::ExecutionContext const& context,
        ice::arctic::ExecutionState& state
    ) noexcept override
    {
        registers[0] = 0;

        auto it = bytecode.begin();
        auto const end = bytecode.end();

        // Skip metadata
        assert(it->op.operation == ByteCode::OC_META && it->op.extension == 1);
        it += 1;
        assert(it->op.operation == ByteCode::OC_META && it->op.extension != 0);
        char* memory = new char[it->op.extension];
        it += 1;



        assert(it->op.operation == ByteCode::OC_EXEC);
        it += 1;

        while(it != end)
        {
            std::cout << std::hex << (ice::u32) it->byte << '|';

            if (it->op.operation == ByteCode::OC_ADD32)
            {
                if (it->op.extension == ByteCode::OE_VALUE)
                {
                    ice::u32 const reg = it->op.registr;
                    it += 1;
                    registers[reg] += it->byte;
                }
                else if (it->op.extension == ByteCode::OE_REG)
                {
                    ice::u32 const reg = it->op.registr;
                    it += 1;
                    registers[reg] += registers[it->byte];
                }
            }
            else if (it->op.operation == ByteCode::OC_MOVA)
            {
                ByteCode::OpExt const ext = it->op.extension;
                it += 1;

                void* const mem = memory + it->byte;
                it += 1;

                if (ext == ByteCode::OE_VALUE)
                {
                    *((ice::u32*)mem) = it->byte;
                }
                else if (ext == ByteCode::OE_REG)
                {
                    *((ice::u32*)mem) = registers[it->byte];
                }
            }
            else if (it->op.operation == ByteCode::OC_MOVR)
            {
                if (it->op.extension == ByteCode::OE_VALUE)
                {
                    ice::u32 const reg = it->op.registr;
                    it += 1;
                    registers[reg] = it->byte;
                }
                else if (it->op.extension == ByteCode::OE_REG)
                {
                    ice::u32 const reg = it->op.registr;
                    it += 1;
                    registers[reg] = registers[it->byte];
                }
            }

            it += 1;
        }

        assert((it - 1)->op.operation == ByteCode::OC_END);

        std::cout << "\n" << std::dec << *((ice::u32*)memory) << '\n';
        delete[] memory;
    }

    ice::u32 registers[4]{ };
};

auto main(int argc, char** argv) -> int
{
    if (argc < 2)
    {
        return -1;
    }


    Buffer<ice::utf8> contents;
#if defined _WIN32
    std::filesystem::path script_path = std::filesystem::absolute(argv[1]);

    HANDLE file_handle = CreateFile(script_path.c_str(), GENERIC_READ, 0, 0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file_handle != INVALID_HANDLE_VALUE)
    {
        DWORD const file_size = GetFileSize(file_handle, nullptr);
        if (file_size != 0)
        {
            contents = Buffer<ice::utf8>{ file_size + 10 };
            memset(contents._buffer, 0, file_size + 10);

            DWORD bytes_read;
            BOOL const read_result = ReadFile(file_handle, contents._buffer, file_size, &bytes_read, nullptr);
        }

        CloseHandle(file_handle);
    }
#else
    FILE* f = fopen(argv[1], "rb+");
    if (f != 0)
    {
        size_t const file_size = GetFileSize(argv[1]);

        if (file_size != 0)
        {
            contents = Buffer<ice::utf8>{ file_size + 10u };
            memset(contents._buffer, 0, file_size + 10u);

            fread(contents._buffer, sizeof(ice::utf8), file_size, f);
        }
        fclose(f);
    }
#endif

    ice::arctic::WordMatcher matcher{ };
    ice::arctic::initialize_ascii_matcher(&matcher);
    {
        ice::arctic::WordProcessor processor = ice::arctic::create_word_processor(
            contents._buffer,
            &matcher
        );

        ice::arctic::Lexer lexer = ice::arctic::create_lexer(
            std::move(processor)
        );


        // GLSL_Transpiler my_glsl_gen{ };
        // HLSL_Transpiler my_hlsl_gen{ };

        ByteCodeGenerator bcg;
        BasicVirtualMachine bvm;

        ice::arctic::Parser parser;
        parser.add_visitor(bcg);
        // parser.add_visitor(my_glsl_gen);
        // parser.add_visitor(my_hlsl_gen);

        auto t1 = std::chrono::high_resolution_clock::now();

        parser.parse(lexer);

        auto t2 = std::chrono::high_resolution_clock::now();

        [[maybe_unused]]
        ice::arctic::ExecutionContext global_context;
        [[maybe_unused]]
        ice::arctic::ExecutionState global_state;

        auto it = bcg.scriptcode.begin();
        auto const end = bcg.scriptcode.end();

        while(it != end)
        {
            if (it->op.extension == ByteCode::OE_META_SYMBOL)
            {
                it += 1;
                std::cout << "SYM 0";
                ice::u32 const size = it->byte;

                for(ice::u32 idx = 0; idx < size; ++idx)
                {
                    it += 1;
                    std::cout << std::hex << "|" << it->byte;
                }
                std::cout << "\n";
            }

            it += 1;
        }

        for ([[maybe_unused]] auto const& block : bcg.codeblocks)
        {
            std::cout << "Block: " << "\n";
            bvm.execute(block, global_context, global_state);
            std::cout << "\n";
        }

        std::cout << std::dec;
        std::cout << "Total time: " << std::chrono::duration_cast<std::chrono::nanoseconds>(t2 - t1).count() << "ns" << std::endl;
    }

    ice::arctic::shutdown_matcher(&matcher);
    return 0;
}
