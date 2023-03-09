#include "GuiGraphics.h"
#include <stdio.h>
//#include "utl/Environment.h"
#include "filesystem"
#include <stb_image.h>
#if defined(IMGUI_IMPL_OPENGL_ES2)
#include <GLES2/gl2.h>
#endif

#include "imgui_internal.h"


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


DirectUpdateFrame::DirectUpdateFrame(GLFWwindow* context)
{
    m_backup_context = glfwGetCurrentContext();
    m_current_context = context;

    direct_eventlog_draw_data.CmdLists = out_list.Data;
    direct_eventlog_draw_data.DisplayPos = m_smvp.display_pos; // Set viewport to status message area: DisplayPos DisplaySize
    direct_eventlog_draw_data.DisplaySize = m_smvp.display_size;
    direct_eventlog_draw_data.FramebufferScale = m_smvp.frame_buffer_scale;
    direct_eventlog_draw_data.CmdListsCount = out_list.Size;

}

DirectUpdateFrame::~DirectUpdateFrame()
{

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
    
    return &m_draw_lists.back();
}


void ImGui::UpdateAllPlatformWindows()
{

}

void ImGui::RenderAllPlatformWindowsDefault()
{

}