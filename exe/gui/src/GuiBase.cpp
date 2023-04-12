#include "GuiBase.h"

#include <filesystem>
#include <iterator>
#include <imgui.h>
#include <imgui_internal.h>
#include <boost/algorithm/string/classification.hpp> // Include boost::for is_any_of
#include <boost/algorithm/string/split.hpp> // Include for boost::split
#include <Windows.h>
#include "TicInterface.h"
#include "utl/Environment.h"

#include <GLFW/glfw3.h>
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>

// for windows open file dialog
#include <windows.h>
#include <shobjidl.h> 
#include <shlobj.h>
#include <codecvt>

GuiEventQueues* GuiEventQueues::instance = nullptr;
GuiEventQueues* GuiEventQueues::getInstance()
{
    if (!instance) {
        instance = new GuiEventQueues();
        return instance;
    }
    else
        return instance;
}

auto GuiEventQueues::DeleteInstance() -> void
{
    if (instance)
        delete instance;

}

bool ImGui::InputText(const char* label, std::string* str, ImGuiInputTextFlags flags, ImGuiInputTextCallback callback, void* user_data)
{
    IM_ASSERT((flags & ImGuiInputTextFlags_CallbackResize) == 0);
    flags |= ImGuiInputTextFlags_CallbackResize;

    InputTextCallback_UserData cb_user_data;
    cb_user_data.Str = str;
    cb_user_data.ChainCallback = callback;
    cb_user_data.ChainCallbackUserData = user_data;
    return InputText(label, (char*)str->c_str(), str->capacity() + 1, flags, InputTextCallback, &cb_user_data);
}

ViewActionHistory::ViewActionHistory()
{
    m_Iterator = m_History.begin();
}

auto ViewActionHistory::Insert(ViewAction new_item) -> void
{
    if (m_Iterator == m_History.end())
    {
        m_History.push_back(new_item);
        m_Iterator = std::prev(m_History.end());
        return;
    }

    if (new_item == *m_Iterator)
        return;

    m_Iterator = m_History.insert(std::next(m_Iterator), new_item);
}

ViewAction ViewActionHistory::GetNext()
{
    if (std::distance(m_History.end(), m_Iterator))
    {
        if (std::next(m_Iterator) == m_History.end())
            return {};

        std::advance(m_Iterator, 1);
        return *m_Iterator;
    }
    return {};
}

ViewAction ViewActionHistory::GetPrevious()
{
    if (std::distance(m_Iterator, m_History.begin()))
    {
        std::advance(m_Iterator, -1);
        return *m_Iterator;
    }
    return {};
}

auto ViewActionHistory::GetCurrentIterator() -> std::list<ViewAction>::iterator
{
    return m_Iterator;
}

auto ViewActionHistory::GetBeginIterator() -> std::list<ViewAction>::iterator
{
    return m_History.begin();
}

auto ViewActionHistory::GetEndIterator() -> std::list<ViewAction>::iterator
{
    return m_History.end();
}

StringStateManager GuiState::errorDialogMessage;
StringStateManager GuiState::contextMessage;
ViewActionHistory GuiState::TreeItemHistoryList;
std::string GuiState::m_JumpLetter;

auto GuiState::clear() -> void
{
    m_CurrentItem.reset();
    if (m_Root.has_ptr())
        m_Root->EnableAutoDelete();

    m_Root.reset();
    
    TreeItemHistoryList.clear();
}

GuiState::~GuiState()
{
    clear();
}

auto DivideTreeItemFullNameIntoTreeItemNames(std::string fullname, std::string separator) -> std::vector<std::string>
{
    std::vector<std::string> SeparatedTreeItemFullName;
    boost::split(SeparatedTreeItemFullName, fullname, boost::is_any_of(separator), boost::token_compress_on);
    return SeparatedTreeItemFullName;
}

auto GetExeFilePath() -> std::string
{
    wchar_t wBuffer[MAX_PATH];
    GetModuleFileNameW(NULL, wBuffer, MAX_PATH);
    auto exePath = std::filesystem::path(std::move(wBuffer)).remove_filename();
    return exePath.string();
}

