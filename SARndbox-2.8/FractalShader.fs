#version 330 core

out vec4 FragColor;
in vec2 TexCoord;

uniform float zoom;
uniform vec2 center;

void main() {
    vec2 c = (gl_FragCoord.xy - vec2(1024.0, 768.0) / 2.0) * zoom + center;

    vec2 z = vec2(0.0, 0.0);
    int maxIterations = 1000;
    int i;
    for (i = 0; i < maxIterations; i++) {
        if (dot(z, z) > 4.0) break;
        z = vec2(z.x*z.x - z.y*z.y + c.x, 2.0*z.x*z.y + c.y);
    }

    float colorValue = float(i) / float(maxIterations);
    FragColor = vec4(vec3(colorValue), 1.0);
}
