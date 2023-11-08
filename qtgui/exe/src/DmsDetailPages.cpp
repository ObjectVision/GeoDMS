// Copyright (C) 2023 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#include <QApplication>
#include <QListWidget>

#include <QObject>
#include <QDockWidget>
#include <QTextBrowser>
#include <QTimer>
#include <Qclipboard.h>

#include "DmsMainWindow.h"
#include "DmsDetailPages.h"
#include "DmsValueInfo.h"
#include "DmsEventLog.h"
#include "dbg/DmsCatch.h"
#include <QMainWindow>

#include "dbg/SeverityType.h"
#include "ptr/OwningPtrSizedArray.h"
#include "ser/FileStreamBuff.h"
#include "ser/MoreStreamBuff.h"
#include "utl/Encodes.h"
#include "utl/Environment.h"
#include "xml/XmlOut.h"

#include "FilePtrHandle.h"

#include "AbstrDataItem.h"
#include "Explain.h"
#include "DataStoreManagerCaller.h"
#include "TicInterface.h"
#include "TreeItemProps.h"

#include "StxInterface.h"

#include "dms_memory_leak_debugging.h"
#ifdef _DEBUG
#define new MYDEBUG_NEW 
#endif

void DmsDetailPages::setActiveDetailPage(ActiveDetailPage new_active_detail_page)
{
    reportF(MsgCategory::commands, SeverityTypeID::ST_MinorTrace, "ShowDetailPage %d", int(new_active_detail_page));
    m_last_active_detail_page = m_active_detail_page;
    m_active_detail_page = new_active_detail_page;
}

void DmsDetailPages::leaveThisConfig() // reset ValueInfo cached results
{
}

void DmsDetailPages::newCurrentItem()
{
    if (m_active_detail_page != ActiveDetailPage::NONE)
        scheduleDrawPage();
}

auto getDetailPagesActionFromActiveDetailPage(ActiveDetailPage new_active_detail_page) -> QAction*
{
    auto main_window = MainWindow::TheOne();
    switch (new_active_detail_page)
    {
    case ActiveDetailPage::GENERAL: { return main_window->m_general_page_action.get(); }
    case ActiveDetailPage::EXPLORE: { return main_window->m_explore_page_action.get(); }
    case ActiveDetailPage::PROPERTIES: { return main_window->m_properties_page_action.get(); }
    case ActiveDetailPage::METADATA: { return main_window->m_metainfo_page_action.get(); }
    case ActiveDetailPage::CONFIGURATION: { return main_window->m_configuration_page_action.get(); }
    case ActiveDetailPage::SOURCEDESCR: { return main_window->m_sourcedescr_page_action.get(); }
    case ActiveDetailPage::NONE:
    default: { return nullptr; };
    }
}

void DmsDetailPages::toggleVisualState(ActiveDetailPage new_active_detail_page, bool toggle)
{
    for (int page = ActiveDetailPage::GENERAL; page != ActiveDetailPage::NONE; page++)
    {
        auto page_action = getDetailPagesActionFromActiveDetailPage((ActiveDetailPage)page);
        if (!page_action)
            continue;

        if (page != new_active_detail_page)
            page_action->setChecked(false);
        else
            page_action->setChecked(toggle);
    }
}

void DmsDetailPages::show(ActiveDetailPage new_active_detail_page)
{
    MainWindow::TheOne()->m_detailpages_dock->setVisible(true);
    toggleVisualState(new_active_detail_page, true);
    setActiveDetailPage(new_active_detail_page);
    scheduleDrawPage();
}

