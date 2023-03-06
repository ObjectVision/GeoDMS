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

#define GLFW_EXPOSE_NATIVE_WIN32
#include "GLFW/glfw3native.h"

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
#include "ShvDllInterface.h"
#include "dbg/DmsCatch.h"

#include "GuiMain.h"
#include "GuiStyles.h"
#include "GuiGraphics.h"
#include "GuiEmail.h"

#include "imgui_internal.h"
GuiMainComponent::GuiMainComponent()
{
    auto flags = GetRegStatusFlags();
    DMS_SetGlobalCppExceptionTranslator(&m_EventLog.GeoDMSExceptionMessage);
    DMS_RegisterMsgCallback(&m_EventLog.GeoDMSMessage, this);
    DMS_SetContextNotification(&m_EventLog.GeoDMSContextMessage, this);
    DMS_RegisterStateChangeNotification(&m_Views.OnOpenEditPaletteWindow, this);
    SHV_SetCreateViewActionFunc(&m_DetailPages.OnViewAction);
}

GuiMainComponent::~GuiMainComponent()
{
    CloseCurrentConfig();
    //m_View.CloseAll(); 
    //m_Treeview.clear();
    //m_State.clear();
    GuiEventQueues::DeleteInstance();

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
        result.replace(ln_part, ln_part + 2, line);

    return result;
}

