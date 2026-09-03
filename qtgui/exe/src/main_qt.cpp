#include <QAbstractNativeEventFilter>
#include <QApplication>
#include <QByteArray>
#include <QFontDatabase>
#include <QMouseEvent>
#include <QPainter>
#include <QPalette>
#include <QResource>
#include <QMessageBox>
#include <QScreen>
#include <QThread> // TODO: remove

#include <memory>
#include <iostream>
#include <stdexcept>

#include "RtcInterface.h"

#include "act/garbage_can.h"
#include "act/MainThread.h" // SetMainThreadID
#include "dbg/DebugLog.h"
#include "dbg/DmsCatch.h"
#include "utl/Environment.h"
#include "utl/FileSystem.h"
#include "utl/Registry.h"
#include "utl/StrFormat.h"
#include "utl/splitPath.h"
#include "xct/DmsException.h"
#include "xct/ErrMsg.h"

#include "stg/AbstrStorageManager.h"
#include "StgBase.h"
#include "ser/FileStreamBuff.h"
#include "ShvDllInterface.h"
#include "ShvUtils.h"

#include "DmsMainWindow.h"
#include "DmsEventLog.h"
#include "DmsAddressBar.h"
#include "DmsDetailPages.h"
#include "DmsSplashScreen.h"
#include "DmsTreeView.h"
#include "TestScript.h"

struct CmdLineException : SharedStr, std::runtime_error {
    CmdLineException(SharedStr x)
    :   SharedStr(x +
            "\nexpected syntax:"
            "\nGeoDmsGuiQt.exe [/L<LogFile>] [/T<TestScript>] [/S<X> /C<X> ...] [/noconfig] [<ConfigFile.dms> [<Item>]]"
            "\n"
            "\n  /L<LogFile>     write a session log; must be the first argument"
            "\n  /T<TestScript>  replay a GUI test script; the exit code is 1 when it reported a script error"
            "\n  /S<X>, /C<X>    set resp. clear status flag <X>; before the configuration file name."
            "\n                  /SA shows hidden items, /SC state colours, /SM debug mode,"
            "\n                  /S1 /S2 /S3 the multi-threading levels, /SP performance logging"
            "\n  /noconfig       start without a configuration"
            "\n  <Item>          item to select as current item, relative to the configuration root"
            "\n"
            "\nfor the complete list of options and status flags, see"
            "\nhttps://github.com/ObjectVision/GeoDMS/wiki/Command-line-options"
        )
    ,   std::runtime_error(SharedStr::c_str())
    {}
    CmdLineException(CharPtr x) : CmdLineException(SharedStr(x)) {}
};

#ifndef _WIN32
static int    s_argc = 0;
static char** s_argv = nullptr;
#endif

std::any interpret_command_line_parameters(CmdLineSetttings& settingsFrame) {
#ifdef _WIN32
    int    argc = __argc;
    char** argv = __argv;
#else
    int    argc = s_argc;
    char** argv = s_argv;
#endif
    --argc;
    ++argv;

    std::any result;

    if ((argc > 0) && (*argv)[0] == '/' && (*argv)[1] == 'L') {
        SharedStr dmsLogFileName = ConvertDosFileName(SharedStr((*argv) + 2));

        auto log_file_handle = std::make_unique<CDebugLog>(MakeAbsolutePath(dmsLogFileName.c_str()), true);
        result = make_noncopyable_any<decltype(log_file_handle)>(std::move(log_file_handle));
        argc--; argv++;
    }

    if ((argc > 0) && (*argv)[0] == '/' && (*argv)[1] == 'T') {
        settingsFrame.m_TestScriptName = ConvertDosFileName(SharedStr((*argv) + 2));
        argc--; argv++;
    }

    ParseRegStatusFlags(argc, argv);

    // Nothing else can have overridden a status flag yet, so whatever is overridden now came from
    // the command line. A configuration that carries its own value for one of these settings has to
    // know that, and yield.
    SetCmdLineStatusFlagMask(GetCachedStatusMask());

    if (argc && (*argv)[0] == '/') {
        CharPtr cmd = (*argv) + 1;
        if (!stricmp(cmd, "noconfig")) {
            settingsFrame.m_NoConfig = true;
            argc--; argv++;
        }
    }
    if (argc) {
        // Unknown-option guard: on Windows '/x' is the option-prefix
        // convention; on Linux it's '-x'. Anything that survives the known-
        // option parsing above and still looks like an option must be a typo
        // or an unsupported flag -- fail loudly rather than silently treating
        // it as a config-file name.
#ifdef _WIN32
        if ((*argv)[0] == '/')
            throw CmdLineException(mySSPrintF("Unknown command-line option {}, or an option given out of turn.", *argv));
#else
        if ((*argv)[0] == '-' && (*argv)[1] != '\0')
            throw CmdLineException(mySSPrintF("Unknown command-line option {}, or an option given out of turn.", *argv));
#endif

        settingsFrame.m_ConfigFileName = SharedStr(*argv);
        argc--; argv++;
    }
    for (; argc; --argc, ++argv) {
        settingsFrame.m_CurrItemFullNames.emplace_back(SharedStr(*argv));
    }
    return result;
}

