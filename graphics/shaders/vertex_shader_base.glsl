R"CPP_RAW_STRING(//"
// Copyright (c) 2020-2024 Sami Väisänen
// vertex shader base

mat4 GetInstanceTransform() {
#ifdef INSTANCED_DRAW
    return mat4(iaModelVectorX,
                iaModelVectorY,
                iaModelVectorZ,
                iaModelVectorW);

#else
    return mat4(1.0);
#endif
}

)CPP_RAW_STRING"