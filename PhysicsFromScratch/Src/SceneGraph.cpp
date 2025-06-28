#include "SceneGraph.hpp"
#include "Physics/Broadphase.hpp"
#include "Physics/Contact.hpp"
#include "Physics/Intersections.hpp"
#include <Profiler.hpp>
// Vendor
#include <SDL3/SDL_events.h>
#include <SDL3/SDL_keycode.h>
#include <imgui.h>
#include <stb_image.h>

#define MAX_BODIES 300

struct RayDebugPushConstant {
  Mat4 viewProj;
  Vec4 rayPositions[2];
};

static RayDebugPushConstant rayPushConstant{};

hlx::VulkanPipeline createRayDebugPipeline(hlx::VkContext &context);

Vec3 GetRayFromMouse(float mouseX, float mouseY, int screenWidth,
                     int screenHeight, const Mat4 &viewMatrix,
                     const Mat4 &projectionMatrix) {
  // Convert screen space to NDC [-1, 1]
  float x = (2.0f * mouseX) / screenWidth - 1.0f;
  float y = (2.0f * mouseY) / screenHeight - 1.0f;
  glm::vec4 rayClip = glm::vec4(x, y, -1.0f, 1.0f);

  // Clip space to view space
  glm::vec4 rayEye = glm::inverse(projectionMatrix) * rayClip;
  rayEye = glm::vec4(rayEye.x, rayEye.y, -1.0f, 0.0f); // Direction vector

  // View space to world space
  glm::vec3 rayWorld =
      glm::normalize(glm::vec3(glm::inverse(viewMatrix) * rayEye));
  return rayWorld;
}

bool RayIntersectsSphere(const glm::vec3 &rayOrigin, const glm::vec3 &rayDir,
                         const glm::vec3 &sphereCenter, float sphereRadius,
                         float &t) {
  glm::vec3 L = sphereCenter - rayOrigin;
  float tca = glm::dot(L, rayDir);
  float d2 = glm::dot(L, L) - tca * tca;
  float radius2 = sphereRadius * sphereRadius;

  if (d2 > radius2)
    return false;
  float thc = sqrt(radius2 - d2);
  t = tca - thc; // distance to intersection
  return true;
}

SceneGraph::SceneGraph(hlx::VkContext &ctx, u32 maxEntityCount,
                       VkCommandPool vkTransferCommandPool,
                       VkCommandPool vkGraphicsCommandPool) {
  names.reserve(maxEntityCount);
  bodies.reserve(maxEntityCount);

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

  // Ray Debug Pipeline
  m_RayDebugPipeline = createRayDebugPipeline(ctx);

  // Buffers
  std::vector<u32> indices;
  std::vector<Vertex> vertices;

  GenerateSphere(vertices, indices, 1.f, 128, 128);
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

  // Allocate temp contacts buffer
  m_pTempContacts =
      static_cast<Contact *>(malloc(sizeof(Contact) * MAX_BODIES * MAX_BODIES));
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
  vkDestroyPipelineLayout(ctx.vkDevice, m_RayDebugPipeline.vkPipelineLayout,
                          ctx.vkAllocationCallbacks);
  vkDestroyPipeline(ctx.vkDevice, m_RayDebugPipeline.vkHandle,
                    ctx.vkAllocationCallbacks);

  ctx.DestroyBuffer(m_VertexBuffer);
  ctx.DestroyBuffer(m_IndexBuffer);

  free(m_pTempContacts);
}

void SceneGraph::TogglePhysics() {
  m_SimulatePhysics = !m_SimulatePhysics;
  // TODO: Maybe do this only when physics simulation is reset
  if (!m_SimulatePhysics) {
    for (size_t i = 0; i < bodies.size(); ++i) {
      bodies[i].linearVelocity = Vec3(0.f);
    }
  }
}

i32 CompareContacts(const void *p1, const void *p2) {
  Contact a = *(Contact *)p1;
  Contact b = *(Contact *)p2;
  if (a.timeOfImpact < b.timeOfImpact) {
    return -1;
  }
  if (a.timeOfImpact == b.timeOfImpact) {
    return 0;
  }
  return 1;
}

