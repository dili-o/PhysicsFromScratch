#include "Contact.hpp"

void ResolveContact(Contact &contact) {
  Body *bodyA = contact.bodyA;
  Body *bodyB = contact.bodyB;

  const f32 elasticityA = bodyA->elasticity;
  const f32 elasticityB = bodyB->elasticity;
  const f32 elasticity = elasticityA * elasticityB;

  // Calculate collision impulse
  const Vec3 &n = contact.normalAB;
  const Vec3 vab = bodyA->linearVelocity - bodyB->linearVelocity;
  const f32 impulseJ = -(1.f + elasticity) * glm::dot(vab, n) /
                       (bodyA->invMass + bodyB->invMass);
  const Vec3 vectorImpulseJ = n * impulseJ;

  bodyA->ApplyImpulseLinear(vectorImpulseJ);
  bodyB->ApplyImpulseLinear(vectorImpulseJ * -1.f);

  // Resolve Positions
  const f32 tA = bodyA->invMass / (bodyA->invMass + bodyB->invMass);
  const f32 tB = bodyB->invMass / (bodyA->invMass + bodyB->invMass);
  const Vec3 ds = contact.ptOnB_WorldSpace - contact.ptOnA_WorldSpace;
  bodyA->transform.position += ds * tA;
  bodyB->transform.position -= ds * tB;
}
