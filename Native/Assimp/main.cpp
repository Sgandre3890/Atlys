#include <glad/gl.h>
#include <GLFW/glfw3.h>
#include <glm/gtc/matrix_transform.hpp>
#include <fstream>
#include <sstream>
#include <cstdio>
#include <cstring>
#include <cmath>

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include <nfd.h>
#include "ModelLoader.h"

// Most of this code is similar if not identical to the LearnOpenGL model loading tutorial:


static GLuint compileShader(GLenum type, const char* src) {
    GLuint s = glCreateShader(type);
    glShaderSource(s, 1, &src, nullptr);
    glCompileShader(s);
    GLint ok; glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) { char log[512]; glGetShaderInfoLog(s, 512, nullptr, log); printf("Shader: %s\n", log); }
    return s;
}
static GLuint createProgram(const char* vert, const char* frag) {
    auto read = [](const char* p){ std::ifstream f(p); std::ostringstream s; s << f.rdbuf(); return s.str(); };
    auto vs = compileShader(GL_VERTEX_SHADER,   read(vert).c_str());
    auto fs = compileShader(GL_FRAGMENT_SHADER, read(frag).c_str());
    GLuint prog = glCreateProgram();
    glAttachShader(prog, vs); glAttachShader(prog, fs);
    glLinkProgram(prog);
    glDeleteShader(vs); glDeleteShader(fs);
    return prog;
}

struct Camera {
    glm::vec3 pos   = { 0, 0, 3 };
    float     yaw   = -90.f;
    float     pitch = 0.f;
    float     speed = 2.5f;
    float     sens  = 0.15f;
    float     fov   = 45.f;

    glm::vec3 front() const {
        return glm::normalize(glm::vec3(
            cos(glm::radians(yaw)) * cos(glm::radians(pitch)),
            sin(glm::radians(pitch)),
            sin(glm::radians(yaw)) * cos(glm::radians(pitch))));
    }
    glm::vec3 right() const { return glm::normalize(glm::cross(front(), {0,1,0})); }
    glm::mat4 view()  const { return glm::lookAt(pos, pos + front(), {0,1,0}); }
};

static Camera g_cam;
static double g_lastX = 0, g_lastY = 0;
static bool   g_drag  = false;

void onMouseBtn(GLFWwindow*, int b, int a, int) {
    if (!ImGui::GetIO().WantCaptureMouse && b == GLFW_MOUSE_BUTTON_LEFT)
        g_drag = (a == GLFW_PRESS);
    if (a == GLFW_RELEASE) g_drag = false;
}
void onMouseMove(GLFWwindow*, double x, double y) {
    if (g_drag && !ImGui::GetIO().WantCaptureMouse) {
        g_cam.yaw   += (float)(x - g_lastX) * g_cam.sens;
        g_cam.pitch  = glm::clamp(g_cam.pitch - (float)(y - g_lastY) * g_cam.sens, -89.f, 89.f);
    }
    g_lastX = x; g_lastY = y;
}
void onScroll(GLFWwindow*, double, double dy) {
    if (!ImGui::GetIO().WantCaptureMouse)
        g_cam.pos += g_cam.front() * (float)dy * 0.3f;
}
static void processKeys(GLFWwindow* win, float dt) {
    if (ImGui::GetIO().WantCaptureKeyboard) return;
    float v = g_cam.speed * dt;
    if (glfwGetKey(win, GLFW_KEY_W)     == GLFW_PRESS || glfwGetKey(win, GLFW_KEY_UP)    == GLFW_PRESS) g_cam.pos += g_cam.front() * v;
    if (glfwGetKey(win, GLFW_KEY_S)     == GLFW_PRESS || glfwGetKey(win, GLFW_KEY_DOWN)  == GLFW_PRESS) g_cam.pos -= g_cam.front() * v;
    if (glfwGetKey(win, GLFW_KEY_A)     == GLFW_PRESS || glfwGetKey(win, GLFW_KEY_LEFT)  == GLFW_PRESS) g_cam.pos -= g_cam.right() * v;
    if (glfwGetKey(win, GLFW_KEY_D)     == GLFW_PRESS || glfwGetKey(win, GLFW_KEY_RIGHT) == GLFW_PRESS) g_cam.pos += g_cam.right() * v;
    if (glfwGetKey(win, GLFW_KEY_E)     == GLFW_PRESS) g_cam.pos.y += v;
    if (glfwGetKey(win, GLFW_KEY_Q)     == GLFW_PRESS) g_cam.pos.y -= v;
}

