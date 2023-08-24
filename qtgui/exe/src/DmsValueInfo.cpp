// Copyright (C) 2023 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#include "DmsMainWindow.h"
#include "DmsDetailPages.h"

#include "DmsValueInfo.h"
#include "ser/MoreStreamBuff.h"
#include "xml/XmlOut.h"

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

