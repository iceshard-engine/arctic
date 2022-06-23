#pragma once
#include "arctic_types.hxx"

namespace ice::arctic
{

    enum class WordCategory : ice::u32
    {
        Unknown,

        AlphaNum, // All Utf8 (non-puctuation/whitespace) characters
        Punctuation, // ASCII-nonex values only
        Whitespace, // ASCII-nonex whitespace only

        EndOfLine, // \n, \r
        EndOfFile, // Special, \0
    };

    struct WordLocation
    {
        static constexpr uint32_t Constant_MaxLine = 0x000f'ffff;
        static constexpr uint32_t Constant_MaxCharacters = 0x0000'0fff;

        ice::u32 line : 20;
        ice::u32 character : 12;
    };

    struct Word
    {
        ice::String value;
        ice::arctic::WordCategory category;
        ice::arctic::WordLocation location;
    };

} // namespace ice::arctic
