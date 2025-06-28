
#include "Intersections.hpp"

bool SphereSphereDynamic(const f32 radiusA, const f32 radiusB, const Vec3 &posA,
                         const Vec3 &posB, const Vec3 &velA, const Vec3 &velB,
                         const f32 dt, Vec3 &ptOnA, Vec3 &ptOnB, f32 &toi);

bool Intersect(Body *bodyA, Body *bodyB, f32 dt_Sec, Contact &contact) {
  // TODO: Only spheres for now
  // TODO: Also Try to reduce how often you call getter and setter functions
  Vec3 positionA = bodyA->transform.GetPosition();
  Vec3 positionB = bodyB->transform.GetPosition();
  if (SphereSphereDynamic(bodyA->transform.GetScale().x,
                          bodyB->transform.GetScale().x, positionA, positionB,
                          bodyA->linearVelocity, bodyB->linearVelocity, dt_Sec,
                          contact.ptOnA_WorldSpace, contact.ptOnB_WorldSpace,
                          contact.timeOfImpact)) {
    bodyA->transform.SetPosition(positionA);
    bodyB->transform.SetPosition(positionB);

    contact.bodyA = bodyA;
    contact.bodyB = bodyB;
    // Step bodies forward to get local space collision points
    bodyA->Update(contact.timeOfImpact);
    bodyB->Update(contact.timeOfImpact);
    // Convert world space contacts to local space
    contact.ptOnA_LocalSpace =
        bodyA->WorldSpaceToBodySpace(contact.ptOnA_WorldSpace);
    contact.ptOnB_LocalSpace =
        bodyB->WorldSpaceToBodySpace(contact.ptOnB_WorldSpace);
    contact.normalAB = bodyA->transform.GetPosition() -
                       bodyB->transform.GetPosition(); // TODO: Change to BA?
    contact.normalAB = glm::normalize(contact.normalAB);
    // Unwind time step
    bodyA->Update(-contact.timeOfImpact);
    bodyB->Update(-contact.timeOfImpact);
    // Calculate the separation distance
    Vec3 ab = bodyB->transform.GetPosition() - bodyA->transform.GetPosition();
    float r = glm::length(ab) -
              (bodyA->transform.GetScale().x + bodyB->transform.GetScale().x);
    contact.separationDistance = r;
    return true;
  }
  return false;
}

bool RaySphere(const Vec3 &rayStart, const Vec3 &rayDir,
               const Vec3 &sphereCenter, const f32 sphereRadius, f32 &t1,
               f32 &t2) {
  const Vec3 m = sphereCenter - rayStart;
  const f32 a = glm::dot(rayDir, rayDir);
  const f32 b = glm::dot(m, rayDir);
  const f32 c = glm::dot(m, m) - sphereRadius * sphereRadius;

  const f32 delta = b * b - a * c;
  const f32 invA = 1.0f / a;

  if (delta < 0) {
    // no real solutions exist
    return false;
  }

  const f32 deltaRoot = sqrtf(delta);
  t1 = invA * (b - deltaRoot);
  t2 = invA * (b + deltaRoot);

  return true;
}

bool SphereSphereDynamic(const f32 radiusA, const f32 radiusB, const Vec3 &posA,
                         const Vec3 &posB, const Vec3 &velA, const Vec3 &velB,
                         const f32 dt, Vec3 &ptOnA, Vec3 &ptOnB, f32 &toi) {
  const Vec3 relativeVelocity = velA - velB;
  const Vec3 startPtA = posA;
  const Vec3 endPtA = posA + relativeVelocity * dt;
  const Vec3 rayDir = endPtA - startPtA;
  f32 t0 = 0;
  f32 t1 = 0;
  if (glm::length2(rayDir) < 0.001f * 0.001f) {
    // Ray is too short , just check if already intersecting
    Vec3 ab = posB - posA;
    f32 radius = radiusA + radiusB + 0.001f;
    if (glm::length2(ab) > radius * radius) {
      return false;
    }
  } else if (!RaySphere(posA, rayDir, posB, radiusA + radiusB, t0, t1)) {
    return false;
  }
  // Change from [0,1] range to [0,dt] range
  t0 *= dt;
  t1 *= dt;
  // If the collision is only in the past , then there’s not future collision
  // this frame
  if (t1 < 0.0f) {
    return false;
  }
  // Get the earliest positive time of impact
  toi = (t0 < 0.0f) ? 0.0f : t0;
  // If the earliest collision is too far in the future , then there’s no
  // collision this frame
  if (toi > dt) {
    return false;
  }
  // Get the points on the respective points of collision and return true
  Vec3 newPosA = posA + velA * toi;
  Vec3 newPosB = posB + velB * toi;
  Vec3 ab = newPosB - newPosA;
  if (glm::length2(ab) > 1e-6f)
    ab = glm::normalize(ab);

  ptOnA = newPosA + ab * radiusA;
  ptOnB = newPosB - ab * radiusB;

  return true;
}
