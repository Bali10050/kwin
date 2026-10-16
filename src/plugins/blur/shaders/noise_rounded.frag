#version 140

#include "sdf.glsl"

uniform sampler2D texUnit;
uniform vec2 noiseTextureSize;
uniform vec4 box;
uniform vec4 cornerRadius;

in vec2 vertex;

out vec4 fragColor;

void main(void)
{
    float f = sdfRoundedBox(vertex, box.xy, box.zw, cornerRadius);
    float coverage = clamp(0.5 - f / fwidth(f), 0.0, 1.0);

    float noise = texture(texUnit, gl_FragCoord.xy / noiseTextureSize).r;
    fragColor = vec4(vec3(noise * coverage), 0.0);
}
