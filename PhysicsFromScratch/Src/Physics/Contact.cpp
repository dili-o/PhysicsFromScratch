#include "Contact.hpp"

void ResolveContact(Contact &contact) {
  Body *bodyA = contact.bodyA;
  Body *bodyB = contact.bodyB;

  const Vec3 &ptOnA = contact.ptOnA_WorldSpace;
  const Vec3 &ptOnB = contact.ptOnB_WorldSpace;

  const f32 elasticityA = bodyA->elasticity;
  const f32 elasticityB = bodyB->elasticity;
  const f32 elasticity = elasticityA * elasticityB;

  const Mat3 invWorldInertiaA = bodyA->GetInverseInertiaTensorWorldSpace();
  const Mat3 invWorldInertiaB = bodyB->GetInverseInertiaTensorWorldSpace();

  // Calculate collision impulse
  const Vec3 &n = contact.normalAB;
  const Vec3 ra = ptOnA - bodyA->GetCenterOfMassWorldSpace();
  const Vec3 rb = ptOnB - bodyB->GetCenterOfMassWorldSpace();

  const Vec3 angularJA = glm::cross(invWorldInertiaA * glm::cross(ra, n), ra);
  const Vec3 angularJB = glm::cross(invWorldInertiaB * glm::cross(rb, n), rb);
  const float angularFactor = glm::dot(angularJA + angularJB, n);

  // Get the world space velocity of the motion and rotation
  const Vec3 velA =
      bodyA->linearVelocity + glm::cross(bodyA->angularVelocity, ra);
  const Vec3 velB =
      bodyB->linearVelocity + glm::cross(bodyB->angularVelocity, rb);

  // Calculate the collision impulse
  const Vec3 vab = velA - velB;
  const float ImpulseJ = (1.0f + elasticity) * glm::dot(vab, n) /
                         (bodyA->invMass + bodyB->invMass + angularFactor);
  const Vec3 vectorImpulseJ = n * ImpulseJ;

  bodyA->ApplyImpulse(ptOnA, vectorImpulseJ * -1.0f);
  bodyB->ApplyImpulse(ptOnB, vectorImpulseJ * 1.0f);

  // Calculate the impulse caused by friction
  const float frictionA = bodyA->friction;
  const float frictionB = bodyB->friction;
  const float friction = frictionA * frictionB;
  // Find the normal direction of the velocity with respect to the normal of the
  // collision
  const Vec3 velNorm = n * glm::dot(n, vab);
  // Find the tangent direction of the velocity with respect to the normal of
  // the collision
  const Vec3 velTang = vab - velNorm;
  // Get the tangential velocities relative to the other body
  if (glm::length2(velTang) > 1e-6f) {
    Vec3 relativeVelTang = glm::normalize(relativeVelTang);
    const Vec3 inertiaA =
        glm::cross(invWorldInertiaA * glm::cross(ra, relativeVelTang), ra);
    const Vec3 inertiaB =
        glm::cross(invWorldInertiaB * glm::cross(rb, relativeVelTang), rb);
    const float invInertia = glm::dot(inertiaA + inertiaB, relativeVelTang);
    // Calculate the tangential impulse for friction
    const float reducedMass =
        1.0f / (bodyA->invMass + bodyB->invMass + invInertia);
    const Vec3 impulseFriction = velTang * reducedMass * friction;
    // Apply kinetic friction
    bodyA->ApplyImpulse(ptOnA, impulseFriction * -1.0f);
    bodyB->ApplyImpulse(ptOnB, impulseFriction * 1.0f);
  }

  // Resolve Positions
  const f32 tA = bodyA->invMass / (bodyA->invMass + bodyB->invMass);
  const f32 tB = bodyB->invMass / (bodyA->invMass + bodyB->invMass);
  const Vec3 ds = contact.ptOnB_WorldSpace - contact.ptOnA_WorldSpace;
  bodyA->transform.position += ds * tA;
  bodyB->transform.position -= ds * tB;
}
