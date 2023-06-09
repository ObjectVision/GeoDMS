#include <QApplication>
#include <QResource>
#include <QByteArray>
#include <memory>

#include "ShvDllInterface.h"
#include "act/Any.h"
#include "utl/Environment.h"
#include "utl/splitPath.h"
#include "dbg/DebugLog.h"
#include "ShvUtils.h"

#include "DmsMainWindow.h"

struct CmdLineException : std::exception
{
    using std::exception::exception; // inherit constructors
};

std::any interpret_command_line_parameters(CmdLineSetttings& settingsFrame)
{
    int    argc = __argc;
    --argc;
    char** argv = __argv;
    ++argv;

    std::any result;

    CharPtr firstParam = argv[0];
    if ((argc > 0) && firstParam[0] == '/' && firstParam[1] == 'L')
    {
        SharedStr dmsLogFileName = ConvertDosFileName(SharedStr(firstParam + 2));

        auto log_file_handle = std::make_unique<CDebugLog>(MakeAbsolutePath(dmsLogFileName.c_str())); //TODO: move to appropriate location, with lifetime of app
        result = make_noncopyable_any<decltype(log_file_handle)>(std::move(log_file_handle));

        SetCachedStatusFlag(RSF_TraceLogFile);
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
    DBG_INIT_COUT;
    SHV_SetAdminMode(true);
    auto exe_path = dms_app.applicationDirPath().toUtf8();
    DMS_Appl_SetExeDir(exe_path);
    return interpret_command_line_parameters(settingsFrame);
}

int main(int argc, char *argv[])
{
    try {
        CmdLineSetttings settingsFrame;
        std::any geoDmsResources; // destruct resources after app completion

        QApplication dms_app(argc, argv);
        geoDmsResources = init_geodms(dms_app, settingsFrame); // destruct resources after app completion

        Q_INIT_RESOURCE(GeoDmsGuiQt);
        MainWindow main_window(settingsFrame);
        dms_app.setWindowIcon(QIcon(":res/images/GeoDmsGui-0.png"));
        //main_window.setWindowIcon(QIcon(":res/images/GeoDmsGui-0.png"));
        main_window.showMaximized();
        return dms_app.exec();
    }
    catch (const CmdLineException& x)
    {
        std::cout << "cmd line error " << x.what() << std::endl;
        std::cout << "expected syntax:" << std::endl;
        std::cout << "GeoDmsGuiQt.exe [options] configFileName.dms [item]" << std::endl;
    }
}