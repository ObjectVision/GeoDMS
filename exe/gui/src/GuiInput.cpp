#include <imgui.h>
#include "GuiInput.h"

static ImGuiKey GLFWKeyToImGuiKey(int key)
{
    switch (key)
    {
    case GLFW_KEY_TAB: return ImGuiKey_Tab;
    case GLFW_KEY_LEFT: return ImGuiKey_LeftArrow;
    case GLFW_KEY_RIGHT: return ImGuiKey_RightArrow;
    case GLFW_KEY_UP: return ImGuiKey_UpArrow;
    case GLFW_KEY_DOWN: return ImGuiKey_DownArrow;
    case GLFW_KEY_PAGE_UP: return ImGuiKey_PageUp;
    case GLFW_KEY_PAGE_DOWN: return ImGuiKey_PageDown;
    case GLFW_KEY_HOME: return ImGuiKey_Home;
    case GLFW_KEY_END: return ImGuiKey_End;
    case GLFW_KEY_INSERT: return ImGuiKey_Insert;
    case GLFW_KEY_DELETE: return ImGuiKey_Delete;
    case GLFW_KEY_BACKSPACE: return ImGuiKey_Backspace;
    case GLFW_KEY_SPACE: return ImGuiKey_Space;
    case GLFW_KEY_ENTER: return ImGuiKey_Enter;
    case GLFW_KEY_ESCAPE: return ImGuiKey_Escape;
    case GLFW_KEY_APOSTROPHE: return ImGuiKey_Apostrophe;
    case GLFW_KEY_COMMA: return ImGuiKey_Comma;
    case GLFW_KEY_MINUS: return ImGuiKey_Minus;
    case GLFW_KEY_PERIOD: return ImGuiKey_Period;
    case GLFW_KEY_SLASH: return ImGuiKey_Slash;
    case GLFW_KEY_SEMICOLON: return ImGuiKey_Semicolon;
    case GLFW_KEY_EQUAL: return ImGuiKey_Equal;
    case GLFW_KEY_LEFT_BRACKET: return ImGuiKey_LeftBracket;
    case GLFW_KEY_BACKSLASH: return ImGuiKey_Backslash;
    case GLFW_KEY_RIGHT_BRACKET: return ImGuiKey_RightBracket;
    case GLFW_KEY_GRAVE_ACCENT: return ImGuiKey_GraveAccent;
    case GLFW_KEY_CAPS_LOCK: return ImGuiKey_CapsLock;
    case GLFW_KEY_SCROLL_LOCK: return ImGuiKey_ScrollLock;
    case GLFW_KEY_NUM_LOCK: return ImGuiKey_NumLock;
    case GLFW_KEY_PRINT_SCREEN: return ImGuiKey_PrintScreen;
    case GLFW_KEY_PAUSE: return ImGuiKey_Pause;
    case GLFW_KEY_KP_0: return ImGuiKey_Keypad0;
    case GLFW_KEY_KP_1: return ImGuiKey_Keypad1;
    case GLFW_KEY_KP_2: return ImGuiKey_Keypad2;
    case GLFW_KEY_KP_3: return ImGuiKey_Keypad3;
    case GLFW_KEY_KP_4: return ImGuiKey_Keypad4;
    case GLFW_KEY_KP_5: return ImGuiKey_Keypad5;
    case GLFW_KEY_KP_6: return ImGuiKey_Keypad6;
    case GLFW_KEY_KP_7: return ImGuiKey_Keypad7;
    case GLFW_KEY_KP_8: return ImGuiKey_Keypad8;
    case GLFW_KEY_KP_9: return ImGuiKey_Keypad9;
    case GLFW_KEY_KP_DECIMAL: return ImGuiKey_KeypadDecimal;
    case GLFW_KEY_KP_DIVIDE: return ImGuiKey_KeypadDivide;
    case GLFW_KEY_KP_MULTIPLY: return ImGuiKey_KeypadMultiply;
    case GLFW_KEY_KP_SUBTRACT: return ImGuiKey_KeypadSubtract;
    case GLFW_KEY_KP_ADD: return ImGuiKey_KeypadAdd;
    case GLFW_KEY_KP_ENTER: return ImGuiKey_KeypadEnter;
    case GLFW_KEY_KP_EQUAL: return ImGuiKey_KeypadEqual;
    case GLFW_KEY_LEFT_SHIFT: return ImGuiKey_LeftShift;
    case GLFW_KEY_LEFT_CONTROL: return ImGuiKey_LeftCtrl;
    case GLFW_KEY_LEFT_ALT: return ImGuiKey_LeftAlt;
    case GLFW_KEY_LEFT_SUPER: return ImGuiKey_LeftSuper;
    case GLFW_KEY_RIGHT_SHIFT: return ImGuiKey_RightShift;
    case GLFW_KEY_RIGHT_CONTROL: return ImGuiKey_RightCtrl;
    case GLFW_KEY_RIGHT_ALT: return ImGuiKey_RightAlt;
    case GLFW_KEY_RIGHT_SUPER: return ImGuiKey_RightSuper;
    case GLFW_KEY_MENU: return ImGuiKey_Menu;
    case GLFW_KEY_0: return ImGuiKey_0;
    case GLFW_KEY_1: return ImGuiKey_1;
    case GLFW_KEY_2: return ImGuiKey_2;
    case GLFW_KEY_3: return ImGuiKey_3;
    case GLFW_KEY_4: return ImGuiKey_4;
    case GLFW_KEY_5: return ImGuiKey_5;
    case GLFW_KEY_6: return ImGuiKey_6;
    case GLFW_KEY_7: return ImGuiKey_7;
    case GLFW_KEY_8: return ImGuiKey_8;
    case GLFW_KEY_9: return ImGuiKey_9;
    case GLFW_KEY_A: return ImGuiKey_A;
    case GLFW_KEY_B: return ImGuiKey_B;
    case GLFW_KEY_C: return ImGuiKey_C;
    case GLFW_KEY_D: return ImGuiKey_D;
    case GLFW_KEY_E: return ImGuiKey_E;
    case GLFW_KEY_F: return ImGuiKey_F;
    case GLFW_KEY_G: return ImGuiKey_G;
    case GLFW_KEY_H: return ImGuiKey_H;
    case GLFW_KEY_I: return ImGuiKey_I;
    case GLFW_KEY_J: return ImGuiKey_J;
    case GLFW_KEY_K: return ImGuiKey_K;
    case GLFW_KEY_L: return ImGuiKey_L;
    case GLFW_KEY_M: return ImGuiKey_M;
    case GLFW_KEY_N: return ImGuiKey_N;
    case GLFW_KEY_O: return ImGuiKey_O;
    case GLFW_KEY_P: return ImGuiKey_P;
    case GLFW_KEY_Q: return ImGuiKey_Q;
    case GLFW_KEY_R: return ImGuiKey_R;
    case GLFW_KEY_S: return ImGuiKey_S;
    case GLFW_KEY_T: return ImGuiKey_T;
    case GLFW_KEY_U: return ImGuiKey_U;
    case GLFW_KEY_V: return ImGuiKey_V;
    case GLFW_KEY_W: return ImGuiKey_W;
    case GLFW_KEY_X: return ImGuiKey_X;
    case GLFW_KEY_Y: return ImGuiKey_Y;
    case GLFW_KEY_Z: return ImGuiKey_Z;
    case GLFW_KEY_F1: return ImGuiKey_F1;
    case GLFW_KEY_F2: return ImGuiKey_F2;
    case GLFW_KEY_F3: return ImGuiKey_F3;
    case GLFW_KEY_F4: return ImGuiKey_F4;
    case GLFW_KEY_F5: return ImGuiKey_F5;
    case GLFW_KEY_F6: return ImGuiKey_F6;
    case GLFW_KEY_F7: return ImGuiKey_F7;
    case GLFW_KEY_F8: return ImGuiKey_F8;
    case GLFW_KEY_F9: return ImGuiKey_F9;
    case GLFW_KEY_F10: return ImGuiKey_F10;
    case GLFW_KEY_F11: return ImGuiKey_F11;
    case GLFW_KEY_F12: return ImGuiKey_F12;
    default: return ImGuiKey_None;
    }
}

