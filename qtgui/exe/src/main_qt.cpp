#include <QAbstractNativeEventFilter>
#include <QApplication>
#include <QByteArray>
#include <QFontDatabase>
#include <QMouseEvent>
#include <QPainter>
#include <QResource>
#include <QScreen>
#include <QSplashScreen>
#include <QThread> // TODO: remove

#include <memory>

#include "RtcInterface.h"

#include "act/Any.h"
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
#include "DmsTreeView.h"
#include "DmsDetailPages.h"
#include "TestScript.h"

int RunTestScript(SharedStr testScriptName);


struct CmdLineException : SharedStr, std::exception
{
    CmdLineException(SharedStr x)
    :   SharedStr(x + 
            "\nexpected syntax:"
            "\nGeoDmsGuiQt.exe [options] [/LLogFile] [/TTestFile] configFileName.dms [item]"
        )
    ,   std::exception(c_str())
    {}
    CmdLineException(CharPtr x) : CmdLineException(SharedStr(x)) {}
};

std::any interpret_command_line_parameters(CmdLineSetttings& settingsFrame)
{
    int    argc = __argc;
    --argc;
    char** argv = __argv;
    ++argv;

    std::any result;

    if ((argc > 0) && (*argv)[0] == '/' && (*argv)[1] == 'L')
    {
        SharedStr dmsLogFileName = ConvertDosFileName(SharedStr((*argv) + 2));

        auto log_file_handle = std::make_unique<CDebugLog>(MakeAbsolutePath(dmsLogFileName.c_str()));
        result = make_noncopyable_any<decltype(log_file_handle)>(std::move(log_file_handle));

        argc--; argv++;
    }

    if ((argc > 0) && (*argv)[0] == '/' && (*argv)[1] == 'T')
    {
        settingsFrame.m_TestScriptName = ConvertDosFileName(SharedStr((*argv) + 2));
        argc--; argv++;
    }

    ParseRegStatusFlags(argc, argv);

    if (argc && (*argv)[0] == '/')
    {
        CharPtr cmd = (*argv) + 1;
        if (!stricmp(cmd, "noconfig"))
        {
            settingsFrame.m_NoConfig = true;
            argc--; argv++;
        }
    }
    if (argc)
    {
        if ((*argv)[0] == '/')
            throw CmdLineException("configuration name expected ");

        settingsFrame.m_ConfigFileName = SharedStr(*argv);
        argc--; argv++;
    }
    for (; argc; --argc, ++argv) {
        settingsFrame.m_CurrItemFullNames.emplace_back(SharedStr(*argv));
    }
    return result;
}

std::any init_geodms(QApplication& dms_app, CmdLineSetttings& settingsFrame) // TODO: move this logic to GeoDmsEngine class, ie separate gui logic from geodms engine logic.
{
    DMS_Shv_Load();
    SHV_SetAdminMode(true);
    auto exe_path = dms_app.applicationDirPath().toUtf8();
    DMS_Appl_SetExeDir(exe_path);
    DMS_Appl_SetFont();
    return interpret_command_line_parameters(settingsFrame);
}

#include "DmsMainWindow.h"
#include "DmsViewArea.h"
#include <qmdiarea.h>

class CustomEventFilter : public QAbstractNativeEventFilter
{
    //    Q_OBJECT
public:
    CustomEventFilter();
    ~CustomEventFilter();

    bool nativeEventFilter(const QByteArray& eventType, void* message, qintptr* result) override;
};

