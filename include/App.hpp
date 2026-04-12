#pragma once

#include <vulkan/vulkan.h>
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "Math.hpp"
#include "Mesh.hpp"
#include "TextureLoader.hpp"

namespace scop
{

	struct QueueFamilyIndices
	{
		std::optional<uint32_t> graphicsFamily;
		std::optional<uint32_t> presentFamily;

		bool isComplete() const
		{
			return graphicsFamily.has_value() && presentFamily.has_value();
		}
	};

	struct SwapChainSupportDetails
	{
		VkSurfaceCapabilitiesKHR capabilities;
		std::vector<VkSurfaceFormatKHR> formats;
		std::vector<VkPresentModeKHR> presentModes;
	};

	class ScopApp
	{
	public:
		ScopApp();
		~ScopApp();

		void run(const std::string &modelPath, const std::string &texturePath);

	private:
		static constexpr uint32_t WIDTH = 1920U;
		static constexpr uint32_t HEIGHT = 1080U;
		static constexpr std::size_t MAX_FRAMES_IN_FLIGHT = 2U;

		static void framebufferResizeCallback(GLFWwindow *window, int width, int height);

		void initWindow();
		void loadAssets(const std::string &modelPath, const std::string &texturePath);
		void centerAndScaleMesh();
		void initVulkan();
		void mainLoop();
		void cleanup();

		void createInstance();
		void createSurface();
		void pickPhysicalDevice();
		void createLogicalDevice();
		void createSwapChain();
		void createImageViews();
		void createRenderPass();
		void createDescriptorSetLayout();
		void createGraphicsPipeline();
		void createFramebuffers();
		void createCommandPool();
		void createDepthResources();
		void createTextureImage();
		void createTextureImageView();
		void createTextureSampler();
		void createVertexBuffer();
		void createIndexBuffer();
		void createUniformBuffers();
		void createDescriptorPool();
		void createDescriptorSets();
		void createCommandBuffers();
		void createSyncObjects();

		void recreateSwapChain();
		void cleanupSwapChain();
		void drawFrame();
		void updateUniformBuffer(uint32_t imageIndex, float dt);
		void processEvents(bool &running, float dt);

		bool checkDeviceExtensionSupport(VkPhysicalDevice device) const;
		bool isDeviceSuitable(VkPhysicalDevice device) const;
		QueueFamilyIndices findQueueFamilies(VkPhysicalDevice device) const;
		SwapChainSupportDetails querySwapChainSupport(VkPhysicalDevice device) const;

		VkSurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR> &availableFormats) const;
		VkPresentModeKHR chooseSwapPresentMode(const std::vector<VkPresentModeKHR> &availablePresentModes) const;
		VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR &capabilities) const;

		uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) const;
		VkFormat findDepthFormat() const;
		VkFormat findSupportedFormat(const std::vector<VkFormat> &candidates, VkImageTiling tiling, VkFormatFeatureFlags features) const;
		bool hasStencilComponent(VkFormat format) const;

		void createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties,
						  VkBuffer &buffer, VkDeviceMemory &bufferMemory);
		void copyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size);
		void createImage(uint32_t width, uint32_t height, VkFormat format, VkImageTiling tiling,
						 VkImageUsageFlags usage, VkMemoryPropertyFlags properties, VkImage &image, VkDeviceMemory &imageMemory);
		VkImageView createImageView(VkImage image, VkFormat format, VkImageAspectFlags aspectFlags) const;
		void transitionImageLayout(VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout);
		void copyBufferToImage(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height);

		VkCommandBuffer beginSingleTimeCommands();
		void endSingleTimeCommands(VkCommandBuffer commandBuffer);
		VkShaderModule createShaderModule(const std::vector<std::uint8_t> &code) const;

		GLFWwindow *window_;
		VkInstance instance_;
		VkSurfaceKHR surface_;
		VkPhysicalDevice physicalDevice_;
		VkDevice device_;
		VkQueue graphicsQueue_;
		VkQueue presentQueue_;

		VkSwapchainKHR swapChain_;
		std::vector<VkImage> swapChainImages_;
		std::vector<VkImageView> swapChainImageViews_;
		std::vector<VkFramebuffer> swapChainFramebuffers_;
		VkFormat swapChainImageFormat_;
		VkExtent2D swapChainExtent_;

		VkRenderPass renderPass_;
		VkDescriptorSetLayout descriptorSetLayout_;
		VkPipelineLayout pipelineLayout_;
		VkPipeline graphicsPipeline_;
		VkCommandPool commandPool_;

		VkImage depthImage_;
		VkDeviceMemory depthImageMemory_;
		VkImageView depthImageView_;

		VkImage textureImage_;
		VkDeviceMemory textureImageMemory_;
		VkImageView textureImageView_;
		VkSampler textureSampler_;

		VkBuffer vertexBuffer_;
		VkDeviceMemory vertexBufferMemory_;
		VkBuffer indexBuffer_;
		VkDeviceMemory indexBufferMemory_;

		std::vector<VkBuffer> uniformBuffers_;
		std::vector<VkDeviceMemory> uniformBuffersMemory_;
		std::vector<void *> uniformBuffersMapped_;
		VkDescriptorPool descriptorPool_;
		std::vector<VkDescriptorSet> descriptorSets_;
		std::vector<VkCommandBuffer> commandBuffers_;

		std::vector<VkSemaphore> imageAvailableSemaphores_;
		std::vector<VkSemaphore> renderFinishedSemaphores_;
		std::vector<VkFence> inFlightFences_;
		std::vector<VkFence> imagesInFlight_;
		std::size_t currentFrame_;

		bool framebufferResized_;
		bool rotationPaused_;
		bool textureEnabled_;
		bool hasRealTexture_;
		float rotationAngle_;
		float rotationSpeed_;
		float textureBlend_;
		float targetTextureBlend_;
		Vec3 translation_;

		bool prevEscape_;
		bool prevT_;
		bool prevSpace_;
		bool prevR_;

		bool hasMaterial_;
		Vec3 materialKd_;
		Vec3 materialKs_;
		float materialNs_;

		MeshData mesh_;
		TextureImage textureData_;
	};

} // namespace scop