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
#include <ice/arctic_script.hxx>

#include <string>
#include <charconv>

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

auto create_symbol(ice::arctic::SyntaxNode const* node) noexcept -> Symbol
{
    if (node->entity == SyntaxEntity::EXP_Value)
    {
        auto const vnode = static_cast<ice::arctic::SyntaxNode_ExpressionValue const*>(node);
        if (vnode->value.type == TokenType::CT_Symbol)
        {
            return create_symbol(vnode->value.value);
        }
    }

    return { };
}

auto to_number_bc(std::u8string_view sv, int base = 10, bool bit64 = false, bool fpoint = false, bool sign = true) noexcept -> ByteCode
{
    assert(bit64 == false);

    std::string_view const num_sv{ (char const*) sv.data(), sv.size() };

    ByteCode result;
    if (fpoint)
    {
        float res;
        std::from_chars(num_sv.begin(), num_sv.end(), res);
        result.byte = *((ice::u32*)(&res));
    }
    else if (sign)
    {
        ice::i32 res;
        std::from_chars(num_sv.begin(), num_sv.end(), res);
        result.byte = *((ice::u32*)(&res));
    }
    else
    {
        ice::u32 res;
        std::from_chars(num_sv.begin(), num_sv.end(), res);
        result.byte = res;
    }

    return result;
}

auto get_operation_level(ice::arctic::SyntaxNode_ExpressionBinaryOperation const* binop, bool unary = false) noexcept -> ice::u32
{
    switch(binop->operation.type)
    {
    case TokenType::OP_Assign:
        return 0;
    case TokenType::OP_Plus:
        return 1;
    case TokenType::OP_Minus:
        return 1 + (unary * 2);
    case TokenType::OP_Mul:
        return 2;
    case TokenType::OP_Div:
        return 2;
    }
    return 0;
}

struct ByteCodeGenerator : public ice::arctic::SyntaxVisitorBase
{
    // ice::u32 bytecode_offset = 0;

    // std::vector<ByteCode> scriptcode;
    // std::vector<std::vector<ByteCode>> codeblocks;
    std::vector<ice::arctic::SyntaxNode const*> nodes;

    void release_child_nodes(
        ice::arctic::SyntaxNodeAllocator& alloc,
        ice::arctic::SyntaxNode const* node
    ) noexcept
    {
        if (node == nullptr) return;

        auto const* child_it = node->child;
        while (child_it != nullptr)
        {
            release_child_nodes(alloc, child_it);

            auto const* next = child_it->sibling;
            alloc.destroy((ice::arctic::SyntaxNode*)child_it);
            child_it = next;
        }
    };

    void release_nodes(ice::arctic::SyntaxNodeAllocator& alloc) noexcept
    {
        fmt::print("Collected nodes: {}\n", nodes.size());
        for (auto const* node : nodes)
        {
            auto const* sib = node;
            while(sib != nullptr)
            {
                release_child_nodes(alloc, sib);

                auto const* next = sib->sibling;
                alloc.destroy((ice::arctic::SyntaxNode*)sib);
                sib = next;
            }
        }
    }

    std::unordered_map<Symbol, std::vector<ByteCode>> functions;
    std::unordered_map<Symbol, ByteCode::Addr> function_idx;
    std::unordered_map<Symbol, ByteCode::Addr> variables;