std::pair<std::string, std::string> GLFWKeyToLetter(int key)
{
    switch (key)
    {
    case GLFW_KEY_A: return {"a", "A"};
    case GLFW_KEY_B: return {"b", "B" };
    case GLFW_KEY_C: return {"c", "C" };
    case GLFW_KEY_D: return {"d", "D" };
    case GLFW_KEY_E: return {"e", "E" };
    case GLFW_KEY_F: return {"f", "F" };
    case GLFW_KEY_G: return {"g", "G"};
    case GLFW_KEY_H: return {"h", "H"};
    case GLFW_KEY_I: return {"i", "I"};
    case GLFW_KEY_J: return {"j", "J"};
    case GLFW_KEY_K: return {"k", "K"};
    case GLFW_KEY_L: return {"l", "L"};
    case GLFW_KEY_M: return {"m", "M"};
    case GLFW_KEY_N: return {"n", "N"};
    case GLFW_KEY_O: return {"o", "O"};
    case GLFW_KEY_P: return {"p", "P"};
    case GLFW_KEY_Q: return {"q", "Q"};
    case GLFW_KEY_R: return {"r", "R"};
    case GLFW_KEY_S: return {"s", "S"};
    case GLFW_KEY_T: return {"t", "T"};
    case GLFW_KEY_U: return {"u", "U"};
    case GLFW_KEY_V: return {"v", "V"};
    case GLFW_KEY_W: return {"w", "W"};
    case GLFW_KEY_X: return {"x", "X"};
    case GLFW_KEY_Y: return {"y", "Y"};
    case GLFW_KEY_Z: return {"z", "Z"};
    default:return {"", ""};
    }
}

