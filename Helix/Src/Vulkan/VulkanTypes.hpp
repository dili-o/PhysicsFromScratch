#pragma once

#include "Defines.hpp"
#include "Vulkan/VulkanUtils.hpp"
#include <optional>

struct SDL_Window;

namespace hlx {
struct HLX_API QueueFamilyIndices {
  std::optional<u32> graphicsFamilyIndex;
  std::optional<u32> transferFamilyIndex;
  std::optional<u32> computeFamilyIndex;

  bool IsComplete() {
    return graphicsFamilyIndex.has_value() && transferFamilyIndex.has_value() &&
           computeFamilyIndex.has_value();
  }
};

bool HLX_API SelectPhysicalDevice(VkInstance vkInstance,
                                  VkPhysicalDevice &vkPhysicalDevice,
                                  VkPhysicalDeviceProperties *pDeviceProperties,
                                  QueueFamilyIndices &queueFamilyIndices,
                                  VkSurfaceKHR vkSurface);

struct HLX_API VulkanBuffer {
  VkBuffer vkHandle{VK_NULL_HANDLE};
  VmaAllocation vmaAllocation{VK_NULL_HANDLE};
  VkDeviceMemory deviceMemory{VK_NULL_HANDLE};
  void *pMappedData{nullptr};
  VmaMemoryUsage vmaMemoryUsage;
  VkBufferUsageFlags usage;
  VkDeviceAddress deviceAddress;
  VkMemoryPropertyFlags properties;
};

struct HLX_API VulkanImage {
  VkImage vkHandle{VK_NULL_HANDLE};
  VmaAllocation vmaAllocation{VK_NULL_HANDLE};
  VkDeviceMemory deviceMemory{VK_NULL_HANDLE};
  VkImageLayout currentLayout{VK_IMAGE_LAYOUT_UNDEFINED};
  VkFormat format;
  VkExtent3D vkExtents;
  u32 mipCount = 1;
  cstring name = nullptr;
};

struct HLX_API VulkanImageView {
  VkImageView vkHandle{VK_NULL_HANDLE};
};

struct HLX_API VulkanSwapchain {
  VkSurfaceFormatKHR vkSurfaceFormat{};
  VkPresentModeKHR vkPresentMode{VK_PRESENT_MODE_FIFO_KHR};
  VkSwapchainKHR vkHandle{VK_NULL_HANDLE};
  std::vector<VkImage> vkImages;
  std::vector<VkImageView> vkImageViews;
  u32 imageCount = 0;
  VkExtent2D vkExtents;
};

// TODO: Add preferred swapchain settings
// Maybe also make a version that only uses pure vulkan primitives
bool HLX_API CreateSwapchain(VkPhysicalDevice vkPhysicalDevice,
                             VkDevice vkDevice,
                             VkAllocationCallbacks *vkAllocationCallbacks,
                             VkSurfaceKHR vkSurface,
                             VulkanSwapchain &swapchain);

void HLX_API DestroySwapchain(VkDevice vkDevice,
                              VkAllocationCallbacks *vkAllocationCallbacks,
                              VulkanSwapchain &swapchain);

void HLX_API RecreateSwapchain(VkPhysicalDevice vkPhysicalDevice,
                               VkDevice vkDevice,
                               VkAllocationCallbacks *vkAllocationCallbacks,
                               VkSurfaceKHR vkSurface,
                               VulkanSwapchain &swapchain);

struct HLX_API VulkanPipeline {
  VkPipeline vkHandle{VK_NULL_HANDLE};
  VkPipelineLayout vkPipelineLayout{VK_NULL_HANDLE};
};

namespace util {
void HLX_API CreateBuffer(VkPhysicalDevice physicalDevice, VkDevice device,
                          VulkanBuffer &buffer, VkDeviceSize size,
                          VkBufferUsageFlags usage,
                          VkMemoryPropertyFlags properties);

void HLX_API CreateVmaBuffer(VmaAllocator vmaAllocator,
                             VkPhysicalDevice physicalDevice, VkDevice device,
                             VulkanBuffer &buffer, VkDeviceSize size,
                             VkBufferUsageFlags usage, VmaMemoryUsage vmaUsage,
                             VmaAllocationCreateFlags vmaFlags,
                             VkMemoryPropertyFlags properties);

void HLX_API CreateVmaImage(VmaAllocator &vmaAllocator,
                            VkImageCreateInfo &createInfo,
                            VmaAllocationCreateInfo &vmaCreateInfo,
                            VulkanImage &image);

void HLX_API CreateImageView(VkDevice device,
                             VkAllocationCallbacks *pAllocationCallbacks,
                             VkImageViewCreateInfo &createInfo,
                             VulkanImageView &imageView);

void HLX_API DestroyVmaImage(VmaAllocator &vmaAllocator, VulkanImage &image);

void HLX_API DestroyImageView(VkDevice device,
                              VkAllocationCallbacks *pAllocationCallbacks,
                              VulkanImageView &imageView);

void HLX_API DestroyBuffer(VkDevice device,
                           VkAllocationCallbacks *allocationCallbacks,
                           VulkanBuffer &buffer);

void HLX_API DestroyVmaBuffer(VmaAllocator vmaAllocator, VulkanBuffer &buffer);

void HLX_API CopyToBuffer(VkCommandBuffer commandBuffer, VkDeviceSize size,
                          VkDeviceSize srcOffset, VkBuffer srcBuffer,
                          VkDeviceSize dstOffset, VkBuffer dstBuffer);

void HLX_API CopyToImage(VkCommandBuffer commandBuffer, VkImage image,
                         VkFormat imageFormat, VkExtent3D imageExtents,
                         VkDeviceSize srcOffset, VkBuffer srcBuffer);

VkCommandBuffer HLX_API BeginSingleTimeCommandBuffer(VkDevice device,
                                                     VkCommandPool commandPool);

void HLX_API EndSingleTimeCommandBuffer(VkDevice device,
                                        VkCommandBuffer commandBuffer,
                                        VkCommandPool commandPool,
                                        VkQueue queue);

} // namespace util

class HLX_API VkContext {
public:
  VkContext() = default;

