#include "SceneGraph.hpp"
// Vendor
#include <imgui.h>
#include <stb_image.h>

SceneGraph::SceneGraph(hlx::VkContext &ctx, u32 maxEntityCount,
                       VkCommandPool vkTransferCommandPool,
                       VkCommandPool vkGraphicsCommandPool) {
  transforms.reserve(maxEntityCount);
  names.reserve(maxEntityCount);

  // Create pipeline
  HASSERT(hlx::CompileShader(SHADER_PATH, "Sphere.vert", "Sphere_vert.spv",
                             VK_SHADER_STAGE_VERTEX_BIT));
  HASSERT(hlx::CompileShader(SHADER_PATH, "Sphere.frag", "Sphere_frag.spv",
                             VK_SHADER_STAGE_FRAGMENT_BIT));

  auto vertShaderCode = hlx::ReadFile(SHADER_PATH "/Spirv/Sphere_vert.spv");
  auto fragShaderCode = hlx::ReadFile(SHADER_PATH "/Spirv/Sphere_frag.spv");

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

  // Vertex Input State
  VkVertexInputBindingDescription vertexInputBinding;
  vertexInputBinding.binding = 0;
  vertexInputBinding.stride = sizeof(Vertex);
  vertexInputBinding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

  std::vector<VkVertexInputAttributeDescription> vertexInputAttributes = {
      {.location = 0,
       .binding = 0,
       .format = VK_FORMAT_R32G32B32_SFLOAT,
       .offset = offsetof(Vertex, position)},
      {.location = 1,
       .binding = 0,
       .format = VK_FORMAT_R32G32B32_SFLOAT,
       .offset = offsetof(Vertex, normal)},
      {.location = 2,
       .binding = 0,
       .format = VK_FORMAT_R32G32_SFLOAT,
       .offset = offsetof(Vertex, texcoord)}};

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
      hlx::init::PipelineDepthStencilStateCreateInfo(true);
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
  VkDescriptorSetLayoutBinding sphereImageBinding{};
  sphereImageBinding.binding = 0;
  sphereImageBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
  sphereImageBinding.descriptorCount = 1;
  sphereImageBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

  VkDescriptorSetLayoutCreateInfo layoutInfo{
      VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
  layoutInfo.flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_PUSH_DESCRIPTOR_BIT_KHR;
  layoutInfo.bindingCount = 1;
  layoutInfo.pBindings = &sphereImageBinding;
  VK_CHECK(vkCreateDescriptorSetLayout(
      ctx.vkDevice, &layoutInfo, ctx.vkAllocationCallbacks, &m_VkSetLayout));

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

  VK_CHECK(vkCreateSampler(ctx.vkDevice, &samplerInfo,
                           ctx.vkAllocationCallbacks, &vkSampler));
  ctx.SetResourceName(VK_OBJECT_TYPE_SAMPLER, reinterpret_cast<u64>(vkSampler),
                      "SphereTextureSampler");

  // Create image
  int texWidth, texHeight, texChannels;
  stbi_uc *pixels =
      stbi_load(ASSETS_PATH "/Images/grey_checkerboard.png", &texWidth,
                &texHeight, &texChannels, STBI_rgb_alpha);
  VkDeviceSize imageSize = texWidth * texHeight * 4;

  if (!pixels) {
    throw std::runtime_error("failed to load texture image!");
  }

  VkImageCreateInfo imageInfo = hlx::init::ImageCreateInfo(
      {(u32)texWidth, (u32)texHeight, 1}, 1, VK_FORMAT_R8G8B8A8_UNORM,
      VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT);
  VmaAllocationCreateInfo vmaInfo =
      hlx::init::VmaAllocationInfo(VMA_MEMORY_USAGE_GPU_ONLY);
  ctx.CreateVmaImage(m_SphereImage, imageInfo, vmaInfo);
  ctx.SetResourceName(VK_OBJECT_TYPE_IMAGE,
                      reinterpret_cast<u64>(m_SphereImage.vkHandle),
                      "SphereTextureImage");
  ctx.CopyToImage(m_SphereImage, pixels, vkGraphicsCommandPool);

  VkImageViewCreateInfo viewInfo = hlx::init::ImageViewCreateInfo(
      m_SphereImage.vkHandle, m_SphereImage.format, VK_IMAGE_ASPECT_COLOR_BIT,
      1);
  hlx::util::CreateImageView(ctx.vkDevice, ctx.vkAllocationCallbacks, viewInfo,
                             m_SphereImageView);

  imageDescriptor.sampler = vkSampler;
  imageDescriptor.imageView = m_SphereImageView.vkHandle;
  imageDescriptor.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

  VkPushConstantRange pushConstant{};
  pushConstant.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
  pushConstant.offset = 0;
  pushConstant.size = sizeof(PushConstant);
  VkPipelineLayoutCreateInfo pipelineLayoutInfo{
      VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
  pipelineLayoutInfo.setLayoutCount = 1;
  pipelineLayoutInfo.pSetLayouts = &m_VkSetLayout;
  pipelineLayoutInfo.pushConstantRangeCount = 1;
  pipelineLayoutInfo.pPushConstantRanges = &pushConstant;

  VK_CHECK(vkCreatePipelineLayout(ctx.vkDevice, &pipelineLayoutInfo,
                                  ctx.vkAllocationCallbacks,
                                  &m_SpherePipeline.vkPipelineLayout));
  // Dynamic Rendering
  VkFormat colorFormats[1] = {ctx.swapchain.vkSurfaceFormat.format};

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
  pipelineCreateInfo.layout = m_SpherePipeline.vkPipelineLayout;
  pipelineCreateInfo.renderPass = VK_NULL_HANDLE;
  pipelineCreateInfo.pNext = &pipelineRenderingCreateInfo;

  VK_CHECK(vkCreateGraphicsPipelines(
      ctx.vkDevice, VK_NULL_HANDLE, 1, &pipelineCreateInfo,
      ctx.vkAllocationCallbacks, &m_SpherePipeline.vkHandle));
  vkDestroyShaderModule(ctx.vkDevice, fragShaderModule, nullptr);
  vkDestroyShaderModule(ctx.vkDevice, vertShaderModule, nullptr);

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
      .pipelineLayout = m_SpherePipeline.vkPipelineLayout,
      .set = 0,
  };

  VK_CHECK(vkCreateDescriptorUpdateTemplate(
      ctx.vkDevice, &updateTemplateCreateInfo, ctx.vkAllocationCallbacks,
      &vkUpdateTemplate));

  // Buffers
  std::vector<u32> indices;
  std::vector<Vertex> vertices;

  GenerateSphere(vertices, indices, 1.f);
  m_IndexCount = indices.size();

  u64 vertexBufferSize = sizeof(Vertex) * vertices.size();
  ctx.CreateVmaBuffer(m_VertexBuffer, vertexBufferSize,
                      VK_BUFFER_USAGE_VERTEX_BUFFER_BIT |
                          VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                      VMA_MEMORY_USAGE_GPU_ONLY, 0,
                      VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, "VertexBuffer");

  ctx.CopyToBuffer(m_VertexBuffer.vkHandle, 0, vertexBufferSize,
                   vertices.data(), vkTransferCommandPool);

  // Index buffer
  u64 indexBufferSize = sizeof(u32) * indices.size();
  ctx.CreateVmaBuffer(m_IndexBuffer, indexBufferSize,
                      VK_BUFFER_USAGE_INDEX_BUFFER_BIT |
                          VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                      VMA_MEMORY_USAGE_GPU_ONLY, 0,
                      VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, "IndexBuffer");

  ctx.CopyToBuffer(m_IndexBuffer.vkHandle, 0, indexBufferSize, indices.data(),
                   vkTransferCommandPool);
}

void SceneGraph::Shutdown(hlx::VkContext &ctx) {

  vmaDestroyImage(ctx.vmaAllocator, m_SphereImage.vkHandle,
                  m_SphereImage.vmaAllocation);
  vkDestroyImageView(ctx.vkDevice, m_SphereImageView.vkHandle,
                     ctx.vkAllocationCallbacks);
  vkDestroySampler(ctx.vkDevice, vkSampler, ctx.vkAllocationCallbacks);
  vkDestroyDescriptorSetLayout(ctx.vkDevice, m_VkSetLayout,
                               ctx.vkAllocationCallbacks);
  vkDestroyDescriptorUpdateTemplate(ctx.vkDevice, vkUpdateTemplate,
                                    ctx.vkAllocationCallbacks);
  vkDestroyPipelineLayout(ctx.vkDevice, m_SpherePipeline.vkPipelineLayout,
                          ctx.vkAllocationCallbacks);
  vkDestroyPipeline(ctx.vkDevice, m_SpherePipeline.vkHandle,
                    ctx.vkAllocationCallbacks);
  ctx.DestroyBuffer(m_VertexBuffer);
  ctx.DestroyBuffer(m_IndexBuffer);
}

void SceneGraph::AddSphere(Transform transform) {
  std::string name = "Sphere_" + std::to_string(names.size());
  names.push_back(name);
  transforms.push_back(transform);
}

void SceneGraph::Render(VkCommandBuffer cb, hlx::Camera &camera) {
  vkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_GRAPHICS,
                    m_SpherePipeline.vkHandle);
  VkDeviceSize offsets[] = {0};
  vkCmdBindVertexBuffers(cb, 0, 1, &m_VertexBuffer.vkHandle, offsets);
  vkCmdBindIndexBuffer(cb, m_IndexBuffer.vkHandle, 0, VK_INDEX_TYPE_UINT32);
  vkCmdPushDescriptorSetWithTemplateKHR(cb, vkUpdateTemplate,
                                        m_SpherePipeline.vkPipelineLayout, 0,
                                        &imageDescriptor);
  PushConstant push{};
  push.viewProj = camera.GetProjection() * camera.GetView();
  for (size_t i = 0; i < transforms.size(); ++i) {
    push.model = transforms[i].GetMat4();

    vkCmdPushConstants(cb, m_SpherePipeline.vkPipelineLayout,
                       VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(push), &push);
    // TODO: Instanced rendering
    vkCmdDrawIndexed(cb, m_IndexCount, 1, 0, 0, 0);
  }

  // Imgui
  ImGuiWindowFlags winFlags = ImGuiWindowFlags_NoResize |
                              ImGuiWindowFlags_NoMove |
                              ImGuiWindowFlags_MenuBar;

  ImGui::SetNextWindowPos(ImVec2(0, 0), ImGuiCond_FirstUseEver);
  ImGui::SetNextWindowSize(ImVec2(388, 588), ImGuiCond_Always);
  if (ImGui::Begin("Editor", nullptr, winFlags)) {
    // Menu Bar ////////////////////////////////////////////////////////////////
    if (ImGui::BeginMenuBar()) {
      if (ImGui::BeginMenu("Scene")) {
        if (ImGui::MenuItem("Add Sphere")) {
          Transform transform{};
          AddSphere(transform);
        }
        ImGui::EndMenu();
      }
      ImGui::EndMenuBar();
    }
    // Tree Nodes //////////////////////////////////////////////////////////////
    for (size_t i = 0; i < names.size(); ++i) {
      ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_Leaf;
      flags |= (m_SelectedObject == i) ? ImGuiTreeNodeFlags_Selected : 0;
      if (ImGui::TreeNodeEx(names[i].c_str(), flags)) {
        if (ImGui::IsItemClicked()) {
          m_SelectedObject = i;
        }
        ImGui::TreePop();
      }
    }
    // Node Properties /////////////////////////////////////////////////////////
    ImGui::SeparatorText("Node Properties");
    if (m_SelectedObject == UINT32_MAX) {
      ImGui::Text("No node selected");
    } else {
      Transform &transform = transforms[m_SelectedObject];
      ImGui::InputFloat3("Position", &transform.position.x, "%.3f");

      Vec3 eulerRadians = glm::eulerAngles(transform.rotation);
      Vec3 eulerDegrees = glm::degrees(eulerRadians);

      ImGui::InputFloat3("Rotation(Degrees)", &eulerDegrees.x, "%.3f");
      if (ImGui::IsItemDeactivatedAfterEdit()) {
        transform.rotation = Quat(glm::radians(eulerDegrees));
      }

      // Scaling is uniform
      f32 currentScale = transform.scale.x;
      ImGui::InputFloat("Scale", &currentScale, 0.f, 0.f, "%.3f");
      if (ImGui::IsItemDeactivatedAfterEdit()) {
        transform.scale = Vec3(currentScale);
      }
    }
  }
  ImGui::End();
}

