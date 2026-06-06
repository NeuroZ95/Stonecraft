#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec2 aTexCoord;

out vec2 TexCoord;
out vec3 FragWorldPos;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;
uniform vec3 cameraPos;

void main() {
    vec4 relativePos = model * vec4(aPos, 1.0);
    gl_Position = projection * view * relativePos;
    TexCoord = aTexCoord;
    FragWorldPos = relativePos.xyz + cameraPos;
}