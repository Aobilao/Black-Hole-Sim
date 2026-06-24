#version 330 core

in vec2 vUV;
out vec4 FragColor;

uniform sampler2D uScene;

const float THRESHOLD = 1.0;

void main() {
    vec3 c = texture(uScene, vUV).rgb;
    float luma = dot(c, vec3(0.2126, 0.7152, 0.0722));
    FragColor = vec4(luma > THRESHOLD ? c : vec3(0.0), 1.0);
}