ImVec2 SetCursorPosToOptionsIconInWindowHeader()
{
    auto oldCursosPos = ImGui::GetCursorPos();
    auto window_size = ImGui::GetWindowSize();
    ImGui::SetCursorPos(ImVec2(window_size.x - 38, 6)); // TODO: hardcoded offsets, do these scale with different screen resolutions?
    return oldCursosPos;
}

void SetClipRectToIncludeOptionsIconInWindowHeader()
{
    auto test = ImGui::GetWindowDrawList();
    auto window_clip_rect_min = ImGui::GetWindowDrawList()->GetClipRectMin();
    ImGui::PushClipRect(ImVec2(window_clip_rect_min.x, window_clip_rect_min.y - 20), ImGui::GetWindowDrawList()->GetClipRectMax(), false);
}

bool MouseHooversOptionsIconInWindowHeader()
{
    auto window_clip_rect_min = ImGui::GetWindowDrawList()->GetClipRectMin();
    auto window_clip_rect_max = ImGui::GetWindowDrawList()->GetClipRectMax();
    auto rect_min = ImVec2(window_clip_rect_max.x - 35, window_clip_rect_min.y);
    auto rect_max = ImVec2(window_clip_rect_max.x - 20, window_clip_rect_min.y + 15);
    return ImGui::IsMouseHoveringRect(rect_min, rect_max);
}

void SetKeyboardFocusToThisHwnd()
{
    auto window = ImGui::GetCurrentWindow();
    SetFocus((HWND)window->Viewport->PlatformHandleRaw);
}

void GuiState::SaveWindowOpenStatusFlags()
{
    UInt32 flags = 0;
    flags = ShowTreeviewWindow ? flags |= GWOF_TreeView : flags &= ~GWOF_TreeView;
    flags = ShowDetailPagesWindow ? flags |= GWOF_DetailPages : flags &= ~GWOF_DetailPages;
    flags = ShowEventLogWindow ? flags |= GWOF_EventLog : flags &= ~GWOF_EventLog;
    flags = ShowOptionsWindow ? flags |= GWOF_Options : flags &= ~GWOF_Options;
    flags = ShowToolbar ? flags |= GWOF_ToolBar : flags &= ~GWOF_ToolBar;
    flags = ShowCurrentItemBar ? flags |= GWOF_CurrentItemBar : flags &= ~GWOF_CurrentItemBar;
    flags = ShowStatusBar ? flags |= GWOF_StatusBar : flags &= ~GWOF_StatusBar;
    SetGeoDmsRegKeyDWord("WindowOpenStatusFlags", flags);
}

void GuiState::SetWindowOpenStatusFlagsOnFirstUse() // first use based on availability of registry param
{
    ShowTreeviewWindow = true;
    ShowDetailPagesWindow = true;
    ShowEventLogWindow = true;
    ShowOptionsWindow = false;
    ShowToolbar = true;
    ShowCurrentItemBar = true;
    ShowStatusBar = false;
    SaveWindowOpenStatusFlags();
}

void GuiState::LoadWindowOpenStatusFlags()
{
    bool exists = false;
    auto flags = false; //ALPHA GetRegFlags("WindowOpenStatusFlags", exists);

    if (!exists)
    {
        SetWindowOpenStatusFlagsOnFirstUse();
        return;
    }

    // update open state based on flags
    ShowTreeviewWindow = flags && GWOF_TreeView;
    ShowDetailPagesWindow = flags && GWOF_DetailPages;
    ShowEventLogWindow = flags && GWOF_EventLog;
    ShowOptionsWindow = flags && GWOF_Options;
    ShowToolbar = flags && GWOF_ToolBar;
    ShowCurrentItemBar = flags && GWOF_CurrentItemBar;
    ShowStatusBar = flags && GWOF_StatusBar;
}

