#include "Body.hpp"
#include "glm/geometric.hpp"

Vec3 Body::GetCenterOfMassWorldSpace() const {
  Vec4 pos = transform.GetMat4() * Vec4(centerOfMass, 1.f);
  return Vec3(pos.x, pos.y, pos.z);
}

Vec3 Body::WorldSpaceToBodySpace(const Vec3 &worldPos) const {
  Vec3 tmp = worldPos - GetCenterOfMassWorldSpace();
  Quat inverseOrient = glm::inverse(transform.GetRotation());
  return inverseOrient * tmp;
}

Vec3 Body::BodySpaceToWorldSpace(const Vec3 &worldPt) const {
  Vec3 worldSpace =
      GetCenterOfMassWorldSpace() + (transform.GetRotation() * worldPt);
  return worldSpace;
}

void Body::ApplyImpulse(const Vec3 &impulsePoint, const Vec3 &impulse) {
  if (invMass == 0.f)
    return;
  ApplyImpulseLinear(impulse);

  Vec3 positionVector = impulsePoint - GetCenterOfMassWorldSpace();
  Vec3 angularImpulse = glm::cross(positionVector, impulse);
  ApplyImpulseAngular(angularImpulse);
}

void Body::ApplyImpulseLinear(const Vec3 &impulse) {
  // TODO: No need to check this since ApplyImpulse does
  if (invMass == 0.f)
    return; // Infinite mass
  // Impulse (J) = mass (m) * dVelocity (dv)
  // dVelocity (dv) = Impulse (J) / mass (m)
  linearVelocity += impulse * invMass;
}

void Body::ApplyImpulseAngular(const Vec3 &impulse) {
  // TODO: No need to check this since ApplyImpulse does
  if (0.0f == invMass) {
    return;
  }
  // L = I w= r x p
  // dL = I dw= r x J
  // =>dw= I^−1 * ( r x J )
  angularVelocity += GetInverseInertiaTensorWorldSpace() * impulse;
  const float maxAngularSpeed =
      30.0f; // 30 rad/s is fast enough for us. But feel free to adjust .
  if (glm::length2(angularVelocity) > maxAngularSpeed * maxAngularSpeed) {
    angularVelocity = glm::normalize(angularVelocity);
    angularVelocity *= maxAngularSpeed;
  }
}

void Body::Update(const f32 dt_Sec) {
  // Do not update objects with infinite mass
  if (invMass == 0.f)
    return;
  // TODO: Make local copies of transform's rotation and position
  transform.SetPosition(transform.GetPosition() + linearVelocity * dt_Sec);

  Vec3 centerOfMassWorld = GetCenterOfMassWorldSpace();
  Vec3 cmToWorldPos = transform.GetPosition() - centerOfMassWorld;
  // Total Torque is equal to external applied torques + internal torque
  // (precession) T= T_external +omega x I * omega T_external =0 because it was
  // applied in the collision response function T= Ia =w x I * w a = I^−1 ( w x
  // I * w )
  Mat3 iTensor = GetInertiaTensorWorldSpace();
  Vec3 angularAcceleration =
      iTensor * (glm::cross(angularVelocity, iTensor * angularVelocity));
  // Update angularVelocity with the acceleration cause by internal torque
  // dw = a * dt
  angularVelocity += angularAcceleration * dt_Sec;

  // The amount of the angular velocity applied in a frame is equal to how much
  // the object should rotate
  Vec3 angleAxisRotation = angularVelocity * dt_Sec;
  Vec3 angleAxisRotationNorm = Vec3(0.f);
  if (glm::length2(angleAxisRotation) > 1e-6f)
    angleAxisRotationNorm = glm::normalize(angleAxisRotation);
  Quat quatRotation =
      glm::angleAxis(glm::length(angleAxisRotation), angleAxisRotationNorm);
  // Update the rotation and Normalize to account for floating-point precession
  // errors
  transform.SetRotation(glm::normalize(quatRotation * transform.GetRotation()));
  // Update the position to rotate around it's center of mass
  transform.SetPosition(centerOfMassWorld +
                        glm::rotate(quatRotation, cmToWorldPos));
}

Mat3 Body::GetInertiaTensorBodySpace() const {
  return GetSphereInertiaTensor(this) * invMass;
}

Mat3 Body::GetInertiaTensorWorldSpace() const {
  Mat3 inertiaTensor = GetSphereInertiaTensor(this) * invMass;
  Mat3 orient = glm::toMat3(transform.GetRotation());
  return orient * inertiaTensor * glm::transpose(orient);
}

Mat3 Body::GetInverseInertiaTensorBodySpace() const {
  // TODO: Right now we are assuming all bodies are spheres
  Mat3 inertiaTensor = GetSphereInertiaTensor(this);
  Mat3 invInertiaTensor = glm::inverse(inertiaTensor) * invMass;
  return invInertiaTensor;
}

Mat3 Body::GetInverseInertiaTensorWorldSpace() const {
  // TODO: Right now we are assuming all bodies are spheres
  Mat3 inertiaTensor = GetSphereInertiaTensor(this);
  Mat3 invInertiaTensor = glm::inverse(inertiaTensor) * invMass;
  Mat3 orient = glm::toMat3(transform.GetRotation());
  invInertiaTensor = orient * invInertiaTensor * glm::transpose(orient);
  return invInertiaTensor;
}

Bounds GetSphereBounds(const Body *pBody, const Vec3 &pos, const Quat &orient) {
  Bounds tmp;
  f32 radius = pBody->transform.GetScale().x;
  tmp.mins = Vec3(-radius) + pos;
  tmp.maxs = Vec3(radius) + pos;
  return tmp;
}

Bounds GetSphereBounds(const Body *pBody) {
  Bounds tmp;
  f32 radius = pBody->transform.GetScale().x;
  tmp.mins = Vec3(-radius);
  tmp.maxs = Vec3(radius);
  return tmp;
}
