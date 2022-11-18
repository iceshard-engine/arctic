#include "arctic_script_bytecode_symbol.hxx"

namespace ice::arctic
{

    ArcticBytecodeSymbol::ArcticBytecodeSymbol(std::vector<ice::u32> symbol_bytes) noexcept
        : _symbol_bytes{ std::move(symbol_bytes) }
    {
    }

    ArcticBytecodeSymbol::~ArcticBytecodeSymbol() noexcept
    {
    }

    bool equal_bytes(ice::Span<ice::u32 const> left_bytes, ice::Span<ice::u32 const> right_bytes) noexcept
    {
        if (left_bytes.size() == right_bytes.size())
        {
            ice::u32 idx = 0;
            ice::u32 const count_bytes =  left_bytes.size();
            for (/* noop */; idx < count_bytes; ++idx)
            {
                if (left_bytes[idx] != right_bytes[idx])
                {
                    break;
                }
            }

            return idx == count_bytes;
        }
        return false;
    }

    bool ArcticBytecodeSymbol::is_equal(ice::arctic::ScriptSymbol const& other) const noexcept
    {
        auto const* right = dynamic_cast<ice::arctic::ArcticBytecodeSymbol const*>(&other);
        if (right != nullptr)
        {
            return equal_bytes(_symbol_bytes, right->_symbol_bytes);
        }
        return false;
    }

    bool ArcticBytecodeSymbol::is_compatible(ice::arctic::ScriptSymbol const& other) const noexcept
    {
        auto const* right = dynamic_cast<ice::arctic::ArcticBytecodeSymbol const*>(&other);
        if (right != nullptr)
        {
            if ((right->_symbol_bytes[0] & 0x8000'0000) && (_symbol_bytes[0] & 0x8000'0000))
            {
                return (right->_symbol_bytes[0] & 0x008f'ffff) == (_symbol_bytes[0] & 0x008f'ffff);
            }
            else if (!(right->_symbol_bytes[0] & 0x8000'0000) && !(_symbol_bytes[0] & 0x8000'0000))
            {
                if (right->_symbol_bytes[1] == _symbol_bytes[1])
                {
                    if (_symbol_bytes[1] == (ice::u32)ScriptNativeType::Function)
                    {
                        ice::Span<ice::u32 const> left_bytes = _symbol_bytes;
                        ice::Span<ice::u32 const> right_bytes = right->_symbol_bytes;
                        // ice::u32 const left_size = left_bytes[2] >> 16;
                        // ice::u32 const right_size = right_bytes[2] >> 16;

                        return equal_bytes(left_bytes.subspan(2/* + left_size*/), right_bytes.subspan(2/* + right_size*/));
                    }
                    else if (_symbol_bytes[1] == (ice::u32)ScriptNativeType::Usertype)
                    {
                        ice::Span<ice::u32 const> left_bytes = _symbol_bytes;
                        ice::Span<ice::u32 const> right_bytes = right->_symbol_bytes;
                        return (right->_symbol_bytes[0] & 0x008f'ffff) == (_symbol_bytes[0] & 0x008f'ffff) && equal_bytes(left_bytes.subspan(1), right_bytes.subspan(1));
                    }
                }
            }
        }
        return false;
    }

    auto ArcticBytecodeSymbol::hash() const noexcept -> ice::u64
    {
        std::string_view const sv_rep{ (char const*) _symbol_bytes.data(), _symbol_bytes.size() * 4 };
        return std::hash<std::string_view>{}(sv_rep);
    }

    void string_to_bytes(ice::String str, std::vector<ice::u32>& bytes_out) noexcept
    {
        assert(str.size() < std::numeric_limits<ice::u16>::max());

        ice::u32 const bytes_idx = bytes_out.size();
        ice::u32 const symbol_size = (ice::u32) ((str.size() + 1) / 4) + 1;
        auto it = str.begin();
        auto const end = str.end();

        ice::u32 byte = 0;
        ice::u32 byte_size = 2;
        while(it != end)
        {
            byte <<= 8;
            byte |= static_cast<ice::u8>(*it);
            byte_size += 1;

            if (byte_size == 4)
            {
                bytes_out.push_back(byte);
                byte = 0;
                byte_size = 0;
            }

            it += 1;
        }

        if (byte_size != 0)
        {
            bytes_out.push_back(byte);
        }

        bytes_out[bytes_idx] |= symbol_size << 16;
    }

    // #Name (up to 65k chars)
    // <Zero> 16[Count] + 8[NameByte0] + 8[NameByte1]
    // <, NameBytes[2...]>

    // #TypeBase
    // <Zero> [NativeValue]
    // (NativeValue == Usertype) [#Name]

    // #TypeSymbol
    // (Native) <Zero> 1[1] + 1[XT] + 1[CT] + 2[in/out/inout] + 1[MUT] + 26[NativeValue]
    // (Non-Native) <Zero> 1[0] + 1[XT] + 1[CT] + 2[in/out/inout] + 1[MUT] + 10[Unused] + 16[SymbolSize]
    // <First> [#TypeBase]
    // (Function) <Second> [ArgCount]
    // (Function) <, ArgCount...> [#TypeSymbol]...

