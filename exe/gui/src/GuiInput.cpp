#include <imgui.h>
#include <imgui_internal.h>
#include "ser/AsString.h"
#include "TicInterface.h"
#include "GuiInput.h"
#include "dbg/DebugReporter.h"

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

auto GLFWKeyToLetter(int key) -> std::string
{
    switch (key)
    {
    case GLFW_KEY_A: return "A";
    case GLFW_KEY_B: return "B";
    case GLFW_KEY_C: return "C";
    case GLFW_KEY_D: return "D";
    case GLFW_KEY_E: return "E";
    case GLFW_KEY_F: return "F";
    case GLFW_KEY_G: return "G";
    case GLFW_KEY_H: return "H";
    case GLFW_KEY_I: return "I";
    case GLFW_KEY_J: return "J";
    case GLFW_KEY_K: return "K";
    case GLFW_KEY_L: return "L";
    case GLFW_KEY_M: return "M";
    case GLFW_KEY_N: return "N";
    case GLFW_KEY_O: return "O";
    case GLFW_KEY_P: return "P";
    case GLFW_KEY_Q: return "Q";
    case GLFW_KEY_R: return "R";
    case GLFW_KEY_S: return "S";
    case GLFW_KEY_T: return "T";
    case GLFW_KEY_U: return "U";
    case GLFW_KEY_V: return "V";
    case GLFW_KEY_W: return "W";
    case GLFW_KEY_X: return "X";
    case GLFW_KEY_Y: return "Y";
    case GLFW_KEY_Z: return "Z";
    default:return {};
    }
}

