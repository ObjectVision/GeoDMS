#include <imgui.h>
#include <imgui_internal.h>
#include "ShvDllInterface.h"
#include "TicInterface.h"
#include "ClcInterface.h"
#include "GeoInterface.h"
#include "StxInterface.h"
#include "RtcInterface.h"

#include "dbg/Debug.h"
#include "dbg/DebugLog.h"
#include "dbg/DmsCatch.h"
#include "ptr/AutoDeletePtr.h"
#include "utl/Environment.h"
#include "utl/splitPath.h"
#include "utl/Environment.h"

#include "AbstrDataItem.h"
#include "AbstrDataObject.h"
#include "AbstrUnit.h"
#include "DataLocks.h"
#include "GuiMenu.h"

// for windows open file dialog
#include <windows.h>
#include <shobjidl.h> 
#include <codecvt>

void GuiMenuComponent::Update(GuiView& ViewPtr)
{
    ImGui::SetNextItemOpen(true, 0);
    if (ImGui::BeginMainMenuBar())
    {
        if (ImGui::IsWindowHovered() && ImGui::IsAnyMouseDown())
            SetKeyboardFocusToThisHwnd();

        m_FileComponent.Update();
        m_EditComponent.Update();
        m_ViewComponent.Update();
        m_ToolsComponent.Update();
        m_WindowComponent.Update(ViewPtr);
        m_HelpComponent.Update();

        ImGui::EndMainMenuBar();
    }
}

GuiMenuFileComponent::GuiMenuFileComponent()
{
    m_fileDialog.SetTitle("Open Configuration File");
    m_fileDialog.SetTypeFilters({ ".dms" });
    GetRecentAndPinnedFiles();
}

GuiMenuFileComponent::~GuiMenuFileComponent()
{
    SetRecentAndPinnedFiles();
}

void GuiMenuFileComponent::GetRecentAndPinnedFiles()
{
    m_PinnedFiles = GetGeoDmsRegKeyMultiString("PinnedFiles");
    m_RecentFiles = GetGeoDmsRegKeyMultiString("RecentFiles");
    CleanRecentOrPinnedFiles(m_PinnedFiles);
    CleanRecentOrPinnedFiles(m_RecentFiles);
}

void GuiMenuFileComponent::SetRecentAndPinnedFiles() 
{
    SetGeoDmsRegKeyMultiString("PinnedFiles", m_PinnedFiles);
    SetGeoDmsRegKeyMultiString("RecentFiles", m_RecentFiles);
}

void GuiMenuFileComponent::CleanRecentOrPinnedFiles(std::vector<std::string> &m_Files)
{
    // recent files
    int i = 0;
    auto it_rf = m_Files.begin();
    while (true)
    {
        it_rf = m_Files.begin();
        for (i = 0; i < m_Files.size(); i++)
        {
            if (!std::filesystem::exists(m_Files.at(i)) || m_Files.at(i).empty())
            {
                m_Files.erase(it_rf+i);
                it_rf = m_Files.begin();
                i = 0;
                break;
            }
        }
        if (m_Files.empty() || it_rf+i == m_Files.end())
            break;
    }
    SetRecentAndPinnedFiles();
}

Int32 GuiMenuFileComponent::ConfigIsInRecentOrPinnedFiles(std::string cfg, std::vector<std::string> &m_Files)
{
    auto it = std::find(m_Files.begin(), m_Files.end(), cfg);
    if (it == m_Files.end())
        return -1;
    return it - m_Files.begin();
}

void GuiMenuFileComponent::AddPinnedOrRecentFile(std::string cfg, std::vector<std::string>& m_Files)
{
    m_Files.emplace_back(cfg);
}

void GuiMenuFileComponent::RemovePinnedOrRecentFile(std::string cfg, std::vector<std::string>& m_Files)
{
    auto it_rf = m_Files.begin();
    for (int i = 0; i < m_Files.size(); i++)
    {
        if (m_Files.at(i)==cfg)
        {
            m_Files.erase(it_rf + i);
            return;
        }
    }
}

