#version 100

attribute vec2 aPosition;
attribute vec2 aTexCoord;

// pixel values
uniform vec2 kScalingFactor;
uniform vec2 kTranslationTerm;
uniform vec2 kViewport;

varying vec2 vTexCoord;

void main()
{
    // scaling
    vec2 scale = kScalingFactor / (kViewport * 0.5);

    // translation, 0,0 pixels is top left corner, i.e. -1, 1 in ndc
    vec2 translate = kTranslationTerm / kViewport * 2.0;
    vec2 basis = vec2(1.0, -1.0);

    vec2 pos = aPosition * scale + vec2(-1.0, 1.0) + (basis * translate);

    vTexCoord = aTexCoord;

    gl_Position = vec4(pos, 1.0, 1.0);
}
