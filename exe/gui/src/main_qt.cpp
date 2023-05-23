// Copyright (C) 2016 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR BSD-3-Clause

#include <QApplication>
#include <QResource>

#include "mainwindow.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    Q_INIT_RESOURCE(GeoDmsGuiQt);
    MainWindow main_window;
    //main_window.setWindowState(Qt::WindowMaximized);
    main_window.showMaximized();
    return app.exec();
}