bool GuiMainComponent::ProcessEvent(GuiEvents e)
{
    auto event_queues = GuiEventQueues::getInstance();
    switch (e)
    {
    case GuiEvents::UpdateCurrentItem:
    {
        if (!m_State.GetCurrentItem())
            return false;

        if (m_State.GetCurrentItem() != m_State.GetRoot())
            m_State.TreeItemHistoryList.Insert({ m_State.GetCurrentItem() });
        break;
    }
    case GuiEvents::ReopenCurrentConfiguration:
    {
        std::string current_item_fullname = m_State.GetCurrentItem() ? m_State.GetCurrentItem()->GetFullName().c_str() : "";
        CloseCurrentConfig();
        m_State.configFilenameManager.Set(GetGeoDmsRegKey("LastConfigFile").c_str());
        m_CurrentItem.SetCurrentItemFullNameDirectly(current_item_fullname);
        event_queues->CurrentItemBarEvents.Add(GuiEvents::UpdateCurrentItemDirectly);
        //if (!m_State.configFilenameManager.Get().empty())
        //    m_State.SetRoot(DMS_CreateTreeFromConfiguration(m_State.configFilenameManager.Get().c_str()));
        break;
    }
    case GuiEvents::OpenNewMapViewWindow: // TODO: merge OpenNewTableViewWindow into this one
    {
        if (!m_State.GetCurrentItem())
            break;

        auto viewstyle_flags = SHV_GetViewStyleFlags(m_State.GetCurrentItem());
        if (viewstyle_flags & ViewStyleFlags::vsfMapView)
            m_Views.AddDMSView(m_State, tvsMapView, "###DMSView" + std::to_string(m_Views.m_dms_views.size()));

        break;
    }
    case GuiEvents::OpenNewTableViewWindow:
    {
        if (!m_State.GetCurrentItem())
            break;

        auto viewstyle_flags = SHV_GetViewStyleFlags(m_State.GetCurrentItem());
        if (viewstyle_flags & ViewStyleFlags::vsfTableView)
            m_Views.AddDMSView(m_State, tvsTableView, "###DMSView" + std::to_string(m_Views.m_dms_views.size()));

        if (viewstyle_flags & ViewStyleFlags::vsfTableContainer)
            m_Views.AddDMSView(m_State, tvsTableContainer, "###DMSView" + std::to_string(m_Views.m_dms_views.size()));

        break;
    }
    case GuiEvents::OpenNewImGuiTableViewWIndow:
    {
        if (!m_State.GetCurrentItem())
            break;

        auto viewstyle_flags = SHV_GetViewStyleFlags(m_State.GetCurrentItem());
        if (viewstyle_flags & ViewStyleFlags::vsfTableView)
            m_Views.AddTableView(m_State);

        //if (viewstyle_flags & ViewStyleFlags::vsfTableContainer)
        //    m_Views.AddDMSView(m_State, tvsTableContainer, ");

        break;
    }
    case GuiEvents::OpenNewDefaultViewWindow:
    {
        if (!m_State.GetCurrentItem())
            break;

        auto dvs = SHV_GetDefaultViewStyle(m_State.GetCurrentItem());
        switch (dvs)
        {
        case tvsMapView: { m_Views.AddDMSView(m_State, tvsMapView, "###DMSView" + std::to_string(m_Views.m_dms_views.size())); break; }
        case tvsTableView: { m_Views.AddDMSView(m_State, tvsTableView, "###DMSView" + std::to_string(m_Views.m_dms_views.size())); break; }
        case tvsTableContainer: { m_Views.AddDMSView(m_State, tvsTableView, "###DMSView" + std::to_string(m_Views.m_dms_views.size())); break; }
        }

        break;
    }
    case GuiEvents::OpenNewStatisticsViewWindow:
    {
        if (!IsDataItem(m_State.GetCurrentItem())) // statistics is for dataitems only
            break;

        m_Views.AddStatisticsView(m_State, std::string("###StatView") + std::to_string(m_Views.m_statistic_views.size()));

        break;
    }
    case GuiEvents::OpenConfigSource:
    {
        if (!m_State.GetCurrentItem())
            break;

        std::string filename = m_State.GetCurrentItem()->GetConfigFileName().c_str();
        std::string line = std::to_string(m_State.GetCurrentItem()->GetConfigFileLineNr());
        std::string command = GetGeoDmsRegKey("DmsEditor").c_str();

        if (!filename.empty() && !line.empty() && !command.empty())
        {
            auto openConfigCmd = FillOpenConfigSourceCommand(command, filename, line);
            const TreeItem* TempItem = m_State.GetCurrentItem();
            auto fullPathCmd = AbstrStorageManager::GetFullStorageName(TempItem, SharedStr(openConfigCmd.c_str()));
            DMS_CALL_BEGIN
            try
            {
                StartChildProcess(NULL, const_cast<Char*>(fullPathCmd.c_str())); //TODO: crash in case fullPathCmd does not exist
            }
            catch (...)
            {
                throw;
            }
            DMS_CALL_END
        }

        break;
    }
    case GuiEvents::ToggleShowCurrentItemBar: { m_State.ShowCurrentItemBar = !m_State.ShowCurrentItemBar; break;}
    case GuiEvents::ToggleShowDetailPagesWindow: { m_State.ShowDetailPagesWindow = !m_State.ShowDetailPagesWindow; break;}
    case GuiEvents::ToggleShowEventLogWindow: { m_State.ShowEventLogWindow = !m_State.ShowEventLogWindow; break;}
    case GuiEvents::ToggleShowToolbar: { m_State.ShowToolbar = !m_State.ShowToolbar; break; }
    case GuiEvents::ToggleShowTreeViewWindow: { m_State.ShowTreeviewWindow = !m_State.ShowTreeviewWindow; break; }
    case GuiEvents::OpenExportWindow: { m_State.ShowExportWindow = true; break; }
    case GuiEvents::StepToErrorSource:
    {
        if (!m_State.GetCurrentItem())
            break;
        auto item_error_source = TreeItem_GetErrorSource(m_State.GetCurrentItem(), true);
        if (item_error_source.first)
        {
            m_State.SetCurrentItem(const_cast<TreeItem*>(item_error_source.first));
            event_queues->MainEvents.Add(GuiEvents::UpdateCurrentItem);
            event_queues->TreeViewEvents.Add(GuiEvents::JumpToCurrentItem);
            event_queues->DetailPagesEvents.Add(GuiEvents::UpdateCurrentItem);
            event_queues->CurrentItemBarEvents.Add(GuiEvents::UpdateCurrentItem);
        }
        break;
    }
    case GuiEvents::CloseAllViews: { m_Views.CloseAll(); break;}
    case GuiEvents::StepToRootErrorSource:
    {
        if (!m_State.GetCurrentItem())
            break;

        TreeItem* prev_item_error_source = m_State.GetCurrentItem();
        while (true)
        {
            auto item_error_source = TreeItem_GetErrorSource(prev_item_error_source, true);
            if (!item_error_source.first)
            {
                if (prev_item_error_source)
                {
                    m_State.SetCurrentItem(prev_item_error_source);
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
    case GuiEvents::ShowLocalSourceDataChangedModalWindow: { ImGui::OpenPopup("Changed LocalData or SourceData path", ImGuiPopupFlags_None); break;}
    case GuiEvents::ShowAboutTextModalWindow: {ImGui::OpenPopup("About", ImGuiPopupFlags_None); break; }
    //case GuiEvents::OpenErrorDialog: { ImGui::OpenPopup("Error"); break; }
    case GuiEvents::Close: { return true; } // Exit application
    }
    return false;
}

void GuiMainComponent::CloseCurrentConfig()
{
    m_DetailPages.clear();
    m_Views.CloseAll();
    m_Treeview.clear();
    m_State.clear();
}

void GuiMainComponent::ShowAboutDialogIfNecessary(GuiState& state)
{
    if (ImGui::BeginPopupModal("About", NULL, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoTitleBar))
    {
        ImGui::FocusWindow(ImGui::GetCurrentWindow());
        ImGui::BringWindowToFocusFront(ImGui::GetCurrentWindow());
        ImGui::BringWindowToDisplayFront(ImGui::GetCurrentWindow());
        ImGui::TextUnformatted(state.aboutDialogMessage.c_str());
        if (ImGui::Button("Ok", ImVec2(120, 0)))
        {
            ImGui::CloseCurrentPopup();
            ImGui::EndPopup();
            return;
        }
        ImGui::EndPopup();
    }
    return;
}

bool GuiMainComponent::ShowLocalOrSourceDataDirChangedDialogIfNecessary(GuiState &state)
{
    //ImGui::OpenPopup("ChangedLDSD", ImGuiPopupFlags_None);
    if (ImGui::BeginPopupModal("Changed LocalData or SourceData path", NULL, ImGuiWindowFlags_AlwaysAutoResize|ImGuiWindowFlags_NoTitleBar))
    {
        //ImGui::BringWindowToDisplayFront(ImGui::GetCurrentWindow());
        ImGui::Text("LocalData or SourceData path changed, restart required for changes to take effect.");
        ImGui::SetItemDefaultFocus();
        
        if (ImGui::Button("Ok", ImVec2(120, 0)))
        {
            ImGui::CloseCurrentPopup();
            ImGui::EndPopup();
            return false;
        }
        ImGui::EndPopup();
    } // TODO: known issue, due to added decoration, specifically for modal windows the close button does not work.
    return false;
}

bool GuiMainComponent::ShowErrorDialogIfNecessary()
{
    if (m_State.errorDialogMessage.HasNew())
    {
        m_State.errorDialogMessage.Get();
        ImGui::OpenPopup("Error");
    }

    if (ImGui::BeginPopupModal("Error", NULL, ImGuiWindowFlags_AlwaysAutoResize|ImGuiWindowFlags_NoTitleBar))
    {
        //ImGui::BringWindowToDisplayFront(ImGui::GetCurrentWindow());
        if (!m_State.errorDialogMessage._Get().empty())
        {
            ImGui::Text(const_cast<char*>(m_State.errorDialogMessage.Get().c_str())); //TODO: interpret error message for link
        }

        if (ImGui::Button("Cancel", ImVec2(120, 0))) { ImGui::CloseCurrentPopup(); }
        ImGui::SetItemDefaultFocus();
        ImGui::SameLine();
        if (ImGui::Button("Abort", ImVec2(120, 0)))
        {
            ImGui::CloseCurrentPopup();
            ImGui::EndPopup();
            return true;
        }
        /*ImGui::SameLine();
        if (ImGui::Button("Email", ImVec2(120, 0)))
        {
            GuiEmail email_system;
            email_system.SendMailUsingDefaultWindowsEmailApplication(m_State.errorDialogMessage.Get());
        }*/

        ImGui::SameLine();
        if (ImGui::Button("Reopen", ImVec2(120, 0)))
        {
            auto event_queues = GuiEventQueues::getInstance();
            event_queues->MainEvents.Add(GuiEvents::ReopenCurrentConfiguration);
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    } // TODO: known issue, due to added decoration, specifically for modal windows the close button does not work.
    return false;
}

bool GuiMainComponent::ShowSourceFileChangeDialogIfNecessary()
{
    //ImGui::BringWindowToDisplayFront(ImGui::GetCurrentWindow());
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
    } // TODO: known issue, due to added decoration, specifically for modal windows the close button does not work.
    return false;
}

void GuiMainComponent::TraverseTreeItemHistoryIfRequested()
{
    ViewAction new_view_action = {};
    if (ImGui::IsMouseClicked(3)) // side-back mous button
    {
        new_view_action = m_State.TreeItemHistoryList.GetPrevious();
    }
    if (ImGui::IsMouseClicked(4)) // side-front mouse button
    {
        new_view_action = m_State.TreeItemHistoryList.GetNext();
    }
    if (new_view_action.tiContext)
    {
        auto event_queues = GuiEventQueues::getInstance();
        m_State.SetCurrentItem(new_view_action.tiContext);

        //if (new_view_action.sAction.contains("dp.vi.attr"))
        //    event_queues->DetailPagesEvents.Add(GuiEvents::FocusValueInfoTab);
        //else
        event_queues->DetailPagesEvents.Add(GuiEvents::UpdateCurrentItem);

        event_queues->TreeViewEvents.Add(GuiEvents::JumpToCurrentItem);
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
        SetCachedStatusFlag(RSF_TraceLogFile);
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
                m_GuiUnitTest.LoadStepsFromScriptFile(*(argv + 1));
                argc--;
                argv++;
            }
        }
    }
}

void GuiMainComponent::CreateMainWindowInWindowedFullscreenMode()
{
    auto primary_monitor = glfwGetPrimaryMonitor();
    int xpos, ypos, width, height;
    glfwGetMonitorWorkarea(primary_monitor, &xpos, &ypos, &width, &height);
    m_MainWindow = glfwCreateWindow(width, height, "", NULL, NULL); // 1280, 720

    auto main_hwnd = glfwGetWin32Window(m_MainWindow);
    SendMessage(main_hwnd, WM_SYSCOMMAND, SC_MAXIMIZE, 0);
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
    CreateMainWindowInWindowedFullscreenMode();

    //const GLFWvidmode* mode = glfwGetVideoMode(primary_monitor);

    //glfwSetWindowMonitor(m_Window, primary_monitor, 0, 0, mode->width, mode->height, mode->refreshRate);
    //const GLFWvidmode* mode = glfwGetVideoMode(primary_monitor);

    //glfwWindowHint(GLFW_RED_BITS, mode->redBits);
    //glfwWindowHint(GLFW_GREEN_BITS, mode->greenBits);
    //glfwWindowHint(GLFW_BLUE_BITS, mode->blueBits);
    //glfwWindowHint(GLFW_REFRESH_RATE, mode->refreshRate);

    //m_Window = glfwCreateWindow(mode->width, mode->height, "", primary_monitor, NULL);


    if (m_MainWindow == NULL)
        return 1;
    glfwMakeContextCurrent(m_MainWindow);
    glfwSwapInterval(1); // Enable vsync

    // setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();

    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;       // Enable Keyboard Controls
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;           // Enable Docking
    io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;         // Enable Multi-Viewport / Platform Windows
    io.ConfigDockingAlwaysTabBar = true;
    io.ConfigViewportsNoDecoration = false;

    // windows always create their own viewport
    io.ConfigViewportsNoAutoMerge = true;
    io.ConfigDockingTransparentPayload = true;

    // setup Dear ImGui style
    ImGui::StyleColorsLight(); // TODO: make this a style option
    //ImGui::StyleColorsDark();
    //ImGui::StyleColorsClassic();

    // when viewports are enabled we tweak WindowRounding/WindowBg so platform windows can look identical to regular ones.
    ImGuiStyle &style = ImGui::GetStyle();
    if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
    {
        style.WindowRounding = 0.0f;
        style.Colors[ImGuiCol_WindowBg].w = 1.0f;
    }

    // Setup Platform/Renderer backends
    ImGui_ImplGlfw_InitForOpenGL(m_MainWindow, true); // TODO: fully remove singletons from gui using glfwSetWindowUserPointerw https://www.glfw.org/docs/latest/group__window.html#ga3d2fc6026e690ab31a13f78bc9fd3651
    ImGui_ImplOpenGL3_Init(glsl_version.c_str());
    if (not glewInit() == GLEW_OK) // OpenGL extentions entry point
        throwErrorF("GLEW", "unsuccessful initialization of openGL helper library glew.");

    SetDmsWindowIcon(m_MainWindow);
    
    // fonts
    FontBuilderRecipy recipy;
    recipy.recipy.emplace_back(CreateNotoSansMediumFontSpec());
    recipy.recipy.emplace_back(CreateNotoSansArabicFontSpec());
    recipy.recipy.emplace_back(CreateNotoSansJapaneseFontSpec());
    recipy.recipy.emplace_back(CreateRemixIconsFontSpec());
    recipy.recipy.emplace_back(CreateNotoSansMathFontSpec());

    //ImWchar text_font_range[] = { 0x20, 0xFFFF, 0 }; // Needs to be kept in scope until font is build
    //ImWchar icon_font_range[] = { 0xEA01, 0xf2DE, 0 };
    m_State.fonts.text_font = SetGuiFont(recipy);
    //m_State.fonts.text_font = SetGuiFont({ "misc/fonts/NotoSans-Medium.ttf" }, 17.0f, -2.0f, text_font_range);// NotoSans - Regular.ttf");// DroidSans.ttf");
    //m_State.fonts.icon_font = SetGuiFont({ "misc/fonts/remixicon.ttf" }, 15.0f, 1.0f, icon_font_range);
    
    // SetGuiFont(std::vector<std::string> font_filenames, float font_size, float font_y_offset)
    
    
    
    //SetGuiFont("misc/fonts/Cambria-01.ttf"); // from C:\Windows\Fonts
    // Load gui state
    m_State.LoadWindowOpenStatusFlags();

    // load ini file
    io.IniFilename = NULL; // disable automatic saving and loading to and from .ini file
    //LoadIniFromRegistry(true);
    m_DockingInitialized = false;// LoadIniFromRegistry();

    InterpretCommandLineParameters();

    return 0;
}

int GuiMainComponent::MainLoop()
{
    ImGuiIO& io = ImGui::GetIO();

    // state
    //ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);
    //glClearColor(1.0, 0.0, 0.0, 1.0);
    glfwSetWindowTitle(m_MainWindow, (m_State.configFilenameManager._Get() + DMS_GetVersion()).c_str()); // default window title
    //glfwSetKeyCallback(m_Window, &m_Input.ProcessKeyEvent);

    InitializeGuiTextures();
    SHV_SetAdminMode(true); // needed for ViewStyleFlags lookup

    const UInt32 frames_to_update = 5;
    UInt32 UpdateFrameCounter = frames_to_update;

    // Main loop
    while (!glfwWindowShouldClose(m_MainWindow))
    {
        
        //if (--UpdateFrameCounter) // when waking up from an event, update n frames
        glfwPollEvents();
        //else 
        //{
        
        //glfwWaitEvents();
        //glfwPostEmptyEvent();
        //glfwWaitEventsTimeout(1.0);
        //    if (UpdateFrameCounter == 0)
        //        UpdateFrameCounter = frames_to_update;
        //}

        if (m_GuiUnitTest.ProcessStep(m_State))
            break;

        //break; // TODO: REMOVE, test for mem leaks

        // Start the Dear ImGui frame
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame(); // TODO: set  to true for UpdateInputEvents?
        m_State.dockspace_id = ImGui::GetID("GeoDMSDockSpace");

        // TreeItem history event
        TraverseTreeItemHistoryIfRequested();
        
        // update all gui components
        if (Update())
            break;
        
        // Modal windows
        if (ShowErrorDialogIfNecessary())
            break;

        ShowSourceFileChangeDialogIfNecessary();

        // Handle new config file
        if (m_State.configFilenameManager.HasNew())
        {
            CloseCurrentConfig();
            auto parent_path = std::filesystem::path(m_State.configFilenameManager.Get()).parent_path();
            auto filename = std::filesystem::path(m_State.configFilenameManager.Get()).filename();

            glfwSetWindowTitle(m_MainWindow, (filename.string() + " in " + parent_path.string() + std::string(" - ") + DMS_GetVersion()).c_str());
            m_State.SetRoot(DMS_CreateTreeFromConfiguration(m_State.configFilenameManager.Get().c_str()));

            //DMS_RegisterMsgCallback(&m_EventLog.GeoDMSMessage, nullptr);

            if (m_State.GetRoot())
            {
                //DMS_TreeItem_RegisterStateChangeNotification(&OnTreeItemChanged, m_State.GetRoot(), nullptr);
                //m_State.GetRoot()->UpdateMetaInfo();
            }
        }
        

        // rendering
        ImGui::Render();
        int display_w, display_h;
        glfwGetFramebufferSize(m_MainWindow, &display_w, &display_h);
        glViewport(0, 0, display_w, display_h);
        //glClearColor(1.0, 0.0, 0.0, 1.0);
        //glClearColor(clear_color.x * clear_color.w, clear_color.y * clear_color.w, clear_color.z * clear_color.w, clear_color.w);
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

        glfwSwapBuffers(m_MainWindow);



        m_GuiUnitTest.Step();
        
        //break;
    }

    // Persistently store gui state in registry
    m_State.SaveWindowOpenStatusFlags();
    SaveIniToRegistry();

    // Cleanup
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glfwDestroyWindow(m_MainWindow);
    glfwTerminate();

    return m_State.return_value;
}

bool GuiMainComponent::Update()
{
    static bool opt_fullscreen = true; //TODO: remove?
    static bool opt_padding = true; //TODO: remove?
    static ImGuiDockNodeFlags dockspace_flags = ImGuiDockNodeFlags_None; //TODO: remove?
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

    //ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0, 0, 0, 0));
    //ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0, 0, 0, 0));
    //ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0, 0, 0, 0));
    if (!ImGui::Begin("GeoDMSGui", nullptr, window_flags))
    {
        ImGui::End();
        return false;
    }

    if (!opt_padding)
        ImGui::PopStyleVar();

    if (opt_fullscreen)
        ImGui::PopStyleVar(2);

    // Submit the DockSpace
    auto io = ImGui::GetIO();
    ImGui::PushStyleColor(ImGuiCol_DockingEmptyBg, ImGui::GetStyleColorVec4(ImGuiCol_WindowBg));
    ImGui::DockSpace(m_State.dockspace_id, ImVec2(0.0f, 0.0f), dockspace_flags);
    ImGui::PopStyleColor();

    auto event_queues = GuiEventQueues::getInstance();
    while (event_queues->MainEvents.HasEvents()) // Handle MainEvents
    {
        auto e = event_queues->MainEvents.Pop();
        if (ProcessEvent(e))
            return true;

        if (e == GuiEvents::ReopenCurrentConfiguration)
        {
            ImGui::End();
            return false;
        }
    }

    // modal windows
    if (ShowLocalOrSourceDataDirChangedDialogIfNecessary(m_State))
        return true;

    ShowAboutDialogIfNecessary(m_State);

    //static auto first_time = true;
    m_Menu.Update(m_State, m_Views);

    if (m_State.ShowCurrentItemBar)
        m_CurrentItem.Update(m_State);
    
    ImGui::End();
    //ImGui::PopStyleColor(3);
    
    // Update all GeoDMSGui components
    if (m_State.ShowToolbar)
        m_Toolbar.Update(&m_State.ShowToolbar, m_State, m_Views);

    if (m_State.ShowDetailPagesWindow)
        m_DetailPages.Update(&m_State.ShowDetailPagesWindow, m_State);

    if (m_State.ShowTreeviewWindow)
        m_Treeview.Update(&m_State.ShowTreeviewWindow, m_State);

    if (m_State.ShowEventLogWindow)
        m_EventLog.Update(&m_State.ShowEventLogWindow, m_State);

    m_Views.UpdateAll(m_State);



    if (!m_FirstFrames)
    {
        // initializations after first n frames
        if (!m_NoConfig)
            m_State.configFilenameManager.Set(GetGeoDmsRegKey("LastConfigFile").c_str());
        m_FirstFrames--;
    }
    else if (m_FirstFrames > 0)
        m_FirstFrames--;

    if (m_State.ShowDemoWindow)
        ImGui::ShowDemoWindow(&m_State.ShowDemoWindow);

    if (m_State.ShowOptionsWindow)
        m_Options.Update(&m_State.ShowOptionsWindow, m_State);

    if (m_State.ShowExportWindow)
        m_Export.Update(&m_State.ShowExportWindow, m_State);

    if (m_State.ShowEventLogOptionsWindow)
        m_EventLog.ShowEventLogOptionsWindow(&m_State.ShowEventLogOptionsWindow);
    

    auto ctx = ImGui::GetCurrentContext();
    static auto first_time_docking = true;
    ImGuiDockContext* dc = &ctx->DockContext;

    auto dockspace_docknode = (ImGuiDockNode*)dc->Nodes.GetVoidPtr(m_State.dockspace_id);
    if (first_time_docking)
    {
        // GeoDMS window ptrs
        auto event_log_window = ImGui::FindWindowByName("EventLog");
        auto tree_view_window = ImGui::FindWindowByName("TreeView");
        auto tree_view_window_dock = ImGui::FindWindowByID(1);
        
        auto detail_pages_window = ImGui::FindWindowByName("Detail Pages");
        auto toolbar_window = ImGui::FindWindowByName("Toolbar");


        //dockspace_docknode->SharedFlags |= ImGuiDockNodeFlags_AutoHideTabBar;
        //dockspace_docknode->LocalFlags |= ImGuiDockNodeFlags_AutoHideTabBar;

        if (dockspace_docknode && dockspace_docknode->HostWindow)
        {
            // TODO: check if dockspace_node is unsplit
            ImGui::DockContextQueueDock(ctx, dockspace_docknode->HostWindow, dockspace_docknode, tree_view_window, ImGuiDir_Left, 0.2f, true);
            ImGui::DockContextQueueDock(ctx, dockspace_docknode->HostWindow, dockspace_docknode, detail_pages_window, ImGuiDir_Right, 0.8f, true);
            ImGui::DockContextQueueDock(ctx, dockspace_docknode->HostWindow, dockspace_docknode, toolbar_window, ImGuiDir_Up, 0.035f, true); //0.025f
            ImGui::DockContextQueueDock(ctx, dockspace_docknode->HostWindow, dockspace_docknode, event_log_window, ImGuiDir_Down, 0.8f, true);
                        
            first_time_docking = false;
        }
    }
    return false;
}
