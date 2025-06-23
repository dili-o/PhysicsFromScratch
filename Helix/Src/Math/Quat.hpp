#pragma once

#include "Matrix.hpp"

#include <glm/gtx/quaternion.hpp>

using Quat = glm::quat;

struct Transform {
  Vec3 position{0.f};
  Quat rotation = Quat(1.0f, 0.0f, 0.0f, 0.0f);
  Vec3 scale{1.f};

  Mat4 GetMat4() const {
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
