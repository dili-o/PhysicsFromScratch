
#include "Intersections.hpp"

bool Intersect(Body *bodyA, Body *bodyB, Contact &contact) {
  const Vec3 ab = bodyB->transform.position - bodyA->transform.position;

  contact.bodyA = bodyA;
  contact.bodyB = bodyB;
  contact.normalAB = glm::normalize(ab);

  contact.ptOnA_WorldSpace =
      bodyA->transform.position + contact.normalAB * bodyA->transform.scale.x;
  contact.ptOnB_WorldSpace =
      bodyB->transform.position - contact.normalAB * bodyB->transform.scale.x;

  // TODO: Assumes all bodies are spheres
  const f32 radiusAB = bodyA->transform.scale.x + bodyB->transform.scale.x;
  return (glm::length2(ab) <= (radiusAB * radiusAB));
}