std::any init_geodms(QApplication& dms_app, CmdLineSetttings& settingsFrame) { // TODO: GeoDMS engine
    Q_INIT_RESOURCE(GeoDmsGuiQt);

    DMS_Shv_Load();
    SHV_SetAdminMode(true);
    SetMainThreadID(); // identify the main thread (formerly done via DMS_Appl_SetExeDir)
    DMS_Appl_SetFont();

    // Set explicit application font and link color from bundled resources so
    // QTextBrowser::toHtml() produces identical output on all platforms.
    {
        int id = QFontDatabase::addApplicationFont(dms_params::dms_font_resource);
        if (id != -1) {
            QString family = QFontDatabase::applicationFontFamilies(id).at(0);
            QApplication::setFont(QFont(family, dms_params::default_font_size));
        }
    }
    {
        QPalette pal = dms_app.palette();
        pal.setColor(QPalette::Link, QColor(0x00, 0x3e, 0x92));
        dms_app.setPalette(pal);
    }

    return interpret_command_line_parameters(settingsFrame);
}

#include "DmsViewArea.h"
#include <QMdiArea>

class CustomEventFilter : public QAbstractNativeEventFilter {
    //    Q_OBJECT
public:
    CustomEventFilter();
    ~CustomEventFilter();

    bool nativeEventFilter(const QByteArray& eventType, void* message, qintptr* result) override;
};

void SaveDetailPage(CharPtr fileName) {
    auto currItem = MainWindow::TheOne()->getCurrentTreeItem();

    auto dmsFileName = ConvertDosFileName(SharedStr(fileName));
    auto expandedFilename = AbstrStorageManager::Expand(currItem, dmsFileName);

    reportF(MsgCategory::commands, SeverityTypeID::ST_MajorTrace, "SaveDetailPage {}", DoubleQuote(expandedFilename.c_str()));

    auto htmlSource = MainWindow::TheOne()->m_detail_pages->toHtml();
    auto htmlSourceAsUtf8 = htmlSource.toUtf8();

    FileOutStreamBuff buff(expandedFilename, false);
    buff.WriteBytes(reinterpret_cast<const Byte*>(htmlSourceAsUtf8.constData()), htmlSourceAsUtf8.size());
}

#ifdef Q_OS_WIN

UInt32 Get4Bytes(const COPYDATASTRUCT* pcds, UInt32 i) {
    if (pcds->cbData < (i + 1) * 4)
        return 0;
    auto uint32_ptr = reinterpret_cast<UInt32*>(pcds->lpData);
    return uint32_ptr[i];
}


