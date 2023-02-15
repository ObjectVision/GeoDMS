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
#include "TreeItemProps.h"

#include "AbstrDataItem.h"
#include "AbstrDataObject.h"
#include "AbstrUnit.h"
#include "DataLocks.h"
#include "GuiMenu.h"

void GuiMenu::Update(GuiState& state, GuiView& ViewPtr)
{
    if (ImGui::BeginMainMenuBar())
    {
        if (ImGui::IsWindowHovered() && ImGui::IsAnyMouseDown())
            SetKeyboardFocusToThisHwnd();

        m_File.Update(state);
        m_Edit.Update(state);
        m_View.Update(state);
        m_Tools.Update(state);
        m_Window.Update(ViewPtr);
        m_Help.Update(state);

        ImGui::EndMainMenuBar();
    }
}

GuiMenuFile::GuiMenuFile()
{
    m_fileDialog.SetTitle("Open Configuration File");
    m_fileDialog.SetTypeFilters({ ".dms" });
    GetRecentAndPinnedFiles();
}

GuiMenuFile::~GuiMenuFile()
{
    SetRecentAndPinnedFiles();
}

void GuiMenuFile::GetRecentAndPinnedFiles()
{
    m_PinnedFiles = GetGeoDmsRegKeyMultiString("PinnedFiles");
    m_RecentFiles = GetGeoDmsRegKeyMultiString("RecentFiles");
    CleanRecentOrPinnedFiles(m_PinnedFiles);
    CleanRecentOrPinnedFiles(m_RecentFiles);
}

void GuiMenuFile::SetRecentAndPinnedFiles() 
{
    SetGeoDmsRegKeyMultiString("PinnedFiles", m_PinnedFiles);
    SetGeoDmsRegKeyMultiString("RecentFiles", m_RecentFiles);
}

std::vector<std::string>::iterator GuiMenuFile::CleanRecentOrPinnedFiles(std::vector<std::string> &m_Files)
{
    // recent files
    auto it_rf = m_Files.begin();
    while (it_rf != m_Files.end())
    {
        if (!std::filesystem::exists(*it_rf) || it_rf->empty())
        {
            it_rf = m_Files.erase(it_rf);
            break;
        }
        it_rf++;
    }
    SetRecentAndPinnedFiles(); // Update registry
    return it_rf;
}

Int32 GuiMenuFile::ConfigIsInRecentOrPinnedFiles(std::string cfg, std::vector<std::string> &m_Files)
{
    auto it = std::find(m_Files.begin(), m_Files.end(), cfg);
    if (it == m_Files.end())
        return -1;
    return it - m_Files.begin();
}

void GuiMenuFile::AddPinnedOrRecentFile(std::string cfg, std::vector<std::string>& m_Files)
{
    m_Files.emplace_back(cfg);
}

void GuiMenuFile::RemovePinnedOrRecentFile(std::string cfg, std::vector<std::string>& m_Files)
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

void GuiMenuFile::UpdateRecentOrPinnedFilesByCurrentConfiguration(GuiState& state, std::vector<std::string>& m_Files)
{
    auto ind = ConfigIsInRecentOrPinnedFiles(state.configFilenameManager._Get(), m_Files);
    auto it = m_Files.begin();
    if (ind != -1) // in recent files
        m_Files.erase(it+ind);

    it = m_Files.begin(); // renew iterator
    m_Files.insert(it, state.configFilenameManager._Get());
    SetRecentAndPinnedFiles();
}



void GuiMenuFile::Update(GuiState& state)
{
    auto event_queues = GuiEventQueues::getInstance();
    static bool expand_file_menu = true;
    if (ImGui::BeginMenuEx("File", "", true))
    {
        if (ImGui::MenuItem("Open Configuration File", "Ctrl+O"))
        {
            auto file_name = StartWindowsFileDialog();
            if (!file_name.empty())
            {
                state.configFilenameManager.Set(file_name);
                SetGeoDmsRegKeyString("LastConfigFile", file_name);
                UpdateRecentOrPinnedFilesByCurrentConfiguration(state, m_RecentFiles);
                CleanRecentOrPinnedFiles(m_RecentFiles);
            }
        }
        

        if (ImGui::MenuItem("Reopen Current Configuration", "Alt+R")) 
            event_queues->MainEvents.Add(GuiEvents::ReopenCurrentConfiguration);

        if (ImGui::MenuItem("Open Demo Config")) 
        {
            state.configFilenameManager.Set("C:\\prj\\tst\\Storage_gdal\\cfg\\regression.dms");
            UpdateRecentOrPinnedFilesByCurrentConfiguration(state, m_RecentFiles);
            CleanRecentOrPinnedFiles(m_RecentFiles);
        }
        /*ImGui::Separator();

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
        }*/

        ImGui::Separator();
        int ind = 1;
        for (auto rfn = m_RecentFiles.begin(); rfn < m_RecentFiles.end(); rfn++)
        {
            ImGui::PushID(ind);
            if (ImGui::Button(ICON_RI_CLOSE_FILL))
            {
                rfn = m_RecentFiles.erase(rfn); //RemovePinnedOrRecentFile(*rfn, m_RecentFiles); //TODO: use iterator
                //UpdateRecentOrPinnedFilesByCurrentConfiguration(m_RecentFiles);
                auto ret_it = CleanRecentOrPinnedFiles(m_RecentFiles);
                if (ret_it != m_RecentFiles.end())
                    ret_it = rfn;

                if (m_RecentFiles.empty() || rfn == m_RecentFiles.end())
                {
                    ImGui::PopID();
                    break;
                }

            }
            ImGui::PopID();

            ImGui::SameLine();
            if (ImGui::MenuItem((std::to_string(ind) + " " + *rfn).c_str()))
            {
                state.configFilenameManager.Set(*rfn);
                SetGeoDmsRegKeyString("LastConfigFile", *rfn);
                UpdateRecentOrPinnedFilesByCurrentConfiguration(state, m_RecentFiles);
                CleanRecentOrPinnedFiles(m_RecentFiles);
                rfn = m_RecentFiles.begin();
                if (rfn == m_RecentFiles.end())
                    break;
            }

            /*ImGui::SameLine();
            ImGui::PushID(ind + 4000); // TODO: check if id is unique
            if (ImGui::Button(ICON_RI_PINN_ON))
            {
                if (ConfigIsInRecentOrPinnedFiles(*rfn, m_PinnedFiles) < 0)
                    AddPinnedOrRecentFile(*rfn, m_PinnedFiles);
                UpdateRecentOrPinnedFilesByCurrentConfiguration(m_PinnedFiles);
                CleanRecentOrPinnedFiles(m_PinnedFiles);
            }
            ImGui::PopID();*/

            ind++;
        }

        ImGui::Separator();
        if (ImGui::MenuItem("Exit")) {}
        ImGui::EndMenu();
    }
}

