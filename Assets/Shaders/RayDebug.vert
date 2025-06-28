#version 450

layout(push_constant) uniform constants{
  mat4 viewProj;
  vec4 rayPositions[2]; // Start and End positions
};

void main() {
  gl_Position = viewProj * vec4(rayPositions[gl_VertexIndex].xyz, 1.f);
}

