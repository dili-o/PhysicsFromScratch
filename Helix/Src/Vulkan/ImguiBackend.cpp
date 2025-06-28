#include "ImguiBackend.hpp"
#include "Assert.hpp"
#include "Vulkan/VulkanUtils.hpp"
// Vendor
#define IMGUI_IMPL_VULKAN_USE_VOLK
#include <SDL3/SDL_events.h>
#include <glm/glm.hpp>
#include <imgui/backends/imgui_impl_sdl3.h>
#include <imgui/backends/imgui_impl_vulkan.h>
#include <imgui_internal.h>

static uint32_t s_vb_size = 665536, s_ib_size = 665536;

namespace hlx {
ImguiBackend::ImguiBackend(VkContext *pCtx, SDL_Window *pWindow,
                           VkCommandPool vkTransferPool) {
  HASSERT(pCtx);
  ctx = pCtx;

  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGui::StyleColorsDark();

  ImGui_ImplSDL3_InitForVulkan(pWindow);

  ImGuiIO &io = ImGui::GetIO();
  io.BackendRendererName = "Helix";
  io.BackendFlags |= ImGuiBackendFlags_RendererHasVtxOffset;
  // io.ConfigFlags |= ImGuiConfigFlags_DockingEnable; // Enable Docking

  // Load font texture atlas //////////////////////////////////////////////////
  u8 *pixels;
  i32 width, height;
  // Load as RGBA 32-bits (75% of the memory is wasted, but default font is so
  // small) because it is more likely to be compatible with user's existing
  // shaders. If your ImTextureId represent a higher-level concept than just a
  // GL texture id, consider calling GetTexDataAsAlpha8() instead to save on GPU
  // memory.
  io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);

  VkImageCreateInfo imageInfo = hlx::init::ImageCreateInfo(
      {(u32)width, (u32)height, 1}, 1, VK_FORMAT_R8G8B8A8_UNORM,
      VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT);
  VmaAllocationCreateInfo vmaInfo =
      hlx::init::VmaAllocationInfo(VMA_MEMORY_USAGE_GPU_ONLY);
  ctx->CreateVmaImage(fontImage, imageInfo, vmaInfo);
  ctx->SetResourceName(VK_OBJECT_TYPE_IMAGE,
                       reinterpret_cast<u64>(fontImage.vkHandle),
                       "ImguiFontImage");
  ctx->CopyToImage(fontImage, pixels, vkTransferPool);

  VkImageViewCreateInfo viewInfo = hlx::init::ImageViewCreateInfo(
      fontImage.vkHandle, fontImage.format, VK_IMAGE_ASPECT_COLOR_BIT, 1);
  hlx::util::CreateImageView(ctx->vkDevice, ctx->vkAllocationCallbacks,
                             viewInfo, fontImageView);

  // Create vertex and index buffers //////////////////////////////////////////
  u32 framesInFlight = 3;
  vertexBuffers.resize(framesInFlight);
  indexBuffers.resize(framesInFlight);
  for (u32 i = 0; i < framesInFlight; ++i) {
    ctx->CreateVmaBuffer(
        vertexBuffers[i], s_vb_size,
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        VMA_MEMORY_USAGE_UNKNOWN, VMA_ALLOCATION_CREATE_MAPPED_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
            VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        "ImguiVertexBuffer");

    ctx->CreateVmaBuffer(
        indexBuffers[i], s_ib_size,
        VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        VMA_MEMORY_USAGE_UNKNOWN, VMA_ALLOCATION_CREATE_MAPPED_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
            VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        "ImguiIndexBuffer");
  }

  // Create Descriptor ///////////////////////////////////////////////////////
  VkDescriptorSetLayoutBinding fontImageBinding{};
  fontImageBinding.binding = 0;
  fontImageBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
  fontImageBinding.descriptorCount = 1;
  fontImageBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