void GenerateSphere(std::vector<Vertex> &outVertices,
                    std::vector<uint32_t> &outIndices, float radius,
                    uint32_t segments, uint32_t rings) {
  outVertices.clear();
  outIndices.clear();

  for (uint32_t y = 0; y <= rings; ++y) {
    float v = float(y) / rings;
    float theta = v * glm::pi<float>();

    for (uint32_t x = 0; x <= segments; ++x) {
      float u = float(x) / segments;
      float phi = u * glm::two_pi<float>();

      float sinTheta = sin(theta);
      float cosTheta = cos(theta);
      float sinPhi = sin(phi);
      float cosPhi = cos(phi);

      Vec3 normal(sinTheta * cosPhi, cosTheta, sinTheta * sinPhi);

      Vec3 position = radius * normal;
      Vec2 texcoord(u, 1.0f - v);

      outVertices.push_back({position, normal, texcoord});
    }
  }

  for (uint32_t y = 0; y < rings; ++y) {
    for (uint32_t x = 0; x < segments; ++x) {
      uint32_t i0 = y * (segments + 1) + x;
      uint32_t i1 = i0 + segments + 1;

      outIndices.push_back(i0);
      outIndices.push_back(i1);
      outIndices.push_back(i0 + 1);

      outIndices.push_back(i0 + 1);
      outIndices.push_back(i1);
      outIndices.push_back(i1 + 1);
    }
  }
}