bool WmCopyData(MSG* copyMsgPtr) {
    auto pcds = reinterpret_cast<const COPYDATASTRUCT*>(copyMsgPtr->lParam);
    if (!pcds)
        return false;
    HWND hWindow = nullptr;
    std::shared_ptr<DataView> dv;
    auto commandCode = (CommandCode)pcds->dwData;
    switch (commandCode) {
    case CommandCode::SendApp: //break; // send msg without HWND
    case CommandCode::SendMain: hWindow = (HWND)(MainWindow::TheOne()->winId()); break;
    case CommandCode::SendFocus: hWindow = GetFocus(); break;
    case CommandCode::SendActiveDmsControl:
    case CommandCode::WmCopyActiveDmsControl: {
        auto aw = MainWindow::TheOne()->m_mdi_area->activeSubWindow();
        if (!aw)
            return false;
        auto va = dynamic_cast<QDmsViewArea*>(aw);
        if (!va)
            return false;
        dv = va->getDataView();
        if (!dv)
            return false;
        hWindow = dv->GetHWnd();
        break;
    }

    case CommandCode::DefaultView:
        MainWindow::TheOne()->defaultView();
        return true;

    case CommandCode::ActivateItem:
        MainWindow::TheOne()->m_address_bar->setPath(CharPtr(pcds->lpData));
        return true;

    case CommandCode::miExportViewPorts:
//        miExportViewPorts.Click;
        return true;

    case CommandCode::Expand:
        MainWindow::TheOne()->expandActiveNode(Get4Bytes(pcds, 0) != 0);
        return true;

    case CommandCode::ExpandAll:
        MainWindow::TheOne()->expandAll();
        return true;

    case CommandCode::ExpandRecursive:
        MainWindow::TheOne()->expandRecursiveFromCurrentItem();
        return true;

    case CommandCode::ShowDetailPage:
        MainWindow::TheOne()->m_detail_pages->show((ActiveDetailPage)Get4Bytes(pcds, 0));
        return true;

    case CommandCode::SaveDetailPage:
        SaveDetailPage(CharPtr(pcds->lpData));
        return true;

    case CommandCode::miDatagridView:
        MainWindow::TheOne()->tableView();
        return true;

    // optional payload: the ChartKind ordinal (0 = Histogram, 1 = Scatter, 2 = Line, 3 = Bar)
    case CommandCode::miHistogramView:
        switch (ChartKind(pcds->cbData >= 4 ? Get4Bytes(pcds, 0) : 0)) {
        case ChartKind::Scatter: MainWindow::TheOne()->scatterChartView(); break;
        case ChartKind::Line:    MainWindow::TheOne()->lineChartView();    break;
        case ChartKind::Bar:     MainWindow::TheOne()->barChartView();     break;
        default:                 MainWindow::TheOne()->histogramChartView(); break;
        }
        return true;

    case CommandCode::CascadeSubWindows:
        MainWindow::TheOne()->m_mdi_area->cascadeSubWindows();
        return true;

    case CommandCode::TileSubWindows:
        MainWindow::TheOne()->m_mdi_area->tileSubWindows();
        return true;

    case CommandCode::SaveValueInfo:
        MainWindow::TheOne()->SaveValueInfoImpl(CharPtr(pcds->lpData));
        return true;

    case CommandCode::ExportPrimaryData:
        MainWindow::TheOne()->exportPrimaryDataWithDialogDefaults();
        return true;

    default:
        return false;
    }

    assert(commandCode <= CommandCode::WmCopyActiveDmsControl);
    UINT message; WPARAM wParam; LPARAM lParam;
    COPYDATASTRUCT cds2;
    if (commandCode < CommandCode::WmCopyActiveDmsControl) {
        message = UINT(Get4Bytes(pcds, 0));
        wParam  = WPARAM(Get4Bytes(pcds, 1));
        lParam  = LPARAM(Get4Bytes(pcds, 2));
    }
    else { // code >= 4
        cds2.dwData = UINT(Get4Bytes(pcds, 0));
        cds2.cbData = (pcds->cbData >= 4) ?  pcds->cbData - 4 :0;
        cds2.lpData = (pcds->cbData >= 4) ? reinterpret_cast<UInt32*>(pcds->lpData) + 1 : nullptr;
        message = WM_COPYDATA;
        wParam  = WPARAM(MainWindow::TheOne()->winId());
        lParam  = LPARAM(&cds2);
    }
    return SendMessage(hWindow, message, wParam, lParam);
}

