#include "mock-sensor-tool.hpp"

#include <iostream>

int main()
{
    int pid;
    std::cout << "Enter PID of process to overload: ";
    std::cin >> pid;
    MockSensor mock(pid);
    return 0;
}