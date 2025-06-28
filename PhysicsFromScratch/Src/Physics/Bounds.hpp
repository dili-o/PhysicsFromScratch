#pragma once
#include <Math/Vector.hpp>

/*
====================================================
Bounds
====================================================
*/
class Bounds {
public:
  Bounds() { Clear(); }
  Bounds(const Bounds &rhs) : mins(rhs.mins), maxs(rhs.maxs) {}
  const Bounds &operator=(const Bounds &rhs);
  ~Bounds() {}

  inline void Clear() {
    mins = Vec3(1e6);
    maxs = Vec3(-1e6);
  }

  bool DoesIntersect(const Bounds &rhs) const;
  void Expand(const Vec3 *pts, const i32 num);
  void Expand(const Vec3 &rhs);
  void Expand(const Bounds &rhs);

  f32 WidthX() const { return maxs.x - mins.x; }
  f32 WidthY() const { return maxs.y - mins.y; }
  f32 WidthZ() const { return maxs.z - mins.z; }

public:
  Vec3 mins;
  Vec3 maxs;
};
