#include <iostream>

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

int main()
{
    constexpr auto f = OpenMethod<&Some::Plus>();

    Some some;
    some.Plus(1);

    auto result = f(&some, 2);
    std::cout << result << std::endl;

    return 0;
}
