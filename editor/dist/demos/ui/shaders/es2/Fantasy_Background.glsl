#version 300 es

// material instance time in seconds.
uniform float kTime;

// The current texture to sample from.
uniform sampler2D kTexture;

// texture coordinates
in vec2 vTexCoord;


void FragmentShaderMain()
{
    vec2 st = vTexCoord;

    float angle = 2.0*3.1417 * sin(kTime*0.02);

    st -= vec2(0.5, 0.5);
    st = mat2(cos(angle), -sin(angle),
              sin(angle), cos(angle)) * st;
    st += vec2(0.5, 0.5);

    st.x = st.x + sin(kTime*0.01);
    st.y = st.y + cos(kTime*0.05);

    st *= vec2(3.0, 3.0);

    fs_out.color = texture(kTexture, st);
}
