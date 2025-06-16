// #version 450
// 
// layout (location = 0) out vec2 outTexCoord;
// 
// void main() {
//     vec2 TexC = vec2((gl_VertexIndex << 1) & 2, gl_VertexIndex & 2);
//     gl_Position = vec4(outTexCoord.xy * 2.0f - 1.0f, 0.0f, 1.0f);
//     gl_Position.y = -gl_Position.y;
// }

#version 450

vec2 positions[3] = vec2[](
    vec2(-1.0, -1.0),
    vec2( 3.0, -1.0),
    vec2(-1.0,  3.0)
);

void main() {
  gl_Position = vec4(positions[gl_VertexIndex], 0.0, 1.0);
}
