#include <imgui.h>
#include <stdio.h>
#include <algorithm>

//#include <stdlib.h>         // abort

#define STB_IMAGE_IMPLEMENTATION
//#include <stb_image.h>
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include <GL/glew.h>

#if defined(IMGUI_IMPL_OPENGL_ES2)
#include <GLES2/gl2.h>
#endif
#include <GLFW/glfw3.h> // Will drag system OpenGL headers

#include "ShvDllInterface.h"
#include "TicInterface.h"
#include "ClcInterface.h"
#include "GeoInterface.h"
#include "StxInterface.h"
#include "RtcInterface.h"
#include "ShvUtils.h"
#include "AbstrUnit.h" // IsUnit
#include "SessionData.h"

#include "GuiMain.h"
#include "GuiStyles.h"
#include "GuiGraphics.h"

GuiMainComponent::GuiMainComponent()
{
    //m_MapViews.reserve(m_MaxViews);
    //m_TableViews.reserve(m_MaxViews);

    auto flags = GetRegStatusFlags();
    DMS_SetGlobalCppExceptionTranslator(&m_EventLog.GeoDMSExceptionMessage);
    DMS_RegisterMsgCallback(&m_EventLog.GeoDMSMessage, nullptr);
}

GuiMainComponent::~GuiMainComponent()
{
    //for (auto& view : m_TableViews)
    //    view.Close(false);

    //for (auto& view : m_MapViews)
    //    view.Close(false);

    //m_TableViews.clear();
    //m_MapViews.clear();

    for (auto& view : m_View.m_Views)
    {
        SHV_DataView_Destroy(view.m_DataView);
        view.m_ActiveItems.clear();
    }

    m_ItemsHolder.clear();
    m_State.clear();

    DMS_ReleaseMsgCallback(&m_EventLog.GeoDMSMessage, nullptr);
}

int GetFreeViewIndex(std::vector<GuiView>& views)
{
    int ind = 0;
    for (auto& view : views)
    {
        if (!view.IsPopulated())
            return ind;
        ind++;
    }
    return ind;
}

void GuiMainComponent::ProcessEvent(GuiEvents e)
{
    switch (e)
    {
    case UpdateCurrentItem:
    {
        if (!m_State.GetCurrentItem())
            return;
        //if (!m_ItemsHolder.contains(m_State.GetCurrentItem()))
        //    m_ItemsHolder.add(m_State.GetCurrentItem());
        DMS_TreeItem_Update(m_State.GetCurrentItem());
        break;
    }
    case UpdateCurrentAndCompatibleSubItems:
    {
        if (!m_State.GetCurrentItem())
            return;

        if (!m_ItemsHolder.contains(m_State.GetCurrentItem()))
        {
            //m_ItemsHolder.add(m_State.GetCurrentItem());
            //if (IsDataItem(m_State.GetCurrentItem()))
            //{
            //    m_ItemsHolder.add(m_State.GetCurrentItem());
            //}

        }
        //DMS_TreeItem_Update(m_CurrentItem);

        if (IsUnit(m_State.GetCurrentItem()))
        {
            auto au = AsUnit(m_State.GetCurrentItem());
            auto subItem = m_State.GetCurrentItem()->_GetFirstSubItem();
            while (subItem)
            {
                if (IsDataItem(subItem))
                {
                    auto adi = AsDataItem(subItem);
                    if (adi->GetAbstrDomainUnit()->UnifyDomain(au, UnifyMode::UM_Throw))
                    {
                        if (!m_ItemsHolder.contains(subItem))
                        {
                            //m_ItemsHolder.add(subItem);
                            //if (IsDataItem(m_State.GetCurrentItem()))
                            //{
                            //    m_ItemsHolder.add(m_State.GetCurrentItem());
                            //}
                        }
                        //DMS_TreeItem_Update(subItem);
                    }
                }
                subItem = subItem->GetNextItem();
            }
        }
        break;
    }
    case ReopenCurrentConfiguration:
    {
        CloseCurrentConfig();
        if (!m_State.configFilenameManager.Get().empty())
        {
            //m_Root = DMS_CreateTreeFromConfiguration(m_State.configFilenameManager.Get().c_str());
            m_State.SetRoot(DMS_CreateTreeFromConfiguration(m_State.configFilenameManager.Get().c_str()));
        }

        break;
    }
    case OpenNewMapViewWindow:
    {
        /*auto freeViewInd = GetFreeViewIndex(m_MapViews);
        if (freeViewInd < m_MapViews.size())
            m_MapViews.at(freeViewInd).ResetView(tvsMapView, "MapView#" + std::to_string(freeViewInd + 1));
        else if (m_MapViewsCursor >= m_MaxViews)
        {
            reportF(SeverityTypeID::ST_Warning, "Maximum number of Mapview windows %d reached.", m_MaxViews);
            break;
        }
        else
        {
            m_MapViews.emplace_back(std::move(GuiView()));
            m_MapViews.back().SetViewName("MapView#" + std::to_string(m_MapViews.size()));
            m_MapViews.back().SetViewStyle(tvsMapView);
            m_MapViewsCursor++;
            break;
        }*/
        m_View.SetViewStyle(tvsMapView);
        m_View.SetViewName("View");
        m_View.InitDataView(m_State.GetCurrentItem());
        break;
    }
    case OpenNewTableViewWindow:
    {
        /*auto freeViewInd = GetFreeViewIndex(m_TableViews);
        if (freeViewInd < m_TableViews.size())
            m_TableViews.at(freeViewInd).ResetView(tvsTableView, "TableView#" + std::to_string(freeViewInd + 1));
        else if (m_TableViewsCursor >= m_MaxViews)
        {
            reportF(SeverityTypeID::ST_Warning, "Maximum number of TableView windows %d reached.", m_MaxViews);
            break;
        }
        else
        {
            m_TableViews.emplace_back();
            m_TableViews.back().SetViewName("TableView#" + std::to_string(m_TableViews.size()));
            m_TableViews.back().SetViewStyle(tvsTableView);
            m_TableViewsCursor++;
            break;
        }*/
        m_View.SetViewStyle(tvsTableView);
        m_View.SetViewName("View");
        m_View.InitDataView(m_State.GetCurrentItem());
        break;
    }
    }
}

