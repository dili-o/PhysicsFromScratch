#include "VulkanTypes.hpp"
#include "Log.hpp"
#include "Vulkan/VulkanUtils.hpp"
// Vendor
#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>
#include <glm/common.hpp>

#ifdef _DEBUG
#define VULKAN_DEBUG_REPORT
#define VULKAN_EXTRA_VALIDATION
#endif // _DEBUG

#define SHADER_DEBUG_SYMBOLS

// TODO: Make this configurable
#define MAX_SWAPCHAIN_IMAGES 3

namespace hlx {

bool SelectPhysicalDevice(VkInstance vkInstance,
                          VkPhysicalDevice &vkPhysicalDevice,
                          VkPhysicalDeviceProperties *pDeviceProperties,
                          QueueFamilyIndices &queueFamilyIndices,
                          VkSurfaceKHR vkSurface) {

  u32 deviceCount = 0;
  VK_CHECK(vkEnumeratePhysicalDevices(vkInstance, &deviceCount, nullptr));

  if (deviceCount == 0) {
    HERROR("Failed to find a GPU that supports Vulkan!");
    return false;
  }

  std::vector<VkPhysicalDevice> physicalDevices(deviceCount);

  VK_CHECK(vkEnumeratePhysicalDevices(vkInstance, &deviceCount,
                                      physicalDevices.data()));

  bool foundSuitableDevice = false;
  for (u32 i = 0; i < deviceCount; ++i) {
    VkPhysicalDevice device = physicalDevices[i];
    vkGetPhysicalDeviceProperties(device, pDeviceProperties);

    VkPhysicalDeviceFeatures deviceFeatures;
    vkGetPhysicalDeviceFeatures(device, &deviceFeatures);

    u32 extensionCount = 0;
    vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount,
                                         nullptr);
    std::vector<VkExtensionProperties> extensionProperties(extensionCount);
    vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount,
                                         extensionProperties.data());

    bool extensionsSupported = false;
    for (size_t idx = 0; idx < extensionProperties.size(); ++idx) {
      if (!strcmp(extensionProperties[idx].extensionName,
                  VK_KHR_SWAPCHAIN_EXTENSION_NAME)) {
        extensionsSupported = true;
        break;
      }
    }
    if (!extensionsSupported) {
      HERROR("{} not supported", VK_KHR_SWAPCHAIN_EXTENSION_NAME);
      break;
    }

    // Check if device has suitable queue family
    u32 queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount,
                                             nullptr);
    std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount,
                                             queueFamilies.data());
    for (u32 idx = 0; idx < queueFamilyCount; ++idx) {
      VkBool32 presentQueueSupport = false;
      vkGetPhysicalDeviceSurfaceSupportKHR(device, idx, vkSurface,
                                           &presentQueueSupport);
      if (presentQueueSupport &&
          queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT &&
          !queueFamilyIndices.graphicsFamilyIndex.has_value()) {
        queueFamilyIndices.graphicsFamilyIndex = idx;
        continue;
      }

      if (queueFamilies[i].queueFlags & VK_QUEUE_COMPUTE_BIT &&
          !queueFamilyIndices.computeFamilyIndex.has_value()) {
        queueFamilyIndices.computeFamilyIndex = idx;
        continue;
      }

      if (queueFamilies[i].queueFlags & VK_QUEUE_TRANSFER_BIT) {
        queueFamilyIndices.transferFamilyIndex = idx;
        continue;
      }
    }
    if (queueFamilyIndices.IsComplete()) {
      foundSuitableDevice = true;
      vkPhysicalDevice = device;
      HINFO("Suitable device found: {}", pDeviceProperties->deviceName);
    }

    if (foundSuitableDevice) {
      break;
    }
  }
  return foundSuitableDevice;
}

// TODO: Maybe separate each component of choosing the swapchain into separate
// functions, also width and height
static void QuerySwapchainSupport(VkPhysicalDevice physicalDevice,
                                  VkSurfaceKHR surface,
                                  VulkanSwapchain &swapchain) {
  VkSurfaceCapabilitiesKHR capabilities{};
  vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, surface,
                                            &capabilities);
  u32 formatCount;
  vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &formatCount,
                                       nullptr);
  HASSERT(formatCount != 0);
  std::vector<VkSurfaceFormatKHR> formats(formatCount);
  vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &formatCount,
                                       formats.data());

  u32 presentModeCount = 0;
  vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface,
                                            &presentModeCount, nullptr);
  HASSERT(presentModeCount != 0);
  std::vector<VkPresentModeKHR> presentModes(presentModeCount);
  vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface,
                                            &formatCount, presentModes.data());

  const VkFormat preferredSurfaceImageFormats[] = {
      VK_FORMAT_B8G8R8A8_UNORM, VK_FORMAT_R8G8B8A8_UNORM,
      VK_FORMAT_B8G8R8_UNORM, VK_FORMAT_R8G8B8_UNORM};
  const VkColorSpaceKHR preferredSurfaceColorSpace =
      VK_COLORSPACE_SRGB_NONLINEAR_KHR;

  u32 surfaceFormatCount = ArraySize(preferredSurfaceImageFormats);
  bool formatFound = false;
  for (u32 i = 0; i < surfaceFormatCount; ++i) {
    for (u32 j = 0; j < formatCount; j++) {
      if (formats[j].format == preferredSurfaceImageFormats[i] &&
          formats[j].colorSpace == preferredSurfaceColorSpace) {
        swapchain.vkSurfaceFormat = formats[j];
        formatFound = true;
        break;
      }
    }
    if (formatFound)
      break;
  }
  // Default to the first format
  if (!formatFound) {
    swapchain.vkSurfaceFormat = formats[0];
    HWARN("Could not find preferred surface format, defaulting to first "
          "available format");
  }

  bool presentModeFound = false;
  for (u32 i = 0; i < presentModeCount; ++i) {
    if (presentModes[i] == VK_PRESENT_MODE_MAILBOX_KHR) {
      swapchain.vkPresentMode = presentModes[i];
      presentModeFound = true;
      break;
    }
  }
  if (!presentModeFound) {
    swapchain.vkPresentMode = VK_PRESENT_MODE_FIFO_KHR;
    HWARN("Could not find preferred present mode, defaulting to FIFO");
  }

  if (capabilities.currentExtent.width != UINT32_MAX) {
    swapchain.vkExtents = capabilities.currentExtent;
  } else {
    VkExtent2D extents = {1280, 720};

    swapchain.vkExtents.width =
        glm::clamp(extents.width, capabilities.minImageExtent.width,
                   capabilities.maxImageExtent.width);
    swapchain.vkExtents.height =
        glm::clamp(extents.height, capabilities.minImageExtent.height,
                   capabilities.maxImageExtent.height);
  }

  u32 imageCount = capabilities.minImageCount + 1;
  if (capabilities.maxImageCount > 0 &&
      imageCount > capabilities.maxImageCount) {
    imageCount = capabilities.maxImageCount;
  }

  swapchain.imageCount = imageCount;
}