    void generate_bytecode_expression_arg_reg(
        ice::arctic::SyntaxNode const* node,
        ice::arctic::ByteCode::OpReg reg,
        std::vector<ByteCode>& codes
    ) noexcept
    {
        if (node->entity == SyntaxEntity::EXP_Value)
        {
            auto const vnode = static_cast<ice::arctic::SyntaxNode_ExpressionValue const*>(node);
            if (vnode->value.type == TokenType::CT_Symbol)
            {
                auto symbol = create_symbol(vnode->value.value);
                if(variables.contains(symbol))
                {
                    fmt::print("VAR: {} == {}\n", dbg_to_string(vnode), variables.at(symbol).loc);
                    // R[X] = [OR_PTR]
                    codes.push_back(ByteCode::Op{ ByteCode::OC_MOVR, ByteCode::OE_VALUE, ByteCode::OR_PTR });
                    codes.push_back(variables.at(symbol));
                    codes.push_back(ByteCode::Op{ ByteCode::OC_MOVR, ByteCode::OE_ADDR, reg });
                }
            }
            else if (vnode->value.type == TokenType::CT_Number)
            {
                // R[X] = <number>
                codes.push_back(ByteCode::Op{ ByteCode::OC_MOVR, ByteCode::OE_VALUE, reg });
                codes.push_back(to_number_bc(vnode->value.value));
            }
        }
        else if (node->entity == SyntaxEntity::EXP_Call)
        {
            auto const callnode = static_cast<ice::arctic::SyntaxNode_ExpressionCall const*>(node);
            auto symbol = create_symbol(callnode->function.value);
            if(function_idx.contains(symbol))
            {
                fmt::print("FUN: {} == {}\n", dbg_to_string(callnode), function_idx.at(symbol).loc);
                // R[X] = [OR_PTR]
                codes.push_back(ByteCode::Op{ ByteCode::OC_MOVR, ByteCode::OE_FUNC, ByteCode::OR_TP });
                codes.push_back(function_idx.at(symbol));
                codes.push_back(ByteCode::Op{ ByteCode::OC_CALL0_VOID, ByteCode::OE_NONE, ByteCode::OR_VOID });
            }
        }
        // else if (node->entity == SyntaxEntity::EXP_UnaryOperation)
        // {
        //     assert(false);
        // }
        else
        {
            // std::cout << (uint32_t)node->entity << std::endl;
            assert(false);
        }
    }

    // void generate_bytecode_expression_assign_op(
    //     ice::arctic::SyntaxNode const* node,
    //     ice::arctic::ByteCode::OpExt extension,
    //     // ice::arctic::ByteCode::Addr address,
    //     std::vector<ByteCode>& codes
    // ) noexcept
    // {
    //     assert(extension != ByteCode::OE_64BIT);
    //     assert(node->entity == SyntaxEntity::EXP_BinaryOperation);
    //     auto const* binop = static_cast<ice::arctic::SyntaxNode_ExpressionBinaryOperation const*>(node);
    //     assert(binop->operation.type == TokenType::OP_Assign);

    //     if (extension == ByteCode::OE_REG)
    //     {
    //         // [OR_PTR] = R0
    //         codes.push_back(ByteCode::Op{ ByteCode::OC_MOVA, ByteCode::OE_REG, ByteCode::OR_R0 });
    //     }
    // }

    void generate_bytecode_expression_native_op(
        ice::arctic::SyntaxNode const* node,
        ice::arctic::ByteCode::OpExt extension,
        std::vector<ByteCode>& codes
    ) noexcept
    {
        if (node->entity == SyntaxEntity::EXP_BinaryOperation)
        {
            auto const* binop = static_cast<ice::arctic::SyntaxNode_ExpressionBinaryOperation const*>(node);
            if (binop->operation.type == TokenType::OP_Plus)
            {
                // R0 = R0 + R1
                codes.push_back(ByteCode::Op{ ByteCode::OC_ADD32, ByteCode::OE_REG, ByteCode::OR_R1 });
            }
            else if (binop->operation.type == TokenType::OP_Minus)
            {
                // R0 = R0 - R1
                codes.push_back(ByteCode::Op{ ByteCode::OC_SUB32, ByteCode::OE_REG, ByteCode::OR_R1 });
            }
            else if (binop->operation.type == TokenType::OP_Mul)
            {
                // R0 = R0 * R1
                codes.push_back(ByteCode::Op{ ByteCode::OC_MUL32, ByteCode::OE_REG, ByteCode::OR_R1 });
            }
            else if (binop->operation.type == TokenType::OP_Div)
            {
                // R0 = R0 / R1
                codes.push_back(ByteCode::Op{ ByteCode::OC_DIV32, ByteCode::OE_REG, ByteCode::OR_R1 });
            }
            // else if (binop->operation.type == TokenType::OP_Assign)
            // {
            //     // OR_PTR = R0
            //     // [OR_PTR] = R1
            //     // R0 = R1
            //     // codes.push_back(ByteCode::Op{ ByteCode::OC_DIV32, ByteCode::OE_REG, ByteCode::OR_R1 });
            //     codes.push_back(ByteCode::Op{ ByteCode::OC_MOVR, ByteCode::OE_REG, ByteCode::OR_PTR });
            //     codes.push_back(ByteCode::Value{ ByteCode::OR_R0 });
            //     codes.push_back(ByteCode::Op{ ByteCode::OC_MOVA, ByteCode::OE_REG, ByteCode::OR_R1 });
            // }
        }
    }

