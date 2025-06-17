#include "Defines.hpp"
#include <Assert.hpp>
#include <Camera.hpp>
#include <Platform.hpp>
#include <SceneGraph.hpp>
#include <Vulkan/ImguiBackend.hpp>
#include <Vulkan/VulkanTypes.hpp>
#include <Vulkan/VulkanUtils.hpp>
// Vendor
#include <SDL3/SDL.h>
#include <SDL3/SDL_events.h>

static bool pOpen = true;

hlx::VulkanPipeline createBackgroundPipeline(hlx::VkContext &ctx);

int main() {
  hlx::Logger logger;
  hlx::Platform platform = hlx::Platform("PhysicsFromScratch");
  hlx::VkContext ctx;
  ctx.Init(platform.GetWindowHandle());
  hlx::Camera camera(Vec3(0.f), 0.1f, 3000.f);

  const u32 kMaxFramesInFlight = 3;

  std::array<VkFence, kMaxFramesInFlight> inFlightFences;
  std::array<VkSemaphore, kMaxFramesInFlight> renderFinishedSemaphores;
  std::array<VkSemaphore, kMaxFramesInFlight> imageAvailableSemaphores;
  VkCommandPool vkGraphicsCommandPool;
  VkCommandPool vkTransferCommandPool;
  std::array<VkCommandBuffer, kMaxFramesInFlight> vkGraphicsCommandBuffers;

  // Sync Objects and Command Buffers
  VkFenceCreateInfo fenceInfo{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
  fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;
  VkSemaphoreCreateInfo semaphoreInfo{VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
  for (size_t i = 0; i < kMaxFramesInFlight; ++i) {
    VK_CHECK(vkCreateFence(ctx.vkDevice, &fenceInfo, ctx.vkAllocationCallbacks,
                           &inFlightFences[i]));
    VK_CHECK(vkCreateSemaphore(ctx.vkDevice, &semaphoreInfo,
                               ctx.vkAllocationCallbacks,
                               &renderFinishedSemaphores[i]));
    VK_CHECK(vkCreateSemaphore(ctx.vkDevice, &semaphoreInfo,
                               ctx.vkAllocationCallbacks,
                               &imageAvailableSemaphores[i]));
  }

  // Command Pool and Buffers
  VkCommandPoolCreateInfo poolInfo{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
  poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
  poolInfo.queueFamilyIndex =
      ctx.queueFamilyIndices.graphicsFamilyIndex.value();
  VK_CHECK(vkCreateCommandPool(ctx.vkDevice, &poolInfo, nullptr,
                               &vkGraphicsCommandPool));
  poolInfo.queueFamilyIndex =
      ctx.queueFamilyIndices.transferFamilyIndex.value();
  VK_CHECK(vkCreateCommandPool(ctx.vkDevice, &poolInfo, nullptr,
                               &vkTransferCommandPool));

  VkCommandBufferAllocateInfo allocInfo{
      VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
  allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
  allocInfo.commandBufferCount = kMaxFramesInFlight;
  allocInfo.commandPool = vkGraphicsCommandPool;

  VK_CHECK(vkAllocateCommandBuffers(ctx.vkDevice, &allocInfo,
                                    vkGraphicsCommandBuffers.data()));

  hlx::ImguiBackend imguiBackend(&ctx, platform.GetWindowHandle(),
                                 vkGraphicsCommandPool);
  SceneGraph sceneGraph(ctx, 100, vkTransferCommandPool, vkGraphicsCommandPool);
  Transform transform{};
  transform.position = Vec3(0.f, 1.f, 0.f);
  sceneGraph.AddSphere(transform);
  transform.position = Vec3(0.f, -1000.f, 0.f);
  transform.scale = Vec3(1000.f);
  sceneGraph.AddSphere(transform);

  hlx::VulkanPipeline backgroundPipeline = createBackgroundPipeline(ctx);

  hlx::VulkanImage depthImage{};
  hlx::VulkanImageView depthImageView{};
  VkImageCreateInfo imageInfo = hlx::init::ImageCreateInfo(
      {ctx.swapchain.vkExtents.width, ctx.swapchain.vkExtents.height, 1}, 1,
      VK_FORMAT_D32_SFLOAT, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT);
  VmaAllocationCreateInfo vmaInfo =
      hlx::init::VmaAllocationInfo(VMA_MEMORY_USAGE_GPU_ONLY);
  hlx::util::CreateVmaImage(ctx.vmaAllocator, imageInfo, vmaInfo, depthImage);
  ctx.SetResourceName(VK_OBJECT_TYPE_IMAGE,
                      reinterpret_cast<u64>(depthImage.vkHandle), "DepthImage");
  VkImageViewCreateInfo viewInfo = hlx::init::ImageViewCreateInfo(
      depthImage.vkHandle, depthImage.format, VK_IMAGE_ASPECT_DEPTH_BIT, 1);
  hlx::util::CreateImageView(ctx.vkDevice, ctx.vkAllocationCallbacks, viewInfo,
                             depthImageView);

  u32 currentFrame = 0;
  bool frameResized = false;
  // The quit flag
  bool quit{false};
  // The event data
  SDL_Event e;
  SDL_zero(e);
  f32 deltaTime;
  u64 lastTime = SDL_GetPerformanceCounter();
  f64 targetFrameTimeMS = 1000.0 / 60.0;

  while (quit == false) {
    u64 currentTime = SDL_GetPerformanceCounter();
    f32 deltaTime = (currentTime - lastTime) /
                    static_cast<float>(SDL_GetPerformanceFrequency());
    f64 frameStartTimeMS = platform.GetAbsoluteTimeMS();

    lastTime = currentTime;
    // Get event data
    while (SDL_PollEvent(&e)) {
      if (imguiBackend.HandleEvents(&e)) {
        continue;
      }
      // If event is quit type
      if (e.type == SDL_EVENT_QUIT) {
        // End the main loop
        quit = true;
      }
      camera.HandleEvents(&e, platform.GetWindowHandle());

      if (e.type == SDL_EVENT_WINDOW_RESIZED) {
        frameResized = true;
      }
      if (e.type == SDL_EVENT_WINDOW_MINIMIZED) {
        bool maximized = false;
        while (!maximized) {
          SDL_WaitEvent(&e);
          maximized = (e.type == SDL_EVENT_WINDOW_RESIZED ||
                       e.type == SDL_EVENT_WINDOW_RESTORED);
        }
        frameResized = true;
      }
    }

    camera.Update(deltaTime);
    sceneGraph.Update(deltaTime);

    // Renderloop
    vkWaitForFences(ctx.vkDevice, 1, &inFlightFences[currentFrame], VK_TRUE,
                    UINT64_MAX);

    uint32_t imageIndex;
    VkResult result = vkAcquireNextImageKHR(
        ctx.vkDevice, ctx.swapchain.vkHandle, UINT64_MAX,
        imageAvailableSemaphores[currentFrame], VK_NULL_HANDLE, &imageIndex);

    if (result == VK_ERROR_OUT_OF_DATE_KHR) {
      hlx::RecreateSwapchain(ctx.vkPhysicalDevice, ctx.vkDevice,
                             ctx.vkAllocationCallbacks, ctx.vkSurface,
                             ctx.swapchain);
      hlx::util::DestroyVmaImage(ctx.vmaAllocator, depthImage);
      hlx::util::DestroyImageView(ctx.vkDevice, ctx.vkAllocationCallbacks,
                                  depthImageView);
      VkImageCreateInfo imageInfo = hlx::init::ImageCreateInfo(
          {ctx.swapchain.vkExtents.width, ctx.swapchain.vkExtents.height, 1}, 1,
          VK_FORMAT_D32_SFLOAT, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT);
      VmaAllocationCreateInfo vmaInfo =
          hlx::init::VmaAllocationInfo(VMA_MEMORY_USAGE_GPU_ONLY);
      hlx::util::CreateVmaImage(ctx.vmaAllocator, imageInfo, vmaInfo,
                                depthImage);
      VkImageViewCreateInfo viewInfo = hlx::init::ImageViewCreateInfo(
          depthImage.vkHandle, depthImage.format, VK_IMAGE_ASPECT_DEPTH_BIT, 1);
      hlx::util::CreateImageView(ctx.vkDevice, ctx.vkAllocationCallbacks,
                                 viewInfo, depthImageView);
      continue;
    } else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
      throw std::runtime_error("failed to acquire swap chain image!");
    }

    vkResetFences(ctx.vkDevice, 1, &inFlightFences[currentFrame]);

    vkResetCommandBuffer(vkGraphicsCommandBuffers[currentFrame], 0);

    VkCommandBufferBeginInfo beginInfo{
        VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    VK_CHECK(vkBeginCommandBuffer(vkGraphicsCommandBuffers[currentFrame],
                                  &beginInfo));

    {
      VkImageMemoryBarrier2 imageBarrier{
          VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2};
      imageBarrier.srcStageMask =
          VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
      imageBarrier.srcAccessMask = VK_ACCESS_2_NONE;
      imageBarrier.dstStageMask =
          VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
      imageBarrier.dstAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
      imageBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
      imageBarrier.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
      imageBarrier.image = ctx.swapchain.vkImages[imageIndex];
      imageBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
      imageBarrier.subresourceRange.baseMipLevel = 0;
      imageBarrier.subresourceRange.levelCount = 1;
      imageBarrier.subresourceRange.baseArrayLayer = 0;
      imageBarrier.subresourceRange.layerCount = 1;

      VkImageMemoryBarrier2 imageBarrierDepth{
          VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2};
      imageBarrierDepth.srcStageMask =
          VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT;
      imageBarrierDepth.srcAccessMask =
          depthImage.currentLayout == VK_IMAGE_LAYOUT_UNDEFINED
              ? VK_ACCESS_2_NONE
              : VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
      imageBarrierDepth.dstStageMask =
          VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT;
      imageBarrierDepth.dstAccessMask =
          VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
      imageBarrierDepth.oldLayout = depthImage.currentLayout;
      imageBarrierDepth.newLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;
      imageBarrierDepth.image = depthImage.vkHandle;
      imageBarrierDepth.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
      imageBarrierDepth.subresourceRange.baseMipLevel = 0;
      imageBarrierDepth.subresourceRange.levelCount = 1;
      imageBarrierDepth.subresourceRange.baseArrayLayer = 0;
      imageBarrierDepth.subresourceRange.layerCount = 1;

      VkImageMemoryBarrier2 imageBarriers[] = {imageBarrier, imageBarrierDepth};
      VkDependencyInfo dependencyInfo{VK_STRUCTURE_TYPE_DEPENDENCY_INFO};
      dependencyInfo.dependencyFlags = 0;
      dependencyInfo.imageMemoryBarrierCount = ArraySize(imageBarriers);
      dependencyInfo.pImageMemoryBarriers = imageBarriers;

      vkCmdPipelineBarrier2(vkGraphicsCommandBuffers[currentFrame],
                            &dependencyInfo);
      depthImage.currentLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;
    }

    VkViewport viewport2{};
    viewport2.x = 0.0f;
    viewport2.y = 0.0f;
    viewport2.width = static_cast<f32>(ctx.swapchain.vkExtents.width);
    viewport2.height = static_cast<f32>(ctx.swapchain.vkExtents.height);
    viewport2.minDepth = 0.0f;
    viewport2.maxDepth = 1.0f;
    vkCmdSetViewport(vkGraphicsCommandBuffers[currentFrame], 0, 1, &viewport2);

    VkRect2D rect{};
    rect.offset = {0, 0};
    rect.extent = {ctx.swapchain.vkExtents.width,
                   ctx.swapchain.vkExtents.height};
    vkCmdSetScissor(vkGraphicsCommandBuffers[currentFrame], 0, 1, &rect);

    VkRenderingAttachmentInfo colorAttachmentInfo{
        VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO};
    colorAttachmentInfo.imageView = ctx.swapchain.vkImageViews[imageIndex];
    colorAttachmentInfo.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    colorAttachmentInfo.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachmentInfo.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachmentInfo.clearValue = {{{0.f, 0.f, 0.1f, 1.0f}}};
    colorAttachmentInfo.resolveMode = VK_RESOLVE_MODE_NONE;
    colorAttachmentInfo.pNext = nullptr;
    colorAttachmentInfo.resolveImageLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    colorAttachmentInfo.resolveImageView = VK_NULL_HANDLE;

    VkRenderingAttachmentInfo depthAttachmentInfo{
        VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO};
    depthAttachmentInfo.imageView = depthImageView.vkHandle;
    depthAttachmentInfo.imageLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;
    depthAttachmentInfo.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depthAttachmentInfo.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    depthAttachmentInfo.clearValue.color = {{0.0f, 0.0f, 0.0f, 1.0f}};
    depthAttachmentInfo.clearValue.depthStencil = {1.f, 0};
    depthAttachmentInfo.resolveMode = VK_RESOLVE_MODE_NONE;
    depthAttachmentInfo.pNext = nullptr;
    depthAttachmentInfo.resolveImageLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    depthAttachmentInfo.resolveImageView = VK_NULL_HANDLE;

    VkRenderingInfo renderInfo{VK_STRUCTURE_TYPE_RENDERING_INFO};
    renderInfo.layerCount = 1;
    renderInfo.renderArea = {
        {0, 0},
        {ctx.swapchain.vkExtents.width, ctx.swapchain.vkExtents.height}};
    renderInfo.viewMask = 0;
    renderInfo.colorAttachmentCount = 1;
    renderInfo.pColorAttachments = &colorAttachmentInfo;
    renderInfo.pDepthAttachment = &depthAttachmentInfo;
    renderInfo.pStencilAttachment = nullptr;

    vkCmdBeginRendering(vkGraphicsCommandBuffers[currentFrame], &renderInfo);

    vkCmdBindPipeline(vkGraphicsCommandBuffers[currentFrame],
                      VK_PIPELINE_BIND_POINT_GRAPHICS,
                      backgroundPipeline.vkHandle);
    vkCmdDraw(vkGraphicsCommandBuffers[currentFrame], 3, 1, 0, 0);

    imguiBackend.BeginFrame();

    sceneGraph.Render(vkGraphicsCommandBuffers[currentFrame], camera);

    // ImGui::ShowDemoWindow(&show_demo);
    imguiBackend.RenderFrame(vkGraphicsCommandBuffers[currentFrame],
                             currentFrame);

    vkCmdEndRendering(vkGraphicsCommandBuffers[currentFrame]);

    {
      VkImageMemoryBarrier2 imageBarrier{
          VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2};
      imageBarrier.srcStageMask =
          VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
      imageBarrier.srcAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
      imageBarrier.dstStageMask =
          VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
      imageBarrier.dstAccessMask = VK_ACCESS_2_NONE;
      imageBarrier.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
      imageBarrier.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
      imageBarrier.image = ctx.swapchain.vkImages[imageIndex];
      imageBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
      imageBarrier.subresourceRange.baseMipLevel = 0;
      imageBarrier.subresourceRange.levelCount = 1;
      imageBarrier.subresourceRange.baseArrayLayer = 0;
      imageBarrier.subresourceRange.layerCount = 1;

      VkDependencyInfo dependencyInfo{VK_STRUCTURE_TYPE_DEPENDENCY_INFO};
      dependencyInfo.dependencyFlags = 0;
      dependencyInfo.imageMemoryBarrierCount = 1;
      dependencyInfo.pImageMemoryBarriers = &imageBarrier;

      vkCmdPipelineBarrier2(vkGraphicsCommandBuffers[currentFrame],
                            &dependencyInfo);
    }

    VK_CHECK(vkEndCommandBuffer(vkGraphicsCommandBuffers[currentFrame]));

    VkSubmitInfo submitInfo{VK_STRUCTURE_TYPE_SUBMIT_INFO};

    VkSemaphore waitSemaphores[] = {imageAvailableSemaphores[currentFrame]};
    VkPipelineStageFlags waitStages[] = {
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = waitSemaphores;
    submitInfo.pWaitDstStageMask = waitStages;

    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &vkGraphicsCommandBuffers[currentFrame];

    VkSemaphore signalSemaphores[] = {renderFinishedSemaphores[currentFrame]};
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = signalSemaphores;

    VK_CHECK(vkQueueSubmit(ctx.vkGraphicsQueue, 1, &submitInfo,
                           inFlightFences[currentFrame]));
    VkPresentInfoKHR presentInfo{VK_STRUCTURE_TYPE_PRESENT_INFO_KHR};
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = signalSemaphores;

    VkSwapchainKHR swapChains[] = {ctx.swapchain.vkHandle};
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = swapChains;

    presentInfo.pImageIndices = &imageIndex;

    result = vkQueuePresentKHR(ctx.vkGraphicsQueue, &presentInfo);

    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR ||
        frameResized) {
      frameResized = false;
      hlx::RecreateSwapchain(ctx.vkPhysicalDevice, ctx.vkDevice,
                             ctx.vkAllocationCallbacks, ctx.vkSurface,
                             ctx.swapchain);
      hlx::util::DestroyVmaImage(ctx.vmaAllocator, depthImage);
      hlx::util::DestroyImageView(ctx.vkDevice, ctx.vkAllocationCallbacks,
                                  depthImageView);
      VkImageCreateInfo imageInfo = hlx::init::ImageCreateInfo(
          {ctx.swapchain.vkExtents.width, ctx.swapchain.vkExtents.height, 1}, 1,
          VK_FORMAT_D32_SFLOAT, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT);
      VmaAllocationCreateInfo vmaInfo =
          hlx::init::VmaAllocationInfo(VMA_MEMORY_USAGE_GPU_ONLY);
      hlx::util::CreateVmaImage(ctx.vmaAllocator, imageInfo, vmaInfo,
                                depthImage);
      VkImageViewCreateInfo viewInfo = hlx::init::ImageViewCreateInfo(
          depthImage.vkHandle, depthImage.format, VK_IMAGE_ASPECT_DEPTH_BIT, 1);
      hlx::util::CreateImageView(ctx.vkDevice, ctx.vkAllocationCallbacks,
                                 viewInfo, depthImageView);
    } else if (result != VK_SUCCESS) {
      throw std::runtime_error("failed to present swap chain image!");
    }
    currentFrame = (currentFrame + 1) % kMaxFramesInFlight;
  }

  vkDeviceWaitIdle(ctx.vkDevice);

  for (u32 i = 0; i < kMaxFramesInFlight; ++i) {
    vkDestroyFence(ctx.vkDevice, inFlightFences[i], ctx.vkAllocationCallbacks);
    vkDestroySemaphore(ctx.vkDevice, imageAvailableSemaphores[i],
                       ctx.vkAllocationCallbacks);
    vkDestroySemaphore(ctx.vkDevice, renderFinishedSemaphores[i],
                       ctx.vkAllocationCallbacks);
  }

  hlx::util::DestroyVmaImage(ctx.vmaAllocator, depthImage);
  hlx::util::DestroyImageView(ctx.vkDevice, ctx.vkAllocationCallbacks,
                              depthImageView);

  imguiBackend.Shutdown();
  sceneGraph.Shutdown(ctx);

  vkDestroyPipelineLayout(ctx.vkDevice, backgroundPipeline.vkPipelineLayout,
                          ctx.vkAllocationCallbacks);
  vkDestroyPipeline(ctx.vkDevice, backgroundPipeline.vkHandle,
                    ctx.vkAllocationCallbacks);

  vkDestroyCommandPool(ctx.vkDevice, vkTransferCommandPool,
                       ctx.vkAllocationCallbacks);
  vkDestroyCommandPool(ctx.vkDevice, vkGraphicsCommandPool,
                       ctx.vkAllocationCallbacks);

  ctx.Shutdown();
}

hlx::VulkanPipeline createBackgroundPipeline(hlx::VkContext &ctx) {
  hlx::VulkanPipeline pipeline{};
  // Pipeline
  HASSERT(hlx::CompileShader(SHADER_PATH, "Background.vert",
                             "Background_vert.spv",
                             VK_SHADER_STAGE_VERTEX_BIT));
  HASSERT(hlx::CompileShader(SHADER_PATH, "Background.frag",
                             "Background_frag.spv",
                             VK_SHADER_STAGE_FRAGMENT_BIT));

  auto vertShaderCode = hlx::ReadFile(SHADER_PATH "/Spirv/Background_vert.spv");
  auto fragShaderCode = hlx::ReadFile(SHADER_PATH "/Spirv/Background_frag.spv");

  VkShaderModule vertShaderModule =
      hlx::CreateShaderModule(ctx.vkDevice, vertShaderCode);
  VkShaderModule fragShaderModule =
      hlx::CreateShaderModule(ctx.vkDevice, fragShaderCode);

  VkPipelineShaderStageCreateInfo vertShaderStageInfo{
      .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
      .stage = VK_SHADER_STAGE_VERTEX_BIT,
      .module = vertShaderModule,
      .pName = "main"};

  VkPipelineShaderStageCreateInfo fragShaderStageInfo{
      .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
      .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
      .module = fragShaderModule,
      .pName = "main"};

  VkPipelineShaderStageCreateInfo shaderStages[] = {vertShaderStageInfo,
                                                    fragShaderStageInfo};

  // Dynamic State
  VkPipelineDynamicStateCreateInfo dynamicState =
      hlx::init::PipelineDynamicStateCreateInfo();
  // Input Assembly
  VkPipelineInputAssemblyStateCreateInfo inputAssembly =
      hlx::init::PipelineInputAssemblyStateCreateInfo();
  // Viewport State
  VkPipelineViewportStateCreateInfo viewportState =
      hlx::init::PipelineViewportStateCreateInfo();
  // Rasterizer State
  VkPipelineRasterizationStateCreateInfo rasterizer =
      hlx::init::PipelineRasterizationStateCreateInfo();
  // Multisampling State
  VkPipelineMultisampleStateCreateInfo multisampling =
      hlx::init::PipelineMultisampleStateCreateInfo();
  // Depth and Stencil State
  VkPipelineDepthStencilStateCreateInfo depthStencil =
      hlx::init::PipelineDepthStencilStateCreateInfo(false);
  // Color Blend State
  VkPipelineColorBlendAttachmentState colorBlendAttachment{};
  colorBlendAttachment.colorWriteMask =
      VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
      VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
  colorBlendAttachment.blendEnable = VK_FALSE;
  colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
  colorBlendAttachment.dstColorBlendFactor =
      VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
  colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
  colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
  colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
  colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;

  VkPipelineColorBlendStateCreateInfo color_blending{
      VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO};
  color_blending.logicOpEnable = VK_FALSE;
  color_blending.logicOp = VK_LOGIC_OP_CLEAR;
  color_blending.attachmentCount = 1;
  color_blending.pAttachments = &colorBlendAttachment;

  // Pipeline Layout
  VkPipelineLayoutCreateInfo pipelineLayoutInfo{
      VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
  pipelineLayoutInfo.setLayoutCount = 0;
  pipelineLayoutInfo.pSetLayouts = nullptr;
  pipelineLayoutInfo.pushConstantRangeCount = 0;
  pipelineLayoutInfo.pPushConstantRanges = nullptr;

  VK_CHECK(vkCreatePipelineLayout(ctx.vkDevice, &pipelineLayoutInfo,
                                  ctx.vkAllocationCallbacks,
                                  &pipeline.vkPipelineLayout));
  // Dynamic Rendering
  VkFormat colorFormats[1] = {ctx.swapchain.vkSurfaceFormat.format};

  VkPipelineRenderingCreateInfo pipelineRenderingCreateInfo{
      VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO_KHR};
  pipelineRenderingCreateInfo.pNext = VK_NULL_HANDLE;
  pipelineRenderingCreateInfo.colorAttachmentCount = 1;
  pipelineRenderingCreateInfo.pColorAttachmentFormats = colorFormats;
  pipelineRenderingCreateInfo.depthAttachmentFormat = VK_FORMAT_D32_SFLOAT;
  pipelineRenderingCreateInfo.stencilAttachmentFormat = VK_FORMAT_UNDEFINED;

  VkPipelineVertexInputStateCreateInfo vertexInputState{
      VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};

  VkGraphicsPipelineCreateInfo pipelineCreateInfo{
      VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};
  pipelineCreateInfo.stageCount = 2;
  pipelineCreateInfo.pStages = shaderStages;
  pipelineCreateInfo.pVertexInputState = &vertexInputState;
  pipelineCreateInfo.pInputAssemblyState = &inputAssembly;
  pipelineCreateInfo.pViewportState = &viewportState;
  pipelineCreateInfo.pRasterizationState = &rasterizer;
  pipelineCreateInfo.pMultisampleState = &multisampling;
  pipelineCreateInfo.pDepthStencilState = &depthStencil;
  pipelineCreateInfo.pColorBlendState = &color_blending;
  pipelineCreateInfo.pDynamicState = &dynamicState;
  pipelineCreateInfo.layout = pipeline.vkPipelineLayout;
  pipelineCreateInfo.renderPass = VK_NULL_HANDLE;
  pipelineCreateInfo.pNext = &pipelineRenderingCreateInfo;

  VK_CHECK(vkCreateGraphicsPipelines(
      ctx.vkDevice, VK_NULL_HANDLE, 1, &pipelineCreateInfo,
      ctx.vkAllocationCallbacks, &pipeline.vkHandle));
  vkDestroyShaderModule(ctx.vkDevice, fragShaderModule, nullptr);
  vkDestroyShaderModule(ctx.vkDevice, vertShaderModule, nullptr);

  return pipeline;
}
