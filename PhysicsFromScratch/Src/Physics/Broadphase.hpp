#pragma once
#include "Body.hpp"
#include <vector>

struct CollisionPair {
  i32 a;
  i32 b;

  bool operator==(const CollisionPair &rhs) const {
    return (((a == rhs.a) && (b == rhs.b)) || ((a == rhs.b) && (b == rhs.a)));
  }
  bool operator!=(const CollisionPair &rhs) const { return !(*this == rhs); }
};

void BroadPhase(const Body *bodies, const i32 num,
                std::vector<CollisionPair> &finalPairs, const f32 dt_sec);
