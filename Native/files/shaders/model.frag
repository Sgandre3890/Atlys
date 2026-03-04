#version 330 core

in vec3 vWorldPos;
in vec3 vNormal;
in vec2 vUV;

out vec4 FragColor;

// Material
uniform vec3      uBaseColor;
uniform bool      uHasTexture;
uniform sampler2D uAlbedoTex;

// Lighting
uniform vec3 uLightPos;
uniform vec3 uLightColor;
uniform vec3 uViewPos;

void main() {
    // ── Albedo ────────────────────────────────────────────────────────────────
    vec3 albedo = uBaseColor;
    if (uHasTexture)
        albedo *= texture(uAlbedoTex, vUV).rgb;

    // ── Blinn-Phong ───────────────────────────────────────────────────────────
    vec3 N = normalize(vNormal);
    vec3 L = normalize(uLightPos - vWorldPos);
    vec3 V = normalize(uViewPos  - vWorldPos);
    vec3 H = normalize(L + V);

    float ambient  = 0.10;
    float diffuse  = max(dot(N, L), 0.0);
    float specular = pow(max(dot(N, H), 0.0), 64.0) * 0.4;

    vec3 colour = (ambient + diffuse) * albedo * uLightColor
                + specular * uLightColor;

    // Simple gamma correction
    colour = pow(colour, vec3(1.0 / 2.2));

    FragColor = vec4(colour, 1.0);
}
