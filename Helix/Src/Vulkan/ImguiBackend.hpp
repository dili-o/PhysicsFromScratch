#pragma once

#include "Defines.hpp"
#include "Vulkan/VulkanTypes.hpp"

namespace hlx {

class ImguiBackend {
public:
  ImguiBackend(VkContext *pCtx, SDL_Window *pWindow,
               VkCommandPool vkTransferPool);
  void Shutdown();
  void BeginFrame();
  void RenderFrame(VkCommandBuffer commandBuffer, u32 currentFrame);
  bool HandleEvents(void *pEvent);

public:
  VkSampler vkSampler{};
  VulkanImage fontImage{};
  VulkanImageView fontImageView{};
  VkDescriptorUpdateTemplate vkUpdateTemplate;
  VkDescriptorImageInfo imageDescriptor{};
  VkDescriptorSetLayout vkSetLayout{};

  std::vector<VulkanBuffer> vertexBuffers;
  std::vector<VulkanBuffer> indexBuffers;
  VulkanPipeline pipeline{};

  VkContext *ctx = nullptr;
};
} // namespace hlx