std::string GetInitialWindowComposition()
{
    std::string result =
        "[Window][GeoDMSGui]\n"
        "Pos=0,21\n"
        "Size=1280,699\n"
        "Collapsed=0\n"
        "\n"
        "[Window][Toolbar]\n"
        "Pos=8,50\n"
        "Size=1264,31\n"
        "Collapsed=0\n"
        "DockId=0x00000009,0\n"
        "\n"
        "[Window][Detail Pages]\n"
        "Pos=891,83\n"
        "Size=381,254\n"
        "Collapsed=0\n"
        "DockId=0x00000004,0\n"
        "\n"
        "[Window][TreeView]\n"
        "Pos=8,83\n"
        "Size=367,254\n"
        "Collapsed=0\n"
        "DockId=0x00000001,0\n"
        "\n"
        "[Window][EventLog]\n"
        "Pos=8,339\n"
        "Size=1264,339\n"
        "Collapsed=0\n"
        "DockId=0x00000006,0\n"
        "\n"
        "[Window][DMSView]\n"
        "Pos=377,83\n"
        "Size=1792,911\n"
        "Collapsed=0\n"
        "DockId=0x00000002,0\n"
        "\n"
        "[Window][Debug##Default]\n"
        "ViewportPos=94,117\n"
        "ViewportId=0x9F5F46A1\n"
        "Size=400,400\n"
        "Collapsed=0\n"
        "\n"
        "[Window][StatusBar]\n"
        "Pos=8,680\n"
        "Size=1264,32\n"
        "Collapsed=0\n"
        "DockId=0x00000008,0\n"
        "\n"
        "[Docking][Data]\n"
        "DockSpace           ID=0x54D8F03E Window=0x47EE5377 Pos=120,185 Size=1264,662 Split=Y\n"
        "  DockNode          ID=0x00000009 Parent=0x54D8F03E SizeRef=2544,31 HiddenTabBar=1 Selected=0x738351EE\n"
        "  DockNode          ID=0x0000000A Parent=0x54D8F03E SizeRef=2544,1286 Split=Y\n"
        "    DockNode        ID=0x00000007 Parent=0x0000000A SizeRef=1264,1252 Split=Y\n"
        "      DockNode      ID=0x00000005 Parent=0x00000007 SizeRef=1264,911 Split=X\n"
        "        DockNode    ID=0x00000003 Parent=0x00000005 SizeRef=881,662 Split=X\n"
        "          DockNode  ID=0x00000001 Parent=0x00000003 SizeRef=367,662 Selected=0x0C84ACA2\n"
        "          DockNode  ID=0x00000002 Parent=0x00000003 SizeRef=895,662 CentralNode=1 Selected=0x1BA3A327\n"
        "        DockNode    ID=0x00000004 Parent=0x00000005 SizeRef=381,662 Selected=0x89482BF9\n"
        "      DockNode      ID=0x00000006 Parent=0x00000007 SizeRef=1264,339 Selected=0xB76E45CC\n"
        "    DockNode        ID=0x00000008 Parent=0x0000000A SizeRef=1264,32 HiddenTabBar=1 Selected=0x51C70801\n"
        "\n";
    return result;
}

void SetWindowCompositionOnFirstUse()
{
    ImGui::LoadIniSettingsFromMemory(GetInitialWindowComposition().c_str());
    //SetGeoDmsRegKeyString("WindowComposition", GetInitialWindowComposition());
}

bool LoadIniFromRegistry(bool reload)
{
    auto ini_registry_contents = GetGeoDmsRegKey("WindowComposition");
    if (reload || ini_registry_contents.empty())
    {
        ImGui::LoadIniSettingsFromMemory(GetInitialWindowComposition().c_str());
        return false;
    }

    ImGui::LoadIniSettingsFromMemory(ini_registry_contents.c_str());
    return true;

    //SetWindowCompositionOnFirstUse();
    //ini_registry_contents = GetGeoDmsRegKey("WindowComposition");
    /*if (!ini_registry_contents.empty())
    {
        reportF(SeverityTypeID::ST_MajorTrace, "Loading GeoDMS window composition from registry.");
        
    }*/
}

void   SaveIniToRegistry()
{
    std::string ini_contents = ImGui::SaveIniSettingsToMemory();
    SetGeoDmsRegKeyString("WindowComposition", ini_contents);
    reportF(SeverityTypeID::ST_MajorTrace, "Storing GeoDMS window composition in registry.");
}

