#include <iostream>
#include <functional>

template <auto M, typename R, typename C, typename... ARGS>
constexpr auto _OpenMethod(R (C::*m)(ARGS...))
{
    struct inner
    {
        static R call(C *self, ARGS... args)
        {
            return (self->*M)(args...);
        }
    };
    return &inner::call;
}

template <auto M>
constexpr auto OpenMethod()
{
    return _OpenMethod<M>(M);
}

struct Some
{
    int Value = 0;

    int Plus(int n)
    {
        Value += n;
        return Value;
    }
};

static int add(int a, int b)
{
    return a + b;
}

int main()
{
    auto tuple = std::make_tuple(1, 2);
    {
        auto result = std::apply(&add, tuple);
        std::cout << result << std::endl;
    }
    {
        auto lambda = [](int a, int b) {
            return a + b;
        };
        auto result = std::apply(lambda, tuple);
        std::cout << result << std::endl;
    }
    {
        std::function<int(int, int)> f = &add;
        auto result = std::apply(f, tuple);
        std::cout << result << std::endl;
    }

    return 0;
}