#endif // Q_OS_WIN

CustomEventFilter::CustomEventFilter() {
     reportD(MsgCategory::other, SeverityTypeID::ST_MinorTrace, "Created CustomEventFilter");
}

CustomEventFilter::~CustomEventFilter() {
    reportD(MsgCategory::other, SeverityTypeID::ST_MinorTrace, "Destroy CustomEventFilter");
}

bool CustomEventFilter::nativeEventFilter(const QByteArray& /*eventType*/, void* message, qintptr* /*result*/ )
{
    SuspendTrigger::Resume();

#ifdef Q_OS_WIN
    MSG* msg = static_cast<MSG*>(message);
    switch (msg->message) {
    case UM_SCALECHANGE:  // RegisterScaleChangeNotifications called in DmsViewArea.cpp, but this message is never received here
        if (auto mw = MainWindow::TheOne()) {
            for (auto* sw : mw->m_mdi_area->subWindowList()) {
                auto dms_sw = dynamic_cast<QDmsViewArea*>(sw);
                if (dms_sw) {
                    dms_sw->on_rescale();
                }
            }
        }
        return true; // Stop further processing of the message

    case WM_KEYDOWN:
        if (msg->wParam == 'W' || msg->wParam == VK_F4)
            if (GetKeyState(VK_CONTROL) & 0x8000)
                if (not (GetKeyState(VK_SHIFT) & 0x8000))
                    if (not (GetKeyState(VK_MENU) & 0x8000))
                        if (auto main_window = MainWindow::TheOne())
                            if (auto mdi_area = main_window->m_mdi_area.get())
                                if (auto current_active_subwindow = mdi_area->activeSubWindow())
                                {
                                    current_active_subwindow->close();
                                    return true;
                                }
        break;

    case UM_PROCESS_MAINTHREAD_OPERS:
        if (auto mw = MainWindow::TheOne())
            mw->ProcessAppOpers();
        return true;

    case UM_COPYDATA:
    case WM_COPYDATA:
        if (msg->hwnd == (HWND)MainWindow::TheOne()->winId()) {
            try {
                return WmCopyData(msg);
            }
            catch (...) {
                auto msgTxt = catchException(false);
                auto userResult = QMessageBox::critical(nullptr, "WM_COPYDATA Error",
                    QString::fromUtf8(msgTxt->GetAsText().c_str()),
                    QMessageBox::Ok | QMessageBox::Cancel);
                if (userResult == QMessageBox::Cancel)
                    terminate();
            }
        }
    }
#endif // Q_OS_WIN

    return false;
}

class DmsMouseForwardBackwardEventFilter : public QObject {
protected:
    bool eventFilter(QObject* obj, QEvent* event) override {
        if (event->type() == QEvent::MouseButtonPress) {
            QMouseEvent* mouse_event = static_cast<QMouseEvent*>(event);
            // back()/forward() navigate the tree (TreeItem lookups, setCurrentTreeItem) and
            // can throw; an event filter must not let that propagate into Qt's event dispatch.
            try {
                switch (mouse_event->button()) {
                case Qt::BackButton: { MainWindow::TheOne()->back(); return true; }
                case Qt::ForwardButton: { MainWindow::TheOne()->forward(); return true; }
                default: break;
                }
            }
            catch (...) {
                catchAndReportException();
                return true;
            }
        }
        return false; // QObject::eventFilter(obj, event);
    }
};


