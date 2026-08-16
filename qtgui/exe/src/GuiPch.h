// Copyright (C) 1998-2026 Object Vision B.V.
// License: GNU GPL 3
/////////////////////////////////////////////

#if defined(_MSC_VER)
#pragma once
#endif

/*
 *  The precompiled header of GeoDmsGuiQt: the Qt widget/event headers that
 *  recur across the GUI TUs plus the stable GeoDMS C interfaces. Injected
 *  into every TU via ForcedIncludeFiles (msbuild) so the moc-generated
 *  TUs are covered too; CMake handles that via target_precompile_headers.
 */

#if !defined(__QTGUI_GUIPCH_H)
#define __QTGUI_GUIPCH_H

#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDialog>
#include <QDockWidget>
#include <QEvent>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QMainWindow>
#include <QMenu>
#include <QMenuBar>
#include <QObject>
#include <QPointer>
#include <QPushButton>
#include <QTextBrowser>
#include <QTextEdit>
#include <QTimer>
#include <QToolBar>
#include <QTreeView>
#include <QVBoxLayout>
#include <QWidget>

#include "RtcInterface.h"
#include "TicInterface.h"
#include "ShvDllInterface.h"

#endif // __QTGUI_GUIPCH_H