void DmsDetailPages::toggle(ActiveDetailPage new_active_detail_page)
{
    if (!MainWindow::TheOne()->m_detailpages_dock->isVisible() || m_active_detail_page != new_active_detail_page || !isVisible())
    {
        setActiveDetailPage(new_active_detail_page);
        show(new_active_detail_page);
    }
    else
    {
        MainWindow::TheOne()->m_detailpages_dock->setVisible(false);
        toggleVisualState(new_active_detail_page, false);
        setActiveDetailPage(ActiveDetailPage::NONE);
    }

    scheduleDrawPage();
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

void DmsDetailPages::toggleSourceDescr()
{
    toggle(ActiveDetailPage::SOURCEDESCR);
}

void DmsDetailPages::toggleMetaInfo()
{
    toggle(ActiveDetailPage::METADATA);
}

void DmsDetailPages::propertiesButtonToggled(QAbstractButton* button, bool checked)
{
    if (!checked)
        return;

    auto main_window = MainWindow::TheOne();
    if (main_window->m_detail_page_properties_buttons->pr_nondefault == button) // non default
        m_AllProperties = false;

    if (main_window->m_detail_page_properties_buttons->pr_all == button) // all
        m_AllProperties = true;

    scheduleDrawPageImpl(0);
}

void DmsDetailPages::sourceDescriptionButtonToggled(QAbstractButton* button, bool checked)
{
    if (!checked)
        return;

    auto main_window = MainWindow::TheOne();
    if (main_window->m_detail_page_source_description_buttons->sd_readonly == button) // read only
        m_SDM = SourceDescrMode::ReadOnly;

    if (main_window->m_detail_page_source_description_buttons->sd_configured == button) // configured
        m_SDM = SourceDescrMode::Configured;

    if (main_window->m_detail_page_source_description_buttons->sd_nonreadonly == button) // non read only
        m_SDM = SourceDescrMode::WriteOnly;

    if (main_window->m_detail_page_source_description_buttons->sd_all == button) // all
        m_SDM = SourceDescrMode::All;

    scheduleDrawPageImpl(0);
}

auto htmlEncodeTextDoc(CharPtr str) -> SharedStr
{
    VectorOutStreamBuff outBuff;
    outBuff.OutStreamBuff::WriteBytes("<HTML><BODY><PRE>");

    auto xmlOut = XML_OutStream_Create(&outBuff, OutStreamBase::SyntaxType::ST_HTM, "", nullptr);

    XML_OutStream_WriteText(xmlOut, str);

    outBuff.OutStreamBuff::WriteBytes("</PRE></BODY></HTML>");
    return SharedStr( outBuff.GetData(), outBuff.GetDataEnd());
}

void DmsDetailPages::scheduleDrawPageImpl(int milliseconds)
{
    bool oldValue = false;
    if (m_DrawPageRequestPending.compare_exchange_strong(oldValue, true))
    {
        assert(m_DrawPageRequestPending);
        AddMainThreadOper(
            [this, milliseconds]()
            {
                assert(IsMainThread());
                static std::time_t pendingDeadline = 0;
                auto deadline = std::time(nullptr) + milliseconds / 1000;
                if (!pendingDeadline || deadline < pendingDeadline)
                {
                    pendingDeadline = deadline;
                    QTimer::singleShot(milliseconds, [this]()
                        {
                            assert(IsMainThread());
                            pendingDeadline = 0;

                            bool oldValue = true;
                            if (m_DrawPageRequestPending.compare_exchange_strong(oldValue, false))
                            {
                                this->drawPage();
                            }
                        }
                    );
                }
            }
        );
    }
}

void DmsDetailPages::scheduleDrawPage()
{
    scheduleDrawPageImpl(10);
}

void DmsDetailPages::onTreeItemStateChange()
{
    if (m_active_detail_page != ActiveDetailPage::GENERAL
    &&  m_active_detail_page != ActiveDetailPage::PROPERTIES)
        return;

    scheduleDrawPageImpl(1000);
}

SharedStr FindURL(const TreeItem* ti)
{
    while (ti)
    {
        auto result = TreeItemPropertyValue(ti, urlPropDefPtr);
        if (!result.empty())
            return result;
        ti = ti->GetTreeParent();
    }
    return {};
}

void DmsDetailPages::drawPage()
{
    if (!MainWindow::IsExisting())
        return;

    auto main_window = MainWindow::TheOne();
    auto* current_item = MainWindow::TheOne()->getCurrentTreeItem();
    if (!current_item)
        return;

    bool ready = true;
    SuspendTrigger::Resume();

    // stream general info for current_item to htm
    VectorOutStreamBuff buffer;
    auto streamType = m_active_detail_page == ActiveDetailPage::CONFIGURATION ? OutStreamBase::ST_DMS : OutStreamBase::ST_HTM;
    auto xmlOut = std::unique_ptr<OutStreamBase>(XML_OutStream_Create(&buffer, streamType, "", calcRulePropDefPtr));
    bool result = true;
    bool showAll = true;
    main_window->hideDetailPagesRadioButtonWidgets(true, true);
    switch (m_active_detail_page)
    {
    case ActiveDetailPage::GENERAL:
        result = TreeItem_XML_DumpGeneral(current_item, xmlOut.get());
        break;
    case ActiveDetailPage::PROPERTIES:
        main_window->hideDetailPagesRadioButtonWidgets(false, true);
        result = DMS_TreeItem_XML_DumpAllProps(current_item, xmlOut.get(), m_AllProperties);
        break;
    case ActiveDetailPage::EXPLORE:
        DMS_TreeItem_XML_DumpExplore(current_item, xmlOut.get(), showAll);
        break;
    case ActiveDetailPage::CONFIGURATION:
    {
        DMS_TreeItem_XML_Dump(current_item, xmlOut.get());
        break;
    }
    case ActiveDetailPage::SOURCEDESCR:
    {
        //(*xmlOut) << TreeItem_GetSourceDescr(current_item, m_SDM, true).c_str();
        main_window->hideDetailPagesRadioButtonWidgets(true, false);
        TreeItem_XML_DumpSourceDescription(current_item, m_SDM, xmlOut.get());
        break;
    }
    case ActiveDetailPage::METADATA:
    {
        SharedStr url = {};
        try
        {
            url = FindURL(current_item);
        }
        catch (...)
        {
            auto errMsg = catchException(false);
            MainWindow::TheOne()->reportErrorAndTryReload(errMsg);
        }

        if (!url.empty())
        {
            if (main_window->ShowInDetailPage(url))
            {
                FilePtrHandle file;
                auto sfwa = DSM::GetSafeFileWriterArray();
                if (!sfwa)
                    return;
                if (file.OpenFH(url, sfwa.get(), FCM_OpenReadOnly, false, NR_PAGES_DIRECTIO))
                {
                    dms::filesize_t fileSize = file.GetFileSize();
                    OwningPtrSizedArray<char> dataBuffer(fileSize + 1, dont_initialize MG_DEBUG_ALLOCATOR_SRC("METADATA"));
                    fread(dataBuffer.begin(), fileSize, 1, file);
                    dataBuffer[fileSize] = char(0);
                    setHtml(dataBuffer.begin());
                }
                return;
            }
        }
        XML_MetaInfoRef(current_item, xmlOut.get());
        break;
    }
    }
    if (result)
    {
        buffer.WriteByte(0); // std::ends
        // set buff to detail page:
        CharPtr contents = buffer.GetData();
        if (m_active_detail_page != ActiveDetailPage::CONFIGURATION)
            setHtml(contents);
        else
            setHtml( htmlEncodeTextDoc(contents).c_str() );
    }
    if (!ready)
        scheduleDrawPageImpl(500);
}

enum class ViewActionType {
    Execute,
    PopupTable,
    EditPropValue,
//    MenuItem,
    DetailPage,
    Url
};

auto DmsDetailPages::activeDetailPageFromName(CharPtrRange sName) -> ActiveDetailPage
{
    if (sName.size() >=3 && !strncmp(sName.begin(), "vi.attr", 3)) return ActiveDetailPage::VALUE_INFO;
    if (!strncmp(sName.begin(), "stat", sName.size())) return ActiveDetailPage::STATISTICS;
    if (!strncmp(sName.begin(), "config", sName.size())) return ActiveDetailPage::CONFIGURATION;
    if (!strncmp(sName.begin(), "general", sName.size())) return ActiveDetailPage::GENERAL;
    if (!strncmp(sName.begin(), "properties", sName.size())) return ActiveDetailPage::PROPERTIES;
    if (!strncmp(sName.begin(), "sourcedescr", sName.size())) return ActiveDetailPage::SOURCEDESCR;
    if (!strncmp(sName.begin(), "metadata", sName.size())) return ActiveDetailPage::METADATA;
    if (!strncmp(sName.begin(), "explore", sName.size())) return ActiveDetailPage::EXPLORE;
    return ActiveDetailPage::NONE;
}

DmsDetailPages::DmsDetailPages(QWidget* parent)
    : QUpdatableTextBrowser(parent)
{
    setOpenLinks(false);
    setOpenExternalLinks(false);
    setProperty("DmsHelperWindowType", DmsHelperWindowType::HW_DETAILPAGES);
    connect(this, &QTextBrowser::anchorClicked, this, &DmsDetailPages::onAnchorClicked);
}

bool DmsDetailPages::update()
{
    return true;
}

QSize DmsDetailPages::sizeHint() const
{
    return QSize(m_default_width, 0);
}

QSize DmsDetailPages::minimumSizeHint() const
{
    return QSize(m_default_width, 0);
}

void DmsDetailPages::resizeEvent(QResizeEvent* event)
{
    m_current_width = width();
    QTextBrowser::resizeEvent(event);
}

#include <QDesktopServices>
void DmsDetailPages::onAnchorClicked(const QUrl& link)
{
    MainWindow::TheOne()->onInternalLinkClick(link);
}
