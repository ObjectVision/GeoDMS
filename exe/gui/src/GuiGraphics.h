#pragma once
#include <string>
#include <GL/glew.h>
#include <GLFW/glfw3.h> // Will drag system OpenGL headers
#include "imgui.h"

static void glfw_error_callback(int error, const char* description);
void SetErrorCallBackForGLFW();
int InitGLFW();
std::string SetGLFWWindowHints();

class DirectUpdateFrame
{
public:
	DirectUpdateFrame(GLFWwindow* context);
	~DirectUpdateFrame();
	auto AddDrawList(ImVec2 pos, ImVec2 size) -> ImDrawList*;

private:
	ImDrawData					   m_draw_data;
	GLFWwindow*					   m_backup_context;
	GLFWwindow*					   m_current_context;
	ImVector<ImDrawListSharedData> m_drawlists_shared_data;
	ImVector<ImDrawList>		   m_draw_lists;
	ImVector<ImDrawList*>		   m_draw_list_ptrs;
};

namespace ImGui
{
	void UpdateAllPlatformWindows();
	void RenderAllPlatformWindowsDefault();
}