void GuiMainComponent::CloseCurrentConfig()
{
    m_View.Close(false);

    /*
    // close the views
    for (auto& view : m_MapViews)
    {
        view.Close(false);
    }

    for (auto& view : m_TableViews)
    {
        view.Close(false);
    }*/

    m_ItemsHolder.clear();
    m_State.clear();
}

int GuiMainComponent::Init()
{
    // set exe dir
    std::string exePath = GetExeFilePath();
    DMS_Appl_SetExeDir(exePath.c_str()); // sets MainThread id

    // setup GLFW window
    SetErrorCallBackForGLFW();

    if (!InitGLFW())
        return 1;

    auto glsl_version = SetGLFWWindowHints();

    m_Window = glfwCreateWindow(1280, 720, "", NULL, NULL);
    if (m_Window == NULL)
        return 1;
    glfwMakeContextCurrent(m_Window);
    glfwSwapInterval(1); // Enable vsync

    // setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();

    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;       // Enable Keyboard Controls
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;           // Enable Docking
    io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;         // Enable Multi-Viewport / Platform Windows

    // setup Dear ImGui style
    ImGui::StyleColorsLight();
    //ImGui::StyleColorsDark();
    //ImGui::StyleColorsClassic();

    // when viewports are enabled we tweak WindowRounding/WindowBg so platform windows can look identical to regular ones.
    ImGuiStyle& style = ImGui::GetStyle();
    if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
    {
        style.WindowRounding = 0.0f;
        style.Colors[ImGuiCol_WindowBg].w = 1.0f;
    }

    // Setup Platform/Renderer backends
    ImGui_ImplGlfw_InitForOpenGL(m_Window, true);
    ImGui_ImplOpenGL3_Init(glsl_version.c_str());
    if (not glewInit() == GLEW_OK) // OpenGL extentions entry point
        throwErrorF("GLEW", "unsuccessful initialization of openGL helper library glew.");

    SetDmsWindowIcon(m_Window);
    SetGuiFont("misc/fonts/DroidSans.ttf");

    return 0;
}

int GuiMainComponent::MainLoop()
{
    ImGuiIO& io = ImGui::GetIO();

    // state
    ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);
    glfwSetWindowTitle(m_Window, (m_State.configFilenameManager.Get() + DMS_GetVersion()).c_str()); // default window title
    glfwSetKeyCallback(m_Window, &m_Input.ProcessKeyEvent);

    InitializeGuiTextures();
    SHV_SetAdminMode(true); // needed for ViewStyleFlags lookup

    const UInt32 frames_to_update = 5;
    UInt32 UpdateFrameCounter = frames_to_update;

    // Main loop
    while (!glfwWindowShouldClose(m_Window)) // memory leaks: 6 up to this point
    {      
        if (m_State.configFilenameManager.HasNew())
        {
            CloseCurrentConfig();
            glfwSetWindowTitle(m_Window, (m_State.configFilenameManager.Get() + std::string(" - ") + DMS_GetVersion()).c_str());
            m_State.SetRoot(DMS_CreateTreeFromConfiguration(m_State.configFilenameManager.Get().c_str()));
            //DMS_RegisterMsgCallback(&m_EventLog.GeoDMSMessage, nullptr);

            DMS_TreeItem_RegisterStateChangeNotification(&OnTreeItemChanged, m_State.GetRoot(), nullptr);
            m_State.GetRoot()->UpdateMetaInfo();
        }

        if (--UpdateFrameCounter) // when waking up from an event, update n frames
            glfwPollEvents();
        else
        {
            glfwWaitEventsTimeout(1.0);
            if (UpdateFrameCounter == 0)
                UpdateFrameCounter = frames_to_update;
        }

        // Start the Dear ImGui frame
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame(); // TODO: set  to true for UpdateInputEvents?

        // update all gui components
        Update(); 

        // rendering
        ImGui::Render();
        int display_w, display_h;
        glfwGetFramebufferSize(m_Window, &display_w, &display_h);
        glViewport(0, 0, display_w, display_h);
        glClearColor(clear_color.x * clear_color.w, clear_color.y * clear_color.w, clear_color.z * clear_color.w, clear_color.w);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        // Update and Render additional Platform Windows
        // (Platform functions may change the current OpenGL context, so we save/restore it to make it easier to paste this code elsewhere.
        //  For this specific demo app we could also call glfwMakeContextCurrent(window) directly)
        if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
        {
            GLFWwindow* backup_current_context = glfwGetCurrentContext();
            ImGui::UpdatePlatformWindows();
            ImGui::RenderPlatformWindowsDefault();
            glfwMakeContextCurrent(backup_current_context);
        }

        glfwSwapBuffers(m_Window);
    }

    // Cleanup
    ImGui_ImplOpenGL3_Shutdown();
    //ImGui_ImplOpenGL2_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glfwDestroyWindow(m_Window);
    glfwTerminate();

    return true;
}

