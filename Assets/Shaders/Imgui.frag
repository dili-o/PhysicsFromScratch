#version 450

layout(binding = 0) uniform sampler2D fontSampler;

layout (location = 0) in vec2 inFragUV;
layout (location = 1) in vec4 inFragColor;
layout (location = 2) flat in uint inTextureId;

layout (location = 0) out vec4 outColor;

void main() {
  outColor = inFragColor * texture(fontSampler, inFragUV.st);
}