auto OnItemClickItemTextTextToClipboard(std::string_view text) -> void
{
    if (text.empty())
        return;
    if (ImGui::IsItemClicked()) // TODO: duplicate code in EventLog
        ImGui::SetClipboardText(&text.front());
    if (ImGui::IsItemHovered() && ImGui::IsMouseDown(ImGuiMouseButton_Left))
    {
        ImGui::SetTooltip("Copied to clipboard");
    }
}

void SetTextBackgroundColor(ImVec2 background_rectangle_size, ImU32 col, ImDrawList* draw_list, ImVec2* cursor_pos)
{
    if (!draw_list)
        draw_list = ImGui::GetWindowDrawList();

    if (!cursor_pos)
    {
        auto cp = ImGui::GetCursorScreenPos();
        cursor_pos = &cp;
    }

    draw_list->AddRectFilled(*cursor_pos, ImVec2((*cursor_pos).x + background_rectangle_size.x, (*cursor_pos).y + background_rectangle_size.y), col);
}

auto GeoDMSWindowTypeToName(GeoDMSWindowTypes wt) -> std::string
{
    switch (wt)
    {
    case GeoDMSWindowTypes::GeoDMSGui:   return "GeoDMSGui";
    case GeoDMSWindowTypes::TreeView:    return "TreeView";
    case GeoDMSWindowTypes::DetailPages: return "DetailPages";
    case GeoDMSWindowTypes::EventLog:    return "EventLog";
    case GeoDMSWindowTypes::MapView:     return "MapView";
    case GeoDMSWindowTypes::TableView:   return "TableView";
    case GeoDMSWindowTypes::Options:     return "Options";
    default:                             return "Unknown";
    }
}

void AutoHideWindowDocknodeTabBar(bool &is_docking_initialized)
{
    if (is_docking_initialized)
        return;

    auto window = ImGui::GetCurrentWindow();
    if (window)
    {
        auto node = window->DockNode;
        if (node && (!node->IsHiddenTabBar())) 
        {
            node->WantHiddenTabBarToggle = true;
            is_docking_initialized = true;
        }
    }
}

bool TryDockViewInGeoDMSDataViewAreaNode(GuiState &state, ImGuiWindow* window)
{
    auto ctx = ImGui::GetCurrentContext();
    ImGuiDockContext* dc = &ctx->DockContext;
    auto dockspace_docknode = (ImGuiDockNode*)dc->Nodes.GetVoidPtr(state.dockspace_id);

    // The following is a specific hardcoded default docking configuration of GeoDMS windows, if the pattern does not match do not dock the View
    // Default starting client area root(Y, 0) >> 7(Y, 1) >> 6(X, 0) >> 3(X, 1) >> target(None)
    if (!dockspace_docknode || dockspace_docknode->SplitAxis != ImGuiAxis_Y || !dockspace_docknode->ChildNodes[0]) // root(Y, 0)
        return false;

    if (!dockspace_docknode->ChildNodes[0]->SplitAxis == ImGuiAxis_Y || !dockspace_docknode->ChildNodes[0]->ChildNodes[1]) // 7(Y, 1)
        return false;

    if (!dockspace_docknode->ChildNodes[0]->ChildNodes[1]->SplitAxis == ImGuiAxis_X || !dockspace_docknode->ChildNodes[0]->ChildNodes[1]->ChildNodes[0]) // 6(X, 0)
        return false;

    if (!dockspace_docknode->ChildNodes[0]->ChildNodes[1]->ChildNodes[0]->SplitAxis == ImGuiAxis_X || !dockspace_docknode->ChildNodes[0]->ChildNodes[1]->ChildNodes[0]->ChildNodes[1]) // 3(X, 1)
        return false;

    // TODO: check if the child node above is not split to avoid crashing

    ImGuiDockNode* target_node = dockspace_docknode->ChildNodes[0]->ChildNodes[1]->ChildNodes[0]->ChildNodes[1];
    ImGui::DockContextQueueDock(ctx, dockspace_docknode->HostWindow, target_node, window, ImGuiDir_None, 0.0f, false);
    return true;
}

