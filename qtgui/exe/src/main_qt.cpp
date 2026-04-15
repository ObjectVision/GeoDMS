#include <QAbstractNativeEventFilter>
#include <QApplication>
#include <QByteArray>
#include <QFontDatabase>
#include <QMouseEvent>
#include <QPainter>
#include <QResource>
#include <QMessageBox>
#include <QScreen>
#include <QThread> // TODO: remove

#include <memory>
#include <iostream>
#include <stdexcept>

#include "RtcInterface.h"

#include "act/garbage_can.h"
#include "dbg/DebugLog.h"
#include "dbg/DmsCatch.h"
#include "utl/Environment.h"
#include "utl/splitPath.h"
#include "xct/DmsException.h"
#include "xct/ErrMsg.h"

#include "stg/AbstrStorageManager.h"
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
            "\nGeoDmsGuiQt.exe [options] [/LLogFile] [/TTestFile] configFileName.dms [item]"
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
    if (argc && (*argv)[0] == '/') {
        CharPtr cmd = (*argv) + 1;
        if (!stricmp(cmd, "noconfig")) {
            settingsFrame.m_NoConfig = true;
            argc--; argv++;
        }
    }
    if (argc) {
#ifdef _WIN32
        if ((*argv)[0] == '/')
            throw CmdLineException("configuration name expected ");
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
    DMS_Shv_Load();
    SHV_SetAdminMode(true);
    auto exe_path = dms_app.applicationDirPath().toUtf8();
    DMS_Appl_SetExeDir(exe_path);
    DMS_Appl_SetFont();
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

    // TODO: implement SaveDetailPage for QWebEngineView
    /*auto htmlSource = MainWindow::TheOne()->m_detail_pages->toHtml();
    auto htmlsourceAsUtf8 = htmlSource.toUtf8();

    reportF(MsgCategory::commands, SeverityTypeID::ST_MajorTrace, "SaveDetailPage %s", DoubleQuote(expandedFilename.c_str()));

    FileOutStreamBuff buff(SharedStr(expandedFilename), nullptr, true, false);
   
    buff.WriteBytes(htmlsourceAsUtf8.data(), htmlsourceAsUtf8.size());*/
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
//        dmfGeneral.miDatagridView.Click;
        return true;

    case CommandCode::miHistogramView:
//        dmfGeneral.miHistogramView.Click;
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
            switch (mouse_event->button()) {
            case Qt::BackButton: { MainWindow::TheOne()->back(); return true; }
            case Qt::ForwardButton: { MainWindow::TheOne()->forward(); return true; }
            default: break;
            }
        }
        return false; // QObject::eventFilter(obj, event);
    }
};


int main_without_SE_handler(int argc, char *argv[]) {
    try {
#ifdef Q_OS_WIN
        qputenv("QT_QPA_PLATFORM", "windows:darkmode=1"); // https://doc.qt.io/qt-6/qguiapplication.html#platform-specific-arguments
#endif
        CmdLineSetttings settingsFrame;
        garbage_can geoDmsResources; // destruct resources after app completion

        auto native_event_filter_on_heap = std::make_unique<CustomEventFilter>();
        auto mouse_forward_backward_event_filter_on_heap = std::make_unique<DmsMouseForwardBackwardEventFilter>();

        auto dms_app_on_heap = std::make_unique<QApplication>(argc, argv);
        geoDmsResources |= init_geodms(*dms_app_on_heap.get(), settingsFrame); // destruct resources after app completion
        dms_app_on_heap->installNativeEventFilter(native_event_filter_on_heap.get());

        SharedStr tsn = settingsFrame.m_TestScriptName;

        Q_INIT_RESOURCE(GeoDmsGuiQt);

        std::unique_ptr<DmsSplashScreen> splash;
        if (tsn.empty())
            splash = showSplashScreen();

        if (tsn.empty())
            splash->showMessage("Initialize GeoDMS Gui");


        MainWindow main_window(settingsFrame);
        dms_app_on_heap->setWindowIcon(QIcon(":/res/images/GeoDmsGuiQt.png"));
        dms_app_on_heap->installEventFilter(mouse_forward_backward_event_filter_on_heap.get());

        std::future<int> testResult;
        bool mustTerminateToken = false;

        if (tsn.empty())
            QTimer::singleShot(1000, &main_window,
                [splashHandle = std::move(splash), &main_window]()
                { 
                    main_window.showMaximized();
                    splashHandle->close();
                    ConfirmMainThreadOperProcessing();
                }
            );

        else
        {
#ifdef Q_OS_WIN
            main_window.show(); // show it without maximizing yet
            HWND hwnd = (HWND)main_window.winId();
            ShowWindow(hwnd, SW_SHOWMAXIMIZED);
            SetWindowPos(hwnd, HWND_BOTTOM, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_SHOWWINDOW);
#else
            main_window.setWindowState(Qt::WindowMaximized);
            main_window.show();
#endif
            ConfirmMainThreadOperProcessing();

            main_window.PostAppOper([tsn, &testResult, &mustTerminateToken]
                {
                    testResult = std::async([tsn, &mustTerminateToken]
                        { 
                            return RunTestScript(tsn, &mustTerminateToken);
                        }
                    );
                }
            );
        }

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
        std::cout << "error          : " << msg->Why() << std::endl;
        std::cout << "context        : " << msg->Why() << std::endl;
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
#ifndef _WIN32
    s_argc = argc;
    s_argv = argv;
#endif
    if ((argc > 1) && (argv[1][0] == '/') && (argv[1][1] == 'F')) {
        ProcessRequestedCmdLineFeedback(argv[1] + 2 );
        return 0;
    }

    tg_maintainer manageOperationContextTasks;

    auto result = main1(argc, argv);

    return result;
}