#include <QListWidget>

#include <QObject>
#include <QDockWidget>
#include <QTextBrowser>

#include "DmsDetailPages.h"
#include "DmsMainWindow.h"
#include <QMainWindow>

#include "ser/MoreStreamBuff.h"
#include "xml/XmlOut.h"
#include "TicInterface.h"

void DmsDetailPages::setActiveDetailPage(ActiveDetailPage new_active_detail_page)
{
    m_active_detail_page = new_active_detail_page;
}

void DmsDetailPages::newCurrentItem()
{
    if (m_active_detail_page==ActiveDetailPage::GENERAL)
        drawGeneralPage();
}

void DmsDetailPages::toggleGeneral()
{
    auto* detail_pages_dock = static_cast<QDockWidget*>(parent());
    if (detail_pages_dock->isHidden())
    {
        detail_pages_dock->show();
        setActiveDetailPage(ActiveDetailPage::GENERAL);
    }
    else
    {
        detail_pages_dock->hide();
        setActiveDetailPage(ActiveDetailPage::NONE);
    }

    drawGeneralPage();
}

void DmsDetailPages::drawGeneralPage()
{
    auto* current_item = static_cast<MainWindow*>(parent()->parent())->getCurrentTreeitem();
    if (!current_item)
        return;

    // stream general info for current_item to htm
    VectorOutStreamBuff buffer;
    auto xmlOut = std::unique_ptr<OutStreamBase>(XML_OutStream_Create(&buffer, OutStreamBase::ST_HTM, "", nullptr));
    bool result = DMS_TreeItem_XML_DumpGeneral(current_item, xmlOut.get(), true);
    buffer.WriteByte(0); // std::ends
    if (result)
    {
        // set buff to detail page:
        setHtml(buffer.GetData());
    }
}

void DmsDetailPages::onAnchorClicked(const QUrl& link)
{
    auto link_string = link.toString().toUtf8();
    int i = 0;
}

void DmsDetailPages::connectDetailPagesAnchorClicked()
{
    setOpenLinks(false);
    setOpenExternalLinks(false);
    connect(this, &QTextBrowser::anchorClicked, this, &DmsDetailPages::onAnchorClicked);
}