    void create_type_symbol2(ice::String type_string, std::vector<ice::u32>& out_bytes) noexcept
    {
        ice::arctic::ScriptNativeType const native_type = native_type_from_string(type_string);
        ice::u32 const first_idx = out_bytes.size();

        if (((ice::u32)native_type & (ice::u32)ScriptNativeType::NativeType) != 0)
        {
            out_bytes.push_back(0x8000'0000 | (ice::u32)native_type);
        }
        else
        {
            out_bytes.push_back(0);
            out_bytes.push_back((ice::u32)native_type);
        }

        if (native_type == ScriptNativeType::Usertype)
        {
            auto ws_char = type_string.find_first_of(' ');
            ice::String const name = type_string.substr(0, ws_char);
            string_to_bytes(name, out_bytes);

            if (ws_char != std::string::npos)
            {
                ice::u32 next_ws = type_string.find_first_of(u8' ', ws_char + 1);
                ice::String mod_str = type_string.substr(ws_char + 1, (next_ws - 1) - ws_char);

                ice::u32 mod_byte = 0;
                while(mod_str.empty() == false)
                {
                    fmt::print("-Mods: '{}'\n", std::string_view{ (char const*)mod_str.data(), mod_str.size() });
                    // XT  mod_byte = 0x0800'0000;
                    // CT  mod_byte = 0x0400'0000;
                    // I/O mod_byte = 0x0300'0000;
                    // MUT mod_byte = 0x0080'0000;
                    if (mod_str == u8"out")
                    {
                        mod_byte |= 0x0280'0000;
                    }
                    else if (mod_str == u8"inout")
                    {
                        mod_byte |= 0x0380'0000;
                    }
                    else if (mod_str == u8"in")
                    {
                        mod_byte |= 0x0100'0000;
                    }
                    else if (mod_str == u8"mut")
                    {
                        mod_byte |= 0x0080'0000;
                    }

                    ws_char = next_ws;
                    if (next_ws == (ice::u32)std::string::npos)
                    {
                        break;
                    }

                    next_ws = type_string.find_first_of(u8' ', ws_char + 1);
                    mod_str = type_string.substr(ws_char + 1, next_ws - ws_char);
                }

                out_bytes[first_idx] |= mod_byte;
            }
        }
        else if (native_type == ScriptNativeType::Function)
        {
            // ice::String const name = type_string.substr(0, type_string.find_first_of('('));
            // fmt::print("-Name: {}\n", std::string_view{ (char const*)name.data(), name.size() });
            // string_to_bytes(name, out_bytes);

            ice::u32 arg_idx = type_string.find_first_of('(') + 1;
            ice::u32 arg_end = type_string.find_first_of(u8",)", arg_idx);
            while(type_string[arg_end] != ')')
            {
                ice::String arg = type_string.substr(arg_idx, arg_end - arg_idx);
                arg = arg.substr(arg.find_first_of(':') + 1);
                arg = arg.substr(arg.find_first_not_of(' '));

                fmt::print("-Name: {}\n", std::string_view{ (char const*)arg.data(), arg.size() });
                create_type_symbol2(arg, out_bytes);

                arg_idx = arg_end + 1;
                arg_end = type_string.find_first_of(u8",)", arg_idx);
            }

            ice::String arg = type_string.substr(arg_idx, arg_end - arg_idx);
            if (arg.empty() == false)
            {
                arg = arg.substr(arg.find_first_of(':') + 1);
                arg = arg.substr(arg.find_first_not_of(' '));
                create_type_symbol2(arg, out_bytes);
            }

            type_string = type_string.substr(arg_end + 1);
            type_string = type_string.substr(type_string.find_first_of(':') + 1);
            type_string = type_string.substr(type_string.find_first_not_of(' '));
            create_type_symbol2(type_string, out_bytes);
        }
        else if (auto ws_char = type_string.find_first_of(u8' '); ws_char != std::string::npos)
        {
            ice::u32 next_ws = type_string.find_first_of(u8' ', ws_char + 1);
            ice::String mod_str = type_string.substr(ws_char + 1, (next_ws - 1) - ws_char);

            ice::u32 mod_byte = 0;
            while(mod_str.empty() == false)
            {
                fmt::print("-Mods: '{}'\n", std::string_view{ (char const*)mod_str.data(), mod_str.size() });
                // XT  mod_byte = 0x0800'0000;
                // CT  mod_byte = 0x0400'0000;
                // I/O mod_byte = 0x0300'0000;
                // MUT mod_byte = 0x0080'0000;
                if (mod_str == u8"out")
                {
                    mod_byte |= 0x0280'0000;
                }
                else if (mod_str == u8"inout")
                {
                    mod_byte |= 0x0380'0000;
                }
                else if (mod_str == u8"in")
                {
                    mod_byte |= 0x0100'0000;
                }
                else if (mod_str == u8"mut")
                {
                    mod_byte |= 0x0080'0000;
                }

                ws_char = next_ws;
                if (next_ws == (ice::u32)std::string::npos)
                {
                    break;
                }

                next_ws = type_string.find_first_of(u8' ', ws_char + 1);
                mod_str = type_string.substr(ws_char + 1, next_ws - ws_char);
            }

            out_bytes[first_idx] |= mod_byte;
        }

        if ((out_bytes[first_idx] & 0x8000'0000) == 0)
        {
            // Insert first byte
            fmt::print("{:0>8X} |= {}\n", out_bytes[0], out_bytes.size() - 1);
            out_bytes[first_idx] |= ((ice::u32) out_bytes.size() - (1 + first_idx)); // TODO: move left 16 bits
        }
    }

    auto create_type_symbol2(ice::String type_string) noexcept -> std::unique_ptr<ice::arctic::ScriptSymbol>
    {
        std::vector<ice::u32> symbol_bytes;
        create_type_symbol2(type_string, symbol_bytes);
        return std::make_unique<ice::arctic::ArcticBytecodeSymbol>(std::move(symbol_bytes));
    }

} // namespace ice::arctic
