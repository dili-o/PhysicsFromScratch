#pragma once

#include <Camera.hpp>
#include <Defines.hpp>
#include <Math/Quat.hpp>
#include <Vulkan/VulkanTypes.hpp>
#include <string>
#include <vector>

struct Transform {
  Vec3 position{0.f};
  Quat rotation = Quat(1.0f, 0.0f, 0.0f, 0.0f);
  Vec3 scale{1.f};

  Mat4 GetMat4() {
    VkPushConstantRange pushConstant{};
    pushConstant.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    pushConstant.offset = 0;
    pushConstant.size = sizeof(Mat4);
    return glm::translate(Mat4(1.f), position) * glm::toMat4(rotation) *
           glm::scale(Mat4(1.f), scale);
  }

  void SetTransform(Mat4 &matrix) {
    position = Vec3(matrix[3]);

    scale.x = glm::length(Vec3(matrix[0]));
    scale.y = glm::length(Vec3(matrix[1]));
    scale.z = glm::length(Vec3(matrix[2]));

    Mat3 rotationMatrix;
    rotationMatrix[0] = Vec3(matrix[0]) / scale.x;
    rotationMatrix[1] = Vec3(matrix[1]) / scale.y;
    rotationMatrix[2] = Vec3(matrix[2]) / scale.z;
    rotation = glm::quat_cast(rotationMatrix);
  }
};

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
  void AddSphere(Transform transform);
  void Render(VkCommandBuffer cb, hlx::Camera &camera);

public:
  std::vector<Transform> transforms;
  std::vector<std::string> names;

private:
  hlx::VulkanPipeline m_SpherePipeline;
  VkDescriptorSetLayout m_VkSetLayout;
  VkDescriptorUpdateTemplate vkUpdateTemplate;
  VkDescriptorImageInfo imageDescriptor;
  hlx::VulkanImage m_SphereImage;
  hlx::VulkanImageView m_SphereImageView;
  VkSampler vkSampler;
  hlx::VulkanBuffer m_VertexBuffer;
  hlx::VulkanBuffer m_IndexBuffer;
  u32 m_IndexCount;

  u32 m_SelectedObject{UINT32_MAX};
};

void GenerateSphere(std::vector<Vertex> &outVertices,
                    std::vector<u32> &outIndices, float radius,
                    u32 segments = 64, u32 rings = 32);
