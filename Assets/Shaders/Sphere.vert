#version 450

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inTexCoord;

layout(location = 0) out vec3 outNormal;
layout(location = 1) out vec2 outTexCoord;
layout(location = 2) out vec3 outModelNorms;

layout(push_constant) uniform constants{
  mat4 viewProj;
  mat4 model;
};

void main() {
  gl_Position = viewProj * model * vec4(inPosition, 1.f);
  // outFragPos = vec3(model * vec4(inPosition, 1.0));
  outModelNorms = inNormal;
  outNormal = mat3(transpose(inverse(model))) * inNormal;
  outTexCoord = inTexCoord;
}
