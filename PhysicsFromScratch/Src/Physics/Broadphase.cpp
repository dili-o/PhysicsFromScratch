#include "Broadphase.hpp"
#include <Profiler.hpp>

struct PsuedoBody {
  i32 id;
  f32 value;
  bool ismin;
};

i32 CompareSAP(const void *a, const void *b) {
  const PsuedoBody *ea = (const PsuedoBody *)a;
  const PsuedoBody *eb = (const PsuedoBody *)b;

  if (ea->value < eb->value) {
    return -1;
  }
  return 1;
}

void SortBodiesBounds(const Body *bodies, const i32 num,
                      PsuedoBody *sortedArray, const f32 dt_sec) {
  Vec3 axis = glm::normalize(Vec3(1, 1, 1));

  for (i32 i = 0; i < num; i++) {
    const Body *body = &bodies[i];
    Bounds bounds = GetSphereBounds(body, body->transform.GetPosition(),
                                    body->transform.GetRotation());

    // Expand the bounds by the linear velocity
    bounds.Expand(bounds.mins + body->linearVelocity * dt_sec);
    bounds.Expand(bounds.maxs + body->linearVelocity * dt_sec);

    const f32 epsilon = 0.01f;
    bounds.Expand(bounds.mins + Vec3(-1, -1, -1) * epsilon);
    bounds.Expand(bounds.maxs + Vec3(1, 1, 1) * epsilon);

    sortedArray[i * 2 + 0].id = i;
    sortedArray[i * 2 + 0].value = glm::dot(axis, bounds.mins);
    sortedArray[i * 2 + 0].ismin = true;

    sortedArray[i * 2 + 1].id = i;
    sortedArray[i * 2 + 1].value = glm::dot(axis, bounds.maxs);
    sortedArray[i * 2 + 1].ismin = false;
  }

  qsort(sortedArray, num * 2, sizeof(PsuedoBody), CompareSAP);
}

void BuildPairs(std::vector<CollisionPair> &collisionPairs,
                const PsuedoBody *sortedBodies, const i32 num) {
  collisionPairs.clear();

  // Now that the bodies are sorted, build the collision pairs
  for (i32 i = 0; i < num * 2; i++) {
    const PsuedoBody &a = sortedBodies[i];
    if (!a.ismin) {
      continue;
    }

    CollisionPair pair;
    pair.a = a.id;

    for (i32 j = i + 1; j < num * 2; j++) {
      const PsuedoBody &b = sortedBodies[j];
      // if we've hit the end of the a element, then we're done creating pairs
      // with a
      if (b.id == a.id) {
        break;
      }

      if (!b.ismin) {
        continue;
      }

      pair.b = b.id;
      collisionPairs.push_back(pair);
    }
  }
}

void SweepAndPrune1D(const Body *bodies, const i32 num,
                     std::vector<CollisionPair> &finalPairs, const f32 dt_sec) {
  PsuedoBody *sortedBodies = (PsuedoBody *)alloca(sizeof(PsuedoBody) * num * 2);

  SortBodiesBounds(bodies, num, sortedBodies, dt_sec);
  BuildPairs(finalPairs, sortedBodies, num);
}

void BroadPhase(const Body *bodies, const i32 num,
                std::vector<CollisionPair> &finalPairs, const f32 dt_sec) {
  HELIX_PROFILER_FUNCTION_COLOR();
  finalPairs.clear();

  SweepAndPrune1D(bodies, num, finalPairs, dt_sec);
}
