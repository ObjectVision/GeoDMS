#include <imgui.h>
#include <stdio.h>
#include <algorithm>

#include "cpc/CompChar.h"
#include "geo/BaseBounds.h"
#include <windows.h>

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
#include "utl/Environment.h"
#include "utl/splitPath.h"
#include "dbg/DebugLog.h"

#include "GuiMain.h"
#include "GuiStyles.h"
#include "GuiGraphics.h"
#include "GuiEmail.h"

#include "imgui_internal.h"
GuiMainComponent::GuiMainComponent()
{
    auto flags = GetRegStatusFlags();
    DMS_SetGlobalCppExceptionTranslator(&m_EventLog.GeoDMSExceptionMessage);
    DMS_RegisterMsgCallback(&m_EventLog.GeoDMSMessage, nullptr);
    DMS_SetContextNotification(&m_StatusBar.GeoDMSContextMessage, nullptr);
}

GuiMainComponent::~GuiMainComponent()
{
    m_View.CloseAll();

    //m_ItemsHolder.clear();
    m_State.clear();

    DMS_ReleaseMsgCallback(&m_EventLog.GeoDMSMessage, nullptr);
}

std::string FillOpenConfigSourceCommand(const std::string_view command, const std::string_view filename, const std::string_view line) 
{
    //"%env:ProgramFiles%\Notepad++\Notepad++.exe" "%F" -n%L
    std::string result = command.data();
    auto fn_part = result.find("%F");
    auto tmp_str = result.substr(fn_part + 2);
    if (fn_part != std::string::npos)
    {
        result.replace(fn_part, fn_part + 2, filename);
        result += tmp_str;
    }

    auto ln_part = result.find("%L");

    if (ln_part != std::string::npos)
        result.replace(ln_part, ln_part+2, line);

    return result;
}