  // TODO: Add Device and Instance extension
  bool Init(SDL_Window *pWindow);
  bool Shutdown();

  void SetResourceName(const VkObjectType type, const u64 handle, cstring name);

  void CreateBuffer(VulkanBuffer &buffer, VkDeviceSize size,
                    VkBufferUsageFlags usage, VkMemoryPropertyFlags properties,
                    cstring name = nullptr) {
    util::CreateBuffer(vkPhysicalDevice, vkDevice, buffer, size, usage,
                       properties);
    SetResourceName(VK_OBJECT_TYPE_BUFFER,
                    reinterpret_cast<u64>(buffer.vkHandle), name);
  }

  void CreateVmaBuffer(VulkanBuffer &buffer, VkDeviceSize size,
                       VkBufferUsageFlags usage, VmaMemoryUsage vmaUsage,
                       VmaAllocationCreateFlags vmaFlags,
                       VkMemoryPropertyFlags properties,
                       cstring name = nullptr) {
    util::CreateVmaBuffer(vmaAllocator, vkPhysicalDevice, vkDevice, buffer,
                          size, usage, vmaUsage, vmaFlags, properties);
    if (name) {
      SetResourceName(VK_OBJECT_TYPE_BUFFER,
                      reinterpret_cast<u64>(buffer.vkHandle), name);

      vmaSetAllocationName(vmaAllocator, buffer.vmaAllocation, name);
    }
  }

  void CreateVmaImage(VulkanImage &image, VkImageCreateInfo &createInfo,
                      VmaAllocationCreateInfo &vmaCreateInfo) {
    util::CreateVmaImage(vmaAllocator, createInfo, vmaCreateInfo, image);
  }

  void DestroyBuffer(VulkanBuffer &buffer) {
    if (buffer.vmaAllocation == VK_NULL_HANDLE)
      util::DestroyBuffer(vkDevice, vkAllocationCallbacks, buffer);
    else
      util::DestroyVmaBuffer(vmaAllocator, buffer);
  }

  void CopyToBuffer(VkBuffer dstBuffer, VkDeviceSize dstOffset,
                    VkDeviceSize size, const void *pData,
                    VkCommandPool commandPool);

  void CopyToImage(VulkanImage image, const void *pData,
                   VkCommandPool commandPool);

public:
  VkInstance vkInstance{VK_NULL_HANDLE};
  VkAllocationCallbacks *vkAllocationCallbacks{nullptr};
  VkDebugUtilsMessengerEXT vkDebugUtilsMessenger{VK_NULL_HANDLE};

  VkSurfaceKHR vkSurface{VK_NULL_HANDLE};
  VkDevice vkDevice{VK_NULL_HANDLE};
  VkPhysicalDevice vkPhysicalDevice{VK_NULL_HANDLE};
  VkPhysicalDeviceProperties vkPhysicalDeviceProperties{};
  QueueFamilyIndices queueFamilyIndices{};

  VmaAllocator vmaAllocator{};

  PFN_vkSetDebugUtilsObjectNameEXT pfnSetDebugUtilsObjectNameEXT;
  PFN_vkCmdBeginDebugUtilsLabelEXT pfnCmdBeginDebugUtilsLabelEXT;
  PFN_vkCmdInsertDebugUtilsLabelEXT pfnCmdInsertDebugUtilsLabelEXT;
  PFN_vkCmdEndDebugUtilsLabelEXT pfnCmdEndDebugUtilsLabelEXT;

  VkQueue vkGraphicsQueue{VK_NULL_HANDLE};
  VkQueue vkTransferQueue{VK_NULL_HANDLE};
  VkQueue vkComputeQueue{VK_NULL_HANDLE};

  VulkanSwapchain swapchain{};
};
} // namespace hlx
