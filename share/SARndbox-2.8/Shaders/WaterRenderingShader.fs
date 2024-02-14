#version 120

uniform sampler2DRect quantitySampler; // Sampler for the water level
varying vec2 vTexCoord; // Received from the vertex shader

void main() {
    // Use the water level data as a proxy for time to create dynamic effects
    float dynamicFactor = texture2DRect(quantitySampler, vTexCoord).r;

    // Simple fractal-like pattern with dynamic modulation
    vec2 coord = fract(vTexCoord * 10.0 + dynamicFactor); // Modulate coordinates with the dynamic factor
    float pattern = step(0.5, fract(coord.x * 0.5 + coord.y * 0.5)); // Simple checkerboard pattern

    // Modulate the pattern further with the dynamic factor to simulate flow
    pattern *= sin(coord.x + coord.y + dynamicFactor) * 0.5 + 0.5;

    gl_FragColor = vec4(vec3(pattern), 1.0); // Assign the color based on the pattern
}