void GuiMainComponent::ProcessEvent(GuiEvents e)
{
    switch (e)
    {
    case GuiEvents::UpdateCurrentItem:
    {
        if (!m_State.GetCurrentItem())
            return;
        
        if (m_State.GetCurrentItem()!=m_State.GetRoot())
            m_State.TreeItemHistoryList.Insert(m_State.GetCurrentItem());
        break;
    }
    case GuiEvents::ReopenCurrentConfiguration:
    {
        CloseCurrentConfig();
        if (!m_State.configFilenameManager.Get().empty())
            m_State.SetRoot(DMS_CreateTreeFromConfiguration(m_State.configFilenameManager.Get().c_str()));
        break;
    }
    case GuiEvents::OpenNewMapViewWindow: // TODO: merge OpenNewTableViewWindow into this one
    {
        if (!m_State.GetCurrentItem())
            break;

        auto viewstyle_flags = SHV_GetViewStyleFlags(m_State.GetCurrentItem());
        if (viewstyle_flags & ViewStyleFlags::vsfMapView)
            m_View.AddView(m_State, m_State.GetCurrentItem(), tvsMapView, "###View" + std::to_string(m_View.m_Views.size()));

        break;
    }
    case GuiEvents::OpenNewTableViewWindow:
    {
        if (!m_State.GetCurrentItem())
            break;

        auto viewstyle_flags = SHV_GetViewStyleFlags(m_State.GetCurrentItem());
        if (viewstyle_flags & ViewStyleFlags::vsfTableView)
            m_View.AddView(m_State, m_State.GetCurrentItem(), tvsTableView, "###View" + std::to_string(m_View.m_Views.size()));

        if (viewstyle_flags & ViewStyleFlags::vsfTableContainer)
            m_View.AddView(m_State, m_State.GetCurrentItem(), tvsTableContainer, "###View" + std::to_string(m_View.m_Views.size()));

        break;
    }
    case GuiEvents::OpenNewDefaultViewWindow:
    {
        if (!m_State.GetCurrentItem())
            break;

        auto dvs = SHV_GetDefaultViewStyle(m_State.GetCurrentItem());
        switch (dvs)
        {
        case tvsMapView:
        {
            m_View.AddView(m_State, m_State.GetCurrentItem(), tvsMapView, "###View" + std::to_string(m_View.m_Views.size()));
            break;
        }
        case tvsTableView:
        {
            m_View.AddView(m_State, m_State.GetCurrentItem(), tvsTableView, "###View" + std::to_string(m_View.m_Views.size()));
            break;
        }
        }

        break;
    }
    case GuiEvents::OpenConfigSource:
    {
        if (!m_State.GetCurrentItem())
            break;

        std::string filename = m_State.GetCurrentItem()->GetConfigFileName().c_str();
        std::string line     = std::to_string(m_State.GetCurrentItem()->GetConfigFileLineNr());
        std::string command  = GetGeoDmsRegKey("DmsEditor").c_str();

        if (!filename.empty() && !line.empty() && !command.empty())
        {
            auto result = FillOpenConfigSourceCommand(command, filename, line);
            const TreeItem *TempItem = m_State.GetCurrentItem();
            result = AbstrStorageManager::GetFullStorageName(TempItem, SharedStr(result.c_str())).c_str();
            WinExec(result.c_str(), SW_HIDE); //TODO: replace with CreateProcess
        }

        break;
    }
    case GuiEvents::ToggleShowCurrentItemBar:
    {
        m_State.ShowCurrentItemBar = !m_State.ShowCurrentItemBar;
        break;
    }
    case GuiEvents::ToggleShowDetailPagesWindow:
    {
        m_State.ShowDetailPagesWindow = !m_State.ShowDetailPagesWindow;
        break;
    }
    case GuiEvents::ToggleShowEventLogWindow:
    {
        m_State.ShowEventLogWindow = !m_State.ShowEventLogWindow;
        break;
    }
    case GuiEvents::ToggleShowToolbar:
    {
        m_State.ShowToolbar = !m_State.ShowToolbar;
        break;
    }
    case GuiEvents::ToggleShowTreeViewWindow:
    {
        m_State.ShowTreeviewWindow = !m_State.ShowTreeviewWindow;
        break;
    }
    case GuiEvents::StepToErrorSource:
    {
        if (!m_State.GetCurrentItem())
            break;
        auto item_error_source = TreeItem_GetErrorSource(m_State.GetCurrentItem());
        if (item_error_source.first)
        { 
            m_State.SetCurrentItem(const_cast<TreeItem*>(item_error_source.first));
            auto event_queues = GuiEventQueues::getInstance();
            event_queues->MainEvents.Add(GuiEvents::UpdateCurrentItem);
            event_queues->TreeViewEvents.Add(GuiEvents::JumpToCurrentItem);
            event_queues->DetailPagesEvents.Add(GuiEvents::UpdateCurrentItem);
            event_queues->CurrentItemBarEvents.Add(GuiEvents::UpdateCurrentItem);
        }
        break;
    }
    case GuiEvents::StepToRootErrorSource:
    {
        if (!m_State.GetCurrentItem())
            break;

        TreeItem* prev_item_error_source = m_State.GetCurrentItem();
        while (true)
        {
            auto item_error_source = TreeItem_GetErrorSource(prev_item_error_source);
            if (!item_error_source.first)
            {
                if (prev_item_error_source)
                {
                    m_State.SetCurrentItem(prev_item_error_source);
                    auto event_queues = GuiEventQueues::getInstance();
                    event_queues->MainEvents.Add(GuiEvents::UpdateCurrentItem);
                    event_queues->TreeViewEvents.Add(GuiEvents::JumpToCurrentItem);
                    event_queues->DetailPagesEvents.Add(GuiEvents::UpdateCurrentItem);
                    event_queues->CurrentItemBarEvents.Add(GuiEvents::UpdateCurrentItem);
                }
                break;
            }
            prev_item_error_source = const_cast<TreeItem*>(item_error_source.first);
        }

        break;
    }
    }
}

void GuiMainComponent::CloseCurrentConfig()
{
    m_View.CloseAll();
    m_State.clear();
}

bool GuiMainComponent::ShowErrorDialogIfNecessary()
{
    //if (!m_State.errorDialogMessage.HasNew())
    //    return false;

    if (ImGui::BeginPopupModal("Error", NULL, ImGuiWindowFlags_AlwaysAutoResize))
    {
        if (!m_State.errorDialogMessage._Get().empty())
        {
            ImGui::Text(const_cast<char*>(m_State.errorDialogMessage.Get().c_str()));
        }

        if (ImGui::Button("Ignore", ImVec2(120, 0))) { ImGui::CloseCurrentPopup(); }
        ImGui::SetItemDefaultFocus();
        ImGui::SameLine();
        if (ImGui::Button("Abort", ImVec2(120, 0))) 
        { 
            ImGui::CloseCurrentPopup();
            ImGui::EndPopup();
            return true;
        }
        ImGui::SameLine();
        if (ImGui::Button("Email", ImVec2(120, 0)))
        {
            GuiEmail email_system;
            email_system.SendMailUsingDefaultWindowsEmailApplication(m_State.errorDialogMessage.Get());
        }

        ImGui::SameLine();
        if (ImGui::Button("Reopen", ImVec2(120, 0)))
        {
            auto event_queues = GuiEventQueues::getInstance();
            event_queues->MainEvents.Add(GuiEvents::ReopenCurrentConfiguration);
        }


        ImGui::EndPopup();
    }
    return false;
}

