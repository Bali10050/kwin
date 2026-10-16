#version 140

#include "sdf.glsl"

uniform sampler2D texUnit;
uniform vec2 noiseTextureSize;
uniform vec4 box;
uniform vec4 cornerRadius;

in vec2 uv;
in vec2 vertex;

out vec4 fragColor;

void main(void)
{
    vec2 uvNoise = vec2(gl_FragCoord.xy / noiseTextureSize);

    fragColor = vec4(texture(texUnit, uvNoise).rrr, 0);

    // Clip the noise to the same rounded box that the blur is clipped to.
    float f = sdfRoundedBox(vertex, box.xy, box.zw, cornerRadius);
    float df = fwidth(f);
    fragColor *= 1.0 - clamp(0.5 + f / df, 0.0, 1.0);
}