    template<bool LeftSide>
    void generate_bytecode_from_span(
        ice::arctic::SyntaxNode const* from,
        ice::arctic::SyntaxNode const* to,
        std::vector<ByteCode>& codes,
        ice::u32 last_level
    ) noexcept
    {
        if constexpr (LeftSide)
        {
            if (from->entity == SyntaxEntity::EXP_Value)
            {
                // fmt::print("{} {:<{}}{}\n", LeftSide ? "<$" : "$>", "|", last_level * 2, dbg_to_string(from));
                generate_bytecode_expression_arg_reg(from, ByteCode::OR_R0, codes);
                from = from->sibling;
            }

            while(from != to)
            {
                if (from->entity == SyntaxEntity::EXP_Value)
                {
                    assert(false);
                }
                else if (from->entity == SyntaxEntity::EXP_Call)
                {
                    assert(false);
                    //generate_bytecode_expression_l0(exp, codes, { 0 });
                }
                else if (from->entity == SyntaxEntity::EXP_BinaryOperation)
                {
                    // fmt::print("{} {:<{}}{}\n", LeftSide ? "<*" : "*>", "|", last_level * 2, dbg_to_string(from));

                    if (from->sibling != to)
                    {
                        // assert(from->sibling != to);
                        generate_bytecode_expression_arg_reg(from->sibling, ByteCode::OR_R1, codes);
                    }

                    generate_bytecode_expression_native_op(from, ByteCode::OE_NONE, codes);

                    if (from->sibling != to)
                    {
                        from = from->sibling;
                        // fmt::print("{} {:<{}}{}\n", LeftSide ? "<*" : "*>", "|", last_level * 2, dbg_to_string(from));
                    }
                }

                // fmt::print("{} {:<{}}{}\n", LeftSide ? "<*" : "*>", "|", last_level * 2, dbg_to_string(from));
                from = from->sibling;
            }
        }
        else
        {
            if (from->entity == SyntaxEntity::EXP_Value)
            {
                // fmt::print("{} {:<{}}{}\n", LeftSide ? "<*" : "*>", "|", last_level * 2, dbg_to_string(from));
                generate_bytecode_expression_arg_reg(from, ByteCode::OR_R0, codes);
                from = from->sibling;
            }

            while(from != to)
            {
                if (from->entity == SyntaxEntity::EXP_Value)
                {
                    assert(false);
                    //generate_bytecode_expression_arg_reg(from, ByteCode::OR_R1, codes);
                }
                else if (from->entity == SyntaxEntity::EXP_Call)
                {
                    // assert(false);
                    generate_bytecode_expression_arg_reg(from, ByteCode::OR_R1, codes);
                }
                else if (from->entity == SyntaxEntity::EXP_BinaryOperation)
                {
                    // fmt::print("{} {:<{}}{}\n", LeftSide ? "<*" : "*>", "|", last_level * 2, dbg_to_string(from));

                    if (from->sibling != to)
                    {
                        assert(from->sibling != to);
                        generate_bytecode_expression_arg_reg(from->sibling, ByteCode::OR_R1, codes);
                    }

                    generate_bytecode_expression_native_op(from, ByteCode::OE_NONE, codes);

                    if (from->sibling != to)
                    {
                        from = from->sibling;
                        // fmt::print("{} {:<{}}{}\n", LeftSide ? "<*" : "*>", "|", last_level * 2, dbg_to_string(from));
                    }
                }

                // fmt::print("{} {:<{}}{}\n", LeftSide ? "<*" : "*>", "|", last_level * 2, dbg_to_string(from));
                from = from->sibling;
            }
        }
    }