void SaveDetailPage(CharPtr fileName)
{
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

UInt32 Get4Bytes(const COPYDATASTRUCT* pcds, UInt32 i)
{
    if (pcds->cbData < (i + 1) * 4)
        return 0;
    auto uint32_ptr = reinterpret_cast<UInt32*>(pcds->lpData);
    return uint32_ptr[i];
}


bool WmCopyData(MSG* copyMsgPtr)
{
    auto pcds = reinterpret_cast<const COPYDATASTRUCT*>(copyMsgPtr->lParam);
    if (!pcds)
        return false;
    HWND hWindow = nullptr;
    DataView* dv = nullptr;
    auto commandCode = (CommandCode)pcds->dwData;
    switch (commandCode)
    {
    case CommandCode::SendApp: break; // send msg without HWND
    case CommandCode::SendMain: hWindow = (HWND)(MainWindow::TheOne()->winId()); break;
    case CommandCode::SendFocus: hWindow = GetFocus(); break;
    case CommandCode::SendActiveDmsControl:
    case CommandCode::WmCopyActiveDmsControl:
    {
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
    MSG msg;
    COPYDATASTRUCT cds2;
    if (commandCode < CommandCode::WmCopyActiveDmsControl)
    {
        msg.message = UINT(Get4Bytes(pcds, 0));
        msg.wParam  = WPARAM(Get4Bytes(pcds, 1));
        msg.lParam  = LPARAM(Get4Bytes(pcds, 2));
    }
    else { // code >= 4
        cds2.dwData = UINT(Get4Bytes(pcds, 0));
        cds2.cbData = (pcds->cbData >= 4) ?  pcds->cbData - 4 :0;
        cds2.lpData = (pcds->cbData >= 4) ? reinterpret_cast<UInt32*>(pcds->lpData) + 1 : nullptr;
        msg.message = WM_COPYDATA;
        msg.wParam  = WPARAM(MainWindow::TheOne()->winId());
        msg.lParam  = LPARAM(&cds2);
    }
    return SendMessage(hWindow, msg.message, msg.wParam, msg.lParam);
}

CustomEventFilter::CustomEventFilter()
{
     reportD(MsgCategory::other, SeverityTypeID::ST_MinorTrace, "Createt CustomEventFilter");
}

CustomEventFilter::~CustomEventFilter()
{
    reportD(MsgCategory::other, SeverityTypeID::ST_MinorTrace, "Destroy CustomEventFilter");
}



bool CustomEventFilter::nativeEventFilter(const QByteArray& /*eventType*/, void* message, qintptr* /*result*/ )
{
    MSG* msg = static_cast<MSG*>(message);

    switch (msg->message)
    {
    case WM_APP + 2:  // RegisterScaleChangeNotifications called in DmsViewArea.cpp, but this message is never received here
        if (auto mw = MainWindow::TheOne())
        {
            for (auto* sw : mw->m_mdi_area->subWindowList())
            {
                auto dms_sw = dynamic_cast<QDmsViewArea*>(sw);
                if (dms_sw)
                {
                    dms_sw->on_rescale();
                }
            }
        }
        return true; // Stop further processing of the message

    case WM_APP + 3:
        ProcessMainThreadOpers();
        return true;

    case WM_APP + 4:
    case WM_COPYDATA:
        if (msg->hwnd == (HWND)MainWindow::TheOne()->winId())
        {
            try {
                return WmCopyData(msg);
            }
            catch (...)
            {
                auto msg = catchException(false);
                auto userResult = MessageBoxA(nullptr, msg->GetAsText().c_str(), "exception in handling of WM_COPYDATA", MB_OKCANCEL | MB_ICONERROR | MB_SYSTEMMODAL);
                if (userResult == IDCANCEL)
                    terminate();
            }
        }
    /*case WM_PAINT:
    {
        RECT test_rect;
        auto main_window = (HWND)MainWindow::TheOne()->winId();
        GetUpdateRect(main_window, &test_rect, false);
    }*/
    }


    return false;
    //    return QAbstractNativeEventFilter::nativeEventFilter(eventType, message, result);
}

class DmsMouseForwardBackwardEventFilter : public QObject
{
protected:
    bool eventFilter(QObject* obj, QEvent* event) override
    {
        if (event->type() == QEvent::MouseButtonPress)
        {
            QMouseEvent* mouse_event = static_cast<QMouseEvent*>(event);
            switch (mouse_event->button())
            {
            case Qt::BackButton: { MainWindow::TheOne()->back(); return true; }
            case Qt::ForwardButton: { MainWindow::TheOne()->forward(); return true; }
            default: break;
            }
        }
        return false; // QObject::eventFilter(obj, event);
    }
};

#include <future>

class DmsSplashScreen : public QSplashScreen {
public:

    DmsSplashScreen(const QPixmap& pixmap)
        : QSplashScreen(pixmap)
    {
    }

    ~DmsSplashScreen() {}

    virtual void drawContents(QPainter* painter)
    {
        QPixmap textPix = QSplashScreen::pixmap();
        painter->setPen(this->m_color);
        painter->drawText(this->m_rect, this->m_alignment, this->m_message);
    }

    void showStatusMessage(const QString& message, const QColor& color = Qt::black)
    {
        this->m_message = message;
        this->m_color = color;
        this->showMessage(this->m_message, this->m_alignment, this->m_color);
    }

    void setMessageRect(QRect rect, int alignment = Qt::AlignLeft)
    {
        this->m_rect = rect;
        this->m_alignment = alignment;
    }

    QString m_message = DMS_GetVersion();
    int     m_alignment = Qt::AlignCenter;
    QColor  m_color;
    QRect   m_rect;
};

int main_without_SE_handler(int argc, char *argv[])
{
    try {
        CmdLineSetttings settingsFrame;
        garbage_t geoDmsResources; // destruct resources after app completion

        auto navive_event_filter_on_heap = std::make_unique<CustomEventFilter>();
        auto mouse_forward_backward_event_filter_on_heap = std::make_unique<DmsMouseForwardBackwardEventFilter>();

        auto dms_app_on_heap = std::make_unique<QApplication>(argc, argv);

        int id = QFontDatabase::addApplicationFont(":/res/fonts/dmstext.ttf");
        QString family = QFontDatabase::applicationFontFamilies(id).at(0);
        QFont dms_text_font(family, 25);

        QPixmap pixmap(":/res/images/ruralriver.jpg");
        std::unique_ptr<DmsSplashScreen> splash = std::make_unique<DmsSplashScreen>(pixmap);
        splash->setMessageRect(QRect(splash->rect().topLeft(), QSize(1024, 200)), Qt::AlignCenter);
        dms_text_font.setBold(true);
        splash->setFont(dms_text_font);
        
        auto screen_at_mouse_pos = dms_app_on_heap->screenAt(QCursor::pos());
        const QPoint currentDesktopsCenter = screen_at_mouse_pos->geometry().center();
        assert(splash->rect().top () == 0);
        assert(splash->rect().left() == 0);
        auto projectedTopLeft = currentDesktopsCenter - splash->rect().center();
        if (projectedTopLeft.y() < screen_at_mouse_pos->geometry().top())
                projectedTopLeft.setY( screen_at_mouse_pos->geometry().top() );
        splash->move(projectedTopLeft);
        splash->show();

        splash->showMessage("Initialize GeoDMS");
        geoDmsResources |= init_geodms(*dms_app_on_heap.get(), settingsFrame); // destruct resources after app completion
        dms_app_on_heap->installNativeEventFilter(navive_event_filter_on_heap.get());

        Q_INIT_RESOURCE(GeoDmsGuiQt);
        splash->showMessage("Initialize GeoDMS Gui");
        MainWindow main_window(settingsFrame);
        dms_app_on_heap->setWindowIcon(QIcon(":/res/images/GeoDmsGuiQt.png"));
        dms_app_on_heap->installEventFilter(mouse_forward_backward_event_filter_on_heap.get());

        QTimer::singleShot(10, [splashHandle = std::move(splash)]() { splashHandle->close(); });

        main_window.showMaximized();

        auto tsn = settingsFrame.m_TestScriptName;
        std::future<int> testResult;
        if (!tsn.empty())
        {
            testResult = std::async([tsn] { return RunTestScript(tsn); });
        }
        auto result = dms_app_on_heap->exec();

        if (!tsn.empty() && !result)
        {
            try
            {
                result = testResult.get();

            }
            catch (...)
            {
                auto msg = catchException(false);
                msg->TellExtra("while getting results from testscript");
                throw DmsException(msg);
            } 
        }
        main_window.CloseConfig();
        return result;
    }
    catch (...)
    {
        auto msg = catchException(false);
        std::cout << "error          : " << msg->Why() << std::endl;
        std::cout << "context        : " << msg->Why() << std::endl;
        MessageBoxA(nullptr, msg->GetAsText().c_str(), "GeoDms terminates due to an unexpected uncaught exception", MB_OK | MB_ICONERROR | MB_SYSTEMMODAL | MB_TASKMODAL);
    }
    return 9;
}

void ProcessRequestedCmdLineFeedback(char* argMsg)
{
    auto exceptionText = DoubleUnQuoteMiddle(argMsg);
    MessageBoxA(nullptr, exceptionText.c_str(), "GeoDmsQt teminates due to a fatal OS Structured Exception", MB_OK | MB_ICONERROR | MB_SYSTEMMODAL | MB_TASKMODAL);
}

int main(int argc, char* argv[])
{
    if ((argc > 1) && (argv[1][0] == '/') && (argv[1][1] == 'F'))
    {
        ProcessRequestedCmdLineFeedback(argv[1] + 2 );
        return 0;
    }

    DMS_SE_CALL_BEGIN

        return main_without_SE_handler(argc, argv);

    DMS_SE_CALL_END
}