std::string StartWindowsFileDialog(std::string start_path, std::wstring file_dialog_text, std::wstring file_dialog_exts)
{
    //std::string last_filename = GetGeoDmsRegKey("LastConfigFile").c_str();
    auto path = std::filesystem::path(start_path);
    std::wstring parent_path = path;
    if (!std::filesystem::is_directory(path))
    {
        parent_path = path.parent_path();
    }
    //auto parent_path = std::filesystem::path(start_path).parent_path();
    COMDLG_FILTERSPEC ComDlgFS[1] = { {file_dialog_text.c_str(), file_dialog_exts.c_str()}}; //L"Configuration files", L"*.dms;*.xml"

    std::string result_file = "";
    std::wstring intermediate_w_string_path;

    HRESULT hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
    if (SUCCEEDED(hr))
    {
        IFileOpenDialog* pFileOpen;
        // Create the FileOpenDialog object.
        hr = CoCreateInstance(CLSID_FileOpenDialog, NULL, CLSCTX_ALL, IID_IFileOpenDialog, reinterpret_cast<void**>(&pFileOpen));
        pFileOpen->SetFileTypes(1, ComDlgFS);

        IShellItem* psiDefault = NULL;
        hr = SHCreateItemFromParsingName(parent_path.c_str(), NULL, IID_PPV_ARGS(&psiDefault));
        pFileOpen->SetDefaultFolder(psiDefault);

        if (SUCCEEDED(hr))
        {
            // Show the Open dialog box.
            hr = pFileOpen->Show((HWND)ImGui::GetCurrentWindow()->Viewport->PlatformHandleRaw);

            // Get the file name from the dialog box.
            if (SUCCEEDED(hr))
            {
                IShellItem* pItem;
                hr = pFileOpen->GetResult(&pItem);
                if (SUCCEEDED(hr))
                {
                    PWSTR pszFilePath;
                    hr = pItem->GetDisplayName(SIGDN_FILESYSPATH, &pszFilePath);

                    // Display the file name to the user.
                    if (SUCCEEDED(hr))
                    {
                        //MessageBoxW(NULL, pszFilePath, L"File Path", MB_OK);
                        //result_file = *pszFilePath;
                        intermediate_w_string_path = std::wstring(pszFilePath);
                        using convert_type = std::codecvt_utf8<wchar_t>;
                        std::wstring_convert<convert_type, wchar_t> converter;

                        //use converter (.to_bytes: wstr->str, .from_bytes: str->wstr)
                        result_file = converter.to_bytes(intermediate_w_string_path);

                        CoTaskMemFree(pszFilePath);
                    }
                    pItem->Release();
                }
            }
            pFileOpen->Release();
        }
        psiDefault->Release();
        CoUninitialize();
    }

    return result_file;
}

static int CALLBACK BrowseCallbackProc(HWND hwnd, UINT uMsg, LPARAM lParam, LPARAM lpData)
{

    if (uMsg == BFFM_INITIALIZED)
    {
        //std::string tmp = (const char*)lpData;
        //std::cout << "path: " << tmp << std::endl;
        SendMessage(hwnd, BFFM_SETSELECTION, TRUE, lpData);
    }

    return 0;
}

std::string BrowseFolder(std::string saved_path)
{
    TCHAR path[MAX_PATH];

    const char* path_param = saved_path.c_str();

    BROWSEINFO bi = { 0 };
    bi.lpszTitle = ("Browse for folder...");
    bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;
    bi.lpfn = BrowseCallbackProc;
    bi.lParam = (LPARAM)path_param;

    LPITEMIDLIST pidl = SHBrowseForFolder(&bi);

    if (pidl != 0)
    {
        //get the name of the folder and put it in path
        SHGetPathFromIDList(pidl, path);

        //free memory used
        IMalloc* imalloc = 0;
        if (SUCCEEDED(SHGetMalloc(&imalloc)))
        {
            imalloc->Free(pidl);
            imalloc->Release();
        }

        return path;
    }

    return "";
}