bool GuiMainComponent::ShowSourceFileChangeDialogIfNecessary()
{
    //TODO: build in timer for checks?
    static std::string changed_files_result; 
    auto changed_files = DMS_ReportChangedFiles(true);
    if (changed_files)
    {
        changed_files_result = (*changed_files).c_str();
        changed_files->Release(changed_files);
        ImGui::OpenPopup("Changed source file(s)");
    }

    if (ImGui::BeginPopupModal("Changed source file(s)", NULL, ImGuiWindowFlags_AlwaysAutoResize))
    {
        auto event_queues = GuiEventQueues::getInstance();
        ImGui::Text(const_cast<char*>(changed_files_result.c_str()));

        if (ImGui::Button("Ok", ImVec2(120, 0))) { ImGui::CloseCurrentPopup(); }
        ImGui::SetItemDefaultFocus();
        ImGui::SameLine();
        if (ImGui::Button("Reopen", ImVec2(120, 0)))
        {
            event_queues->MainEvents.Add(GuiEvents::ReopenCurrentConfiguration);
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
    return false;
}

void GuiMainComponent::TraverseTreeItemHistoryIfRequested()
{
    TreeItem* new_current_item = NULL;
    if (ImGui::IsMouseClicked(3)) // side-back mous button
    {
        new_current_item = m_State.TreeItemHistoryList.GetPrevious();
    }
    if (ImGui::IsMouseClicked(4)) // side-front mouse button
    {
        new_current_item = m_State.TreeItemHistoryList.GetNext();
    }
    if (new_current_item)
    {
        auto event_queues = GuiEventQueues::getInstance();
        m_State.SetCurrentItem(new_current_item);
        event_queues->TreeViewEvents.Add(GuiEvents::JumpToCurrentItem);
        event_queues->DetailPagesEvents.Add(GuiEvents::UpdateCurrentItem);
        event_queues->CurrentItemBarEvents.Add(GuiEvents::UpdateCurrentItem);
    }
}

void GuiMainComponent::InterpretCommandLineParameters()
{
    int    argc = __argc;
    --argc;
    char** argv = __argv; 
    ++argv;

    CharPtr firstParam = argv[0];
    if ((argc > 0) && firstParam[0] == '/' && firstParam[1] == 'L')
    {
        SharedStr dmsLogFileName = ConvertDosFileName(SharedStr(firstParam + 2));

        m_DebugLog = std::make_unique<CDebugLog>(MakeAbsolutePath(dmsLogFileName.c_str()));
        SetRegStatusFlag(RSF_TraceLogFile);
        argc--; argv++;
    }

    ParseRegStatusFlags(argc, argv);
    
    for (; argc; --argc, ++argv) {
        if ((*argv)[0] == '/')
        {
            CharPtr cmd = (*argv) + 1;
            if (!stricmp(cmd, "noconfig"))
                m_NoConfig = true;
            if (!stricmp(cmd, "script")) // gui unit tests
            {
                m_GuiUnitTest.LoadStepsFromScriptFile(*(argv+1));
                argc--;
                argv++;
            }
        }
    }
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

    // windows always create their own viewport
    io.ConfigViewportsNoAutoMerge = true;
    io.ConfigDockingTransparentPayload = true;

    // setup Dear ImGui style
    ImGui::StyleColorsLight(); // TODO: make this a style option
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

    // Load gui state
    m_State.LoadWindowOpenStatusFlags();

    // load ini file
    io.IniFilename = NULL; // disable automatic saving and loading to and from .ini file
    LoadIniFromRegistry();

    // command line params
    InterpretCommandLineParameters();

    return 0;
}

int GuiMainComponent::MainLoop()
{
    ImGuiIO& io = ImGui::GetIO();

    // state
    ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);
    glfwSetWindowTitle(m_Window, (m_State.configFilenameManager._Get() + DMS_GetVersion()).c_str()); // default window title
    //glfwSetKeyCallback(m_Window, &m_Input.ProcessKeyEvent);

    InitializeGuiTextures();
    SHV_SetAdminMode(true); // needed for ViewStyleFlags lookup

    const UInt32 frames_to_update = 5;
    UInt32 UpdateFrameCounter = frames_to_update;

    // Main loop
    while (!glfwWindowShouldClose(m_Window))
    {
        if (--UpdateFrameCounter) // when waking up from an event, update n frames
            glfwPollEvents();
        else 
        {
            glfwWaitEventsTimeout(1.0);
            if (UpdateFrameCounter == 0)
                UpdateFrameCounter = frames_to_update;
        }

        if (m_GuiUnitTest.ProcessStep(m_State))
            break;

        // Start the Dear ImGui frame
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame(); // TODO: set  to true for UpdateInputEvents?

        // Error dialogue
        if (ShowErrorDialogIfNecessary())
            break;

        // Updated source files
        ShowSourceFileChangeDialogIfNecessary();

        // Handle new config file
        if (m_State.configFilenameManager.HasNew())
        {
            CloseCurrentConfig();
            auto parent_path = std::filesystem::path(m_State.configFilenameManager.Get()).parent_path();
            auto filename = std::filesystem::path(m_State.configFilenameManager.Get()).filename();

            glfwSetWindowTitle(m_Window, (filename.string() + " in " + parent_path.string() + std::string(" - ") + DMS_GetVersion()).c_str());
            m_State.SetRoot(DMS_CreateTreeFromConfiguration(m_State.configFilenameManager.Get().c_str()));

            //DMS_RegisterMsgCallback(&m_EventLog.GeoDMSMessage, nullptr);

            if (m_State.GetRoot())
            {
                //DMS_TreeItem_RegisterStateChangeNotification(&OnTreeItemChanged, m_State.GetRoot(), nullptr);
                m_State.GetRoot()->UpdateMetaInfo();
            }
        }

        // TreeItem history event
        TraverseTreeItemHistoryIfRequested();

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
        if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
        {
            GLFWwindow* backup_current_context = glfwGetCurrentContext();
            ImGui::UpdatePlatformWindows();
            ImGui::RenderPlatformWindowsDefault();
            glfwMakeContextCurrent(backup_current_context);
        }

        glfwSwapBuffers(m_Window);


        if (!m_FirstFrames)
        {
            // initializations after first n frames
            if (!m_NoConfig)
                m_State.configFilenameManager.Set(GetGeoDmsRegKey("LastConfigFile").c_str());
            m_FirstFrames--;
        }
        else if (m_FirstFrames > 0)
            m_FirstFrames--;

        m_GuiUnitTest.Step();
    }

    // Persistently store gui state in registry
    m_State.SaveWindowOpenStatusFlags();
    SaveIniToRegistry();

    // Cleanup
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glfwDestroyWindow(m_Window);
    glfwTerminate();

    return m_State.return_value;
}

void GuiMainComponent::Update()
{
    auto event_queues = GuiEventQueues::getInstance();
    static bool opt_fullscreen = true;
    static bool opt_padding = true;
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
        dockspace_flags &= ~ImGuiDockNodeFlags_PassthruCentralNode;

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
        ImGuiID dockspace_id = ImGui::GetID("GeoDMSDockSpace");
        ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), dockspace_flags);
    }

    while (event_queues->MainEvents.HasEvents()) // Handle MainEvents
    {
        auto e = event_queues->MainEvents.Pop();
        ProcessEvent(e);

        if (e == GuiEvents::ReopenCurrentConfiguration)
        {
            ImGui::End();
            return;
        }
    }

    // Update all GeoDMSGui components
    m_MenuComponent.Update(m_State, m_View);

    if (m_State.ShowCurrentItemBar)
        m_CurrentItemComponent.Update(m_State);

    if (m_State.ShowToolbar)
        m_Toolbar.Update(&m_State.ShowToolbar, m_State, m_View);

    if (m_State.ShowDetailPagesWindow)
        m_DetailPages.Update(&m_State.ShowDetailPagesWindow, m_State);

    if (m_State.ShowTreeviewWindow)
        m_TreeviewComponent.Update(&m_State.ShowTreeviewWindow, m_State);

    if (m_State.ShowEventLogWindow)
        m_EventLog.Update(&m_State.ShowEventLogWindow, m_State);

    if (m_State.ShowStatusBar)
        m_StatusBar.Update(&m_State.ShowStatusBar, m_State);

    m_View.UpdateAll(m_State);

    if (m_State.ShowDemoWindow)
        ImGui::ShowDemoWindow(&m_State.ShowDemoWindow);

    // option windows
    if (m_State.ShowOptionsWindow)
        m_Options.Update(&m_State.ShowOptionsWindow);

    if (m_State.ShowEventLogOptionsWindow)
        ShowEventLogOptionsWindow(&m_State.ShowEventLogOptionsWindow);
    
    ImGui::End();
}
