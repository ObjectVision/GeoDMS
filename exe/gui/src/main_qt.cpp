// Copyright (C) 2016 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR BSD-3-Clause
#include <QApplication>
#include <QResource>
#include "ShvDllInterface.h"

#include "mainwindow.h"

int init_geodms()
{

    return 0;
}

int main(int argc, char *argv[])
{
    DMS_Shv_Load();
    DBG_INIT_COUT;

    QApplication app(argc, argv);
    Q_INIT_RESOURCE(GeoDmsGuiQt);
    MainWindow main_window;
    main_window.setWindowIcon(QIcon(":res/images/GeoDmsGui-0.png"));

    //main_window.setWindowState(Qt::WindowMaximized);
    main_window.showMaximized();
    return app.exec();
}