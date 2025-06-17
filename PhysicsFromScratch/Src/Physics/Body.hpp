#pragma once
#include <Defines.hpp>
#include <Math/Quat.hpp>

const f32 graivty = 10.f;

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

struct Body {
  Transform transform;
  Vec3 centerOfMass;
  Vec3 linearVelocity;
  f32 invMass;

  inline Vec3 GetCenterOfMassWorldSpace() const {
    Vec4 pos = transform.GetMat4() * Vec4(centerOfMass, 0.f);
    return Vec3(pos.x, pos.y, pos.z);
  }

  inline Vec3 GetCenterOfMassModelSpace() const { return centerOfMass; }

  inline Vec3 WorldSpaceToBodySpace(const Vec3 &worldPos) const {
    Vec3 tmp = worldPos - GetCenterOfMassWorldSpace();
    Quat inverseOrient = glm::inverse(transform.rotation);
    return inverseOrient * tmp;
  }

  inline Vec3 BodySpaceToWorldSpace(const Vec3 &worldPt) const {
    Vec3 worldSpace =
        GetCenterOfMassWorldSpace() + (transform.rotation * worldPt);
    return worldSpace;
  }

  inline void ApplyImpulseLinear(const Vec3 &impulse) {
    if (invMass == 0.f)
      return; // Infinite mass
    // Impulse (J) = mass (m) * dVelocity (dv)
    // dVelocity (dv) = Impulse (J) / mass (m)
    linearVelocity += impulse * invMass;
  }
};