// TODO: Remember to transition the swapchain images after calling this
// function
bool CreateSwapchain(VkPhysicalDevice vkPhysicalDevice, VkDevice vkDevice,
                     VkAllocationCallbacks *vkAllocationCallbacks,
                     VkSurfaceKHR vkSurface, VulkanSwapchain &swapchain) {
  QuerySwapchainSupport(vkPhysicalDevice, vkSurface, swapchain);
  VkSurfaceCapabilitiesKHR surface_capabilities;
  vkGetPhysicalDeviceSurfaceCapabilitiesKHR(vkPhysicalDevice, vkSurface,
                                            &surface_capabilities);

  VkSwapchainCreateInfoKHR createInfo{};
  createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
  createInfo.surface = vkSurface;
  createInfo.minImageCount = swapchain.imageCount;
  createInfo.imageFormat = swapchain.vkSurfaceFormat.format;
  createInfo.imageColorSpace = swapchain.vkSurfaceFormat.colorSpace;
  createInfo.imageExtent = swapchain.vkExtents;
  createInfo.imageArrayLayers = 1;
  createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
  createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
  createInfo.preTransform = surface_capabilities.currentTransform;
  createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
  createInfo.presentMode = swapchain.vkPresentMode;
  createInfo.clipped = VK_TRUE;
  createInfo.oldSwapchain = swapchain.vkHandle;

  VK_CHECK(vkCreateSwapchainKHR(vkDevice, &createInfo, vkAllocationCallbacks,
                                &swapchain.vkHandle));

  HASSERT(swapchain.imageCount <= MAX_SWAPCHAIN_IMAGES);
  vkGetSwapchainImagesKHR(vkDevice, swapchain.vkHandle, &swapchain.imageCount,
                          nullptr);
  swapchain.vkImages.resize(swapchain.imageCount);
  swapchain.vkImageViews.resize(swapchain.imageCount);
  vkGetSwapchainImagesKHR(vkDevice, swapchain.vkHandle, &swapchain.imageCount,
                          swapchain.vkImages.data());

  for (u32 i = 0; i < swapchain.imageCount; ++i) {
    // Create VulkanImage resources for the swapchain images

    // image->vk_handle = vkImages[i];
    // image->current_layout = VK_IMAGE_LAYOUT_UNDEFINED;
    // image->format = swapchain.vkSurface_format.format;
    // image->vk_extents = {swapchainExtents.width, swapchainExtents.height,
    // 1}; image->mip_count = 1;

    // VulkanImageView *image_view =
    // access_image_view(swapchain.image_views[i]); image_view->image =
    // swapchain.images[i];

    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = swapchain.vkImages[i];
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = swapchain.vkSurfaceFormat.format;
    viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = 1;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = 1;

    VK_CHECK(vkCreateImageView(vkDevice, &viewInfo, vkAllocationCallbacks,
                               &swapchain.vkImageViews[i]));
  }

  HINFO("Created swapchain {}x{} successfully", swapchain.vkExtents.width,
        swapchain.vkExtents.height);
  return true;
}

void DestroySwapchain(VkDevice vkDevice,
                      VkAllocationCallbacks *vkAllocationCallbacks,
                      VulkanSwapchain &swapchain) {
  for (u32 i = 0; i < swapchain.imageCount; ++i) {
    vkDestroyImageView(vkDevice, swapchain.vkImageViews[i],
                       vkAllocationCallbacks);
    swapchain.vkImages[i] = VK_NULL_HANDLE;
    swapchain.vkImageViews[i] = VK_NULL_HANDLE;
  }

  vkDestroySwapchainKHR(vkDevice, swapchain.vkHandle, vkAllocationCallbacks);
  swapchain.vkHandle = VK_NULL_HANDLE;
}

void RecreateSwapchain(VkPhysicalDevice vkPhysicalDevice, VkDevice vkDevice,
                       VkAllocationCallbacks *vkAllocationCallbacks,
                       VkSurfaceKHR vkSurface, VulkanSwapchain &swapchain) {
  vkDeviceWaitIdle(vkDevice);
  DestroySwapchain(vkDevice, vkAllocationCallbacks, swapchain);

  CreateSwapchain(vkPhysicalDevice, vkDevice, vkAllocationCallbacks, vkSurface,
                  swapchain);
}

u32 FindMemoryType(VkPhysicalDevice physicalDevice, u32 typeFilter,
                   VkMemoryPropertyFlags properties) {
  VkPhysicalDeviceMemoryProperties memProperties;
  vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProperties);

  for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
    if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags &
                                    properties) == properties) {
      return i;
    }
  }

  throw std::runtime_error("failed to find suitable memory type!");
}

