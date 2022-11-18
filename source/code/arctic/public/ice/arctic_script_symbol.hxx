#pragma once
#include <ice/arctic_types.hxx>
#include <memory>

namespace ice::arctic
{

    // #Name (up to 65k chars)
    // <Zero> 16[Count] + 8[NameByte0] + 8[NameByte1]
    // <, NameBytes[2...]>

    // #TypeBase
    // <Zero> [NativeValue]
    // (NativeValue == Usertype/Function) [#Name]

    // #TypeSymbol
    // <Zero> 16[TypeBytes] + 1[XT] + 1[CT] + 1[MUT] + 13[Unused]
    // <First> [#TypeBase]
    // (Function) <Second> [ArgCount]
    // (Function) <, ArgCount...> [#TypeSymbol]...

    class ScriptSymbol
    {
    public:
        virtual ~ScriptSymbol() noexcept = default;

        virtual void dbg_print() const noexcept = 0;

        // virtual bool is_named() const noexcept = 0;
        virtual bool is_native() const noexcept = 0;
        virtual bool is_mutable() const noexcept = 0;
        virtual bool is_function() const noexcept = 0;
        virtual bool is_context() const noexcept = 0;

        virtual bool is_equal(ice::arctic::ScriptSymbol const& other) const noexcept = 0;
        // virtual bool is_similar(ice::arctic::ScriptSymbol const& other) const noexcept = 0;
        virtual bool is_compatible(ice::arctic::ScriptSymbol const& other) const noexcept = 0;
        virtual bool is_convertible(ice::arctic::ScriptSymbol const& other) const noexcept = 0;

        virtual auto hash() const noexcept -> ice::u64 = 0;
    };

    auto create_type_symbol2(ice::String type_string) noexcept -> std::unique_ptr<ice::arctic::ScriptSymbol>;

    inline operator==(ice::arctic::ScriptSymbol const& left, ice::arctic::ScriptSymbol const& right) noexcept
    {
        return left.is_equal(right);
    }

    inline operator!=(ice::arctic::ScriptSymbol const& left, ice::arctic::ScriptSymbol const& right) noexcept
    {
        return !(left == right);
    }

} // namespace ice::arctic

namespace std
{

    template <>
    struct hash<ice::arctic::ScriptSymbol>
    {
        using argument_type = ice::arctic::ScriptSymbol;
        using result_type = size_t;

        result_type operator() (const argument_type& symbol) const
        {
            return symbol.hash();
        }
    };

} // namespace std
