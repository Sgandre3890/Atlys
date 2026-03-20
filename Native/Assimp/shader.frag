#version 330 core

in vec3 v_WorldPos;
in vec3 v_Normal;
in vec2 v_TexCoord;

out vec4 FragColor;

// 0 = solid (full lighting)
// 1 = flat color (used for wireframe / points pass)
uniform int  u_RenderMode;
uniform vec4 u_FlatColor;   // used when u_RenderMode == 1

uniform vec4      u_BaseColor;
uniform int       u_HasAlbedo;
uniform sampler2D u_AlbedoTex;
uniform float     u_Brightness;
uniform float     u_Contrast;
uniform vec3      u_TintColor;

uniform int   u_NumLights;
uniform vec3  u_LightDir[4]; // Can have up to 4 lights
uniform vec3  u_LightColor[4];
uniform float u_LightIntensity[4];

uniform vec3  u_AmbientColor;
uniform float u_AmbientIntensity;

uniform float u_SpecularPower;
uniform float u_SpecularStrength;

uniform vec3  u_EmissiveColor;
uniform float u_EmissiveIntensity;

uniform vec3  u_RimColor;
uniform float u_RimPower;
uniform float u_RimStrength;

uniform int   u_FogEnabled;
uniform vec3  u_FogColor;
uniform float u_FogNear;
uniform float u_FogFar;

uniform vec3  u_CameraPos;

void main() {
    // Flat mode: just output the solid color (used for wireframe / points overlay)
    if (u_RenderMode == 1) {
        FragColor = u_FlatColor;
        return;
    }

    vec4 albedo = u_BaseColor;
    if (u_HasAlbedo == 1)
        albedo *= texture(u_AlbedoTex, v_TexCoord);

    vec3 N = normalize(v_Normal);
    vec3 V = normalize(u_CameraPos - v_WorldPos);

    vec3 color = u_AmbientColor * u_AmbientIntensity * albedo.rgb;

    for (int i = 0; i < u_NumLights; ++i) {
        vec3  L    = normalize(u_LightDir[i]);
        vec3  H    = normalize(L + V);
        float diff = max(dot(N, L), 0.0);
        float spec = pow(max(dot(N, H), 0.0), u_SpecularPower) * u_SpecularStrength;
        color += u_LightColor[i] * u_LightIntensity[i] * (albedo.rgb * diff + vec3(spec));
    }

    color += u_EmissiveColor * u_EmissiveIntensity;

    float rim = pow(1.0 - max(dot(N, V), 0.0), u_RimPower) * u_RimStrength;
    color += u_RimColor * rim;

    color *= u_TintColor;
    color  = (color - 0.5) * u_Contrast + 0.5;
    color *= u_Brightness;

    color = pow(max(color, vec3(0.0)), vec3(1.0 / 2.2)); // Gamma

    if (u_FogEnabled == 1) {
        float dist   = length(u_CameraPos - v_WorldPos);
        float fogAmt = clamp((dist - u_FogNear) / (u_FogFar - u_FogNear), 0.0, 1.0);
        color = mix(color, u_FogColor, fogAmt);
    }

    FragColor = vec4(color, albedo.a);
}