namespace util {
void CreateBuffer(VkPhysicalDevice physicalDevice, VkDevice device,
                  VulkanBuffer &buffer, VkDeviceSize size,
                  VkBufferUsageFlags usage, VkMemoryPropertyFlags properties) {
  VkBufferCreateInfo bufferInfo{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
  bufferInfo.size = size;
  bufferInfo.usage = usage;
  bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

  VK_CHECK(vkCreateBuffer(device, &bufferInfo, nullptr, &buffer.vkHandle));

  VkMemoryRequirements memRequirements;
  vkGetBufferMemoryRequirements(device, buffer.vkHandle, &memRequirements);

  VkMemoryAllocateInfo allocInfo{};
  allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
  allocInfo.allocationSize = memRequirements.size;
  allocInfo.memoryTypeIndex = FindMemoryType(
      physicalDevice, memRequirements.memoryTypeBits, properties);

  if (vkAllocateMemory(device, &allocInfo, nullptr, &buffer.deviceMemory) !=
      VK_SUCCESS) {
    throw std::runtime_error("failed to allocate buffer memory!");
  }

  vkBindBufferMemory(device, buffer.vkHandle, buffer.deviceMemory, 0);

  buffer.usage = usage;
  buffer.properties = properties;
  buffer.pMappedData = nullptr;
  buffer.deviceAddress = 0;

  buffer.vmaAllocation = VK_NULL_HANDLE;
  buffer.vmaMemoryUsage = VMA_MEMORY_USAGE_MAX_ENUM;

  if (usage & VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT) {
    VkBufferDeviceAddressInfo addressInfo{
        VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO};
    addressInfo.buffer = buffer.vkHandle;
    buffer.deviceAddress = vkGetBufferDeviceAddress(device, &addressInfo);
  }
}

void CreateVmaBuffer(VmaAllocator vmaAllocator, VkPhysicalDevice physicalDevice,
                     VkDevice device, VulkanBuffer &buffer, VkDeviceSize size,
                     VkBufferUsageFlags usage, VmaMemoryUsage vmaUsage,
                     VmaAllocationCreateFlags vmaFlags,
                     VkMemoryPropertyFlags properties) {
  VkBufferCreateInfo bufferInfo{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
  bufferInfo.size = size;
  bufferInfo.usage = usage;
  bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

  VmaAllocationCreateInfo memoryInfo{};
  memoryInfo.usage = vmaUsage;
  memoryInfo.requiredFlags = properties;
  // Note to self: VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT for buffers that
  // change a lot in a frame,
  // VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT for buffers that
  // change only once per frame
  memoryInfo.flags = vmaFlags;

  VmaAllocationInfo allocInfo{};

  VK_CHECK(vmaCreateBuffer(vmaAllocator, &bufferInfo, &memoryInfo,
                           &buffer.vkHandle, &buffer.vmaAllocation,
                           &allocInfo));

  buffer.usage = usage;
  buffer.properties = properties;
  buffer.vmaMemoryUsage = vmaUsage;
  buffer.pMappedData = nullptr;
  buffer.deviceAddress = 0;

  buffer.deviceMemory = VK_NULL_HANDLE;

  if (vmaFlags & VMA_ALLOCATION_CREATE_MAPPED_BIT) {
    vmaMapMemory(vmaAllocator, buffer.vmaAllocation, &buffer.pMappedData);
  }

  if (usage & VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT) {
    VkBufferDeviceAddressInfo addressInfo{
        VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO};
    addressInfo.buffer = buffer.vkHandle;
    buffer.deviceAddress = vkGetBufferDeviceAddress(device, &addressInfo);
  }
}

void CreateVmaImage(VmaAllocator &vmaAllocator, VkImageCreateInfo &createInfo,
                    VmaAllocationCreateInfo &vmaCreateInfo,
                    VulkanImage &image) {

  VK_CHECK(vmaCreateImage(vmaAllocator, &createInfo, &vmaCreateInfo,
                          &image.vkHandle, &image.vmaAllocation, nullptr));
  image.deviceMemory = VK_NULL_HANDLE;
  image.currentLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  image.format = createInfo.format;
  image.vkExtents = createInfo.extent;
  image.mipCount = createInfo.mipLevels;
}

void CreateImageView(VkDevice device,
                     VkAllocationCallbacks *pAllocationCallbacks,
                     VkImageViewCreateInfo &createInfo,
                     VulkanImageView &imageView) {
  VK_CHECK(vkCreateImageView(device, &createInfo, pAllocationCallbacks,
                             &imageView.vkHandle));
}

void DestroyVmaImage(VmaAllocator &vmaAllocator, VulkanImage &image) {
  vmaDestroyImage(vmaAllocator, image.vkHandle, image.vmaAllocation);
  image.vkHandle = VK_NULL_HANDLE;
  image.vmaAllocation = VK_NULL_HANDLE;
}

void HLX_API DestroyImageView(VkDevice device,
                              VkAllocationCallbacks *pAllocationCallbacks,
                              VulkanImageView &imageView) {
  vkDestroyImageView(device, imageView.vkHandle, pAllocationCallbacks);
  imageView.vkHandle = VK_NULL_HANDLE;
}

void DestroyBuffer(VkDevice device, VkAllocationCallbacks *allocationCallbacks,
                   VulkanBuffer &buffer) {
  if (buffer.pMappedData)
    vkUnmapMemory(device, buffer.deviceMemory);

  vkDestroyBuffer(device, buffer.vkHandle, allocationCallbacks);
  vkFreeMemory(device, buffer.deviceMemory, allocationCallbacks);

  buffer.vkHandle = VK_NULL_HANDLE;
  buffer.deviceMemory = VK_NULL_HANDLE;
  buffer.deviceAddress = 0;
}

void DestroyVmaBuffer(VmaAllocator vmaAllocator, VulkanBuffer &buffer) {

  if (buffer.pMappedData)
    vmaUnmapMemory(vmaAllocator, buffer.vmaAllocation);

  vmaDestroyBuffer(vmaAllocator, buffer.vkHandle, buffer.vmaAllocation);
  buffer.vkHandle = VK_NULL_HANDLE;
  buffer.vmaAllocation = VK_NULL_HANDLE;
  buffer.deviceAddress = 0;
}

void CopyToBuffer(VkCommandBuffer commandBuffer, VkDeviceSize size,
                  VkDeviceSize srcOffset, VkBuffer srcBuffer,
                  VkDeviceSize dstOffset, VkBuffer dstBuffer) {

  VkBufferCopy2 region{VK_STRUCTURE_TYPE_BUFFER_COPY_2};
  region.srcOffset = srcOffset;
  region.dstOffset = dstOffset;
  region.size = size;

  VkCopyBufferInfo2 bufferInfo{VK_STRUCTURE_TYPE_COPY_BUFFER_INFO_2};
  bufferInfo.srcBuffer = srcBuffer;
  bufferInfo.dstBuffer = dstBuffer;
  bufferInfo.regionCount = 1;
  bufferInfo.pRegions = &region;

  vkCmdCopyBuffer2(commandBuffer, &bufferInfo);
}

void CopyToImage(VkCommandBuffer commandBuffer, VkImage image,
                 VkFormat imageFormat, VkExtent3D imageExtents,
                 VkDeviceSize srcOffset, VkBuffer srcBuffer) {

  VkBufferImageCopy2 region{VK_STRUCTURE_TYPE_BUFFER_IMAGE_COPY_2};
  region.bufferOffset = srcOffset;
  region.imageSubresource.aspectMask = HasDepthOrStencil(imageFormat)
                                           ? VK_IMAGE_ASPECT_DEPTH_BIT
                                           : VK_IMAGE_ASPECT_COLOR_BIT;
  region.imageSubresource.mipLevel = 0;
  region.imageSubresource.baseArrayLayer = 0;
  region.imageSubresource.layerCount = 1;
  region.imageOffset = {0, 0, 0};
  region.imageExtent = imageExtents;

  VkCopyBufferToImageInfo2 bufferImageInfo{
      VK_STRUCTURE_TYPE_COPY_BUFFER_TO_IMAGE_INFO_2};
  bufferImageInfo.srcBuffer = srcBuffer;
  bufferImageInfo.dstImage = image;
  bufferImageInfo.dstImageLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
  bufferImageInfo.regionCount = 1;
  bufferImageInfo.pRegions = &region;

  vkCmdCopyBufferToImage2(commandBuffer, &bufferImageInfo);
}

VkCommandBuffer BeginSingleTimeCommandBuffer(VkDevice device,
                                             VkCommandPool commandPool) {
  VkCommandBufferAllocateInfo allocInfo{};
  allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
  allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
  allocInfo.commandPool = commandPool;
  allocInfo.commandBufferCount = 1;

  VkCommandBuffer commandBuffer;
  vkAllocateCommandBuffers(device, &allocInfo, &commandBuffer);

  VkCommandBufferBeginInfo beginInfo{};
  beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
  beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

  vkBeginCommandBuffer(commandBuffer, &beginInfo);

  return commandBuffer;
}

void EndSingleTimeCommandBuffer(VkDevice device, VkCommandBuffer commandBuffer,
                                VkCommandPool commandPool, VkQueue queue) {
  vkEndCommandBuffer(commandBuffer);

  VkCommandBufferSubmitInfo commandSubmitInfo{
      VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO};
  commandSubmitInfo.commandBuffer = commandBuffer;

  VkSubmitInfo2 submitInfo{VK_STRUCTURE_TYPE_SUBMIT_INFO_2};
  submitInfo.commandBufferInfoCount = 1;
  submitInfo.pCommandBufferInfos = &commandSubmitInfo;

  vkQueueSubmit2(queue, 1, &submitInfo, VK_NULL_HANDLE);
  vkQueueWaitIdle(queue);

  vkFreeCommandBuffers(device, commandPool, 1, &commandBuffer);
}

} // namespace util

bool VkContext::Init(SDL_Window *pWindow) {
  VkResult res = volkInitialize();
  if (res != VK_SUCCESS) {
    throw std::runtime_error("Volk failed to initialize!");
  }

  VkInstanceCreateInfo createInfo{};
  VkApplicationInfo appInfo{};
  appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
  appInfo.pApplicationName = "HelloTriangle";
  appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
  appInfo.pEngineName = "No Engine";
  appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
  appInfo.apiVersion = VK_API_VERSION_1_3;

  std::vector<cstring> requiredExtensions;
  std::vector<cstring> validationLayerNames;

  u32 platformExtensionCount = 0;
  const char *const *platformExtensions =
      SDL_Vulkan_GetInstanceExtensions(&platformExtensionCount);
  for (u32 i = 0; i < platformExtensionCount; ++i) {
    requiredExtensions.push_back(platformExtensions[i]);
  }

  bool debugUtilsExtensionPresent = false;
#ifdef VULKAN_DEBUG_REPORT
  u32 instanceExtensionCount = 0;
  vkEnumerateInstanceExtensionProperties(nullptr, &instanceExtensionCount,
                                         nullptr);
  VkExtensionProperties *instanceExtensions = (VkExtensionProperties *)malloc(
      sizeof(VkExtensionProperties) * instanceExtensionCount);

  vkEnumerateInstanceExtensionProperties(nullptr, &instanceExtensionCount,
                                         instanceExtensions);

  for (size_t i = 0; i < instanceExtensionCount; i++) {
    HTRACE("{}", instanceExtensions[i].extensionName);
    if (!strcmp(instanceExtensions[i].extensionName,
                VK_EXT_DEBUG_UTILS_EXTENSION_NAME)) {
      debugUtilsExtensionPresent = true;
      requiredExtensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
      continue;
    }
    if (!strcmp(instanceExtensions[i].extensionName,
                VK_KHR_SHADER_NON_SEMANTIC_INFO_EXTENSION_NAME)) {
      debugUtilsExtensionPresent = true;
      requiredExtensions.push_back(
          VK_KHR_SHADER_NON_SEMANTIC_INFO_EXTENSION_NAME);
      continue;
    }
  }

  validationLayerNames.push_back("VK_LAYER_KHRONOS_validation");

  u32 layerNotFoundIndex = 0;
  auto checkLayerSupport = [&validationLayerNames, &layerNotFoundIndex]() {
    u32 layerCount;
    vkEnumerateInstanceLayerProperties(&layerCount, nullptr);
    std::vector<VkLayerProperties> availableLayers(layerCount);
    vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

    for (u32 i = 0; i < validationLayerNames.size(); ++i) {
      bool layerFound = false;
      const char *layerName = validationLayerNames[i];

      for (const auto &layerProperty : availableLayers) {
        if (strcmp(layerName, layerProperty.layerName) == 0) {
          layerFound = true;
          break;
        }
      }

      if (!layerFound) {
        layerNotFoundIndex = i;
        return false;
      }
    }

    return true;
  };

  if (!checkLayerSupport()) {
    HERROR("A validation layer {} was not found!",
           validationLayerNames[layerNotFoundIndex]);
    validationLayerNames.erase(validationLayerNames.begin() +
                               layerNotFoundIndex);
  }
  VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo{};
  debugCreateInfo.sType =
      VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
  debugCreateInfo.messageSeverity =
      VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
      VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT |
      VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT;
  debugCreateInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                                VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
  debugCreateInfo.pfnUserCallback = DebugCallback;
#if defined(VULKAN_EXTRA_VALIDATION)
  const VkValidationFeatureEnableEXT features_requested[] = {
      VK_VALIDATION_FEATURE_ENABLE_GPU_ASSISTED_EXT,
      VK_VALIDATION_FEATURE_ENABLE_GPU_ASSISTED_RESERVE_BINDING_SLOT_EXT,
      VK_VALIDATION_FEATURE_ENABLE_SYNCHRONIZATION_VALIDATION_EXT,
      VK_VALIDATION_FEATURE_ENABLE_BEST_PRACTICES_EXT,
  };
  VkValidationFeaturesEXT features = {};
  features.sType = VK_STRUCTURE_TYPE_VALIDATION_FEATURES_EXT;
  features.pNext = (VkDebugUtilsMessengerCreateInfoEXT *)&debugCreateInfo;
  features.enabledValidationFeatureCount = ArraySize(features_requested);
  features.pEnabledValidationFeatures = features_requested;
  createInfo.pNext = &features;
#else
  createInfo.pNext = (VkDebugUtilsMessengerCreateInfoEXT *)&debugCreateInfo;
#endif // VULKAN_EXTRA_VALIDATION
#endif // VULKAN_DEBUG_REPORT

  createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
  createInfo.pApplicationInfo = &appInfo;
  createInfo.enabledExtensionCount = requiredExtensions.size();
  createInfo.ppEnabledExtensionNames = requiredExtensions.data();
  createInfo.enabledLayerCount = 0;
  createInfo.enabledLayerCount = validationLayerNames.size();
  createInfo.ppEnabledLayerNames = validationLayerNames.data();
  VK_CHECK(vkCreateInstance(&createInfo, nullptr, &vkInstance));
  volkLoadInstance(vkInstance);

#ifdef VULKAN_DEBUG_REPORT
  auto func = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(
      vkInstance, "vkCreateDebugUtilsMessengerEXT");
  if (func != nullptr) {
    if (func(vkInstance, &debugCreateInfo, vkAllocationCallbacks,
             &vkDebugUtilsMessenger) != VK_SUCCESS) {
      HERROR("Failed to set up debug messenger!");
    }
  } else {
    HERROR("Failed to set up debug messenger!");
  }
#endif

  // Surface
  if (!SDL_Vulkan_CreateSurface(pWindow, vkInstance, vkAllocationCallbacks,
                                &vkSurface)) {
    HERROR("Failed to create surface!");
    return false;
  }

  std::vector<cstring> deviceExtensions{};
  deviceExtensions.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);
  deviceExtensions.push_back(VK_EXT_MESH_SHADER_EXTENSION_NAME);
  deviceExtensions.push_back(VK_KHR_PUSH_DESCRIPTOR_EXTENSION_NAME);

  // Create Physical Device
  if (!SelectPhysicalDevice(vkInstance, vkPhysicalDevice,
                            &vkPhysicalDeviceProperties, queueFamilyIndices,
                            vkSurface)) {
    HERROR("Failed to create physical device!");
    return false;
  }

  // Create Logical Device
  std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
  f32 queuePriority = 1.f;

  VkDeviceQueueCreateInfo mainQueueInfo{
      VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};
  mainQueueInfo.queueFamilyIndex =
      queueFamilyIndices.graphicsFamilyIndex.value();
  mainQueueInfo.queueCount = 1;
  mainQueueInfo.pQueuePriorities = &queuePriority;
  queueCreateInfos.push_back(mainQueueInfo);

  if (queueFamilyIndices.graphicsFamilyIndex !=
      queueFamilyIndices.computeFamilyIndex) {
    VkDeviceQueueCreateInfo queueCreateInfo{
        VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};
    queueCreateInfo.queueFamilyIndex =
        queueFamilyIndices.computeFamilyIndex.value();
    queueCreateInfo.queueCount = 1;
    queueCreateInfo.pQueuePriorities = &queuePriority;
    queueCreateInfos.push_back(queueCreateInfo);
  }

  if (queueFamilyIndices.graphicsFamilyIndex !=
      queueFamilyIndices.transferFamilyIndex) {
    VkDeviceQueueCreateInfo queueCreateInfo{
        VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};
    queueCreateInfo.queueFamilyIndex =
        queueFamilyIndices.transferFamilyIndex.value();
    queueCreateInfo.queueCount = 1;
    queueCreateInfo.pQueuePriorities = &queuePriority;
    queueCreateInfos.push_back(queueCreateInfo);
  }

  VkPhysicalDeviceFeatures deviceFeatures{};
  // TODO: Check if this is available
  deviceFeatures.samplerAnisotropy = VK_TRUE;
  deviceFeatures.fillModeNonSolid = VK_TRUE;
  deviceFeatures.geometryShader = VK_TRUE;
  deviceFeatures.robustBufferAccess = VK_TRUE;

  VkDeviceCreateInfo deviceCreateInfo{VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};
  deviceCreateInfo.pQueueCreateInfos = queueCreateInfos.data();
  deviceCreateInfo.queueCreateInfoCount = queueCreateInfos.size();
  deviceCreateInfo.pEnabledFeatures = &deviceFeatures;
  deviceCreateInfo.enabledExtensionCount = deviceExtensions.size();
  deviceCreateInfo.ppEnabledExtensionNames = deviceExtensions.data();

  // 16bit storage buffers
  VkPhysicalDeviceVulkan11Features features11 = {
      VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES};
  features11.storageBuffer16BitAccess = VK_TRUE;

  // Enable Bindless descriptors and 16 bit floats in shaders and Timeline
  // semaphores
  VkPhysicalDeviceVulkan12Features features12 = {
      VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES};
  features12.shaderFloat16 = VK_TRUE;
  features12.shaderSampledImageArrayNonUniformIndexing = VK_TRUE;
  features12.runtimeDescriptorArray = VK_TRUE;
  features12.descriptorBindingVariableDescriptorCount = VK_TRUE;
  features12.descriptorBindingSampledImageUpdateAfterBind = VK_TRUE;
  features12.descriptorBindingPartiallyBound = VK_TRUE;
  features12.bufferDeviceAddress = VK_TRUE;

  // Enable Dynamic Rendering and Synchronization 2
  VkPhysicalDeviceVulkan13Features features13 = {
      VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES};
  features13.dynamicRendering = VK_TRUE;
  features13.synchronization2 = VK_TRUE;
  features13.maintenance4 = VK_TRUE;
  features13.pNext = &features11;

#ifdef VULKAN_EXTRA_VALIDATION
  deviceFeatures.fragmentStoresAndAtomics = VK_TRUE;
  deviceFeatures.vertexPipelineStoresAndAtomics = VK_TRUE;
  deviceFeatures.shaderInt64 = VK_TRUE;

  VkPhysicalDeviceFaultFeaturesEXT deviceFaultFeatures{
      VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FAULT_FEATURES_EXT};
  deviceFaultFeatures.deviceFault = VK_TRUE;
  deviceFaultFeatures.deviceFaultVendorBinary = VK_TRUE;

  features12.vulkanMemoryModel = VK_TRUE;
  features12.vulkanMemoryModelDeviceScope = VK_TRUE;
  features12.uniformAndStorageBuffer8BitAccess = VK_TRUE;
  features12.timelineSemaphore = VK_TRUE;
  features12.storageBuffer8BitAccess = VK_TRUE;
  features12.pNext = &deviceFaultFeatures;

#endif
  features11.pNext = &features12;

  // Mesh Shaders
  VkPhysicalDeviceMeshShaderFeaturesEXT meshShaderFeature{
      VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MESH_SHADER_FEATURES_EXT};
  meshShaderFeature.taskShader = VK_TRUE;
  meshShaderFeature.meshShader = VK_TRUE;
  meshShaderFeature.meshShaderQueries = VK_TRUE;
  meshShaderFeature.pNext = &features13;

  deviceCreateInfo.pNext = &meshShaderFeature;

  VkResult ress = (vkCreateDevice(vkPhysicalDevice, &deviceCreateInfo,
                                  vkAllocationCallbacks, &vkDevice));
  HINFO("Created Logical Device: {}", (u32)ress);
  volkLoadDevice(vkDevice);
  VmaVulkanFunctions vmaVulkanFunctions{};
  vmaVulkanFunctions.vkGetInstanceProcAddr = vkGetInstanceProcAddr;
  vmaVulkanFunctions.vkGetDeviceProcAddr = vkGetDeviceProcAddr;
  vmaVulkanFunctions.vkGetPhysicalDeviceProperties =
      vkGetPhysicalDeviceProperties;
  vmaVulkanFunctions.vkGetPhysicalDeviceMemoryProperties =
      vkGetPhysicalDeviceMemoryProperties;
  vmaVulkanFunctions.vkAllocateMemory = vkAllocateMemory;
  vmaVulkanFunctions.vkFreeMemory = vkFreeMemory;
  vmaVulkanFunctions.vkMapMemory = vkMapMemory;
  vmaVulkanFunctions.vkUnmapMemory = vkUnmapMemory;
  vmaVulkanFunctions.vkFlushMappedMemoryRanges = vkFlushMappedMemoryRanges;
  vmaVulkanFunctions.vkInvalidateMappedMemoryRanges =
      vkInvalidateMappedMemoryRanges;
  vmaVulkanFunctions.vkBindBufferMemory = vkBindBufferMemory;
  vmaVulkanFunctions.vkBindImageMemory = vkBindImageMemory;
  vmaVulkanFunctions.vkGetBufferMemoryRequirements =
      vkGetBufferMemoryRequirements;
  vmaVulkanFunctions.vkGetImageMemoryRequirements =
      vkGetImageMemoryRequirements;
  vmaVulkanFunctions.vkCreateBuffer = vkCreateBuffer;
  vmaVulkanFunctions.vkDestroyBuffer = vkDestroyBuffer;
  vmaVulkanFunctions.vkCreateImage = vkCreateImage;
  vmaVulkanFunctions.vkDestroyImage = vkDestroyImage;
  vmaVulkanFunctions.vkCmdCopyBuffer = vkCmdCopyBuffer;
  vmaVulkanFunctions.vkGetBufferMemoryRequirements2KHR =
      vkGetBufferMemoryRequirements2KHR;
  vmaVulkanFunctions.vkGetImageMemoryRequirements2KHR =
      vkGetImageMemoryRequirements2KHR;
  vmaVulkanFunctions.vkBindBufferMemory2KHR = vkBindBufferMemory2KHR;
  vmaVulkanFunctions.vkBindImageMemory2KHR = vkBindImageMemory2KHR;
  vmaVulkanFunctions.vkGetPhysicalDeviceMemoryProperties2KHR =
      vkGetPhysicalDeviceMemoryProperties2KHR;

  VmaAllocatorCreateInfo allocatorCreateInfo = {};
  allocatorCreateInfo.physicalDevice = vkPhysicalDevice;
  allocatorCreateInfo.device = vkDevice;
  allocatorCreateInfo.instance = vkInstance;
  allocatorCreateInfo.pVulkanFunctions = &vmaVulkanFunctions;
  allocatorCreateInfo.flags = VMA_ALLOCATOR_CREATE_EXTERNALLY_SYNCHRONIZED_BIT |
                              VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;

  VK_CHECK(vmaCreateAllocator(&allocatorCreateInfo, &vmaAllocator));

  //  Get the function pointers to Debug Utils functions.
  if (debugUtilsExtensionPresent) {
    pfnSetDebugUtilsObjectNameEXT =
        (PFN_vkSetDebugUtilsObjectNameEXT)vkGetDeviceProcAddr(
            vkDevice, "vkSetDebugUtilsObjectNameEXT");
    pfnCmdBeginDebugUtilsLabelEXT =
        (PFN_vkCmdBeginDebugUtilsLabelEXT)vkGetDeviceProcAddr(
            vkDevice, "vkCmdBeginDebugUtilsLabelEXT");
    pfnCmdInsertDebugUtilsLabelEXT =
        (PFN_vkCmdInsertDebugUtilsLabelEXT)vkGetDeviceProcAddr(
            vkDevice, "vkCmdInsertDebugUtilsLabelEXT");
    pfnCmdEndDebugUtilsLabelEXT =
        (PFN_vkCmdEndDebugUtilsLabelEXT)vkGetDeviceProcAddr(
            vkDevice, "vkCmdEndDebugUtilsLabelEXT");

    HASSERT(pfnSetDebugUtilsObjectNameEXT);
    HASSERT(pfnCmdBeginDebugUtilsLabelEXT);
    HASSERT(pfnCmdInsertDebugUtilsLabelEXT);
    HASSERT(pfnCmdEndDebugUtilsLabelEXT);
  }

  vkGetDeviceQueue(vkDevice, queueFamilyIndices.graphicsFamilyIndex.value(), 0,
                   &vkGraphicsQueue);
  SetResourceName(VK_OBJECT_TYPE_QUEUE, reinterpret_cast<u64>(vkGraphicsQueue),
                  "GraphicsQueue");

  if (queueFamilyIndices.graphicsFamilyIndex !=
      queueFamilyIndices.computeFamilyIndex) {
    vkGetDeviceQueue(vkDevice, queueFamilyIndices.computeFamilyIndex.value(), 0,
                     &vkComputeQueue);

    SetResourceName(VK_OBJECT_TYPE_QUEUE, reinterpret_cast<u64>(vkComputeQueue),
                    "ComputeQueue");
  }

  vkTransferQueue = vkGraphicsQueue;
  if (queueFamilyIndices.graphicsFamilyIndex !=
      queueFamilyIndices.transferFamilyIndex) {
    vkGetDeviceQueue(vkDevice, queueFamilyIndices.transferFamilyIndex.value(),
                     0, &vkTransferQueue);

    SetResourceName(VK_OBJECT_TYPE_QUEUE,
                    reinterpret_cast<u64>(vkTransferQueue), "TransferQueue");
  }

  CreateSwapchain(vkPhysicalDevice, vkDevice, vkAllocationCallbacks, vkSurface,
                  swapchain);

  HINFO("VkContext initialised");
  return true;
}

bool VkContext::Shutdown() {
  DestroySwapchain(vkDevice, vkAllocationCallbacks, swapchain);

  vkDestroySurfaceKHR(vkInstance, vkSurface, vkAllocationCallbacks);

  vmaDestroyAllocator(vmaAllocator);

  vkDestroyDevice(vkDevice, vkAllocationCallbacks);

  auto func = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(
      vkInstance, "vkDestroyDebugUtilsMessengerEXT");
  if (func != nullptr) {
    func(vkInstance, vkDebugUtilsMessenger, vkAllocationCallbacks);
  }
  vkDestroyInstance(vkInstance, vkAllocationCallbacks);
  HINFO("VkContext shutdown");
  return true;
}

void VkContext::SetResourceName(const VkObjectType type, const u64 handle,
                                cstring name) {
#ifdef VULKAN_DEBUG_REPORT
  VkDebugUtilsObjectNameInfoEXT nameInfo = {
      VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT};
  nameInfo.objectType = type;
  nameInfo.objectHandle = handle;
  nameInfo.pObjectName = name;
  pfnSetDebugUtilsObjectNameEXT(vkDevice, &nameInfo);
#endif // VULKAN_DEBUG_REPORT
}

void VkContext::CopyToBuffer(VkBuffer dstBuffer, VkDeviceSize dstOffset,
                             VkDeviceSize size, const void *pData,
                             VkCommandPool commandPool) {
  VkCommandBuffer singleTimeBuffer =
      util::BeginSingleTimeCommandBuffer(vkDevice, commandPool);

  VulkanBuffer stagingBuffer{};
  CreateVmaBuffer(stagingBuffer, size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                  VMA_MEMORY_USAGE_UNKNOWN, 0,
                  VK_MEMORY_PROPERTY_HOST_COHERENT_BIT |
                      VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
  vmaMapMemory(vmaAllocator, stagingBuffer.vmaAllocation,
               &stagingBuffer.pMappedData);
  memcpy(stagingBuffer.pMappedData, pData, size);
  vmaUnmapMemory(vmaAllocator, stagingBuffer.vmaAllocation);
  stagingBuffer.pMappedData = nullptr;

  util::CopyToBuffer(singleTimeBuffer, size, 0, stagingBuffer.vkHandle,
                     dstOffset, dstBuffer);

  util::EndSingleTimeCommandBuffer(vkDevice, singleTimeBuffer, commandPool,
                                   vkTransferQueue);

  DestroyBuffer(stagingBuffer);
}

void VkContext::CopyToImage(VulkanImage image, const void *pData,
                            VkCommandPool commandPool) {
  VkCommandBuffer singleTimeBuffer =
      util::BeginSingleTimeCommandBuffer(vkDevice, commandPool);

  VulkanBuffer stagingBuffer{};
  VkDeviceSize size = image.vkExtents.width * image.vkExtents.height *
                      4; // Assuming 4 component
  CreateVmaBuffer(stagingBuffer, size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                  VMA_MEMORY_USAGE_UNKNOWN, 0,
                  VK_MEMORY_PROPERTY_HOST_COHERENT_BIT |
                      VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
  vmaMapMemory(vmaAllocator, stagingBuffer.vmaAllocation,
               &stagingBuffer.pMappedData);
  memcpy(stagingBuffer.pMappedData, pData, size);
  vmaUnmapMemory(vmaAllocator, stagingBuffer.vmaAllocation);
  stagingBuffer.pMappedData = nullptr;

  {
    VkImageMemoryBarrier2 imageBarrier{
        VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2};
    imageBarrier.srcStageMask = VK_PIPELINE_STAGE_2_NONE;
    imageBarrier.srcAccessMask = VK_ACCESS_2_NONE;
    imageBarrier.dstStageMask = VK_PIPELINE_STAGE_2_COPY_BIT;
    imageBarrier.dstAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT;
    imageBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    imageBarrier.image = image.vkHandle;
    imageBarrier.subresourceRange.aspectMask = HasDepthOrStencil(image.format)
                                                   ? VK_IMAGE_ASPECT_DEPTH_BIT
                                                   : VK_IMAGE_ASPECT_COLOR_BIT;
    imageBarrier.subresourceRange.baseMipLevel = 0;
    imageBarrier.subresourceRange.levelCount = 1;
    imageBarrier.subresourceRange.baseArrayLayer = 0;
    imageBarrier.subresourceRange.layerCount = 1;

    VkDependencyInfo dependencyInfo{VK_STRUCTURE_TYPE_DEPENDENCY_INFO};
    dependencyInfo.dependencyFlags = 0;
    dependencyInfo.imageMemoryBarrierCount = 1;
    dependencyInfo.pImageMemoryBarriers = &imageBarrier;

    vkCmdPipelineBarrier2(singleTimeBuffer, &dependencyInfo);
  }

  util::CopyToImage(singleTimeBuffer, image.vkHandle, image.format,
                    image.vkExtents, 0, stagingBuffer.vkHandle);

  {
    VkImageMemoryBarrier2 imageBarrier{
        VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2};
    imageBarrier.srcStageMask = VK_PIPELINE_STAGE_2_COPY_BIT;
    imageBarrier.srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT;
    imageBarrier.dstStageMask = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
    imageBarrier.dstAccessMask = VK_ACCESS_2_SHADER_SAMPLED_READ_BIT;
    imageBarrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    imageBarrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    imageBarrier.image = image.vkHandle;
    imageBarrier.subresourceRange.aspectMask = HasDepthOrStencil(image.format)
                                                   ? VK_IMAGE_ASPECT_DEPTH_BIT
                                                   : VK_IMAGE_ASPECT_COLOR_BIT;
    imageBarrier.subresourceRange.baseMipLevel = 0;
    imageBarrier.subresourceRange.levelCount = 1;
    imageBarrier.subresourceRange.baseArrayLayer = 0;
    imageBarrier.subresourceRange.layerCount = 1;

    VkDependencyInfo dependencyInfo{VK_STRUCTURE_TYPE_DEPENDENCY_INFO};
    dependencyInfo.dependencyFlags = 0;
    dependencyInfo.imageMemoryBarrierCount = 1;
    dependencyInfo.pImageMemoryBarriers = &imageBarrier;

    vkCmdPipelineBarrier2(singleTimeBuffer, &dependencyInfo);
  }

  util::EndSingleTimeCommandBuffer(vkDevice, singleTimeBuffer, commandPool,
                                   vkGraphicsQueue);

  DestroyBuffer(stagingBuffer);
}

} // namespace hlx
