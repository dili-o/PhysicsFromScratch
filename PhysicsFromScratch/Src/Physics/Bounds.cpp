#include "Bounds.hpp"

const Bounds &Bounds::operator=(const Bounds &rhs) {
  mins = rhs.mins;
  maxs = rhs.maxs;
  return *this;
}

bool Bounds::DoesIntersect(const Bounds &rhs) const {
  if (maxs.x < rhs.mins.x || maxs.y < rhs.mins.y || maxs.z < rhs.mins.z) {
    return false;
  }
  if (rhs.maxs.x < mins.x || rhs.maxs.y < mins.y || rhs.maxs.z < mins.z) {
    return false;
  }
  return true;
}

void Bounds::Expand(const Vec3 *pts, const i32 num) {
  for (i32 i = 0; i < num; i++) {
    Expand(pts[i]);
  }
}

void Bounds::Expand(const Vec3 &rhs) {
  if (rhs.x < mins.x) {
    mins.x = rhs.x;
  }
  if (rhs.y < mins.y) {
    mins.y = rhs.y;
  }
  if (rhs.z < mins.z) {
    mins.z = rhs.z;
  }

  if (rhs.x > maxs.x) {
    maxs.x = rhs.x;
  }
  if (rhs.y > maxs.y) {
    maxs.y = rhs.y;
  }
  if (rhs.z > maxs.z) {
    maxs.z = rhs.z;
  }
}

void Bounds::Expand(const Bounds &rhs) {
  Expand(rhs.mins);
  Expand(rhs.maxs);
}