    auto traverse_expression_tree(
        ice::arctic::SyntaxNode const* node,
        std::vector<ByteCode>& codes,
        ice::u32 last_level
    ) noexcept -> ice::arctic::SyntaxNode const*
    {
        ice::arctic::SyntaxNode const* first = node;
        while(node != nullptr)
        {
            ice::arctic::SyntaxNode const* next = node->sibling;

            if (next != nullptr && next->entity == SyntaxEntity::EXP_BinaryOperation)
            {
                auto const* binop = static_cast<ice::arctic::SyntaxNode_ExpressionBinaryOperation const*>(next);

                ice::u32 const new_level = get_operation_level(binop);
                if (new_level > last_level)
                {
                    // We enter this if we have already generated bytecode resulting in a value on R0
                    if (node != first)
                    {
                        // std::cout << "YUCK!\n";

                        auto it = first;
                        while(it != node && it->sibling != node)
                        {
                            it = it->sibling;
                        }

                        // (1) We calculate anything that is still remaining and aligned with the current prio level
                        if (first != it)
                        {
                            generate_bytecode_from_span<false>(first, it, codes, last_level);
                            first = it;
                        }

                        // (2) Move anything we have calculated until now, onto the stack. (if we have just a single operator as the first node)
                        if (first->entity == SyntaxEntity::EXP_BinaryOperation)
                        {
                            // std::cout << "BOO!\n";
                            codes.push_back(ByteCode::Op{ ByteCode::OC_MOVS, ByteCode::OE_REG, ByteCode::OR_R0 });
                            codes.push_back(ByteCode::Op{ ByteCode::OC_ADD32, ByteCode::OE_VALUE_SP, ByteCode::OR_R4 });
                        }
                    }

                    // std::cout << "ENTER : " << dbg_to_string(node) << std::endl;

                    // (3) We execute the next sub-tree
                    next = traverse_expression_tree(node, codes, new_level);

                    // (4) We move the result to the right side and load the stack safed value on the left side again.
                    if (node != first)
                    {
                        // std::cout << "YUCK 2!\n";

                        // (5) if the first is an operator then we need to move values around.
                        if (first->entity == SyntaxEntity::EXP_BinaryOperation)
                        {
                            // (6) Move R0 to R1 (we just calculated the right side)
                            codes.push_back(ByteCode::Op{ ByteCode::OC_MOVR, ByteCode::OE_REG, ByteCode::OR_R1 });
                            codes.push_back(ByteCode::Value{ ByteCode::OR_R0 });

                            // (7) Load the value from the stack into R0
                            codes.push_back(ByteCode::Op{ ByteCode::OC_SUB32, ByteCode::OE_VALUE_SP, ByteCode::OR_R4 });
                            codes.push_back(ByteCode::Op{ ByteCode::OC_MOVR, ByteCode::OE_STACK, ByteCode::OR_R0 });
                        }

                        // (8) Execute the code.
                        generate_bytecode_from_span<true>(first, node, codes, last_level);
                    }
                    first = next;
                }
                else if (new_level < last_level)
                {
                    node = next;
                    break;
                }
            }

            node = next;
        }

        if (first != nullptr)
        {
            generate_bytecode_from_span<false>(first, node, codes, last_level);
        }
        // std::cout << "EXIT" << std::endl;
        return node;
    }

    void generate_bytecode_expression_l0(
        ice::arctic::SyntaxNode const* node,
        std::vector<ByteCode>& codes,
        ByteCode::Addr address,
        bool assign_result = false
    ) noexcept
    {
        assert(node->entity == SyntaxEntity::EXP_Expression);
        node = node->child;

        // First arg (LHS) is done outside of the loop
        ice::arctic::SyntaxNode* first_node = nullptr;
        if (node->entity == SyntaxEntity::EXP_BinaryOperation)
        {
            auto const* binop = static_cast<ice::arctic::SyntaxNode_ExpressionBinaryOperation const*>(node);
            assign_result = binop->operation.type == TokenType::OP_Assign;

            first_node = node->sibling;
        }
        else if (node->entity == SyntaxEntity::EXP_Value && node->sibling->entity == SyntaxEntity::EXP_BinaryOperation)
        {
            auto const* binop = static_cast<ice::arctic::SyntaxNode_ExpressionBinaryOperation const*>(node->sibling);
            assign_result = binop->operation.type == TokenType::OP_Assign;

            first_node = node->sibling->sibling;
        }

        assert(first_node != nullptr);

        if (assign_result)
        {
            traverse_expression_tree(first_node, codes, 1);

            if (node->entity == SyntaxEntity::EXP_Value)
            {
                auto symbol = create_symbol(node);
                codes.push_back(ByteCode::Op{ ByteCode::OC_MOVR, ByteCode::OE_VALUE, ByteCode::OR_PTR });
                codes.push_back(variables.at(symbol));
            }
            else
            {
                assert(address.loc != 0);
                codes.push_back(ByteCode::Op{ ByteCode::OC_MOVR, ByteCode::OE_VALUE, ByteCode::OR_PTR });
                codes.push_back(address);
            }
            codes.push_back(ByteCode::Op{ ByteCode::OC_MOVA, ByteCode::OE_REG, ByteCode::OR_R0 });
        }
        else
        {
            traverse_expression_tree(node, codes, 1);
        }
    }

