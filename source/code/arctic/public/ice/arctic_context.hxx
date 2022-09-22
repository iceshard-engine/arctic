#pragma once
#include <ice/arctic_types.hxx>

namespace ice::arctic
{

    enum class ExecutionFlags : ice::u32
    {
        None = 0x00'00,

        //! \brief The context does not allow for coroutines.
        ContextAtomic = 0x00'01,

        //! \brief The context provides data in arrays.
        ContextArrays = 0x08'00,
    };

    struct ExecutionContext
    {
        void const** function_pointers;
        void const** constant_pointers;
        void** context_pointers;
    };

} // namespace ice::arctic
