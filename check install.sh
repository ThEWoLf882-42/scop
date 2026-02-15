# 0) Load your SDK env
source /goinfre/$USER/VulkanSDK/1.3.296.0/setup-env.sh

# 1) Basic env check
echo "VULKAN_SDK=$VULKAN_SDK"
echo "VK_ICD_FILENAMES=$VK_ICD_FILENAMES"
echo "VK_LAYER_PATH=$VK_LAYER_PATH"

# 2) Tools must be found
command -v vulkaninfo
command -v glslc

# 3) Runtime check (must succeed)
vulkaninfo > /tmp/vulkaninfo.txt 2>/tmp/vulkaninfo.err
echo "vulkaninfo exit code: $?"
grep -E "Vulkan Instance Version|deviceName|apiVersion" /tmp/vulkaninfo.txt | head -n 10

# 4) Validation layer check (must load)
VK_INSTANCE_LAYERS=VK_LAYER_KHRONOS_validation vulkaninfo > /tmp/vkval.txt 2>/tmp/vkval.err
echo "validation exit code: $?"
head -n 20 /tmp/vkval.err

# 5) Optional visual test
command -v vkcube && vkcube