void SceneGraph::Update(const f32 dt_Sec) {
  HELIX_PROFILER_FUNCTION_COLOR();
  if (!m_SimulatePhysics)
    return;
  for (size_t i = 0; i < bodies.size(); i++) {
    HELIX_PROFILER_ZONE("Apply Gravity", HELIX_PROFILER_COLOR_BARRIER)
    Body &body = bodies[i];
    // Calculate impulse due to graivty
    // Impulse (J) = Mass (m) * Acceleration (g) * dTime (dt)
    f32 mass = 1.f / body.invMass;
    Vec3 impulseGravity = Vec3(0.f, -gravity, 0.f) * mass * dt_Sec;
    body.ApplyImpulseLinear(impulseGravity);
    HELIX_PROFILER_ZONE_END()
  }

  // BroadPhase
  std::vector<CollisionPair> collisionPairs;
  BroadPhase(bodies.data(), (int)bodies.size(), collisionPairs, dt_Sec);

  //
  // NarrowPhase (perform actual collision detection)
  //
  int numContacts = 0;
  const int maxContacts = bodies.size() * bodies.size();
  HELIX_PROFILER_ZONE("NarrowPhase", HELIX_PROFILER_COLOR_BARRIER)
  for (int i = 0; i < collisionPairs.size(); i++) {
    const CollisionPair &pair = collisionPairs[i];
    Body *bodyA = &bodies[pair.a];
    Body *bodyB = &bodies[pair.b];
    // Skip body pairs with infinite mass
    if (0.0f == bodyA->invMass && 0.0f == bodyB->invMass) {
      continue;
    }
    Contact contact;
    if (Intersect(bodyA, bodyB, dt_Sec, contact)) {
      m_pTempContacts[numContacts] = contact;
      numContacts++;
    }
  }
  HELIX_PROFILER_ZONE_END()

  // Sort the times of impact from first to last
  if (numContacts > 1) {
    HELIX_PROFILER_ZONE("Sort TOI", HELIX_PROFILER_COLOR_BARRIER)
    qsort(m_pTempContacts, numContacts, sizeof(Contact), CompareContacts);
    HELIX_PROFILER_ZONE_END()
  }

  // Apply ballistic impulses
  float accumulatedTime = 0.0f;
  HELIX_PROFILER_ZONE("Apply Ballistic Impulses", HELIX_PROFILER_COLOR_BARRIER)
  for (int i = 0; i < numContacts; i++) {
    Contact &contact = m_pTempContacts[i];
    const float dt = contact.timeOfImpact - accumulatedTime;
    // Position update
    HELIX_PROFILER_ZONE("Apply Ballistic Impulses::Update Bodies", 0xffa500)
    for (int j = 0; j < bodies.size(); j++) {
      bodies[j].Update(dt);
    }
    HELIX_PROFILER_ZONE_END()
    ResolveContact(contact);
    accumulatedTime += dt;
  }
  HELIX_PROFILER_ZONE_END()

  // Update the positions for the rest of this frame’s time
  const float timeRemaining = dt_Sec - accumulatedTime;
  if (timeRemaining > 0.0f) {
    HELIX_PROFILER_ZONE("Update remaining positions",
                        HELIX_PROFILER_COLOR_BARRIER)
    for (int i = 0; i < bodies.size(); i++) {
      bodies[i].Update(timeRemaining);
    }
    HELIX_PROFILER_ZONE_END()
  }
}

void SceneGraph::HandleEvents(const SDL_Event *pEvent, SDL_Window *pWindow,
                              hlx::Camera *pCamera) {
  switch (pEvent->type) {
  case SDL_EVENT_MOUSE_BUTTON_DOWN: {
    if (pEvent->button.button == BUTTON_LEFT) {
      HTRACE("SceneGraph: [x]: {}, [y]: {}", pEvent->button.x,
             pEvent->button.y);
      i32 width = 0, height = 0;
      SDL_GetWindowSize(pWindow, &width, &height);

      const Vec3 &rayOrigin = pCamera->GetPosition();
      Vec3 rayDir =
          GetRayFromMouse(pEvent->button.x, pEvent->button.y, width, height,
                          pCamera->GetView(), pCamera->GetProjection());

      // rayPushConstant.rayPositions[1] =
      //     Vec4(glm::normalize(rayDir) * 40.f + rayOrigin, 1.f);
      bool intersected = false;
      f32 closestT = std::numeric_limits<f32>::max();
      for (size_t i = 0; i < bodies.size(); ++i) {
        f32 t;
        Body &body = bodies[i];
        if (RayIntersectsSphere(rayOrigin, rayDir, body.transform.GetPosition(),
                                body.transform.GetScale().x, t)) {
          if (t > 0.f && t < closestT) {
            m_SelectedObject = i;
            closestT = t;
            rayPushConstant.rayPositions[0] = Vec4(rayOrigin, 1.f);
            rayPushConstant.rayPositions[1] =
                Vec4(body.transform.GetPosition(), 1.f);
          }
        }
      }
    }
  } break;
  default:
    break;
  };
}

