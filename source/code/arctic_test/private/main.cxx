#include <Windows.h>
#include <filesystem>
#include <iostream>
#include <format>
#include <string_view>
#include <unordered_map>

#include "arctic_word_matcher.hxx"
#include "arctic_word_processor.hxx"
#include "arctic_lexer.hxx"
#include "arctic_parser.hxx"

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
        auto msg = std::format(
            "TypeDef: {} == {}, is_alias: {}",
            std::string_view{ (const char*)node->name.value.data(), node->name.value.size() },
            std::string_view{ (const char*)node->base_type.value.data(), node->base_type.value.size() },
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
        auto msg = std::format(
            "Struct: {}",
            std::string_view{ (const char*)node->name.value.data(), node->name.value.size() }
        );

        std::cout << msg << "\n";

        ice::arctic::SyntaxNode const* child = node->child;
        while (child != nullptr)
        {
            ice::arctic::SyntaxNode_StructMember const* member = static_cast<ice::arctic::SyntaxNode_StructMember const*>(child);

            auto msg2 = std::format(
                "- {} : {}",
                std::string_view{ (const char*)member->name.value.data(), member->name.value.size() },
                std::string_view{ (const char*)member->type.value.data(), member->type.value.size() }
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
        std::cout << (const char*)_buffer.c_str() << "\n";
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
        ice::arctic::SyntaxNode_AnnotationAttribute const* attrib = nullptr;
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
        std::cout << "\nstruct VertexShaderInputs\n";
        std::cout << "{\n";
        std::cout << (const char*)_input_struct.c_str();
        std::cout << "}\n";
        std::cout << "\nstruct PixelShaderInputs\n";
        std::cout << "{\n";
        std::cout << (const char*)_output_struct.c_str();
        std::cout << "  float4 out_pos : SV_Position;\n";
        std::cout << "}\n";

        std::cout << (const char*)_buffer.c_str() << "\n";
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
        ice::arctic::SyntaxNode_AnnotationAttribute const* attrib = nullptr;
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

auto main(int argc, char** argv) -> int
{
    if (argc == 0)
    {
        return -1;
    }

    std::filesystem::path script_path = std::filesystem::absolute(argv[1]);

    Buffer<ice::utf8> contents;

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

    ice::u32 token_count = 0;

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


        GLSL_Transpiler my_glsl_gen{ };
        HLSL_Transpiler my_hlsl_gen{ };

        ice::arctic::Parser parser;
        parser.add_visitor(my_glsl_gen);
        parser.add_visitor(my_hlsl_gen);

        auto t1 = std::chrono::high_resolution_clock::now();

        parser.parse(lexer);

        auto t2 = std::chrono::high_resolution_clock::now();

        std::cout << "Tokens: " << token_count << std::endl;
        std::cout << "Total time: " << std::chrono::duration_cast<std::chrono::milliseconds>(t2 - t1).count() << "ms" << std::endl;
    }

    ice::arctic::shutdown_matcher(&matcher);
    return 0;
}
