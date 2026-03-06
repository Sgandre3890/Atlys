#version 330 core

in vec3 vWorldPos;
in vec3 vNormal;
in vec4 vTangent;
in vec2 vUV0;
in vec2 vUV1;
in vec4 vColor;
in mat3 vTBN;

out vec4 FragColor;

// Material
uniform vec3  uBaseColor;
uniform float uBaseAlpha;
uniform vec3  uEmissive;
uniform float uMetallic;
uniform float uRoughness;
uniform float uOcclusionStrength;
uniform float uNormalScale;
uniform float uAlphaCutoff;
uniform int   uAlphaMode;       // 0=OPAQUE 1=MASK 2=BLEND
uniform bool  uHasVertexColors;
uniform bool  uHasTangents;

uniform bool      uHasBaseColorTex;
uniform sampler2D uBaseColorTex;
uniform bool      uHasNormalTex;
uniform sampler2D uNormalTex;
uniform bool      uHasMetallicRoughTex;
uniform sampler2D uMetallicRoughTex;
uniform bool      uHasEmissiveTex;
uniform sampler2D uEmissiveTex;
uniform bool      uHasOcclusionTex;
uniform sampler2D uOcclusionTex;

// Lighting
uniform vec3 uLightPos;
uniform vec3 uLightColor;
uniform vec3 uViewPos;

const float PI = 3.14159265359;

// ── PBR helpers ───────────────────────────────────────────────────────────────
float DistributionGGX(vec3 N, vec3 H, float roughness) {
    float a = roughness*roughness;
    float a2 = a*a;
    float NdH = max(dot(N,H),0.0);
    float denom = NdH*NdH*(a2-1.0)+1.0;
    return a2 / (PI*denom*denom);
}

float GeometrySchlick(float NdV, float roughness) {
    float r = roughness+1.0;
    float k = (r*r)/8.0;
    return NdV / (NdV*(1.0-k)+k);
}

float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness) {
    return GeometrySchlick(max(dot(N,V),0.0),roughness)
         * GeometrySchlick(max(dot(N,L),0.0),roughness);
}

vec3 FresnelSchlick(float cosTheta, vec3 F0) {
    return F0 + (1.0-F0)*pow(clamp(1.0-cosTheta,0.0,1.0),5.0);
}

void main() {
    // ── Base colour ───────────────────────────────────────────────────────────
    vec4 base = vec4(uBaseColor, uBaseAlpha);
    if (uHasBaseColorTex) base *= texture(uBaseColorTex, vUV0);
    if (uHasVertexColors) base *= vColor;

    // Alpha test / blend
    if (uAlphaMode == 1 && base.a < uAlphaCutoff) discard;

    // ── Normal ────────────────────────────────────────────────────────────────
    vec3 N = normalize(vNormal);
    if (uHasNormalTex && uHasTangents) {
        vec3 tn = texture(uNormalTex, vUV0).rgb * 2.0 - 1.0;
        tn.xy *= uNormalScale;
        N = normalize(vTBN * tn);
    }

    // ── Metallic / roughness ──────────────────────────────────────────────────
    float metallic  = uMetallic;
    float roughness = uRoughness;
    if (uHasMetallicRoughTex) {
        vec4 mr = texture(uMetallicRoughTex, vUV0);
        roughness *= mr.g;
        metallic  *= mr.b;
    }
    roughness = clamp(roughness, 0.04, 1.0);

    // ── Occlusion ─────────────────────────────────────────────────────────────
    float occlusion = 1.0;
    if (uHasOcclusionTex) {
        vec2 occUV = vUV0; // some models use UV1 for occlusion
        occlusion = 1.0 + uOcclusionStrength*(texture(uOcclusionTex,occUV).r - 1.0);
    }

    // ── PBR shading ───────────────────────────────────────────────────────────
    vec3 albedo = pow(base.rgb, vec3(2.2));   // to linear
    vec3 V = normalize(uViewPos - vWorldPos);
    vec3 L = normalize(uLightPos - vWorldPos);
    vec3 H = normalize(V+L);

    vec3 F0 = mix(vec3(0.04), albedo, metallic);
    float NdL = max(dot(N,L), 0.0);

    // Cook-Torrance BRDF
    float D = DistributionGGX(N,H,roughness);
    float G = GeometrySmith(N,V,L,roughness);
    vec3  F = FresnelSchlick(max(dot(H,V),0.0),F0);

    vec3 kD = (vec3(1.0)-F) * (1.0-metallic);
    vec3 specular = (D*G*F) / max(4.0*max(dot(N,V),0.0)*NdL, 0.001);

    float dist = length(uLightPos - vWorldPos);
    float atten = 1.0 / (dist*dist + 1.0);
    vec3 radiance = uLightColor * atten * 8.0;

    vec3 Lo = (kD*albedo/PI + specular) * radiance * NdL;

    // Ambient
    vec3 ambient = vec3(0.03) * albedo * occlusion;

    // Emissive
    vec3 emissive = uEmissive;
    if (uHasEmissiveTex) emissive *= texture(uEmissiveTex, vUV0).rgb;
    emissive = pow(emissive, vec3(2.2));

    vec3 colour = ambient + Lo + emissive;

    // Tone map + gamma
    colour = colour / (colour + vec3(1.0));  // Reinhard
    colour = pow(colour, vec3(1.0/2.2));

    FragColor = vec4(colour, base.a);
}
