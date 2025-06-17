
#include "Intersections.hpp"
#include "Body.hpp"
bool Intersect(const Body *bodyA, const Body *bodyB) {
  const Vec3 ab = bodyA->transform.position - bodyB->transform.position;

  // TODO: Assumes all bodies are spheres
  const f32 radiusAB = bodyA->transform.scale.x + bodyB->transform.scale.x;
  return (glm::length2(ab) <= (radiusAB * radiusAB));
}
