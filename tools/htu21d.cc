#include <iostream>
#include <iic.h>

int main(int argc, char **argv)
{
    i2c::htu21d device = i2c::htu21d::open(1, 0x40);
    float temp = device.read_temperature();

    std::cout << temp << std::endl;

    float humidity = device.read_humidity();
    std::cout << humidity << std::endl;

}