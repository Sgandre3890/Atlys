#version 330 core

layout(location=0) in vec3 aPos;
layout(location=1) in vec3 aNormal;
layout(location=2) in vec4 aTangent;
layout(location=3) in vec2 aUV0;
layout(location=4) in vec2 aUV1;
layout(location=5) in vec4 aColor;

out vec3 vWorldPos;
out vec3 vNormal;
out vec4 vTangent;
out vec2 vUV0;
out vec2 vUV1;
out vec4 vColor;
out mat3 vTBN;

uniform mat4 uModel;
uniform mat4 uView;
uniform mat4 uProjection;
uniform bool uHasTangents;

void main() {
    vec4 worldPos = uModel * vec4(aPos, 1.0);
    vWorldPos = worldPos.xyz;

    mat3 nm = transpose(inverse(mat3(uModel)));
    vNormal  = normalize(nm * aNormal);
    vTangent = aTangent;
    vUV0     = aUV0;
    vUV1     = aUV1;
    vColor   = aColor;

    // Build TBN for normal mapping
    if (uHasTangents) {
        vec3 T = normalize(nm * aTangent.xyz);
        vec3 N = vNormal;
        T = normalize(T - dot(T,N)*N);
        vec3 B = cross(N,T) * aTangent.w;
        vTBN = mat3(T,B,N);
    } else {
        vTBN = mat3(1.0);
    }

    gl_Position = uProjection * uView * worldPos;
}