void GuiMenuFileComponent::UpdateRecentOrPinnedFilesByCurrentConfiguration(std::vector<std::string>& m_Files)
{
    auto ind = ConfigIsInRecentOrPinnedFiles(m_State.configFilenameManager._Get(), m_Files);
    auto it = m_Files.begin();
    if (ind != -1) // in recent files
        m_Files.erase(it+ind);

    it = m_Files.begin(); // renew iterator
    m_Files.insert(it, m_State.configFilenameManager._Get());
    SetRecentAndPinnedFiles();
}

std::string StartWindowsFileDialog()
{
    std::string last_filename = GetGeoDmsRegKey("LastConfigFile").c_str();
    auto parent_path = std::filesystem::path(last_filename).parent_path();
    COMDLG_FILTERSPEC ComDlgFS[1] = {{L"Configuration files", L"*.dms;*.xml"}};

    std::string result_file = "";
    std::wstring test_file;

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
                        test_file = std::wstring(pszFilePath);
                        using convert_type = std::codecvt_utf8<wchar_t>;
                        std::wstring_convert<convert_type, wchar_t> converter;

                        //use converter (.to_bytes: wstr->str, .from_bytes: str->wstr)
                        result_file = converter.to_bytes(test_file);
                        
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

void GuiMenuFileComponent::Update()
{
    /*m_fileDialog.Display();

    if (m_fileDialog.HasSelected())
    {
        std::string new_filename = m_fileDialog.GetSelected().string();
        m_State.configFilenameManager.Set(new_filename);
        SetGeoDmsRegKeyString("LastConfigFile", new_filename);

        UpdateRecentOrPinnedFilesByCurrentConfiguration(m_RecentFiles);
        CleanRecentOrPinnedFiles(m_RecentFiles);
        m_fileDialog.ClearSelected();
    }*/

    if (ImGui::BeginMenu("File"))
    {
        //if (ImGui::MenuItem("Open Configuration File", "Ctrl+O")) 
            //m_fileDialog.Open();

        if (ImGui::MenuItem("Open Configuration File", "Ctrl+O"))
        {
            auto file_name = StartWindowsFileDialog();
            if (!file_name.empty())
            {
                m_State.configFilenameManager.Set(file_name);
                SetGeoDmsRegKeyString("LastConfigFile", file_name);
                UpdateRecentOrPinnedFilesByCurrentConfiguration(m_RecentFiles);
                CleanRecentOrPinnedFiles(m_RecentFiles);
            }
        }

        if (ImGui::MenuItem("Reopen Current Configuration", "Alt+R")) 
        {
            m_State.MainEvents.Add(ReopenCurrentConfiguration);
        }
        if (ImGui::MenuItem("Open Demo Config")) 
        {
            m_State.configFilenameManager.Set("C:\\prj\\tst\\Storage_gdal\\cfg\\regression.dms");
            UpdateRecentOrPinnedFilesByCurrentConfiguration(m_RecentFiles);
            CleanRecentOrPinnedFiles(m_RecentFiles);
        }
        ImGui::Separator();

        int ind = 1000;
        for (auto pfn = m_PinnedFiles.begin(); pfn<m_PinnedFiles.end(); pfn++)
        {
            ImGui::PushID(ind);// TODO: make sure id is unique for this window
            if (ImGui::Button(ICON_RI_CLOSE_FILL))
            {
                RemovePinnedOrRecentFile(*pfn, m_PinnedFiles);
                UpdateRecentOrPinnedFilesByCurrentConfiguration(m_PinnedFiles);
                CleanRecentOrPinnedFiles(m_PinnedFiles);
                pfn = m_PinnedFiles.begin();
                if (pfn == m_PinnedFiles.end())
                {
                    ImGui::PopID();
                    break;
                }
            }
            ImGui::PopID();
            ImGui::SameLine();

            if (ImGui::MenuItem((*pfn).c_str()))
            {
                m_State.configFilenameManager.Set(*pfn);
                SetGeoDmsRegKeyString("LastConfigFile", *pfn);
                UpdateRecentOrPinnedFilesByCurrentConfiguration(m_PinnedFiles);
                CleanRecentOrPinnedFiles(m_PinnedFiles);
                pfn = m_PinnedFiles.begin();
                if (pfn == m_PinnedFiles.end())
                    break;
            }
            ind++;
        }

        ImGui::Separator();

        ind = 1;
        for (auto rfn = m_RecentFiles.begin(); rfn < m_RecentFiles.end(); rfn++)
        {
            ImGui::PushID(ind); // TODO: check if id is unique
            if (ImGui::Button(ICON_RI_CLOSE_FILL))
            {
                RemovePinnedOrRecentFile(*rfn, m_RecentFiles);
                UpdateRecentOrPinnedFilesByCurrentConfiguration(m_RecentFiles);
                CleanRecentOrPinnedFiles(m_RecentFiles);
                rfn = m_RecentFiles.begin();
                if (rfn == m_RecentFiles.end())
                {
                    ImGui::PopID();
                    break;
                }

            }
            ImGui::PopID();

            ImGui::SameLine();
            if (ImGui::MenuItem((std::to_string(ind) + " " + *rfn).c_str()))
            {
                m_State.configFilenameManager.Set(*rfn);
                SetGeoDmsRegKeyString("LastConfigFile", *rfn);
                UpdateRecentOrPinnedFilesByCurrentConfiguration(m_RecentFiles);
                CleanRecentOrPinnedFiles(m_RecentFiles);
                rfn = m_RecentFiles.begin();
                if (rfn == m_RecentFiles.end())
                    break;
            }

            ImGui::SameLine();
            ImGui::PushID(ind + 4000); // TODO: check if id is unique
            if (ImGui::Button(ICON_RI_PINN_ON))
            {
                if (ConfigIsInRecentOrPinnedFiles(*rfn, m_PinnedFiles) < 0)
                    AddPinnedOrRecentFile(*rfn, m_PinnedFiles);
                UpdateRecentOrPinnedFilesByCurrentConfiguration(m_PinnedFiles);
                CleanRecentOrPinnedFiles(m_PinnedFiles);
            }
            ImGui::PopID();

            ind++;
        }

        ImGui::Separator();
        if (ImGui::MenuItem("Exit")) {}
        ImGui::EndMenu();
    }
}

void GuiMenuEditComponent::Update()
{
    if (ImGui::BeginMenu("Edit"))
    {
        if (ImGui::MenuItem("Dear ImGui Demo Window"))
            m_State.ShowDemoWindow = true;
        if (ImGui::MenuItem("Config Source", "Ctrl+E")) {}
        if (ImGui::MenuItem("Definition", "Ctrl+Alt+E")) {}
        if (ImGui::MenuItem("Classification and Palette", "Ctrl+Alt+C")) {}
        ImGui::Separator();
        if (ImGui::MenuItem("Update Treeitem", "Ctrl+U")) {}
        if (ImGui::MenuItem("Update Subtree", "Ctrl+T")) {}
        if (ImGui::MenuItem("Invalidate Treeitem", "Ctrl+I")) {}
        ImGui::EndMenu();
    }
}

void GuiMenuViewComponent::Update()
{
    if (ImGui::BeginMenu("View"))
    {
        ImGui::PushItemFlag(ImGuiItemFlags_SelectableDontClosePopup, true);
        if (ImGui::MenuItem("Default")) {}
        if (ImGui::MenuItem("Map", "Ctrl+M")) {}
        if (ImGui::MenuItem("Table", "Ctrl+D")) 
        {
            m_State.ShowTableviewWindow = true;
        }
        if (ImGui::MenuItem("Histogram", "Ctrl+H")) {}
        ImGui::Separator();
        if (ImGui::BeginMenu("Process Schemes"))
        {
            ImGui::MenuItem("Subitems");
            ImGui::MenuItem("Suppliers");
            ImGui::MenuItem("Expression");
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Source Descr variant"))
        {
            ImGui::EndMenu();
        }

        ImGui::Separator();
        if (ImGui::MenuItem("Treeview", "Alt+0")) 
        {
            m_State.ShowTreeviewWindow = !m_State.ShowTreeviewWindow;
        }
        if (ImGui::MenuItem("Detail Pages", "Alt+1")) 
        {
            m_State.ShowDetailPagesWindow = !m_State.ShowDetailPagesWindow;
        }
        if (ImGui::MenuItem("Eventlog", "Alt+2")) 
        {
            m_State.ShowEventLogWindow = !m_State.ShowEventLogWindow;
        }
        if (ImGui::MenuItem("Toolbar", "Alt+3")) 
        {
            m_State.ShowToolbar = !m_State.ShowToolbar;
        }
        if (ImGui::MenuItem("Current Item bar", "Alt+4")) 
        {
            m_State.ShowCurrentItemBar = !m_State.ShowCurrentItemBar;
        }
        if (ImGui::MenuItem("Hidden Items", "Alt+5")) {}
        ImGui::PopItemFlag();
        ImGui::EndMenu();
    }
}

void GuiMenuToolsComponent::Update()
{
    if (ImGui::BeginMenu("Tools"))
    {
        if (ImGui::MenuItem("Options", "Ctrl+Alt+O")) 
        {
            m_State.ShowOptionsWindow = true;
        }
        if (ImGui::MenuItem("Debug Report", "Ctrl+Alt+T")) {}
        ImGui::EndMenu();
    }
}

void GuiMenuWindowComponent::Update(GuiView& ViewPtr)
{
    if (ImGui::BeginMenu("Window"))
    {
        if (ImGui::MenuItem("Tile Horizontal", "Ctrl+Alt+W")) {}
        if (ImGui::MenuItem("Tile Vertical", "Ctrl+Alt+V")) {}
        if (ImGui::MenuItem("Cascade", "Shift+Ctrl+W")) {}
        if (ImGui::MenuItem("Close", "Ctrl+W")) {}
        if (ImGui::MenuItem("Close All", "Ctrl+L")) {}
        if (ImGui::MenuItem("Close All But This", "Ctrl+B")) {}
        ImGui::Separator();

        int index = 0;
        for (auto& view : ViewPtr.m_Views)
        {
            ImGui::PushID(index);
            if (ImGui::Button(view.m_Name.c_str()))
            {
                ViewPtr.SetDoView(true);
                ViewPtr.SetViewIndex(index);
                if (view.m_ViewStyle==tvsMapView)
                    m_State.MapViewEvents.Add(OpenInMemoryDataView);
                else if (view.m_ViewStyle==tvsTableView)
                    m_State.TableViewEvents.Add(OpenInMemoryDataView);
            }
            ImGui::PopID();
            index++;
        }


        // TODO: add ViewPtr available views
        /*for (auto& view : TableViewsPtr)
        {
            if (!view.IsPopulated())
                continue;

            if (ImGui::Button(view.GetViewName().c_str()))
            {
                view.SetDoView(!view.DoView());
            }
            
            ImGui::SameLine();
            ImGui::PushID(view.GetViewName().c_str()); // unique id
            //ImGui::PushStyleColor();
            if (ImGui::Button(ICON_RI_CLOSE_FILL))
            {
                view.Close(false);
            }
            ImGui::PopID();
        }

        for (auto& view : MapViewsPtr)
        {
            if (!view.IsPopulated())
                continue;

            if (ImGui::Button(view.GetViewName().c_str()))
            {
                view.SetDoView(!view.DoView());
            }

            ImGui::SameLine();
            ImGui::PushID(view.GetViewName().c_str()); // unique id
            if (ImGui::Button(ICON_RI_CLOSE_FILL))
            {
                view.Close(false);
            }
            ImGui::PopID();
        }*/

        ImGui::Separator();
        ImGui::EndMenu();
    }
}

void GuiMenuHelpComponent::Update()
{
    if (ImGui::BeginMenu("Help"))
    {
        if (ImGui::MenuItem("About...")) {}
        if (ImGui::MenuItem("Online")) {}
        ImGui::Separator();
        if (ImGui::MenuItem("Copyright Notice")) {}
        if (ImGui::MenuItem("Disclaimer")) {}
        if (ImGui::MenuItem("Data Source Reference")) {}
        ImGui::EndMenu();
    }
}