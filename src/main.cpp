#include "scop/VulkanRenderer.hpp"
#include <string>

int main(int argc, char **argv)
{
    std::string obj;
    if (argc >= 2)
        obj = argv[1];

    scop::VulkanRenderer app;
    app.run(obj);
    return 0;
}