struct Light {
    float dir[3]       = { 0.5f, 1.0f, 0.3f };
    float color[3]     = { 1.0f, 1.0f, 1.0f };
    float intensity    = 1.0f;
    bool  enabled      = true;
};

struct LightingSettings {
    Light lights[4];
    int   numLights = 1;

    float ambientColor[3]     = { 0.2f, 0.2f, 0.25f };
    float ambientIntensity    = 0.5f;

    float specularPower       = 64.f;
    float specularStrength    = 0.5f;

    float emissiveColor[3]    = { 0.0f, 0.0f, 0.0f };
    float emissiveIntensity   = 0.0f;

    float rimColor[3]         = { 1.0f, 1.0f, 1.0f };
    float rimPower            = 3.0f;
    float rimStrength         = 0.0f;

    float brightness          = 1.0f;
    float contrast            = 1.0f;
    float tintColor[3]        = { 1.0f, 1.0f, 1.0f };

    bool  fogEnabled          = false;
    float fogColor[3]         = { 0.15f, 0.15f, 0.18f };
    float fogNear             = 5.f;
    float fogFar              = 30.f;
};

static void applyLighting(GLuint sh, const LightingSettings& L) {
    auto loc = [&](const char* n){ return glGetUniformLocation(sh, n); };
    char buf[64];

    int active = 0;
    for (int i = 0; i < L.numLights; ++i) {
        if (!L.lights[i].enabled) continue;
        snprintf(buf, sizeof(buf), "u_LightDir[%d]", active);
        glUniform3fv(loc(buf), 1, L.lights[i].dir);
        snprintf(buf, sizeof(buf), "u_LightColor[%d]", active);
        glUniform3fv(loc(buf), 1, L.lights[i].color);
        snprintf(buf, sizeof(buf), "u_LightIntensity[%d]", active);
        glUniform1f(loc(buf), L.lights[i].intensity);
        ++active;
    }
    glUniform1i(loc("u_NumLights"), active);

    glUniform3fv(loc("u_AmbientColor"),     1, L.ambientColor);
    glUniform1f (loc("u_AmbientIntensity"), L.ambientIntensity);
    glUniform1f (loc("u_SpecularPower"),    L.specularPower);
    glUniform1f (loc("u_SpecularStrength"), L.specularStrength);
    glUniform3fv(loc("u_EmissiveColor"),    1, L.emissiveColor);
    glUniform1f (loc("u_EmissiveIntensity"),L.emissiveIntensity);
    glUniform3fv(loc("u_RimColor"),         1, L.rimColor);
    glUniform1f (loc("u_RimPower"),         L.rimPower);
    glUniform1f (loc("u_RimStrength"),      L.rimStrength);
    glUniform1f (loc("u_Brightness"),       L.brightness);
    glUniform1f (loc("u_Contrast"),         L.contrast);
    glUniform3fv(loc("u_TintColor"),        1, L.tintColor);
    glUniform1i (loc("u_FogEnabled"),       L.fogEnabled ? 1 : 0);
    glUniform3fv(loc("u_FogColor"),         1, L.fogColor);
    glUniform1f (loc("u_FogNear"),          L.fogNear);
    glUniform1f (loc("u_FogFar"),           L.fogFar);
    glUniform1i (loc("u_RenderMode"), 0);
    glUniform4f (loc("u_FlatColor"),  1.f, 1.f, 1.f, 1.f);
}