void SceneGraph::AddSphere(Body body) {
  std::string name = "Sphere_" + std::to_string(names.size());
  names.push_back(name);

  bodies.push_back(body);
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
  for (size_t i = 0; i < bodies.size(); ++i) {
    push.model = bodies[i].transform.GetMat4();

    vkCmdPushConstants(cb, m_SpherePipeline.vkPipelineLayout,
                       VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(push), &push);
    // TODO: Instanced rendering
    vkCmdDrawIndexed(cb, m_IndexCount, 1, 0, 0, 0);
  }

  vkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_GRAPHICS,
                    m_RayDebugPipeline.vkHandle);

  rayPushConstant.viewProj = push.viewProj;
  rayPushConstant.rayPositions[0];
  rayPushConstant.rayPositions[1];

  vkCmdPushConstants(cb, m_RayDebugPipeline.vkPipelineLayout,
                     VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(rayPushConstant),
                     &rayPushConstant);
  // TODO: Instanced rendering
  vkCmdDraw(cb, 2, 1, 0, 0);

  ImGuiWindowFlags winFlags = ImGuiWindowFlags_NoResize |
                              // Imgui
                              ImGuiWindowFlags_NoMove |
                              ImGuiWindowFlags_MenuBar;

  ImGui::SetNextWindowPos(ImVec2(0, 0), ImGuiCond_FirstUseEver);
  ImGui::SetNextWindowSize(ImVec2(388, 588), ImGuiCond_Always);
  if (ImGui::Begin("Editor", nullptr, winFlags)) {
    // Menu Bar ////////////////////////////////////////////////////////////////
    if (ImGui::BeginMenuBar()) {
      if (ImGui::BeginMenu("Scene")) {
        if (ImGui::MenuItem("Add Sphere")) {
          Body body{};
          body.transform.SetPosition(Vec3(0.f, 10.f, 0.f));
          body.transform.SetRotation(Quat(1.f, 0.f, 0.f, 0.f));
          body.linearVelocity = Vec3(0.f, 0.f, 0.f);
          body.invMass = 1.f;
          body.elasticity = 0.f;
          body.friction = 0.5f;
          AddSphere(body);
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
      Body &body = bodies[m_SelectedObject];
      Transform &transform = body.transform;
      if (m_SimulatePhysics)
        ImGui::BeginDisabled();

      Vec3 position = transform.GetPosition();
      ImGui::InputFloat3("Position", &position.x, "%.3f");
      if (ImGui::IsItemDeactivatedAfterEdit()) {
        transform.SetPosition(position);
      }

      Vec3 eulerRadians = glm::eulerAngles(transform.GetRotation());
      Vec3 eulerDegrees = glm::degrees(eulerRadians);
      ImGui::InputFloat3("Rotation(Degrees)", &eulerDegrees.x, "%.3f");
      if (ImGui::IsItemDeactivatedAfterEdit()) {
        transform.SetRotation(Quat(glm::radians(eulerDegrees)));
      }

      // Scaling is uniform
      f32 currentScale = transform.GetScale().x;
      ImGui::InputFloat("Scale", &currentScale, 0.f, 0.f, "%.3f");
      if (ImGui::IsItemDeactivatedAfterEdit()) {
        transform.SetScale(Vec3(currentScale));
      }

      // Mass
      f32 mass = 1.f / body.invMass;
      ImGui::InputFloat("Mass", &mass, 0.f, 0.f, "%.3f");
      if (ImGui::IsItemDeactivatedAfterEdit()) {
        body.invMass = 1.f / mass;
      }

      // Elasticity
      ImGui::SliderFloat("Elasticity", &body.elasticity, 0.f, 1.f, "%.3f");

      // Friction
      ImGui::SliderFloat("Friction", &body.friction, 0.f, 1.f, "%.3f");

      if (m_SimulatePhysics)
        ImGui::EndDisabled();

      ImGui::BeginDisabled();
      // linear Velocity
      ImGui::InputFloat3("Linear Velocity", &body.linearVelocity.x, "%.3f");
      // Angular Velocity
      ImGui::InputFloat3("Angular Velocity", &body.angularVelocity.x, "%.3f");
      ImGui::Text("Angle of rotation: %.3f",
                  glm::degrees(glm::length(body.angularVelocity)));
      ImGui::EndDisabled();
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

hlx::VulkanPipeline createRayDebugPipeline(hlx::VkContext &ctx) {
  hlx::VulkanPipeline pipeline{};
  // Pipeline
  HASSERT(hlx::CompileShader(SHADER_PATH, "RayDebug.vert", "RayDebug_vert.spv",
                             VK_SHADER_STAGE_VERTEX_BIT));
  HASSERT(hlx::CompileShader(SHADER_PATH, "RayDebug.frag", "RayDebug_frag.spv",
                             VK_SHADER_STAGE_FRAGMENT_BIT));

  auto vertShaderCode = hlx::ReadFile(SHADER_PATH "/Spirv/RayDebug_vert.spv");
  auto fragShaderCode = hlx::ReadFile(SHADER_PATH "/Spirv/RayDebug_frag.spv");

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
  inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_LINE_LIST;
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
  VkPushConstantRange pushConst{};
  pushConst.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
  pushConst.offset = 0;
  pushConst.size = sizeof(RayDebugPushConstant);
  VkPipelineLayoutCreateInfo pipelineLayoutInfo{
      VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
  pipelineLayoutInfo.setLayoutCount = 0;
  pipelineLayoutInfo.pSetLayouts = nullptr;
  pipelineLayoutInfo.pushConstantRangeCount = 1;
  pipelineLayoutInfo.pPushConstantRanges = &pushConst;

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
