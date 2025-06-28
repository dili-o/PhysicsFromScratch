#pragma once
#include "Bounds.hpp"
#include <Defines.hpp>
#include <Math/Transform.hpp>

const f32 gravity = 10.f;

struct Body {
  Transform transform;
  Vec3 centerOfMass; // This is in local space
  Vec3 linearVelocity;
  Vec3 angularVelocity;
  f32 invMass;
  f32 elasticity;
  f32 friction;

  Vec3 GetCenterOfMassWorldSpace() const;
  inline Vec3 GetCenterOfMassModelSpace() const { return centerOfMass; }

  Vec3 WorldSpaceToBodySpace(const Vec3 &worldPos) const;
  Vec3 BodySpaceToWorldSpace(const Vec3 &worldPt) const;

  Mat3 GetInertiaTensorBodySpace() const;
  Mat3 GetInertiaTensorWorldSpace() const;
  Mat3 GetInverseInertiaTensorBodySpace() const;
  Mat3 GetInverseInertiaTensorWorldSpace() const;

  void ApplyImpulse(const Vec3 &impulsePoint, const Vec3 &impulse);
  void ApplyImpulseLinear(const Vec3 &impulse);
  void ApplyImpulseAngular(const Vec3 &impulse);

  void Update(const f32 dt_Sec);
};

inline Mat3 GetSphereInertiaTensor(const Body *body) {
  Mat3 tensor(0.f);
  tensor[0][0] =
      2.f * body->transform.GetScale().x * body->transform.GetScale().x / 5.f;
  tensor[1][1] =
      2.f * body->transform.GetScale().x * body->transform.GetScale().x / 5.f;
  tensor[2][2] =
      2.f * body->transform.GetScale().x * body->transform.GetScale().x / 5.f;
  return tensor;
}

Bounds GetSphereBounds(const Body *pBody, const Vec3 &pos, const Quat &orient);
Bounds GetSphereBounds(const Body *pBody);