void OpenUrlInWindowsDefaultBrowser(const std::string url)
{
    // TODO: make implementation OS invariant.
    ShellExecuteA(0, NULL, url.c_str(), NULL, NULL, SW_SHOWNORMAL);
}

void PostEmptyEventToGLFWContext()
{
    glfwPostEmptyEvent();
}

void StringToTable(std::string& input, md_table& result, std::string separator)
{
    result.rows.clear();
    auto lines = DivideTreeItemFullNameIntoTreeItemNames(input, "\n");
    for (auto& line : lines)
    {
        auto colon_separated_line = DivideTreeItemFullNameIntoTreeItemNames(line, separator);
        if (!colon_separated_line.empty())
        {
            result.rows.emplace_back();

            element_type type = element_type::NONE;
            bool         is_failed = false;
            bool         is_selected = false;
            UInt8        indentation = 0;
            std::string	 text = "";
            std::string  link = "";

            for (auto& part : colon_separated_line)
                result.rows.back().elements.back().parts.emplace_back(element_type::TEXT, false, false, 0, part, "");
        }
    }
}

void DrawProperties(GuiState& state, md_table& properties)
{
    /*auto event_queues = GuiEventQueues::getInstance();
    if (ImGui::GetContentRegionAvail().y < 0) // table needs space, crashes otherwise
        return;

    int button_index = 0; //TODO: does assumption of max 2 columns hold?
    ImGui::BeginTable(" ", 2, ImGuiTableFlags_None | ImGuiTableFlags_NoHostExtendX);// ImGuiTableFlags_Resizable ImGuiTableFlags_ScrollX ImGuiTableFlags_NoHostExtendY // ImVec2(0.0f, ImGui::GetTextLineHeightWithSpacing() * 8)
    for (auto& row : properties)
    {
        ImGui::TableNextRow();
        UInt8 column_index = 0;
        for (auto& col : row)
        {
            if (column_index == 2) // hardcoded 2
                break;
            ImGui::TableSetColumnIndex(column_index);
            if (col.background_is_red)
                SetTextBackgroundColor(ImVec2(ImGui::GetScrollMaxX(), ImGui::GetTextLineHeight() + 1.0));// ImGui::GetWindowSize
            if (col.type == PET_HEADING)
            {
                ImGui::Text(col.text.c_str());//ImGui::TextWrapped(col.text.c_str());
            }
            else if (col.type == PET_LINK)
            {
                //ImGui::PushID(button_index++);
                //ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(51.0f / 255.0f, 102.0f / 255.0f, 204.0f / 255.0f, 1.0f));

                ImGui::TextWrapped(col.text.c_str());

                if (ImGui::IsItemClicked())
                {
                    auto jumpItem = TreeItem_GetBestItemAndUnfoundPart(state.GetRoot(), col.text.c_str());
                    if (jumpItem.first && jumpItem.first != state.GetRoot())
                    {
                        state.SetCurrentItem(const_cast<TreeItem*>(jumpItem.first));
                        event_queues->TreeViewEvents.Add(GuiEvents::JumpToCurrentItem);
                        event_queues->CurrentItemBarEvents.Add(GuiEvents::UpdateCurrentItem);
                        event_queues->DetailPagesEvents.Add(GuiEvents::UpdateCurrentItem);
                        event_queues->MainEvents.Add(GuiEvents::UpdateCurrentItem);
                    }
                }
                //ImGui::PopID();
                ImGui::PopStyleColor();
            }
            else if (col.type == PET_TEXT)
            {
                ImGui::TextWrapped(col.text.c_str());
                //ImGui::Text(col.text.c_str());
            }
            else if (col.type == PET_SEPARATOR)
            {
                ImGui::Separator();
                column_index++;
                ImGui::TableSetColumnIndex(column_index);
                ImGui::Separator();
            }
            if (ImGui::IsItemHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
                SetKeyboardFocusToThisHwnd();
            column_index++;
            OnItemClickItemTextTextToClipboard(col.text);
        }
    }
    ImGui::EndTable();*/
}