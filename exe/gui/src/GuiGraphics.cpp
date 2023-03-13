#include "GuiGraphics.h"
#include <stdio.h>
//#include "utl/Environment.h"
#include "filesystem"
#include <stb_image.h>
#if defined(IMGUI_IMPL_OPENGL_ES2)
#include <GLES2/gl2.h>
#endif

#include "imgui_internal.h"
#include "imgui_impl_opengl3.h"


//#include <GLFW/glfw3.h> // Will drag system OpenGL headers

//#include "ptr/SharedStr.h"

static void glfw_error_callback(int error, const char* description)
{
    fprintf(stderr, "Glfw Error %d: %s\n", error, description);
}

void SetErrorCallBackForGLFW()
{
    glfwSetErrorCallback(glfw_error_callback);
}

int InitGLFW()
{
    return glfwInit();
}

std::string SetGLFWWindowHints()
{
    // Decide GL+GLSL versions
#if defined(IMGUI_IMPL_OPENGL_ES2)
    // GL ES 2.0 + GLSL 100
    std::string glsl_version = "#version 100";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 2);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
    glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_ES_API);
#elif defined(__APPLE__)
    // GL 3.2 + GLSL 150
    std::string glsl_version = "#version 150";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);  // 3.2+ only
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);            // Required on Mac
#else
    // GL 3.0 + GLSL 130
    std::string glsl_version = "#version 130";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
    //glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);  // 3.2+ only
    //glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);            // 3.0+ only
#endif
    return glsl_version;
}


DirectUpdateFrame::DirectUpdateFrame(GLFWwindow* context, ImVec2 display_pos, ImVec2 display_size)
{
    m_backup_context = glfwGetCurrentContext();
    m_current_context = context;
    
    m_draw_data.DisplayPos = display_pos;
    m_draw_data.DisplaySize = display_size;
    m_draw_data.FramebufferScale = ImVec2(1.0, 1.0);
}

DirectUpdateFrame::~DirectUpdateFrame()
{
    m_draw_data.CmdLists = m_draw_list_ptrs.Data;
    ImGui_ImplOpenGL3_RenderDrawData(&m_draw_data);
    glfwSwapBuffers(m_current_context);
    glfwMakeContextCurrent(m_backup_context); // restore backup context
}

auto DirectUpdateFrame::AddDrawList(ImVec2 pos, ImVec2 size) -> ImDrawList*
{
    ImGuiContext& g = *GImGui;

    // setup drawlist shared data
    m_drawlists_shared_data.push_back({});
    m_drawlists_shared_data.back().TexUvWhitePixel = g.DrawListSharedData.TexUvWhitePixel;
    
    // setup draw list
    m_draw_lists.push_back({ &m_drawlists_shared_data.back() });
    m_draw_list_ptrs.push_back(&m_draw_lists.back());
    m_draw_lists.back()._ResetForNewFrame();
    m_draw_lists.back().PushTextureID(g.Font->ContainerAtlas->TexID);
    m_draw_lists.back().PushClipRect(pos, ImVec2(pos.x+size.x, pos.y+size.y), false);

    m_draw_data.CmdListsCount++;

    return &m_draw_lists.back();
}


void ImGui::UpdateAllPlatformWindows()
{

}

void ImGui::RenderAllPlatformWindowsDefault()
{

}