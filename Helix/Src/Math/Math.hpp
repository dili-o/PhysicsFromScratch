#pragma once

#include "Defines.hpp"
#include <numbers>

#define H_PI std::numbers::pi_v<f32>

inline f32 DegToRad(f32 deg) {
  static constexpr f32 piDiv180 = H_PI / 180.0f;
  return deg * piDiv180;
}

inline f32 RadToDeg(f32 rad) {
  static constexpr float oneEightyDivPi = 180.0f / H_PI;
  return rad * oneEightyDivPi;
}