void GuiMenuEdit::Update(GuiState& state)
{
    if (ImGui::BeginMenu("Edit"))
    {
        if (ImGui::MenuItem("Dear ImGui Demo Window"))
            state.ShowDemoWindow = true;
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

void GuiMenuView::Update(GuiState& state)
{
    auto event_queues = GuiEventQueues::getInstance();
    if (ImGui::BeginMenu("View"))
    {
        ImGui::PushItemFlag(ImGuiItemFlags_SelectableDontClosePopup, true);
        if (ImGui::MenuItem("Default")) {}
        if (ImGui::MenuItem("Map", "Ctrl+M")) {}
        if (ImGui::MenuItem("Table", "Ctrl+D")) 
        {
            //m_State.ShowTableviewWindow = true;
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
            if (ImGui::MenuItem("Configured Source Descriptions"))
                state.SourceDescrMode = SourceDescrMode::Configured;
            if (ImGui::MenuItem("ReadOnly Storage Managers"))
                state.SourceDescrMode = SourceDescrMode::ReadOnly;
            if (ImGui::MenuItem("Non ReadOnly Storage Managers"))
                state.SourceDescrMode = SourceDescrMode::WriteOnly;
            if (ImGui::MenuItem("All Storage Managers"))
                state.SourceDescrMode = SourceDescrMode::All;

            ImGui::EndMenu();
        }

        ImGui::Separator();
        ImGui::Checkbox("##toggle_treeview", &state.ShowTreeviewWindow);
        ImGui::SameLine();
        if (ImGui::MenuItem("Treeview", "Alt+0")) 
            state.ShowTreeviewWindow = !state.ShowTreeviewWindow;

        ImGui::Checkbox("##toggle_detail_pages", &state.ShowDetailPagesWindow);
        ImGui::SameLine();
        if (ImGui::MenuItem("Detail Pages", "Alt+1")) 
            state.ShowDetailPagesWindow = !state.ShowDetailPagesWindow;

        ImGui::Checkbox("##toggle_eventlog", &state.ShowEventLogWindow);
        ImGui::SameLine();
        if (ImGui::MenuItem("Eventlog", "Alt+2")) 
            state.ShowEventLogWindow = !state.ShowEventLogWindow;

        ImGui::Checkbox("##toggle_toolbar", &state.ShowToolbar);
        ImGui::SameLine();
        if (ImGui::MenuItem("Toolbar", "Alt+3")) 
            state.ShowToolbar = !state.ShowToolbar;

        ImGui::Checkbox("##toggle_current_item_bar", &state.ShowCurrentItemBar);
        ImGui::SameLine();
        if (ImGui::MenuItem("Current Item bar", "Alt+4")) 
            state.ShowCurrentItemBar = !state.ShowCurrentItemBar;

        static bool hidden_items = false;
        //static bool status_bar = false;
        ImGui::Checkbox("##toggle_hidden_items", &hidden_items);
        ImGui::SameLine();
        if (ImGui::MenuItem("Hidden Items", "Alt+5")) {} // TODO: implement

        ImGui::Checkbox("##toggle_status_bar", &state.ShowStatusBar);
        ImGui::SameLine();
        if (ImGui::MenuItem("Status Bar", "Alt+6"))
            state.ShowStatusBar = !state.ShowStatusBar;

        ImGui::PopItemFlag();
        ImGui::EndMenu();
    }
}

void GuiMenuTools::Update(GuiState& state)
{
    if (ImGui::BeginMenu("Tools"))
    {
        if (ImGui::MenuItem("Options", "Ctrl+Alt+O")) 
        {
            state.ShowOptionsWindow = true;
        }
        if (ImGui::MenuItem("Debug Report", "Ctrl+Alt+T")) {}
        ImGui::EndMenu();
    }
}

void GuiMenuWindow::Update(GuiView& ViewPtr)
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

        /*int index = 0;
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
        }*/


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

void GuiMenuHelp::Update(GuiState& state)
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