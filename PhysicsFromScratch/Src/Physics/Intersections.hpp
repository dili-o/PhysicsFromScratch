#pragma once

#include "Contact.hpp"

bool RaySphere(const Vec3 &rayStart, const Vec3 &rayDir,
               const Vec3 &sphereCenter, const f32 sphereRadius, f32 &t1,
               f32 &t2);

bool Intersect(Body *bodyA, Body *bodyB, f32 dt_Sec, Contact &contact);