  VkDescriptorSetLayoutCreateInfo layoutInfo{
      VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
  layoutInfo.flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_PUSH_DESCRIPTOR_BIT_KHR;
  layoutInfo.bindingCount = 1;
  layoutInfo.pBindings = &fontImageBinding;
  VK_CHECK(vkCreateDescriptorSetLayout(
      ctx->vkDevice, &layoutInfo, ctx->vkAllocationCallbacks, &vkSetLayout));

  VkSamplerCreateInfo samplerInfo{VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
  samplerInfo.minFilter = VK_FILTER_NEAREST;
  samplerInfo.magFilter = VK_FILTER_NEAREST;
  samplerInfo.minLod = 0.f;
  samplerInfo.maxLod = VK_LOD_CLAMP_NONE;
  samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
  samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
  samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
  samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
  samplerInfo.anisotropyEnable = VK_FALSE;
  samplerInfo.maxAnisotropy = 0;
  samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
  samplerInfo.unnormalizedCoordinates = VK_FALSE;
  samplerInfo.compareEnable = VK_FALSE;
  samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;

  VK_CHECK(vkCreateSampler(ctx->vkDevice, &samplerInfo,
                           ctx->vkAllocationCallbacks, &vkSampler));
  ctx->SetResourceName(VK_OBJECT_TYPE_SAMPLER, reinterpret_cast<u64>(vkSampler),
                       "ImguiFontSampler");

  imageDescriptor.sampler = vkSampler;
  imageDescriptor.imageView = fontImageView.vkHandle;
  imageDescriptor.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

  // Create Pipeline /////////////////////////////////////////////////////////
  // Pipeline
  HASSERT(hlx::CompileShader(SHADER_PATH, "Imgui.vert", "Imgui_vert.spv",
                             VK_SHADER_STAGE_VERTEX_BIT));
  HASSERT(hlx::CompileShader(SHADER_PATH, "Imgui.frag", "Imgui_frag.spv",
                             VK_SHADER_STAGE_FRAGMENT_BIT));

  auto vertShaderCode = hlx::ReadFile(SHADER_PATH "/Spirv/Imgui_vert.spv");
  auto fragShaderCode = hlx::ReadFile(SHADER_PATH "/Spirv/Imgui_frag.spv");

  VkShaderModule vertShaderModule =
      hlx::CreateShaderModule(ctx->vkDevice, vertShaderCode);
  VkShaderModule fragShaderModule =
      hlx::CreateShaderModule(ctx->vkDevice, fragShaderCode);

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

  // Vertex Input State
  VkVertexInputBindingDescription vertexInputBinding;
  vertexInputBinding.binding = 0;
  vertexInputBinding.stride = 20;
  vertexInputBinding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

  std::vector<VkVertexInputAttributeDescription> vertexInputAttributes = {
      {.location = 0,
       .binding = 0,
       .format = VK_FORMAT_R32G32_SFLOAT,
       .offset = 0},
      {.location = 1,
       .binding = 0,
       .format = VK_FORMAT_R32G32_SFLOAT,
       .offset = 8},
      {.location = 2,
       .binding = 0,
       .format = VK_FORMAT_R8G8B8A8_UNORM,
       .offset = 16}};

  VkPipelineVertexInputStateCreateInfo vertexInputState{
      VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
  vertexInputState.vertexBindingDescriptionCount = 1;
  vertexInputState.pVertexBindingDescriptions = &vertexInputBinding;
  vertexInputState.vertexAttributeDescriptionCount =
      vertexInputAttributes.size();
  vertexInputState.pVertexAttributeDescriptions = vertexInputAttributes.data();

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
      hlx::init::PipelineDepthStencilStateCreateInfo();
  // Color Blend State
  VkPipelineColorBlendAttachmentState colorBlendAttachment{};
  colorBlendAttachment.colorWriteMask =
      VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
      VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
  colorBlendAttachment.blendEnable = VK_TRUE;
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
  VkPushConstantRange pushConstant{};
  pushConstant.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
  pushConstant.offset = 0;
  pushConstant.size = sizeof(glm::vec4);
  VkPipelineLayoutCreateInfo pipelineLayoutInfo{
      VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
  pipelineLayoutInfo.setLayoutCount = 1;
  pipelineLayoutInfo.pSetLayouts = &vkSetLayout;
  pipelineLayoutInfo.pushConstantRangeCount = 1;
  pipelineLayoutInfo.pPushConstantRanges = &pushConstant;

  VK_CHECK(vkCreatePipelineLayout(ctx->vkDevice, &pipelineLayoutInfo,
                                  ctx->vkAllocationCallbacks,
                                  &pipeline.vkPipelineLayout));
  // Dynamic Rendering
  VkFormat colorFormats[1] = {ctx->swapchain.vkSurfaceFormat.format};

  VkPipelineRenderingCreateInfo pipelineRenderingCreateInfo{
      VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO_KHR};
  pipelineRenderingCreateInfo.pNext = VK_NULL_HANDLE;
  pipelineRenderingCreateInfo.colorAttachmentCount = 1;
  pipelineRenderingCreateInfo.pColorAttachmentFormats = colorFormats;
  pipelineRenderingCreateInfo.depthAttachmentFormat = VK_FORMAT_D32_SFLOAT;
  pipelineRenderingCreateInfo.stencilAttachmentFormat = VK_FORMAT_UNDEFINED;

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
      ctx->vkDevice, VK_NULL_HANDLE, 1, &pipelineCreateInfo,
      ctx->vkAllocationCallbacks, &pipeline.vkHandle));
  vkDestroyShaderModule(ctx->vkDevice, fragShaderModule, nullptr);
  vkDestroyShaderModule(ctx->vkDevice, vertShaderModule, nullptr);

  const VkDescriptorUpdateTemplateEntry descriptorUpdateTemplateEntries[1] = {{
      .dstBinding = 0,
      .dstArrayElement = 0,
      .descriptorCount = 1,
      .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
      .offset = 0,
      .stride = 0 // not required if descriptorCount is 1
  }};

  const VkDescriptorUpdateTemplateCreateInfo updateTemplateCreateInfo = {
      .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_UPDATE_TEMPLATE_CREATE_INFO,
      .pNext = NULL,
      .flags = 0,
      .descriptorUpdateEntryCount = ArraySize(descriptorUpdateTemplateEntries),
      .pDescriptorUpdateEntries = descriptorUpdateTemplateEntries,
      .templateType = VK_DESCRIPTOR_UPDATE_TEMPLATE_TYPE_PUSH_DESCRIPTORS_KHR,
      .descriptorSetLayout = VK_NULL_HANDLE, // ignored by given templateType
      .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
      .pipelineLayout = pipeline.vkPipelineLayout,
      .set = 0,
  };

  VK_CHECK(vkCreateDescriptorUpdateTemplate(
      ctx->vkDevice, &updateTemplateCreateInfo, ctx->vkAllocationCallbacks,
      &vkUpdateTemplate));
}

void ImguiBackend::Shutdown() {
  vmaDestroyImage(ctx->vmaAllocator, fontImage.vkHandle,
                  fontImage.vmaAllocation);
  vkDestroyImageView(ctx->vkDevice, fontImageView.vkHandle,
                     ctx->vkAllocationCallbacks);
  vkDestroySampler(ctx->vkDevice, vkSampler, ctx->vkAllocationCallbacks);
  vkDestroyDescriptorSetLayout(ctx->vkDevice, vkSetLayout,
                               ctx->vkAllocationCallbacks);
  vkDestroyDescriptorUpdateTemplate(ctx->vkDevice, vkUpdateTemplate,
                                    ctx->vkAllocationCallbacks);
  vkDestroyPipelineLayout(ctx->vkDevice, pipeline.vkPipelineLayout,
                          ctx->vkAllocationCallbacks);
  vkDestroyPipeline(ctx->vkDevice, pipeline.vkHandle,
                    ctx->vkAllocationCallbacks);

  for (u32 i = 0; i < 3; ++i) {
    ctx->DestroyBuffer(vertexBuffers[i]);
    ctx->DestroyBuffer(indexBuffers[i]);
  }
}

void ImguiBackend::BeginFrame() {
  ImGui_ImplSDL3_NewFrame();
  ImGui::NewFrame();
}

void ImguiBackend::RenderFrame(VkCommandBuffer commandBuffer,
                               u32 currentFrame) {
  // NOTE: Command buffer should already be recording
  ImGui::Render();
  ImDrawData *draw_data = ImGui::GetDrawData();

  // Avoid rendering when minimized, scale coordinates for retina displays
  // (screen coordinates != framebuffer coordinates)
  int fb_width =
      (int)(draw_data->DisplaySize.x * draw_data->FramebufferScale.x);
  int fb_height =
      (int)(draw_data->DisplaySize.y * draw_data->FramebufferScale.y);
  if (fb_width <= 0 || fb_height <= 0)
    return;

  size_t vertex_size = draw_data->TotalVtxCount * sizeof(ImDrawVert);
  size_t index_size = draw_data->TotalIdxCount * sizeof(ImDrawIdx);

  if (vertex_size >= s_vb_size || index_size >= s_ib_size) {
    HERROR("ImGui Backend Error: vertex/index overflow!");
    return;
  }

  if (vertex_size == 0 && index_size == 0) {
    return;
  }

  // Upload vertex and index data
  ImDrawVert *vtx_dst = NULL;
  ImDrawIdx *idx_dst = NULL;

  const VulkanBuffer &vtx_buf = vertexBuffers[currentFrame];
  vtx_dst = (ImDrawVert *)vtx_buf.pMappedData;
  HASSERT(vtx_dst);

  for (int n = 0; n < draw_data->CmdListsCount; n++) {
    const ImDrawList *cmd_list = draw_data->CmdLists[n];
    memcpy(vtx_dst, cmd_list->VtxBuffer.Data,
           cmd_list->VtxBuffer.Size * sizeof(ImDrawVert));
    vtx_dst += cmd_list->VtxBuffer.Size;
  }

  VulkanBuffer &idx_buf = indexBuffers[currentFrame];
  idx_dst = (ImDrawIdx *)idx_buf.pMappedData;
  HASSERT(idx_dst);

  for (int n = 0; n < draw_data->CmdListsCount; n++) {
    const ImDrawList *cmd_list = draw_data->CmdLists[n];
    memcpy(idx_dst, cmd_list->IdxBuffer.Data,
           cmd_list->IdxBuffer.Size * sizeof(ImDrawIdx));
    idx_dst += cmd_list->IdxBuffer.Size;
  }

  VkExtent2D extents{(u32)fb_width, (u32)fb_height};

  VkViewport viewport{};
  viewport.x = 0.0f;
  viewport.y = 0.0f;
  viewport.width = (f32)extents.width;
  viewport.height = (f32)extents.height;
  viewport.minDepth = 0.0f;
  viewport.maxDepth = 1.0f;
  vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

  vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                    pipeline.vkHandle);
  vkCmdPushDescriptorSetWithTemplateKHR(commandBuffer, vkUpdateTemplate,
                                        pipeline.vkPipelineLayout, 0,
                                        &imageDescriptor);

  VkDeviceSize offsets[] = {0};
  vkCmdBindVertexBuffers(commandBuffer, 0, 1,
                         &vertexBuffers[currentFrame].vkHandle, offsets);
  vkCmdBindIndexBuffer(commandBuffer, indexBuffers[currentFrame].vkHandle, 0,
                       VK_INDEX_TYPE_UINT16);

  float scale[2];
  scale[0] = 2.0f / draw_data->DisplaySize.x;
  scale[1] = 2.0f / draw_data->DisplaySize.y;
  float translate[2];
  translate[0] = -1.0f - draw_data->DisplayPos.x * scale[0];
  translate[1] = -1.0f - draw_data->DisplayPos.y * scale[1];

  float uniform[4];
  uniform[0] = scale[0];
  uniform[1] = scale[1];
  uniform[2] = translate[0];
  uniform[3] = translate[1];

  vkCmdPushConstants(commandBuffer, pipeline.vkPipelineLayout,
                     VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(f32) * 4,
                     (void *)uniform);

  // Will project scissor/clipping rectangles into framebuffer space
  ImVec2 clip_off = draw_data->DisplayPos; // (0,0) unless using multi-viewports
  ImVec2 clip_scale =
      draw_data->FramebufferScale; // (1,1) unless using retina display which
                                   // are often (2,2)

  // Render command lists
  int counts = draw_data->CmdListsCount;

  u32 vtx_buffer_offset = 0, index_buffer_offset = 0;
  for (int n = 0; n < counts; n++) {
    const ImDrawList *cmd_list = draw_data->CmdLists[n];

    for (int cmd_i = 0; cmd_i < cmd_list->CmdBuffer.Size; cmd_i++) {
      const ImDrawCmd *pcmd = &cmd_list->CmdBuffer[cmd_i];
      if (pcmd->UserCallback) {
        // User callback (registered via ImDrawList::AddCallback)
        pcmd->UserCallback(cmd_list, pcmd);
      } else {
        // Project scissor/clipping rectangles into framebuffer space
        ImVec4 clip_rect;
        clip_rect.x = (pcmd->ClipRect.x - clip_off.x) * clip_scale.x;
        clip_rect.y = (pcmd->ClipRect.y - clip_off.y) * clip_scale.y;
        clip_rect.z = (pcmd->ClipRect.z - clip_off.x) * clip_scale.x;
        clip_rect.w = (pcmd->ClipRect.w - clip_off.y) * clip_scale.y;

        if (clip_rect.x < fb_width && clip_rect.y < fb_height &&
            clip_rect.z >= 0.0f && clip_rect.w >= 0.0f) {
          // Apply scissor/clipping rectangle
          VkRect2D rect{};
          rect.offset = {(i32)clip_rect.x, (i32)clip_rect.y};
          rect.extent = {(u32)(clip_rect.z - clip_rect.x),
                         (u32)(clip_rect.w - clip_rect.y)};
          vkCmdSetScissor(commandBuffer, 0, 1, &rect);

          vkCmdDrawIndexed(commandBuffer, pcmd->ElemCount, 1,
                           index_buffer_offset + pcmd->IdxOffset,
                           vtx_buffer_offset + pcmd->VtxOffset, 0);
        }
      }
    }
    index_buffer_offset += cmd_list->IdxBuffer.Size;
    vtx_buffer_offset += cmd_list->VtxBuffer.Size;
  }
}

bool ImguiBackend::HandleEvents(void *pEvent) {
  SDL_Event *event = (SDL_Event *)pEvent;
  ImGui_ImplSDL3_ProcessEvent(event);
  if (event->type == SDL_EVENT_QUIT || event->type == SDL_EVENT_WINDOW_RESIZED)
    return false;
  return ImGui::GetCurrentContext()->NavWindow ||
         ImGui::GetIO().WantCaptureMouse;
}

} // namespace hlx