    void visit(ice::arctic::SyntaxNode const* node) noexcept override
    {
        nodes.push_back(node);
        std::cout << "- " << to_string(node->entity) << "\n";

        if (node->entity == SyntaxEntity::DEF_Function)
        {
            ice::arctic::SyntaxNode_Function const* const fn = static_cast<ice::arctic::SyntaxNode_Function const*>(node);

            // scriptcode.push_back(ByteCode::Op{ ByteCode::OC_META, ByteCode::OE_META_SYMBOL, ByteCode::OR_VOID });

            variables.clear();
            auto fn_symbol = create_symbol(fn->name.value);
            {
                // scriptcode.push_back(ByteCode::Value{ (ice::u32) fn_symbol.bcrep.size() });
                // scriptcode.insert(scriptcode.end(), fn_symbol.bcrep.begin(), fn_symbol.bcrep.end());
                variables.emplace(fn_symbol, ByteCode::Addr{ 0 });
            }

            if (node->sibling && node->sibling->child)
            {
                std::vector<ByteCode> codes;

                ice::arctic::SyntaxNode const* exp = node->sibling->child;

                codes.push_back(ByteCode::Op{ ByteCode::OC_META, ByteCode::OpExt{ 1 /*ver*/ }, ByteCode::OR_VOID });
                codes.push_back(ByteCode::Op{ ByteCode::OC_META, ByteCode::OpExt{ 32 /*stack_size*/ }, ByteCode::OR_VOID });
                codes.push_back(ByteCode::Op{ ByteCode::OC_EXEC, ByteCode::OpExt{ 1 /*ver*/ }, ByteCode::OR_VOID });

                ice::u32 var_addr = 4;
                while(exp != nullptr)
                {
                    if (exp->entity == SyntaxEntity::DEF_Variable)
                    {
                        ice::arctic::SyntaxNode_Variable const* const var = static_cast<ice::arctic::SyntaxNode_Variable const*>(exp);

                        auto symbol = create_symbol(var->name.value);
                        variables.emplace(symbol, ByteCode::Addr{ var_addr });
                        // std::cout << variables.at(symbol).loc << std::endl;

                        if (var->child != nullptr && var->child->entity == SyntaxEntity::EXP_Expression)
                        {
                            // codes.push_back(ByteCode::Op{ ByteCode::OC_MOVR, ByteCode::OE_VALUE, ByteCode::OR_PTR });
                            // codes.push_back(ByteCode::Value{ var_addr - 4 });
                            generate_bytecode_expression_l0(var->child, codes, { var_addr }, true);
                        }

                        var_addr += 4;
                    }
                    else if (exp->entity == SyntaxEntity::EXP_Expression)
                    {
                        generate_bytecode_expression_l0(exp, codes, { 0 });
                    }


                    exp = exp->sibling;
                }

                codes.push_back(ByteCode::Op{ ByteCode::OC_END });
                // codeblocks.push_back(std::move(codes));

                functions.emplace(fn_symbol, std::move(codes));
                function_idx.emplace(fn_symbol, ByteCode::Addr{ (ice::u32) function_idx.size() });
            }
        }
    }

