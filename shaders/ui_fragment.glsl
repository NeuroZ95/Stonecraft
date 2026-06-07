#version 330 core
in vec2 TexCoords;
out vec4 color;

uniform sampler2D text;
uniform vec3 textColor;
uniform bool isText;
uniform bool isTexture;
uniform vec4 solidColor;

void main() {
    if (isText) {
        vec4 sampled = vec4(1.0, 1.0, 1.0, texture(text, TexCoords).r);
        color = vec4(textColor, 1.0) * sampled;
    } else if (isTexture) {
        // Умножаем цвет текстуры на solidColor для возможности тонирования элементов UI
        color = texture(text, TexCoords) * solidColor;
        if (color.a < 0.05) {
            discard;
        }
    } else {
        color = solidColor;
    }
}