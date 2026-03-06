// main.cpp – GLTF/GLB viewer with ImGui UI
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <tinyfiledialogs.h>

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

#include "shader.h"
#include "model.h"
#include "camera.h"

#include <iostream>
#include <string>

// Global variables
static int   g_winW=1280, g_winH=720;
static float g_dt=0.f, g_lastFrame=0.f;
static bool  g_firstMouse=true;
static float g_lastX=640.f, g_lastY=360.f;
static Camera* g_cam=nullptr;

enum class AppState { START, VIEW };
static AppState g_state = AppState::START;

// Callbacks for GLFW events
static void cbFramebuffer(GLFWwindow*, int w, int h){ glViewport(0,0,w,h); }
static void cbWindow(GLFWwindow*, int w, int h){ g_winW=w; g_winH=h; }
static void cbScroll(GLFWwindow*,double,double y){
    if(g_state==AppState::VIEW && !ImGui::GetIO().WantCaptureMouse && g_cam)
        g_cam->ProcessMouseScroll((float)y);
}
static void cbMouse(GLFWwindow* w, double x, double y){
    if(g_state!=AppState::VIEW || ImGui::GetIO().WantCaptureMouse || !g_cam) return;
    if(glfwGetMouseButton(w,GLFW_MOUSE_BUTTON_RIGHT)!=GLFW_PRESS){g_firstMouse=true;return;}
    if(g_firstMouse){g_lastX=(float)x;g_lastY=(float)y;g_firstMouse=false;}
    g_cam->ProcessMouseMovement((float)x-g_lastX, g_lastY-(float)y);
    g_lastX=(float)x; g_lastY=(float)y;
}
static void doMove(GLFWwindow* w){
    if(g_state!=AppState::VIEW || ImGui::GetIO().WantCaptureKeyboard || !g_cam) return;
    if(glfwGetKey(w,GLFW_KEY_W)==GLFW_PRESS) g_cam->ProcessKeyboard(CameraMovement::FORWARD, g_dt);
    if(glfwGetKey(w,GLFW_KEY_S)==GLFW_PRESS) g_cam->ProcessKeyboard(CameraMovement::BACKWARD,g_dt);
    if(glfwGetKey(w,GLFW_KEY_A)==GLFW_PRESS) g_cam->ProcessKeyboard(CameraMovement::LEFT,    g_dt);
    if(glfwGetKey(w,GLFW_KEY_D)==GLFW_PRESS) g_cam->ProcessKeyboard(CameraMovement::RIGHT,   g_dt);
    if(glfwGetKey(w,GLFW_KEY_Q)==GLFW_PRESS) g_cam->ProcessKeyboard(CameraMovement::DOWN,    g_dt);
    if(glfwGetKey(w,GLFW_KEY_E)==GLFW_PRESS) g_cam->ProcessKeyboard(CameraMovement::UP,      g_dt);
}

// Main entry point
int main(int argc, char* argv[]){
    if(!glfwInit()){ std::cerr<<"GLFW init failed\n"; return 1; }
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR,3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR,3);
    glfwWindowHint(GLFW_OPENGL_PROFILE,GLFW_OPENGL_CORE_PROFILE);