    auto finalize() noexcept -> std::vector<ByteCode>
    {
        std::vector<ByteCode> scriptcode;

        ice::u32 symbol_bytes = 0;
        for (auto const& entry : functions)
        {
            symbol_bytes += entry.first.bcrep.size();
        }

        ice::u32 const initial_bytecode_offset = symbol_bytes + functions.size() * 3;
        ice::u32 bytecode_offset = initial_bytecode_offset;

        std::vector<ice::u32> fn_offsets;
        for (auto const& entry : functions)
        {
            auto const& fn_symbol = entry.first;
            auto const& bytecode = entry.second;

            fn_offsets.push_back(bytecode_offset);
            scriptcode.push_back(ByteCode::Op{ ByteCode::OC_META, ByteCode::OE_META_SYMBOL, ByteCode::OR_VOID });
            scriptcode.push_back(ByteCode::Value{ bytecode_offset });
            scriptcode.push_back(ByteCode::Value{ (ice::u32) fn_symbol.bcrep.size() });
            scriptcode.insert(scriptcode.end(), fn_symbol.bcrep.begin(), fn_symbol.bcrep.end());

            bytecode_offset += bytecode.size();
        }

        for (auto& entry : functions)
        {
            auto it = entry.second.begin();
            auto const end = entry.second.end();

            while(it != end)
            {
                if (it->op.extension == ByteCode::OE_FUNC)
                {
                    it += 1;
                    fmt::print("{} = {}\n", it->byte, fn_offsets[it->byte]);
                    it->byte = fn_offsets[it->byte];
                }
                it += 1;
            }

            scriptcode.insert(scriptcode.end(), entry.second.begin(), end);
        }

        for (auto const& entry : functions)
        {
            scriptcode.insert(scriptcode.end(), entry.second.begin(), entry.second.end());
        }

        return scriptcode;
    }
};



class BasicVirtualMachine : public ice::arctic::VirtualMachine
{
public:
    void execute(
        ice::arctic::ByteCode const* scriptcode,
        ice::arctic::ByteCode const* bytecode,
        ice::arctic::ExecutionContext const& context,
        ice::arctic::ExecutionState& state
    ) noexcept
    {
        auto const* it = bytecode;
        while(it->op.operation != ByteCode::OC_END)
        {
            it += 1;
        }

        execute(scriptcode, std::span<ice::arctic::ByteCode const>{ bytecode, it + 1 }, context, state);
    }

