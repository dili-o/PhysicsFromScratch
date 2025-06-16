#version 450

layout(location = 0) out vec4 outColor;

void main() {
    float t = gl_FragCoord.y / 1080.0;
    vec3 bottomColor = vec3(0.95, 0.95, 1.0);
    vec3 topColor = vec3(0.6, 0.8, 1.0);

    vec3 color = mix(topColor, bottomColor, t);
    outColor = vec4(color, 1.0);
}