void GuiInput::ProcessKeyEvent(GLFWwindow* window, int key, int scancode, int action, int mods)
{
    if (action == GLFW_PRESS || action == GLFW_REPEAT) // Pass down key event to ImGui io state
        ImGui::GetIO().AddKeyEvent(GLFWKeyToImGuiKey(key), true);
    else
        ImGui::GetIO().AddKeyEvent(GLFWKeyToImGuiKey(key), false);

    if (!(action == GLFW_PRESS))
        return;

    // unmodified key press for step to in TreeView
    if (key >= GLFW_KEY_A && key <= GLFW_KEY_Z)
    {
        m_State.m_JumpLetter = GLFWKeyToLetter(key);
    }

	switch (key)
	{
    case GLFW_KEY_D:
    {
        if (mods == GLFW_MOD_CONTROL) // CTRL-D
        {
            //m_State.ShowTableviewWindow = true;
            //m_State.TableViewIsActive = true;
            //m_State.MainEvents.Add(GuiEvents::UpdateCurrentAndCompatibleSubItems);
            //m_State.TableViewEvents.Add(GuiEvents::UpdateCurrentAndCompatibleSubItems);
            m_State.MainEvents.Add(OpenNewTableViewWindow);
        }

        return;
    }
	case GLFW_KEY_E:
	{
		if (mods == GLFW_MOD_CONTROL) // CTRL-E
			m_State.ShowConfigSource = true;
		return;
	}
    case GLFW_KEY_M:
    {
        if (mods == GLFW_MOD_CONTROL) // CTRL-M
        {
            m_State.MainEvents.Add(OpenNewMapViewWindow);
            //m_State.ShowMapviewWindow = true;
            //m_State.MainEvents.Add(GuiEvents::UpdateCurrentItem);
            //m_State.MapViewEvents.Add(GuiEvents::UpdateCurrentItem);
        }
        return;
    }
    case GLFW_KEY_O:
    {
        if (mods == GLFW_MOD_SHIFT)   // SHIFT-O
            m_State.ShowOpenFileWindow = true;
        else if (mods == (GLFW_MOD_CONTROL | GLFW_MOD_ALT)) // CTRL-ALT-O
            m_State.ShowOptionsWindow = true;
        return;
    }
    case GLFW_KEY_R:
    {
        if (mods == GLFW_MOD_ALT)
        {
            m_State.MainEvents.Add(GuiEvents::ReopenCurrentConfiguration);
            m_State.GuiMenuFileComponentEvents.Add(GuiEvents::ReopenCurrentConfiguration);
        }

        return;
    }
    case GLFW_KEY_T:
    {
        if (mods == GLFW_MOD_CONTROL) // CTRL-T
            m_State.MainEvents.Add(GuiEvents::UpdateCurrentAndCompatibleSubItems);
        return;
    }

	case GLFW_KEY_0:
	{
		if (mods == GLFW_MOD_ALT)    // ALT-0
			m_State.ShowTreeviewWindow = !m_State.ShowTreeviewWindow;
		return;
	}
	case GLFW_KEY_1:
	{
		if (mods == GLFW_MOD_ALT)    // ALT-1
			m_State.ShowDetailPagesWindow = !m_State.ShowDetailPagesWindow;
		return;
	}
	case GLFW_KEY_2:
	{
		if (mods == GLFW_MOD_ALT)    // ALT-2
			m_State.ShowEventLogWindow = !m_State.ShowEventLogWindow;
		return;
	}
    case GLFW_KEY_3:
    {
        if (mods == GLFW_MOD_ALT)    // ALT-3
            m_State.ShowToolbar = !m_State.ShowToolbar;
        return;
    }
	}
}