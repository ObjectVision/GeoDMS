#pragma once
#include <vector>
#include <GLFW/glfw3.h> // See https://www.glfw.org/docs/latest/input_guide.html#input_key

#include "GuiBase.h"

class GuiInput : GuiBaseComponent
{
public:
	static void ProcessKeyEvent(GLFWwindow* window, int key, int scancode, int action, int mods);

private:
	static GuiState m_State;
};