// Restore the placement saved by MainWindow::persistWindowGeometry() and show the window.
// restoreGeometry() is DPI- and screen-aware and clamps the window onto the currently available
// screen, so a geometry saved on a larger/again-scaled display no longer reopens oversized (which
// previously looked like a maximized window). Fall back to maximized on first run or unreadable data.
static void ShowMainWindowWithSavedGeometry(MainWindow& main_window)
{
    auto geomHex = GetGeoDmsRegKey("WindowGeometry");
    QByteArray geom = geomHex.empty()
        ? QByteArray()
        : QByteArray::fromHex(QByteArray(geomHex.c_str()));
    if (geom.isEmpty() || !main_window.restoreGeometry(geom))
    {
        main_window.showMaximized();
        return;
    }

    const bool restoredMaximized = main_window.isMaximized() || main_window.isFullScreen();
    main_window.show();

#ifdef Q_OS_WIN
    // Windows discards the nCmdShow of a process's FIRST ShowWindow call when the launcher supplied
    // STARTUPINFO.wShowWindow -- which Explorer does for every shortcut whose "Run:" field is not
    // "Normal window". Our own installer created its start-menu shortcuts with SW_SHOWMAXIMIZED, so
    // Qt's show() above lost that race: the window appeared at the placement restored just now and
    // was maximized by the shell a frame later, which reads as "it never remembers where I put it".
    // A second ShowWindow is no longer overridden, so re-assert what was restored. The installer now
    // writes SW_SHOWNORMAL, but every shortcut already on disk keeps its flag -- hence this stays.
    ShowWindow((HWND)main_window.winId(), restoredMaximized ? SW_SHOWMAXIMIZED : SW_SHOWNORMAL);
#endif
}

int main_without_SE_handler(int argc, char *argv[]) {
#ifdef Q_OS_WIN
    qputenv("QT_QPA_PLATFORM", "windows:darkmode=1"); // https://doc.qt.io/qt-6/qguiapplication.html#platform-specific-arguments
#endif
    auto dms_app_on_heap = std::make_unique<QApplication>(argc, argv);
    try {
        CmdLineSetttings settingsFrame;
        garbage_can geoDmsResources; // destruct resources after app completion

        auto native_event_filter_on_heap = std::make_unique<CustomEventFilter>();
        auto mouse_forward_backward_event_filter_on_heap = std::make_unique<DmsMouseForwardBackwardEventFilter>();

        geoDmsResources |= init_geodms(*dms_app_on_heap.get(), settingsFrame); // destruct resources after app completion
        dms_app_on_heap->installNativeEventFilter(native_event_filter_on_heap.get());

        SharedStr tsn = settingsFrame.m_TestScriptName;

        // Resolve up front what -- if anything -- this session loads, because it decides how the
        // window comes up. With a configuration to parse there is no splash screen and no one-second
        // wait: the main window is shown first and the parsing starts behind it, so that an error
        // dialog has a visible, taskbar-registered parent (#1162). The splash is for the idle start,
        // where there is nothing else to look at yet.
        ResolveStartupConfig(settingsFrame);
        bool hasStartupConfig = !settingsFrame.m_ConfigFileName.empty();
        bool useSplashScreen = tsn.empty() && !hasStartupConfig;

        std::unique_ptr<DmsSplashScreen> splash;
        if (useSplashScreen) {
            splash = showSplashScreen();
            splash->showMessage("Initialize GeoDMS Gui");
        }

        MainWindow main_window;
        dms_app_on_heap->setWindowIcon(QIcon(":/res/images/GeoDmsGuiQt.png"));
        dms_app_on_heap->installEventFilter(mouse_forward_backward_event_filter_on_heap.get());

        std::future<int> testResult;
        bool mustTerminateToken = false;

        if (useSplashScreen)
            QTimer::singleShot(1000, &main_window,
                [splashHandle = std::move(splash), &main_window]()
                {
                    ShowMainWindowWithSavedGeometry(main_window);
                    splashHandle->close();
                    ConfirmMainThreadOperProcessing();
                }
            );

        else
        {
            if (tsn.empty())
                ShowMainWindowWithSavedGeometry(main_window);
            else
            {
                // A test script drives the window itself and force-maximizes it below; that is not a
                // placement the user chose, so it must not end up in the WindowGeometry key.
                SetPersistWindowGeometry(false);
#ifdef Q_OS_WIN
                main_window.show(); // show it without maximizing yet
                HWND hwnd = (HWND)main_window.winId();
                ShowWindow(hwnd, SW_SHOWMAXIMIZED);
                SetWindowPos(hwnd, HWND_BOTTOM, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_SHOWWINDOW);
#else
                main_window.setWindowState(Qt::WindowMaximized);
                main_window.show();
#endif
            }
            ConfirmMainThreadOperProcessing();
        }

        // Queue the load before the test script is posted, so the script still runs on the loaded
        // configuration -- the same order as when the MainWindow constructor did this.
        if (hasStartupConfig)
            main_window.LoadConfig(settingsFrame.m_ConfigFileName.c_str()
                , settingsFrame.m_CurrItemFullNames.empty()
                    ? ""
                    : settingsFrame.m_CurrItemFullNames.back().c_str());

        if (!tsn.empty())
            main_window.PostAppOper([tsn, &testResult, &mustTerminateToken]
                {
                    testResult = std::async([tsn, &mustTerminateToken]
                        { 
                            return RunTestScript(tsn, &mustTerminateToken);
                        }
                    );
                }
            );

        auto result = dms_app_on_heap->exec();
        mustTerminateToken = true;

        if (!tsn.empty() && !result) {
            try {
                main_window.ProcessAppOpers(); // flush remaining operation(s)
                result = testResult.get();
            }
            catch (...) {
                auto msg = catchException(false);
                msg->TellExtra("while getting results from testscript");
                throw DmsException(msg);
            } 
        }
        main_window.CloseConfig();
        return result;
    }
    catch (...) {
        auto msg = catchException(false);
        // One rendering, the same one the dialog shows: the two labelled lines this used to print
        // both reported Why(), so a multi-line message -- the command-line syntax help in particular
        // -- arrived twice with nothing to tell the copies apart.
        std::cout << msg->GetAsText() << std::endl;
        QMessageBox::critical(nullptr, "GeoDMS Error",
            QString::fromUtf8(msg->GetAsText().c_str()));
    }
    return 9;
}

