#pragma once
#include <ice/arctic_context.hxx>
#include <coroutine>
#include <cassert>

namespace ice::arctic::detail
{

    template<typename Result>
    class Generator
    {
    public:
        struct Promise;

        explicit Generator(std::coroutine_handle<Promise> coro) noexcept;
        Generator(Generator<Result>&&) noexcept;
        Generator(Generator<Result> const&) noexcept = delete;

        auto operator=(Generator<Result>&& other) noexcept -> Generator&;
        auto operator=(Generator<Result> const& other) noexcept -> Generator& = delete;

        ~Generator() noexcept;

        auto next() noexcept -> Result;

    public:
        struct Promise
        {
            auto initial_suspend() noexcept -> std::suspend_always { return {}; };
            auto yield_value(Result value) noexcept -> std::suspend_always { _value = std::move(value); return {}; }
            void return_value(Result value) noexcept { _value = std::move(value); }
            auto final_suspend() noexcept -> std::suspend_always { return {}; };

            void unhandled_exception() noexcept
            {
                assert(false);
            }

            auto get_return_object() noexcept -> Generator<Result>;

            Result _value;
        };

        using promise_type = Promise;

    private:
        std::coroutine_handle<Promise> _coro;
    };

    template<typename Result>
    inline Generator<Result>::Generator(std::coroutine_handle<Promise> coro) noexcept
        : _coro{ std::move(coro) }
    {
    }

    template<typename Result>
    inline Generator<Result>::Generator(Generator&& other) noexcept
        : _coro{ std::exchange(other._coro, nullptr) }
    {
    }

    template<typename Result>
    inline auto Generator<Result>::operator=(Generator&& other) noexcept -> Generator&
    {
        if (this == &other)
        {
            if (_coro)
            {
                assert(_coro.done());
                _coro.destroy();
            }

            _coro = std::exchange(other._coro, nullptr);
        }

        return *this;
    }

    template<typename Result>
    inline Generator<Result>::~Generator() noexcept
    {
        if (_coro)
        {
            //assert(_coro.done());
            _coro.destroy();
        }
    }

    template<typename Result>
    inline auto Generator<Result>::next() noexcept -> Result
    {
        if (!_coro.done())
        {
            _coro.resume();
        }

        return std::move(_coro.promise()._value);
    }

    template<typename Result>
    inline auto Generator<Result>::Promise::get_return_object() noexcept -> Generator<Result>
    {
        return Generator{ std::coroutine_handle<Generator::Promise>::from_promise(*this) };
    }

} // namespace ice::arctic