#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT,GL_TRUE);
#endif
    glfwWindowHint(GLFW_SAMPLES,4);

    GLFWwindow* win=glfwCreateWindow(g_winW,g_winH,"GLTF Loader",nullptr,nullptr);
    if(!win){ std::cerr<<"Window failed\n"; glfwTerminate(); return 1; }
    glfwMakeContextCurrent(win);
    glfwSwapInterval(1);
    glfwGetWindowSize(win,&g_winW,&g_winH);
    glfwSetFramebufferSizeCallback(win,cbFramebuffer);
    glfwSetWindowSizeCallback(win,cbWindow);
    glfwSetCursorPosCallback(win,cbMouse);
    glfwSetScrollCallback(win,cbScroll);

    if(!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)){
        std::cerr<<"glad failed\n"; glfwTerminate(); return 1;
    }
    std::cout<<"GL: "<<glGetString(GL_VERSION)<<"\n";

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_MULTISAMPLE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);

    // I'm GUI steup code, so it's all in main() for simplicity
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    // Style
    ImGui::StyleColorsDark();
    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowRounding    = 8.f;
    style.FrameRounding     = 6.f;
    style.GrabRounding      = 6.f;
    style.ItemSpacing       = ImVec2(10,8);
    style.FramePadding      = ImVec2(12,8);
    style.WindowPadding     = ImVec2(20,20);
    style.Colors[ImGuiCol_WindowBg]       = ImVec4(0.08f,0.09f,0.14f,0.92f);
    style.Colors[ImGuiCol_Button]         = ImVec4(0.20f,0.36f,0.72f,1.00f);
    style.Colors[ImGuiCol_ButtonHovered]  = ImVec4(0.35f,0.55f,0.95f,1.00f);
    style.Colors[ImGuiCol_ButtonActive]   = ImVec4(0.15f,0.28f,0.60f,1.00f);
    style.Colors[ImGuiCol_Header]         = ImVec4(0.20f,0.36f,0.72f,0.60f);
    style.Colors[ImGuiCol_HeaderHovered]  = ImVec4(0.35f,0.55f,0.95f,0.80f);

    ImGui_ImplGlfw_InitForOpenGL(win,true);
    ImGui_ImplOpenGL3_Init("#version 330");

    // This is for the Retina displays on mac
    int fbW,fbH; glfwGetFramebufferSize(win,&fbW,&fbH);
    float dpi = (float)fbW / (float)g_winW;
    std::cout<<"DPI scale: "<<dpi<<"\n";
    //handles retina automatically
    io.DisplayFramebufferScale = ImVec2(dpi, dpi);
    io.Fonts->Clear();
    ImFontConfig cfg;
    cfg.SizePixels = 16.f;   // logical pixels - ImGui scales internally
    io.Fonts->AddFontDefault(&cfg);
    ImGui_ImplOpenGL3_CreateFontsTexture();

    // Other classes
    Camera cam(glm::vec3(0,0,4));
    g_cam=&cam;

    Shader modelShader;
    try{ modelShader=Shader("shaders/model.vert","shaders/model.frag"); }
    catch(const std::exception& e){ std::cerr<<e.what()<<"\n"; glfwTerminate(); return 1; }

    Model model; std::string modelName; bool loaded=false;

    auto fitCam=[&](){
        glm::vec3 c=(model.aabbMin+model.aabbMax)*.5f;
        float r=glm::length(model.aabbMax-model.aabbMin)*.5f;
        if(r<.001f)r=1.f;
        cam.Position=c+glm::vec3(0,r*.5f,r*2.5f);
        cam.Speed=r;
    };

    if(argc>=2){
        if(model.load(argv[1])){
            loaded=true;
            modelName=std::string(argv[1]);
            modelName=modelName.substr(modelName.find_last_of("/\\")+1);
            fitCam(); g_state=AppState::VIEW;
        }
    }

    // Render loop
    while(!glfwWindowShouldClose(win)){
        float now=(float)glfwGetTime();
        g_dt=now-g_lastFrame; g_lastFrame=now;

        glfwPollEvents();
        doMove(win);
        if(glfwGetKey(win,GLFW_KEY_ESCAPE)==GLFW_PRESS)
            glfwSetWindowShouldClose(win,1);

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        glClearColor(0.07f,0.08f,0.13f,1.f);
        glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);

        if(g_state==AppState::START){
            // Start screen UI
            ImGui::SetNextWindowPos(ImVec2(0,0));
            ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize);
            ImGui::SetNextWindowBgAlpha(0.f);
            ImGui::Begin("##bg", nullptr,
                ImGuiWindowFlags_NoDecoration|ImGuiWindowFlags_NoInputs|
                ImGuiWindowFlags_NoNav|ImGuiWindowFlags_NoBringToFrontOnFocus);
            ImGui::End();

            // Centred panel
            ImVec2 ds = io.DisplaySize;
            float pw=500.f, ph=280.f;
            ImGui::SetNextWindowPos(ImVec2(ds.x*.5f-pw*.5f, ds.y*.5f-ph*.5f),
                                    ImGuiCond_Always);
            ImGui::SetNextWindowSize(ImVec2(pw,ph), ImGuiCond_Always);
            ImGui::Begin("##panel", nullptr,
                ImGuiWindowFlags_NoDecoration|ImGuiWindowFlags_NoMove|
                ImGuiWindowFlags_NoResize|ImGuiWindowFlags_NoScrollbar);

            // Title
            float titleW = ImGui::CalcTextSize("GLTF Model Viewer").x;
            ImGui::SetCursorPosX((pw-titleW)*.5f);
            ImGui::TextColored(ImVec4(0.55f,0.82f,1.f,1.f), "GLTF Model Viewer");

            ImGui::Spacing();
            float subW = ImGui::CalcTextSize("Open a .glb or .gltf file to get started").x;
            ImGui::SetCursorPosX((pw-subW)*.5f);
            ImGui::TextColored(ImVec4(0.55f,0.65f,0.75f,1.f),
                               "Open a .glb or .gltf file to get started");

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            // Open button
            float bw=240.f, bh=36.f;
            ImGui::SetCursorPosX((pw-bw)*.5f);
            if(ImGui::Button("Open Model...", ImVec2(bw,bh))){
                const char* ft[]={"*.glb","*.gltf"};
                const char* r=tinyfd_openFileDialog(
                    "Open GLTF/GLB","",2,ft,"GLTF/GLB Files",0);
                if(r){
                    std::string path=r;
                    model.destroy(); loaded=false;
                    if(model.load(path)){
                        loaded=true;
                        modelName=path.substr(path.find_last_of("/\\")+1);
                        fitCam(); g_state=AppState::VIEW;
                    }
                }
            }

            if(loaded){
                ImGui::Spacing();
                std::string hint="Last: "+modelName;
                float hw=ImGui::CalcTextSize(hint.c_str()).x;
                ImGui::SetCursorPosX((pw-hw)*.5f);
                ImGui::TextColored(ImVec4(0.4f,0.7f,0.4f,1.f),"%s",hint.c_str());
            }

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();
            const char* controls="WASD + RMB drag to fly   Scroll to zoom   ESC to quit";
            float cw=ImGui::CalcTextSize(controls).x;
            ImGui::SetCursorPosX((pw-cw)*.5f);
            ImGui::TextColored(ImVec4(0.4f,0.5f,0.6f,1.f),"%s",controls);

            ImGui::End();

        } else {
            // The model viewing screen
            glEnable(GL_DEPTH_TEST);
            glm::vec3 c=(model.aabbMin+model.aabbMax)*.5f;
            float r=glm::length(model.aabbMax-model.aabbMin)*.5f;
            if(r<.001f)r=1.f;

            glm::mat4 view=cam.GetViewMatrix();
            glm::mat4 proj=glm::perspective(
                glm::radians(cam.Zoom),
                (float)g_winW/(float)g_winH,
                r*.001f, r*100.f);

            modelShader.use();
            modelShader.setMat4("uView",view);
            modelShader.setMat4("uProjection",proj);
            modelShader.setVec3("uLightPos",c+glm::vec3(0,r*3.f,r*2.f));
            modelShader.setVec3("uLightColor",{1,1,1});
            modelShader.setVec3("uViewPos",cam.Position);
            if(loaded) model.draw(modelShader);

            // ─The HUD overlay 
            glDisable(GL_DEPTH_TEST);
            ImGui::SetNextWindowPos(ImVec2(0,0));
            ImGui::SetNextWindowSize(ImVec2(io.DisplaySize.x, 0));
            ImGui::SetNextWindowBgAlpha(0.78f);
            ImGui::Begin("##hud", nullptr,
                ImGuiWindowFlags_NoDecoration|ImGuiWindowFlags_NoMove|
                ImGuiWindowFlags_NoResize|ImGuiWindowFlags_NoScrollbar|
                ImGuiWindowFlags_NoNav);

            if(ImGui::Button("< Back")) g_state=AppState::START;
            ImGui::SameLine();

            // Model name centred
            std::string lbl=loaded?modelName:"No model loaded";
            float lw=ImGui::CalcTextSize(lbl.c_str()).x;
            float cx=io.DisplaySize.x*.5f - lw*.5f;
            ImGui::SetCursorPosX(cx);
            ImGui::TextColored(ImVec4(0.82f,0.93f,1.f,1.f),"%s",lbl.c_str());

            ImGui::End();

            // Controls hint bottom-left
            ImGui::SetNextWindowPos(ImVec2(10, io.DisplaySize.y-40));
            ImGui::SetNextWindowBgAlpha(0.f);
            ImGui::Begin("##hint",nullptr,
                ImGuiWindowFlags_NoDecoration|ImGuiWindowFlags_NoInputs|
                ImGuiWindowFlags_NoNav|ImGuiWindowFlags_NoMove);
            ImGui::TextColored(ImVec4(0.35f,0.45f,0.55f,1.f),
                "WASD/QE move   RMB drag look   Scroll zoom");
            ImGui::End();
        }

        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        glfwSwapBuffers(win);
    }

    model.destroy();
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    glfwTerminate();
    return 0;
}