#include <QApplication>
#include <QResource>
#include <QByteArray>
#include <memory>
#include "ShvDllInterface.h"
#include "utl/Environment.h"
#include "utl/splitPath.h"
#include "dbg/DebugLog.h"
#include "ShvUtils.h"

#include "DmsMainWindow.h"

void interpret_command_line_parameters()
{
    int    argc = __argc;
    --argc;
    char** argv = __argv;
    ++argv;

    CharPtr firstParam = argv[0];
    if ((argc > 0) && firstParam[0] == '/' && firstParam[1] == 'L')
    {
        SharedStr dmsLogFileName = ConvertDosFileName(SharedStr(firstParam + 2));

        auto log_location = std::make_unique<CDebugLog>(MakeAbsolutePath(dmsLogFileName.c_str())); //TODO: move to appropriate location, with lifetime of app
        SetCachedStatusFlag(RSF_TraceLogFile);
        argc--; argv++;
    }

    ParseRegStatusFlags(argc, argv);

    for (; argc; --argc, ++argv) {
        if ((*argv)[0] == '/')
        {
            CharPtr cmd = (*argv) + 1;
            //if (!stricmp(cmd, "noconfig")) TODO: implement noconfig
                //m_NoConfig = true;
        }
    }
}

int init_geodms(QApplication& dms_app) // TODO: move this logic to GeoDmsEngine class, ie separate gui logic from geodms engine logic.
{
    DMS_Shv_Load();
    DBG_INIT_COUT;
    SHV_SetAdminMode(true);
    auto exe_path = dms_app.applicationDirPath().toUtf8();
    DMS_Appl_SetExeDir(exe_path);
    interpret_command_line_parameters();
    
    return 0;
}

int main(int argc, char *argv[])
{
    QApplication dms_app(argc, argv);
    init_geodms(dms_app);

    
    Q_INIT_RESOURCE(GeoDmsGuiQt);
    MainWindow main_window;
    main_window.setWindowIcon(QIcon(":res/images/GeoDmsGui-0.png"));
    main_window.showMaximized();
    return dms_app.exec();
}