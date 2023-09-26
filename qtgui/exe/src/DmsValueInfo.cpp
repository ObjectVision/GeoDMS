// Copyright (C) 2023 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#include "DmsMainWindow.h"
#include "DmsDetailPages.h"
#include "DmsViewArea.h"

#include "DmsValueInfo.h"
#include "ser/MoreStreamBuff.h"
#include "xml/XmlOut.h"

#include <QDockWidget>

ValueInfoPanel::ValueInfoPanel(QWidget* parent)
    : QMdiSubWindow(parent)
{

}

ValueInfoPanel::~ValueInfoPanel()
{
    auto main_window = MainWindow::TheOne();
    if (!main_window) // main window destructor might already be in session as its base class destructor calls for childWidget destruction
        return;
    auto active_subwindow = main_window->m_value_info_mdi_area->activeSubWindow();
    if (!active_subwindow)
    {
        main_window->m_value_info_dock->setVisible(false);
        main_window->resizeDocks({ main_window->m_detailpages_dock }, { main_window->m_detail_pages->m_default_width}, Qt::Horizontal);
        //main_window->m_detailpages_dock->setVisible(true);
    }
    main_window->resizeDocksToNaturalSize();
    //main_window->resizeDocks({main_window->m_detailpages_dock}, {500, 0}, Qt::Horizontal);
}

QSize ValueInfoPanel::sizeHint() const
{
    return QSize(500,0);
}

QSize ValueInfoPanel::minimumSizeHint() const
{
    return QSize(500, 0);
}

ValueInfoBrowser::ValueInfoBrowser(QWidget* parent, SharedDataItemInterestPtr studyObject, SizeT index)
    : QUpdatableTextBrowser(parent)
    , m_Context(Explain::CreateContext())
    , m_StudyObject(studyObject)
    , m_Index(index)
{
    setOpenLinks(false);
    setOpenExternalLinks(false);
    connect(this, &QTextBrowser::anchorClicked, MainWindow::TheOne()->m_detail_pages, &DmsDetailPages::onAnchorClicked);
}

bool ValueInfoBrowser::update()
{
    vos_buffer_type buffer;

    ExternalVectorOutStreamBuff outStreamBuff(buffer);

    auto xmlOut = OutStream_HTM(&outStreamBuff, "html", nullptr);
    SuspendTrigger::Resume();

    bool done = Explain::AttrValueToXML(m_Context.get(), m_StudyObject, &xmlOut, m_Index, "", true);
    outStreamBuff.WriteByte(char(0));

    setText(outStreamBuff.GetData());

    // clean-up;
    return done;
}