void GuiMainComponent::Update()
{
    SuspendTrigger::DoSuspend();

    static bool opt_fullscreen = true;
    static bool opt_padding = false;
    static ImGuiDockNodeFlags dockspace_flags = ImGuiDockNodeFlags_None;
    // We are using the ImGuiWindowFlags_NoDocking flag to make the parent window not dockable into,
    // because it would be confusing to have two docking targets within each others.
    ImGuiWindowFlags window_flags = ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoDocking;
    if (opt_fullscreen)
    {
        const ImGuiViewport* viewport = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(viewport->WorkPos);
        ImGui::SetNextWindowSize(viewport->WorkSize);
        ImGui::SetNextWindowViewport(viewport->ID);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
        window_flags |= ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove;
        window_flags |= ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;
    }
    else
    {
        dockspace_flags &= ~ImGuiDockNodeFlags_PassthruCentralNode;
    }

    // When using ImGuiDockNodeFlags_PassthruCentralNode, DockSpace() will render our background
    // and handle the pass-thru hole, so we ask Begin() to not render a background.
    if (dockspace_flags & ImGuiDockNodeFlags_PassthruCentralNode)
        window_flags |= ImGuiWindowFlags_NoBackground;

    // Important: note that we proceed even if Begin() returns false (aka window is collapsed).
    // This is because we want to keep our DockSpace() active. If a DockSpace() is inactive,
    // all active windows docked into it will lose their parent and become undocked.
    // We cannot preserve the docking relationship between an active window and an inactive docking, otherwise
    // any change of dockspace/settings would lead to windows being stuck in limbo and never being visible.
    if (!opt_padding)
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    
    
    if (!ImGui::Begin("GeoDMSGui", nullptr, window_flags))
    {
        ImGui::End();
        return;
    }

    if (!opt_padding)
        ImGui::PopStyleVar();

    if (opt_fullscreen)
        ImGui::PopStyleVar(2);

    // Submit the DockSpace
    ImGuiIO& io = ImGui::GetIO();
    if (io.ConfigFlags & ImGuiConfigFlags_DockingEnable)
    {
        ImGuiID dockspace_id = ImGui::GetID("MyDockSpace");
        ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), dockspace_flags);
    }

    while (m_State.MainEvents.HasEvents()) // Handle MainEvents
    {
        auto e = m_State.MainEvents.Pop();
        ProcessEvent(e);

        if (e == ReopenCurrentConfiguration)
        {
            ImGui::End();
            return;
        }
    }

    // Update all GeoDMSGui components
    m_MenuComponent.Update(m_View);

    if (m_State.ShowCurrentItemBar)
        m_CurrentItemComponent.Update();
    
    if (m_State.ShowToolbar)
        m_Toolbar.Update(&m_State.ShowToolbar, m_View);

    if (m_State.ShowDetailPagesWindow)
        m_DetailPages.Update(&m_State.ShowDetailPagesWindow);

    if (m_State.ShowTreeviewWindow)
        m_TreeviewComponent.Update(&m_State.ShowTreeviewWindow);

    if (m_State.ShowEventLogWindow)
        m_EventLog.Update(&m_State.ShowEventLogWindow);

    if (m_View.IsPopulated())
    {
        if (m_View.DoView())
            m_View.Update();
        else
            m_View.Close(true);
    }

    /*for (auto& view : m_MapViews)
    {
        if (!view.IsPopulated())
            continue;

        if (view.DoView())
            view.Update();
        else
            view.Close(true);
    }

    for (auto& view : m_TableViews)
    {
        if (!view.IsPopulated())
            continue;

        if (view.DoView())
            view.Update();
        else
            view.Close(true);
    }*/

    if (m_State.ShowDemoWindow)
        ImGui::ShowDemoWindow(&m_State.ShowDemoWindow);

    // option windows
    if (m_State.ShowOptionsWindow)
        m_Options.Update(&m_State.ShowOptionsWindow);

    if (m_State.ShowEventLogOptionsWindow)
        ShowEventLogOptionsWindow(&m_State.ShowEventLogOptionsWindow);
    
    ImGui::End();

    SuspendTrigger::Resume();
}