void ProcessRequestedCmdLineFeedback(char* argMsg) {
    auto exceptionText = DoubleUnQuoteMiddle(argMsg);
    QMessageBox::critical(nullptr, "GeoDMS Fatal Error",
        QString::fromStdString(std::string(exceptionText.c_str())));
}

#include "VersionComponent.h"
static VersionComponent s_QT("qt " QT_VERSION_STR);

#include "OperationContext.h"

int main1(int argc, char* argv[]) {

    DMS_SE_CALL_BEGIN

        return main_without_SE_handler(argc, argv);

    DMS_SE_CALL_END
}

int main(int argc, char* argv[]) {
#ifdef _WIN32
    // Lock the DLL search path before any LoadLibrary call (Qt plugin
    // discovery, GDAL drivers, RunDllProc) so a planted DLL in CWD or PATH
    // cannot hijack the process. LOAD_LIBRARY_SEARCH_DEFAULT_DIRS narrows
    // unflagged loads to <appdir> + AddDllDirectory-registered dirs + System32;
    // SetDllDirectoryW(L"") removes CWD from the legacy search order used by
    // older LoadLibrary call sites.
    ::SetDefaultDllDirectories(LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);
    ::SetDllDirectoryW(L"");

    // The "(Not Responding)" caption, the frosted overlay AND the rerouting of
    // input all belong to the DWM ghost HWND that replaces a window whose thread
    // hasn't retrieved messages for ~5s. During suspendible computation the main
    // thread polls GetQueueStatus (which the hang detector doesn't count) instead
    // of pumping, so a long calculation ghosts on the user's first interaction --
    // and from then on clicks land in the ghost's queue, starving the very
    // HasWaitingMessages() check that would make MustSuspend() yield (#1156).
    // Without ghosting, input keeps arriving in our real queue and the suspend
    // trigger answers it within its 1s timer tick. Irreversible per process.
    ::DisableProcessWindowsGhosting();
#else
    s_argc = argc;
    s_argv = argv;
#endif
    if ((argc > 1) && (argv[1][0] == '/') && (argv[1][1] == 'F')) {
        ProcessRequestedCmdLineFeedback(argv[1] + 2 );
        return 0;
    }

    int result;
    {
        tg_maintainer manageOperationContextTasks;
        result = main1(argc, argv);
    }
    DMS_Stg_Terminate();
    DMS_Rtc_Terminate();

    return result;
}