static void directionWidget(const char* label, float dir[3]) {
    glm::vec3 d = glm::normalize(glm::vec3(dir[0], dir[1], dir[2]));
    float az = glm::degrees(atan2f(d.x, d.z));
    float el = glm::degrees(asinf(glm::clamp(d.y, -1.f, 1.f)));

    bool changed = false;
    ImGui::PushID(label);
    changed |= ImGui::SliderFloat("Azimuth",   &az, -180.f, 180.f);
    changed |= ImGui::SliderFloat("Elevation", &el,  -89.f,  89.f);
    ImGui::PopID();

    if (changed) {
        float r = cosf(glm::radians(el));
        dir[0] = r * sinf(glm::radians(az));
        dir[1] = sinf(glm::radians(el));
        dir[2] = r * cosf(glm::radians(az));
    }
}

int main() {
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_SAMPLES, 4);
    GLFWwindow* win = glfwCreateWindow(1280, 720, "Model Viewer", nullptr, nullptr);
    glfwMakeContextCurrent(win);
    glfwSetMouseButtonCallback(win, onMouseBtn);
    glfwSetCursorPosCallback(win,   onMouseMove);
    glfwSetScrollCallback(win,      onScroll);

    gladLoadGL(glfwGetProcAddress);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_MULTISAMPLE);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK); // face culling
    glfwSwapInterval(1);  // vsync 

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::GetIO().FontGlobalScale = 1.1f;
    ImGui::StyleColorsDark();
    ImGui_ImplGlfw_InitForOpenGL(win, true);
    ImGui_ImplOpenGL3_Init("#version 330");

    NFD_Init();

    GLuint shader = createProgram("shader.vert", "shader.frag");

    Model          model;
    bool           modelLoaded  = false;
    std::string    statusMsg    = "Click \"Open Model\" to get started.";
    float          bgColor[3]   = { 0.15f, 0.15f, 0.18f };
    // 0 = Solid  1 = Wireframe  2 = Solid+Wireframe overlay  3 = Points
    int            renderMode    = 0;
    float          wireColor[3]  = { 0.0f, 0.8f, 1.0f };
    float          wireThickness = 1.0f;

    // Performance toggles
    bool           cullFace      = true;   
    int            cullMode      = 0;      // 0 = back 1 = front 2 = both
    bool           depthTest     = true;
    bool           depthWrite    = true;
    bool           msaa          = true;
    bool           vsync         = true;
    int            fpsLimit      = 0;      // 0 = unlimited
    LightingSettings lighting;

    nfdfilteritem_t filters[] = {
        { "3D Models", "glb,gltf,obj,fbx,dae,stl,ply,3ds" },
        { "All Files", "*" }
    };

    double lastTime    = glfwGetTime();
    double fpsTimer    = 0.0;
    int    fpsFrames   = 0;
    float  fpsDisplay  = 60.f;  

    while (!glfwWindowShouldClose(win)) {
        double now = glfwGetTime();
        float  dt  = (float)(now - lastTime);
        lastTime   = now;

        glfwPollEvents();
        processKeys(win, dt);

        fpsFrames++;
        fpsTimer += dt;
        if (fpsTimer >= 1.0) {
            fpsDisplay = fpsFrames / (float)fpsTimer;
            fpsFrames  = 0;
            fpsTimer   = 0.0;
        }

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImVec2(330, 0), ImGuiCond_Always);
        ImGui::Begin("Model Viewer", nullptr,
                     ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove);

        if (ImGui::Button("Open a Model", ImVec2(-1, 36))) {
            nfdchar_t* p = nullptr;
            if (NFD_OpenDialog(&p, filters, 2, nullptr) == NFD_OKAY) {
                destroyModel(model);
                std::string path(p);
                modelLoaded = loadModel(path, model);
                statusMsg   = modelLoaded ? "Loaded: " + path : "Failed: " + path;
                if (modelLoaded) g_cam = Camera{};
                NFD_FreePath(p);
            }
        }
        ImGui::Spacing();
        ImGui::TextWrapped("%s", statusMsg.c_str());
        if (modelLoaded) ImGui::Text("Meshes: %zu", model.meshes.size());

        ImGui::Spacing();
        ImGui::Separator();

        if (ImGui::CollapsingHeader("Camera")) {
            ImGui::SliderFloat("Speed",  &g_cam.speed, 0.1f, 20.f);
            ImGui::SliderFloat("FOV",    &g_cam.fov,   20.f, 120.f);
            ImGui::Text("Pos: %.2f  %.2f  %.2f", g_cam.pos.x, g_cam.pos.y, g_cam.pos.z);
            if (ImGui::Button("Reset Camera")) g_cam = Camera{};
            ImGui::TextDisabled("WASD/arrows to move,  Q/E = up/down  drag =look around, scroll = zoom");
        }

        if (ImGui::CollapsingHeader("Lights", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::SliderInt("Active lights", &lighting.numLights, 1, 4);

            for (int i = 0; i < lighting.numLights; ++i) {
                char label[32]; snprintf(label, sizeof(label), "Light %d", i + 1);
                if (ImGui::TreeNode(label)) {
                    ImGui::Checkbox("Enabled",    &lighting.lights[i].enabled);
                    ImGui::ColorEdit3("Color",     lighting.lights[i].color);
                    ImGui::SliderFloat("Intensity",&lighting.lights[i].intensity, 0.f, 5.f);
                    ImGui::Text("Direction:");
                    directionWidget(label,         lighting.lights[i].dir);
                    ImGui::TreePop();
                }
            }

            ImGui::Spacing();
            if (ImGui::TreeNode("Ambient")) {
                ImGui::ColorEdit3("Ambient Color",    lighting.ambientColor);
                ImGui::SliderFloat("Ambient Intensity", &lighting.ambientIntensity, 0.f, 2.f);
                ImGui::TreePop();
            }
            if (ImGui::TreeNode("Specular")) {
                ImGui::SliderFloat("Shininess",  &lighting.specularPower,    1.f,  512.f);
                ImGui::SliderFloat("Strength",   &lighting.specularStrength, 0.f,  2.f);
                ImGui::TreePop();
            }
            if (ImGui::TreeNode("Emissive")) {
                ImGui::ColorEdit3("Emissive Color",     lighting.emissiveColor);
                ImGui::SliderFloat("Emissive Intensity",&lighting.emissiveIntensity, 0.f, 5.f);
                ImGui::TreePop();
            }
            if (ImGui::TreeNode("Rim Light")) {
                ImGui::ColorEdit3("Rim Color",    lighting.rimColor);
                ImGui::SliderFloat("Rim Power",  &lighting.rimPower,    1.f, 10.f);
                ImGui::SliderFloat("Rim Strength",&lighting.rimStrength, 0.f,  2.f);
                ImGui::TreePop();
            }
        }

        if (ImGui::CollapsingHeader("Display")) {
            ImGui::SliderFloat("Brightness", &lighting.brightness, 0.f, 3.f);
            ImGui::SliderFloat("Contrast",   &lighting.contrast,   0.f, 3.f);
            ImGui::ColorEdit3("Tint",         lighting.tintColor);
            ImGui::Spacing();
            ImGui::ColorEdit3("Background",   bgColor);
            ImGui::Text("Render Mode:");
            ImGui::RadioButton("Solid",              &renderMode, 0); ImGui::SameLine();
            ImGui::RadioButton("Wireframe",          &renderMode, 1); ImGui::SameLine();
            ImGui::RadioButton("Solid + Wire",       &renderMode, 2);
            ImGui::RadioButton("Points",             &renderMode, 3);
            if (renderMode == 2) {
                ImGui::ColorEdit3("Wire Color",  wireColor);
                ImGui::SliderFloat("Wire Width", &wireThickness, 0.5f, 5.f);
            }
        }

        if (ImGui::CollapsingHeader("Fog")) {
            ImGui::Checkbox("Enable Fog",      &lighting.fogEnabled);
            if (lighting.fogEnabled) {
                ImGui::ColorEdit3("Fog Color",  lighting.fogColor);
                ImGui::SliderFloat("Fog Near", &lighting.fogNear, 0.f, 50.f);
                ImGui::SliderFloat("Fog Far",  &lighting.fogFar,  1.f, 200.f);
            }
        }

        if (ImGui::CollapsingHeader("Presets")) {
            ImGui::TextDisabled("Lighting");
            if (ImGui::Button("Studio",          ImVec2(-1,0))) {
                lighting = LightingSettings{};
                lighting.lights[0].dir[0]=0.5f; lighting.lights[0].dir[1]=1.f; lighting.lights[0].dir[2]=0.3f;
                lighting.lights[0].intensity = 1.2f;
                lighting.ambientIntensity = 0.6f;
                lighting.specularPower = 128.f; lighting.specularStrength = 0.8f;
                renderMode = 0;
            }
            if (ImGui::Button("Full Bright",     ImVec2(-1,0))) {
                lighting = LightingSettings{};
                lighting.ambientColor[0]=1.f; lighting.ambientColor[1]=1.f; lighting.ambientColor[2]=1.f;
                lighting.ambientIntensity = 1.f;
                lighting.lights[0].intensity = 0.f;
                lighting.specularStrength = 0.f;
                lighting.brightness = 1.f; lighting.contrast = 1.f;
                renderMode = 0;
            }
            if (ImGui::Button("Flat / Shadeless",ImVec2(-1,0))) {
                lighting = LightingSettings{};
                lighting.lights[0].intensity = 0.f;
                lighting.ambientIntensity = 1.5f;
                lighting.specularStrength = 0.f;
                renderMode = 0;
            }
            if (ImGui::Button("Sunset",          ImVec2(-1,0))) {
                lighting = LightingSettings{};
                lighting.lights[0].color[0]=1.f; lighting.lights[0].color[1]=0.5f; lighting.lights[0].color[2]=0.1f;
                lighting.lights[0].dir[0]=1.f; lighting.lights[0].dir[1]=0.1f; lighting.lights[0].dir[2]=0.2f;
                lighting.lights[0].intensity = 2.f;
                lighting.ambientColor[0]=0.3f; lighting.ambientColor[1]=0.15f; lighting.ambientColor[2]=0.2f;
                lighting.ambientIntensity = 0.8f;
                lighting.rimColor[0]=1.f; lighting.rimColor[1]=0.4f; lighting.rimColor[2]=0.1f;
                lighting.rimStrength = 0.6f; lighting.rimPower = 3.f;
                renderMode = 0;
            }
            if (ImGui::Button("Night",           ImVec2(-1,0))) {
                lighting = LightingSettings{};
                lighting.lights[0].color[0]=0.4f; lighting.lights[0].color[1]=0.5f; lighting.lights[0].color[2]=1.f;
                lighting.lights[0].intensity = 0.4f;
                lighting.ambientColor[0]=0.05f; lighting.ambientColor[1]=0.05f; lighting.ambientColor[2]=0.1f;
                lighting.ambientIntensity = 0.3f;
                lighting.rimColor[0]=0.3f; lighting.rimColor[1]=0.4f; lighting.rimColor[2]=1.f;
                lighting.rimStrength = 1.f; lighting.rimPower = 4.f;
                float nb[3]={0.03f,0.03f,0.06f}; memcpy(bgColor,nb,sizeof(nb));
                renderMode = 0;
            }
            if (ImGui::Button("Overcast",        ImVec2(-1,0))) {
                lighting = LightingSettings{};
                lighting.lights[0].dir[0]=0.f; lighting.lights[0].dir[1]=1.f; lighting.lights[0].dir[2]=0.f;
                lighting.lights[0].color[0]=0.8f; lighting.lights[0].color[1]=0.85f; lighting.lights[0].color[2]=0.9f;
                lighting.lights[0].intensity = 0.7f;
                lighting.ambientColor[0]=0.4f; lighting.ambientColor[1]=0.45f; lighting.ambientColor[2]=0.5f;
                lighting.ambientIntensity = 0.9f;
                lighting.specularStrength = 0.1f;
                float ob[3]={0.35f,0.38f,0.42f}; memcpy(bgColor,ob,sizeof(ob));
                renderMode = 0;
            }
            if (ImGui::Button("Neon / Cyberpunk",ImVec2(-1,0))) {
                lighting = LightingSettings{};
                lighting.numLights = 3;
                lighting.lights[0].color[0]=1.f; lighting.lights[0].color[1]=0.0f; lighting.lights[0].color[2]=0.6f;
                lighting.lights[0].dir[0]=-1.f; lighting.lights[0].dir[1]=0.5f; lighting.lights[0].dir[2]=0.3f;
                lighting.lights[0].intensity = 1.5f;
                
                lighting.lights[1].color[0]=0.f; lighting.lights[1].color[1]=0.8f; lighting.lights[1].color[2]=1.f;
                lighting.lights[1].dir[0]=1.f; lighting.lights[1].dir[1]=0.2f; lighting.lights[1].dir[2]=-0.3f;
                lighting.lights[1].intensity = 1.2f;
                
                lighting.lights[2].color[0]=0.5f; lighting.lights[2].color[1]=0.f; lighting.lights[2].color[2]=1.f;
                lighting.lights[2].dir[0]=0.f; lighting.lights[2].dir[1]=-0.5f; lighting.lights[2].dir[2]=-1.f;
                lighting.lights[2].intensity = 0.8f;
                lighting.ambientColor[0]=0.05f; lighting.ambientColor[1]=0.0f; lighting.ambientColor[2]=0.1f;
                lighting.ambientIntensity = 0.3f;
                lighting.specularPower = 256.f; lighting.specularStrength = 1.5f;
                lighting.rimColor[0]=0.f; lighting.rimColor[1]=1.f; lighting.rimColor[2]=0.8f;
                lighting.rimStrength = 1.2f; lighting.rimPower = 3.f;
                float nb[3]={0.02f,0.0f,0.06f}; memcpy(bgColor,nb,sizeof(nb));
                renderMode = 0;
            }
        }

        if (ImGui::CollapsingHeader("Performance")) {
            bool changed = false;

            if (ImGui::Checkbox("Back-face Culling", &cullFace)) changed = true;
            if (cullFace) {
                ImGui::Indent();
                ImGui::Text("Cull:");
                ImGui::SameLine();
                changed |= ImGui::RadioButton("Back",  &cullMode, 0); ImGui::SameLine();
                changed |= ImGui::RadioButton("Front", &cullMode, 1); ImGui::SameLine();
                changed |= ImGui::RadioButton("Both",  &cullMode, 2);
                ImGui::Unindent();
            }
            if (ImGui::Checkbox("Depth Test",  &depthTest))  changed = true;
            if (ImGui::Checkbox("Depth Write", &depthWrite)) changed = true;
            if (ImGui::Checkbox("MSAA",        &msaa))       changed = true;
            ImGui::Spacing();
            if (ImGui::Checkbox("VSync", &vsync))
                glfwSwapInterval(vsync ? 1 : 0);
        }

        ImGui::End();

        // FPS overlay 
        {
            
            int winW, winH; glfwGetWindowSize(win, &winW, &winH);
            ImGui::SetNextWindowPos(ImVec2((float)winW - 8.f, 8.f), ImGuiCond_Always, ImVec2(1.f, 0.f));
            ImGui::SetNextWindowBgAlpha(0.6f);
            ImGui::SetNextWindowSize(ImVec2(90.f, 0.f));
            ImGui::Begin("##fps", nullptr,
                ImGuiWindowFlags_NoDecoration     |
                ImGuiWindowFlags_NoInputs         |
                ImGuiWindowFlags_NoNav            |
                ImGuiWindowFlags_NoMove           |
                ImGuiWindowFlags_NoSavedSettings  |
                ImGuiWindowFlags_AlwaysAutoResize);
            ImGui::Text("FPS: %.0f", fpsDisplay);
            ImGui::End();
        }

        ImGui::Render();

        
        int w, h; glfwGetFramebufferSize(win, &w, &h);
        glViewport(0, 0, w, h);
        glClearColor(bgColor[0], bgColor[1], bgColor[2], 1.f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        
        if (cullFace) {
            glEnable(GL_CULL_FACE);
            glCullFace(cullMode == 0 ? GL_BACK : cullMode == 1 ? GL_FRONT : GL_FRONT_AND_BACK);
        } else {
            glDisable(GL_CULL_FACE);
        }
        depthTest  ? glEnable(GL_DEPTH_TEST)   : glDisable(GL_DEPTH_TEST);
        glDepthMask(depthWrite ? GL_TRUE : GL_FALSE);
        msaa       ? glEnable(GL_MULTISAMPLE)  : glDisable(GL_MULTISAMPLE);

        if (modelLoaded) {
            glm::mat4 view = g_cam.view();
            glm::mat4 proj = glm::perspective(glm::radians(g_cam.fov), (float)w / h, 0.01f, 500.f);

            glUseProgram(shader);
            glUniformMatrix4fv(glGetUniformLocation(shader, "u_View"),       1, GL_FALSE, glm::value_ptr(view));
            glUniformMatrix4fv(glGetUniformLocation(shader, "u_Projection"), 1, GL_FALSE, glm::value_ptr(proj));
            glUniform3fv      (glGetUniformLocation(shader, "u_CameraPos"),  1, glm::value_ptr(g_cam.pos));

            if (renderMode == 0) {
                
                glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
                applyLighting(shader, lighting);
                drawModel(model, shader);

            } else if (renderMode == 1) {
                
                glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
                glLineWidth(wireThickness);
                glUniform1i(glGetUniformLocation(shader, "u_RenderMode"), 1);
                glm::vec4 wc(wireColor[0], wireColor[1], wireColor[2], 1.f);
                glUniform4fv(glGetUniformLocation(shader, "u_FlatColor"), 1, glm::value_ptr(wc));
                drawModel(model, shader);
                glLineWidth(1.f);

            } else if (renderMode == 2) {
                
                glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
                applyLighting(shader, lighting);  
                drawModel(model, shader);

                glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
                glLineWidth(wireThickness);
                glEnable(GL_POLYGON_OFFSET_LINE);
                glPolygonOffset(-1.f, -1.f);
                glUniform1i(glGetUniformLocation(shader, "u_RenderMode"), 1);
                glm::vec4 wc(wireColor[0], wireColor[1], wireColor[2], 1.f);
                glUniform4fv(glGetUniformLocation(shader, "u_FlatColor"), 1, glm::value_ptr(wc));
                drawModel(model, shader);          // pass 2: wire overlay
                glDisable(GL_POLYGON_OFFSET_LINE);
                glLineWidth(1.f);

            } else if (renderMode == 3) {
                
                glPolygonMode(GL_FRONT_AND_BACK, GL_POINT);
                glPointSize(3.f);
                glUniform1i(glGetUniformLocation(shader, "u_RenderMode"), 1);
                glm::vec4 wc(wireColor[0], wireColor[1], wireColor[2], 1.f);
                glUniform4fv(glGetUniformLocation(shader, "u_FlatColor"), 1, glm::value_ptr(wc));
                drawModel(model, shader);
                glPointSize(1.f);
            }

            glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
        }

        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        glfwSwapBuffers(win);
    }

    NFD_Quit();
    destroyModel(model);
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    glfwTerminate();
}
