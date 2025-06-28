#pragma once

#include "Physics/Body.hpp"
#include <Camera.hpp>
#include <Vulkan/VulkanTypes.hpp>
#include <string>
#include <vector>

struct Contact;

struct Vertex {
  Vec3 position;
  Vec3 normal;
  Vec2 texcoord;
};

struct PushConstant {
  Mat4 viewProj;
  Mat4 model;
};

class SceneGraph {
public:
  SceneGraph(hlx::VkContext &ctx, u32 maxEntityCount,
             VkCommandPool vkTransferCommandPool,
             VkCommandPool vkGraphicsCommandPool);

  void Shutdown(hlx::VkContext &ctx);

  void TogglePhysics();
  void Update(const f32 dt_Sec);
  void HandleEvents(const SDL_Event *pEvent, SDL_Window *pWindow,
                    hlx::Camera *pCamera);

  void AddSphere(Body body);
  void Render(VkCommandBuffer cb, hlx::Camera &camera);

public:
  std::vector<std::string> names;
  std::vector<Body> bodies;

private:
  Contact *m_pTempContacts{nullptr};
  hlx::VulkanPipeline m_SpherePipeline;
  hlx::VulkanPipeline m_RayDebugPipeline;
  VkDescriptorSetLayout m_VkSetLayout;
  VkDescriptorUpdateTemplate vkUpdateTemplate;
  VkDescriptorImageInfo imageDescriptor;
  hlx::VulkanImage m_SphereImage;
  hlx::VulkanImageView m_SphereImageView;
  VkSampler vkSampler;
  hlx::VulkanBuffer m_VertexBuffer;
  hlx::VulkanBuffer m_IndexBuffer;
  u32 m_IndexCount;
  bool m_SimulatePhysics = false;

  u32 m_SelectedObject{UINT32_MAX};
};

void GenerateSphere(std::vector<Vertex> &outVertices,
                    std::vector<u32> &outIndices, float radius,
                    u32 segments = 64, u32 rings = 32);
