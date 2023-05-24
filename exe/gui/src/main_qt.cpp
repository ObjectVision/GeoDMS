// Copyright (C) 2016 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR BSD-3-Clause
#include <QApplication>
#include <QResource>
#include <QByteArray>
#include <memory>
#include "ShvDllInterface.h"
#include "utl/Environment.h"
#include "utl/splitPath.h"
#include "dbg/DebugLog.h"

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

int init_geodms()
{
    //std::string exePath = GetExeFilePath();
    //DMS_Appl_SetExeDir(exePath.c_str()); // sets MainThread id
    //QByteArray test(" ");
    //auto exe_path = QCoreApplication::applicationDirPath().toUtf8();
    
    //interpret_command_line_parameters();
    return 0;
}

int main(int argc, char *argv[])
{
    //DMS_Shv_Load();
    //DBG_INIT_COUT;

    QApplication app(argc, argv);
    Q_INIT_RESOURCE(GeoDmsGuiQt);
    MainWindow main_window;
    main_window.setWindowIcon(QIcon(":res/images/GeoDmsGui-0.png"));
    main_window.showMaximized();
    return app.exec();
}