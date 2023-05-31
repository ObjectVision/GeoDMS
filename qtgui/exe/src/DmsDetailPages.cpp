#include <QListWidget>

#include <QObject>
#include <QDockWidget>
#include <QTextBrowser>

#include "DmsDetailPages.h"
#include "DmsMainWindow.h"
#include <QMainWindow>

#include "dbg/SeverityType.h"
#include "ser/MoreStreamBuff.h"
#include "xml/XmlOut.h"
#include "TicInterface.h"

void DmsDetailPages::setActiveDetailPage(ActiveDetailPage new_active_detail_page)
{
    m_active_detail_page = new_active_detail_page;
}

void DmsDetailPages::newCurrentItem()
{
    if (m_active_detail_page != ActiveDetailPage::NONE)
        drawPage();
}

void DmsDetailPages::toggle(ActiveDetailPage new_active_detail_page)
{
    auto* detail_pages_dock = static_cast<QDockWidget*>(parent());
    if (detail_pages_dock->isHidden() || m_active_detail_page != new_active_detail_page)
    {
        detail_pages_dock->show();
        setActiveDetailPage(new_active_detail_page);
    }
    else
    {
        detail_pages_dock->hide();
        setActiveDetailPage(ActiveDetailPage::NONE);
    }

    drawPage();
}

void DmsDetailPages::toggleGeneral()
{
    toggle(ActiveDetailPage::GENERAL);
}

void DmsDetailPages::toggleExplorer()
{
    toggle(ActiveDetailPage::EXPLORE);
}

void DmsDetailPages::toggleProperties()
{
    toggle(ActiveDetailPage::PROPERTIES);
}

void DmsDetailPages::toggleConfiguration()
{
    toggle(ActiveDetailPage::CONFIGURATION);
}

void DmsDetailPages::drawPage()
{
    auto* current_item = static_cast<MainWindow*>(parent()->parent())->getCurrentTreeItem();
    if (!current_item)
        return;

    // stream general info for current_item to htm
    VectorOutStreamBuff buffer;
    auto xmlOut = std::unique_ptr<OutStreamBase>(XML_OutStream_Create(&buffer, OutStreamBase::ST_HTM, "", nullptr));
    bool result = true;
    bool showAll = true;
    switch (m_active_detail_page)
    {
    case ActiveDetailPage::GENERAL: 
        result = DMS_TreeItem_XML_DumpGeneral(current_item, xmlOut.get(), showAll);
        break;
    case ActiveDetailPage::PROPERTIES:
        result = DMS_TreeItem_XML_DumpAllProps(current_item, xmlOut.get(), showAll);
        break;
    case ActiveDetailPage::EXPLORE:
        DMS_TreeItem_XML_DumpExplore(current_item, xmlOut.get(), showAll);
        break;
    case ActiveDetailPage::CONFIGURATION:
        DMS_TreeItem_XML_Dump(current_item, xmlOut.get());
        break;
    }
    if (result)
    {
        buffer.WriteByte(0); // std::ends
        // set buff to detail page:
        setHtml(buffer.GetData());
    }
}

void DmsDetailPages::onAnchorClicked(const QUrl& link)
{
    auto link_string = link.toString().toUtf8();

    // log link action
    MainWindow::EventLog(SeverityTypeID::ST_MajorTrace, link_string.data());
}

void DmsDetailPages::connectDetailPagesAnchorClicked()
{
    setOpenLinks(false);
    setOpenExternalLinks(false);
    connect(this, &QTextBrowser::anchorClicked, this, &DmsDetailPages::onAnchorClicked);
}
