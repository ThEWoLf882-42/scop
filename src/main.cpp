#include <scop/VulkanApp.hpp>
#include <iostream>

int main()
{
    try
    {
        scop::VulkanApp app;
        app.run();
    }
    catch (const std::exception &e)
    {
        std::cerr << "Fatal: " << e.what() << "\n";
        return 1;
    }
    return 0;
}
