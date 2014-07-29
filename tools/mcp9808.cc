#include <iostream>
#include <iic.h>

int main(int argc, char **argv)
{
    i2c::mcp9808 device = i2c::mcp9808::open(1, 0x18);
    float temp = device.read_temperature();

    std::cout << "T " << temp << std::endl;
}
