#include <scop/VulkanApp.hpp>
#include <scop/VulkanRenderer.hpp>

namespace scop
{

    void VulkanApp::run()
    {
        VulkanRenderer renderer;
        renderer.run();
    }

}
