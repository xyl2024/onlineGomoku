#include <iostream>

#include <functional>

void print(const char* str, int num)
{
    std::cout << str << ", " << num << std::endl;
}

int main()
{
    print("hello", 666);
    /// bind生成了一个新的 可调用对象
    auto fun = std::bind(&print, "hello", std::placeholders::_1);
    fun(666);
    auto fun2 = std::bind(print, std::placeholders::_1, std::placeholders::_2);
    fun2("hello", 666);
    return 0;
}