    void execute(
        ice::arctic::ByteCode const* scriptcode,
        std::span<ice::arctic::ByteCode const> const& bytecode,
        ice::arctic::ExecutionContext const& context,
        ice::arctic::ExecutionState& state
    ) noexcept override
    {
        // registers[0] = 0;

        auto it = bytecode.begin();
        auto const end = bytecode.end();

        fmt::print("{}\n", to_string(it->op.operation));

        // Skip metadata
        // assert(it->op.operation == ByteCode::OC_META && it->op.extension == 1);
        // it += 1;
        // assert(it->op.operation == ByteCode::OC_META && it->op.extension != 0);
        // char* memory = new char[it->op.extension + 32];
        // memset(memory, '\0', it->op.extension + 32);
        // memset(registers, '\0', sizeof(registers));
        // it += 1;

        char* stack = state.stack_pointer;
        char* memory = stack + 128;
        ice::u32* registers = state.registers;

        memset(registers, '\0', sizeof(state.registers));


        assert(it->op.operation == ByteCode::OC_EXEC);
        it += 1;

        while(it != end)
        {
            assert(it->op.operation != ByteCode::OC_META);
            assert(it->op.operation != ByteCode::OC_EXEC);
            std::cout << std::dec << "\nReg[" << registers[0] << ", " << registers[1]<< ", " << registers[ByteCode::OR_PTR] << "]" << '\n';
            std::cout << std::dec << to_string(it->op.operation) << " (" << to_string(it->op.extension) << ", " << (int)it->op.registr << ")" << '\n';

            if (it->op.operation == ByteCode::OC_ADD32)
            {
                assert(it->op.extension != ByteCode::OE_VALUE);
                if (it->op.extension == ByteCode::OE_VALUE_SP)
                {
                    registers[ByteCode::OR_SP] += it->op.registr;
                }
                else if (it->op.extension == ByteCode::OE_REG)
                {
                    ice::u32 const reg = it->op.registr;
                    registers[ByteCode::OR_R0] += registers[reg];
                    // std::cout << "[" << registers[reg] << "]" << std::endl;
                }
            }
            else if (it->op.operation == ByteCode::OC_SUB32)
            {
                assert(it->op.extension != ByteCode::OE_VALUE);
                if (it->op.extension == ByteCode::OE_VALUE_SP)
                {
                    registers[ByteCode::OR_SP] -= it->op.registr;
                }
                else if (it->op.extension == ByteCode::OE_REG)
                {
                    ice::u32 const reg = it->op.registr;
                    registers[ByteCode::OR_R0] -= registers[reg];
                }
            }
            else if (it->op.operation == ByteCode::OC_MUL32)
            {
                assert(it->op.extension != ByteCode::OE_VALUE);
                if (it->op.extension == ByteCode::OE_VALUE_SP)
                {
                    registers[ByteCode::OR_SP] *= it->op.registr;
                }
                else if (it->op.extension == ByteCode::OE_REG)
                {
                    ice::u32 const reg = it->op.registr;
                    registers[ByteCode::OR_R0] *= registers[reg];
                    // std::cout << "[" << registers[ByteCode::OR_R0] << "]" << std::endl;
                    // std::cout << "[" << registers[reg] << "]" << std::endl;
                }
            }
            else if (it->op.operation == ByteCode::OC_DIV32)
            {
                assert(it->op.extension != ByteCode::OE_VALUE);
                if (it->op.extension == ByteCode::OE_VALUE_SP)
                {
                    registers[ByteCode::OR_SP] /= it->op.registr;
                }
                else if (it->op.extension == ByteCode::OE_REG)
                {
                    ice::u32 const reg = it->op.registr;
                    registers[ByteCode::OR_R0] /= registers[reg];
                }
            }
            else if (it->op.operation == ByteCode::OC_MOVA)
            {
                ByteCode::OpExt const ext = it->op.extension;

                void* const mem = memory + registers[ByteCode::OR_PTR];
                if (ext == ByteCode::OE_VALUE)
                {
                    it += 1;
                    *((ice::u32*)mem) = it->byte;
                }
                else if (ext == ByteCode::OE_REG)
                {
                    *((ice::u32*)mem) = registers[it->op.registr];
                }
            }
            else if (it->op.operation == ByteCode::OC_MOVS)
            {
                void* const mem = stack + registers[ByteCode::OR_SP];
                if (it->op.extension == ByteCode::OE_REG)
                {
                    ice::u32 const reg = it->op.registr;
                    *((ice::u32*)mem) = registers[reg];
                }
            }
            else if (it->op.operation == ByteCode::OC_MOVR)
            {
                if (it->op.extension == ByteCode::OE_VALUE || it->op.extension == ByteCode::OE_FUNC)
                {
                    ice::u32 const reg = it->op.registr;
                    it += 1;
                    registers[reg] = it->byte;
                }
                else if (it->op.extension == ByteCode::OE_ADDR)
                {
                    void* const mem = memory + registers[ByteCode::OR_PTR];
                    registers[it->op.registr] = *((ice::u32*)mem);
                }
                else if (it->op.extension == ByteCode::OE_STACK)
                {
                    void* const mem = stack + registers[ByteCode::OR_SP];
                    registers[it->op.registr] = *((ice::u32*)mem);
                }
                else if (it->op.extension == ByteCode::OE_REG)
                {
                    ice::u32 const reg = it->op.registr;
                    it += 1;
                    registers[reg] = registers[it->byte];
                }
            }
            else if (it->op.operation == ByteCode::OC_CALL0_VOID)
            {
                // void* const mem = stack + registers[ByteCode::OR_TP];
                // if (it->op.extension == ByteCode::OE_NONE)
                {
                    fmt::print("CALLED: {}\n", registers[ByteCode::OR_TP]);
                    execute(scriptcode, scriptcode + registers[ByteCode::OR_TP] + 2, context, state);
                    // ice::u32 const reg = it->op.registr;
                    // *((ice::u32*)mem) = registers[reg];
                }
            }

            std::cout << std::dec << "= Reg[" << registers[0] << ", " << registers[1]<< ", " << registers[ByteCode::OR_PTR] << "]" << '\n';
            it += 1;
        }

        assert((it - 1)->op.operation == ByteCode::OC_END);

        std::cout << "\n" << std::dec << *((ice::u32*)memory) << '\n';
        std::cout << "\n" << std::dec << *((ice::u32*)memory + 1) << '\n';
        std::cout << "\n" << std::dec << *((ice::u32*)memory + 2) << '\n';
        // delete[] memory;
    }

