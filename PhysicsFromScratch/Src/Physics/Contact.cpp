#include "Contact.hpp"

void ResolveContact(Contact &contact) {
  Body *bodyA = contact.bodyA;
  Body *bodyB = contact.bodyB;

  bodyA->linearVelocity = Vec3(0.f);
  bodyB->linearVelocity = Vec3(0.f);

  // Resolve Positions
  const f32 tA = bodyA->invMass / (bodyA->invMass + bodyB->invMass);
  const f32 tB = bodyB->invMass / (bodyA->invMass + bodyB->invMass);
  const Vec3 ds = contact.ptOnB_WorldSpace - contact.ptOnA_WorldSpace;
  bodyA->transform.position += ds * tA;
  bodyB->transform.position -= ds * tB;
}
