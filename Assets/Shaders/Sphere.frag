#version 450

layout(location = 0) in vec3 inNormal;
layout(location = 1) in vec2 inTexCoord;

layout(location = 0) out vec4 outColor;

layout(set = 0, binding = 0) uniform sampler2D sphereTexture;

uint hash(uint a)
{
   a = (a+0x7ed55d16) + (a<<12);
   a = (a^0xc761c23c) ^ (a>>19);
   a = (a+0x165667b1) + (a<<5);
   a = (a+0xd3a2646c) ^ (a<<9);
   a = (a+0xfd7046c5) + (a<<3);
   a = (a^0xb55a4f09) ^ (a>>16);
   return a;
}

// LearnOpenGL Phong Shading
struct Light {
    vec3 direction;
    vec3 ambient;
    vec3 diffuse;
};

Light sun;

void main() {
#ifdef RANDOM
  uint mhash = hash(uint(gl_PrimitiveID));
  vec3 color = vec3(float(mhash & 255), float((mhash >> 8) & 255), float((mhash >> 16) & 255)) / 255.0;
  outColor = vec4(color , 1.f);
#else
  vec3 sphereColor = texture(sphereTexture, inTexCoord).rgb; 
  // ambient
  sun.direction = normalize(vec3(1, -1, -1));
  sun.ambient = vec3(0.2f);
  sun.diffuse = vec3(0.8f);
  vec3 ambient = sun.ambient * sphereColor;
  
  // diffuse 
  vec3 norm = normalize(inNormal);
  vec3 lightDir = normalize(-sun.direction);  
  float diff = max(dot(norm, lightDir), 0.0);
  vec3 diffuse = sun.diffuse * diff * sphereColor;

  vec3 result = ambient + diffuse;
  outColor = vec4(result, 1.0);
#endif // RANDOM
}