    // char stack[256]{ };
    // ice::u32 registers[ByteCode::OR_VOID + 1]{ };
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

        if constexpr(true)
        {
            ice::arctic::WordProcessor processor = ice::arctic::create_word_processor(
                contents._buffer,
                &matcher
            );

            ice::arctic::Lexer lexer = ice::arctic::create_lexer(
                std::move(processor)
            );

            std::unique_ptr<ice::arctic::Script> script = ice::arctic::load_script(lexer);
            fmt::print("Script contains {} functions.\n", script->count_functions());
            return 0;
        }

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

        ice::arctic::SyntaxVisitorBase* visitors[]{
            &bcg
        };

        std::unique_ptr<ice::arctic::SyntaxNodeAllocator> alloc = ice::arctic::create_simple_allocator();
        std::unique_ptr<ice::arctic::Parser> parser = ice::arctic::create_default_parser();
        // parser.add_visitor(bcg);
        // parser.add_visitor(my_glsl_gen);
        // parser.add_visitor(my_hlsl_gen);

        auto t1 = std::chrono::high_resolution_clock::now();
        parser->parse(lexer, *alloc, visitors);
        auto t2 = std::chrono::high_resolution_clock::now();
        parser = nullptr;

        auto const scriptcode = bcg.finalize();

        [[maybe_unused]]
        ice::arctic::ExecutionContext global_context;
        [[maybe_unused]]
        ice::arctic::ExecutionState global_state;

        char static_stack[256];
        global_state.stack_pointer = static_stack;
        global_state.stack_size = 256;

        auto it = scriptcode.begin();
        auto const end = scriptcode.end();

        std::u8string_view symbol = u8"main";
        if (argc >= 3)
        {
            fmt::print("{}\n", argv[2]);
            symbol = { (char8_t const*) argv[2], strlen(argv[2]) };
        }

        ByteCode const* requested_code = nullptr;
        auto const requested_symbol = create_symbol(symbol);

        while(it != end && it->op.extension != ByteCode::OE_META_END)
        {
            if (it->op.extension == ByteCode::OE_META_SYMBOL)
            {
                it += 1;
                auto const fn_addr = it;

                it += 1;
                std::cout << "SYM 0\n";
                ice::u32 const size = it->byte;

                std::span<ByteCode const> const symbol_bc_view{ it + 1, size };
                Symbol symbol_bc;
                symbol_bc.bcrep.insert(symbol_bc.bcrep.end(), symbol_bc_view.begin(), symbol_bc_view.end());

                if (symbol_bc == requested_symbol)
                {
                    requested_code =  scriptcode.data() + fn_addr->byte + 2;
                    fmt::print("{} = {}\n", fn_addr->byte, to_string(requested_code->op.operation));
                }

                // for(ice::u32 idx = 0; idx < size; ++idx)
                // {
                //     it += 1;
                //     std::cout << std::hex << "|" << it->byte;
                // }
                // std::cout << "\n";
            }

            it += 1;
        }

        BasicVirtualMachine bvm;
        if (requested_code != nullptr)
        {
            bvm.execute(scriptcode.data(), requested_code, global_context, global_state);
        }

        bcg.release_nodes(*alloc);

        // for ([[maybe_unused]] auto const& block : bcg.codeblocks)
        // {
        //     std::cout << "Block: " << "\n";
        //      bvm.execute(block, global_context, global_state);
        //     std::cout << "\n";
        // }

        std::cout << std::dec;
        std::cout << "Total time: " << std::chrono::duration_cast<std::chrono::nanoseconds>(t2 - t1).count() << "ns" << std::endl;
    }

    ice::arctic::shutdown_matcher(&matcher);
    return 0;
}
