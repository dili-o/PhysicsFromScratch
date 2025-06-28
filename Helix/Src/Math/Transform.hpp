#pragma once

#include "Quat.hpp"

#ifdef STORE_MATRIX
struct Transform {
public:
  inline Mat4 GetMat4() const { return m_Model; }

  inline Vec3 GetPosition() const { return Vec3(m_Model[3]); }

  inline Quat GetRotation() const {
    Mat3 rotationMatrix;

    Vec3 scale = GetScale();

    rotationMatrix[0] = Vec3(m_Model[0]) / scale.x;
    rotationMatrix[1] = Vec3(m_Model[1]) / scale.y;
    rotationMatrix[2] = Vec3(m_Model[2]) / scale.z;

    return glm::quat_cast(rotationMatrix);
  }

  inline Vec3 GetScale() const {
    Vec3 scale;
    scale.x = glm::length(Vec3(m_Model[0]));
    scale.y = glm::length(Vec3(m_Model[1]));
    scale.z = glm::length(Vec3(m_Model[2]));

    return scale;
  }

  inline void SetPosition(const Vec3 position) {
    m_Model = glm::translate(m_Model, position);
  }

  inline void SetRotation(const Quat rotation) {
    Mat4 rotationMatrix = glm::toMat4(rotation);
    m_Model = m_Model * rotationMatrix;
  }

  inline void SetScale(const Vec3 scale) {
    m_Model = glm::scale(m_Model, scale);
  }

  inline void SetTransform(const Mat4 &&matrix) { m_Model = matrix; }

private:
  Mat4 m_Model{1.f};
};
#else
struct Transform {
public:
  inline Mat4 GetMat4() const {
    return glm::translate(Mat4(1.f), m_Position) * glm::toMat4(m_Rotation) *
           glm::scale(Mat4(1.f), m_Scale);
  }

  inline Vec3 GetPosition() const { return m_Position; }
  inline Quat GetRotation() const { return m_Rotation; }
  inline Vec3 GetScale() const { return m_Scale; }

  inline void SetPosition(const Vec3 position) { m_Position = position; }
  inline void SetRotation(const Quat rotation) { m_Rotation = rotation; }
  inline void SetScale(const Vec3 scale) { m_Scale = scale; }

  inline void SetTransform(Mat4 &matrix) {
    m_Position = Vec3(matrix[3]);

    m_Scale.x = glm::length(Vec3(matrix[0]));
    m_Scale.y = glm::length(Vec3(matrix[1]));
    m_Scale.z = glm::length(Vec3(matrix[2]));

    Mat3 m_RotationMatrix;
    m_RotationMatrix[0] = Vec3(matrix[0]) / m_Scale.x;
    m_RotationMatrix[1] = Vec3(matrix[1]) / m_Scale.y;
    m_RotationMatrix[2] = Vec3(matrix[2]) / m_Scale.z;
    m_Rotation = glm::quat_cast(m_RotationMatrix);
  }

private:
  Vec3 m_Position{0.f};
  Quat m_Rotation = Quat(1.0f, 0.0f, 0.0f, 0.0f);
  Vec3 m_Scale{1.f};
};
#endif // STORE_MATRIX