void GuiInput::ProcessDMSKeyEvent(GLFWwindow* window, int key, int scancode, int action, int mods)
{
    if (action == GLFW_RELEASE)
        return;

    auto event_queues = GuiEventQueues::getInstance();

    // unmodified key press for step to in TreeView
    auto treeview_window = ImGui::FindWindowByName("TreeView"); // TODO: one location for Window names ie TreeView
    if (ImGui::GetCurrentContext()->WindowsFocusOrder.back() == treeview_window && key >= GLFW_KEY_A && key <= GLFW_KEY_Z && !(mods == GLFW_MOD_CONTROL) && !(mods == GLFW_MOD_ALT) && !(mods == GLFW_MOD_SHIFT))
    {
        event_queues->TreeViewEvents.Add(GuiEvents::TreeViewJumpKeyPress);
        GuiState::m_JumpLetter = GLFWKeyToLetter(key);
    }

    switch (key)
    {
    case GLFW_KEY_D:
    {
        /*if (mods & (GLFW_MOD_CONTROL & GLFW_MOD_ALT)) // Ctrl+Alt+D
            event_queues->MainEvents.Add(GuiEvents::OpenNewDefaultViewWindow);*/
        if (mods == GLFW_MOD_CONTROL) // CTRL-D 
        {
            event_queues->MainEvents.Add(GuiEvents::OpenNewTableViewWindow);
        }

        return;
    }
    case GLFW_KEY_E:
    {
        if (mods == GLFW_MOD_CONTROL) // CTRL-E
        {
            event_queues->MainEvents.Add(GuiEvents::OpenConfigSource);
        }
        else if (mods == GLFW_MOD_ALT) // Open menu-edit
        {
            event_queues->MenuBarEvents.Add(GuiEvents::MenuOpenEdit);
        }

        return;
    }
    case GLFW_KEY_F:
    {
        if (mods == GLFW_MOD_ALT) // Open menu-file
            event_queues->MenuBarEvents.Add(GuiEvents::MenuOpenFile);
        return;
    }
    case GLFW_KEY_H:
    {
        if (mods == GLFW_MOD_ALT) // Open menu-help
            event_queues->MenuBarEvents.Add(GuiEvents::MenuOpenHelp);
        return;
    }
    case GLFW_KEY_I:
    {
        if (mods == GLFW_MOD_CONTROL) // Open statistics view window
            event_queues->MainEvents.Add(GuiEvents::OpenNewStatisticsViewWindow);
        return;
    }
    case GLFW_KEY_L:
    {
        if (mods == GLFW_MOD_CONTROL) // CTRL-L
            event_queues->MainEvents.Add(GuiEvents::CloseAllViews);
        return;
    }
    case GLFW_KEY_M:
    {
        if (mods == GLFW_MOD_CONTROL) // CTRL-M
            event_queues->MainEvents.Add(GuiEvents::OpenNewMapViewWindow);
        return;
    }
    case GLFW_KEY_O:
    {
        //if (mods == GLFW_MOD_SHIFT)   // SHIFT-O
        //    m_State.ShowOpenFileWindow = true;
        //else if (mods == (GLFW_MOD_CONTROL | GLFW_MOD_ALT)) // CTRL-ALT-O
        //    m_State.ShowOptionsWindow = true;
        return;
    }
    case GLFW_KEY_R:
    {
        if (mods == GLFW_MOD_ALT)
        {
            event_queues->MainEvents.Add(GuiEvents::ReopenCurrentConfiguration);
            event_queues->GuiMenuFileComponentEvents.Add(GuiEvents::ReopenCurrentConfiguration);
        }

        return;
    }
    case GLFW_KEY_T:
    {
        if (mods & (GLFW_MOD_ALT|GLFW_MOD_CONTROL)) // CTRL-ALT-T
        {
#if defined(MG_DEBUG)
            DebugReporter::ReportAll();
#endif
        }
        else if (mods == GLFW_MOD_CONTROL) // CTRL-T
            event_queues->MainEvents.Add(GuiEvents::UpdateCurrentAndCompatibleSubItems);
        else if (mods == GLFW_MOD_ALT) // ALT-T Open menu-tools
            event_queues->MenuBarEvents.Add(GuiEvents::MenuOpenTools);

        return;
    }
    case GLFW_KEY_V:
    {
        if (mods == GLFW_MOD_ALT) // Open menu-view
            event_queues->MenuBarEvents.Add(GuiEvents::MenuOpenView); 
        return;
    }
    case GLFW_KEY_W:
    {
        if (mods == GLFW_MOD_ALT) // Open menu-window
            event_queues->MenuBarEvents.Add(GuiEvents::MenuOpenWindow);
        return;
    }
    case GLFW_KEY_0:
    {
        if (mods == GLFW_MOD_ALT)    // ALT-0
            event_queues->MainEvents.Add(GuiEvents::ToggleShowTreeViewWindow);
        return;
    }
    case GLFW_KEY_1:
    {
        if (mods == GLFW_MOD_ALT)    // ALT-1
            event_queues->MainEvents.Add(GuiEvents::ToggleShowDetailPagesWindow);
        return;
    }
    case GLFW_KEY_2:
    {
        if (mods == GLFW_MOD_ALT)    // ALT-2
            event_queues->MainEvents.Add(GuiEvents::ToggleShowEventLogWindow);
        return;
    }
    case GLFW_KEY_3:
    {
        if (mods == GLFW_MOD_ALT)    // ALT-3
            event_queues->MainEvents.Add(GuiEvents::ToggleShowToolbar);
        return;
    }
    case GLFW_KEY_F2:
    {
        // TODO: implement transitive shift-F2
        //auto unfound_part = IString::Create("");
        //auto item_error_source = DMS_TreeItem_GetErrorSource(m_State.GetCurrentItem(), &unfound_part);
        //unfound_part->Release(unfound_part);
        if (mods == GLFW_MOD_SHIFT)
            event_queues->MainEvents.Add(GuiEvents::StepToRootErrorSource);
        else
            event_queues->MainEvents.Add(GuiEvents::StepToErrorSource);
        return;
        // 
        //if (!item_error_source)
        //    return;
        
        //m_State.SetCurrentItem(item_error_source);
        //m_State.CurrentItemBarEvents.Add(GuiEvents::UpdateCurrentItem);
        //m_State.MainEvents.Add(GuiEvents::UpdateCurrentItem);
        //m_State.DetailPagesEvents.Add(GuiEvents::UpdateCurrentItem);
        //m_State.TreeViewEvents.Add(GuiEvents::JumpToCurrentItem);
    
    }
    case GLFW_KEY_UP:
    {
        event_queues->TreeViewEvents.Add(GuiEvents::AscendVisibleTree);
        return;
    }
    case GLFW_KEY_DOWN:
    {
        if (action == GLFW_REPEAT)
            int i = 0;
        event_queues->TreeViewEvents.Add(GuiEvents::DescendVisibleTree);
        return;
    }
    }
}