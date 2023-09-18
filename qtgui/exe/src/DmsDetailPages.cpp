// Copyright (C) 2023 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#include <QListWidget>

#include <QObject>
#include <QDockWidget>
#include <QTextBrowser>
#include <QTimer>

#include "DmsDetailPages.h"
#include "DmsMainWindow.h"
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

auto Realm(const auto& x) -> CharPtrRange
{
    auto colonPos = std::find(x.begin(), x.end(), ':');
    if (colonPos == x.end())
        return {};
    return { x.begin(), colonPos };
}

bool ShowInDetailPage(auto x)
{
    auto realm = Realm(x);
    if (realm.size() == 3 && !strncmp(realm.begin(), "dms", 3))
        return true;
    if (!realm.empty())
        return false;
    CharPtrRange knownSuffix(".adms");
    return std::search(x.begin(), x.end(), knownSuffix.begin(), knownSuffix.end()) != x.end();
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
            if (ShowInDetailPage(url))
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

bool IsPostRequest(const QUrl& /*link*/)
{
    return false;
}

enum class ViewActionType {
    Execute,
    PopupTable,
    EditPropValue,
//    MenuItem,
    DetailPage,
    Url
};

void PopupTable(SizeT /*recNo*/)
//var diCode, diLabel, diAction: TAbstrDataItem;
//rlCode, rlLabel, rlAction: TDataReadLock;
//pm: TPopupMenu;
//mi: TMenuItem;
//i, n: SizeT;
{
/*
    Assert(m_oSender is TFrame);
    diCode: = DMS_TreeItem_GetItem(m_tiFocus, 'code', DMS_AbstrDataItem_GetStaticClass);
    diLabel: = DMS_TreeItem_GetItem(m_tiFocus, 'label', DMS_AbstrDataItem_GetStaticClass);
    diAction: = DMS_TreeItem_GetItem(m_tiFocus, 'action', DMS_AbstrDataItem_GetStaticClass);

    rlCode: = DMS_DataReadLock_Create(diCode, true); try
    rlLabel : = DMS_DataReadLock_Create(diLabel, true); try
    rlAction: = DMS_DataReadLock_Create(diAction, true); try
    if (not Assigned(TFrame(m_oSender).PopupMenu)) then
    TFrame(m_oSender).PopupMenu : = TPopupMenu.Create(TFrame(m_oSender));
    pm: = TFrame(m_oSender).PopupMenu;
    pm.Items.Clear;
    pm.Items.Tag : = Integer(diAction);
    n: = DMS_Unit_GetCount(DMS_DataItem_GetDomainUnit(diCode));
    i: = 0; while i <> n do
    begin
    if DMS_NumericAttr_GetValueAsInt32(diCode, i) = m_RecNo then
    begin
    mi : = TMenuItem.Create(pm);
    mi.Caption : = AnyDataItem_GetValueAsString(diLabel, i);
    mi.OnClick : = PopupAction;
    mi.Tag     : = i;
    pm.Items.Add(mi);
    end;
    INC(i);
    end;
    pm.Popup(m_X, m_Y);
    finally DMS_DataReadLock_Release(rlAction); end
    finally DMS_DataReadLock_Release(rlLabel); end
    finally DMS_DataReadLock_Release(rlCode); end
*/
}

void EditPropValue(TreeItem* /*tiContext*/, CharPtrRange /*url*/, SizeT /*recNo*/)
{

}

auto dp_FromName(CharPtrRange sName) -> ActiveDetailPage
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

/*bool DmsDetailPages::event(QEvent* event)
{
    if (event->type() == QEvent::Resize)
    {
        int i = 0;
    }

    return QTextBrowser::event(event);
}*/

DmsDetailPages::DmsDetailPages(QWidget* parent)
    : QTextBrowser(parent)
{}

QSize DmsDetailPages::sizeHint() const
{
    return QSize(500, 0);
}

void DmsDetailPages::resizeEvent(QResizeEvent* event)
{
    m_current_width = width();
    QTextBrowser::resizeEvent(event);
}

void DmsDetailPages::DoViewAction(TreeItem* tiContext, CharPtrRange sAction)
{
    assert(tiContext);
    SuspendTrigger::Resume();

    auto colonPos = std::find(sAction.begin(), sAction.end(), ':');
    auto sMenu = CharPtrRange(sAction.begin(), colonPos);
    auto sPathWithSub = CharPtrRange(colonPos == sAction.end() ? colonPos : colonPos + 1, sAction.end());

    auto queryPos = std::find(sPathWithSub.begin(), sPathWithSub.end(), '?');
    auto sPath = CharPtrRange(sPathWithSub.begin(), queryPos);
    auto sSub  = CharPtrRange(queryPos == sPathWithSub.end() ? queryPos : queryPos + 1, sPathWithSub.end());

//    DMS_TreeItem_RegisterStateChangeNotification(OnTreeItemChanged, m_tiFocus, TClientHandle(self)); // Resource aquisition which must be matched by a call to LooseFocus

    auto separatorPos = std::find(sMenu.begin(), sMenu.end(), '!');
//    MakeMin(separatorPos, std::find(sMenu.begin(), sMenu.end(), '#')); // TODO: unify syntax to '#' or '!'
    auto sRecNr = CharPtrRange(separatorPos < sMenu.end() ? separatorPos + 1 : separatorPos, sMenu.end());
    sMenu.second = separatorPos;

    SizeT recNo = UNDEFINED_VALUE(SizeT);
    if (!sRecNr.empty())
        AssignValueFromCharPtrs(recNo, sRecNr.begin(), sRecNr.end());

    if (!strncmp(sMenu.begin(), "popuptable", sMenu.size()))
    {
        PopupTable(recNo);
        return;
    }

    if (!strncmp(sMenu.begin(), "edit", sMenu.size()))
    {
        EditPropValue(tiContext, sPathWithSub, recNo);
        return;
    }

    if (!strncmp(sMenu.begin(), "goto", sMenu.size()))
    {
        tiContext = const_cast<TreeItem*>(tiContext->FindBestItem(sPath).first.get()); // TODO: make result FindBestItem non-const
        if (tiContext)
            MainWindow::TheOne()->setCurrentTreeItem(tiContext);
        return;
    }

    if (sMenu.size() >= 3 && !strncmp(sMenu.begin(), "dp.", 3))
    {
        sMenu.first += 3;
        auto detail_page_type = dp_FromName(sMenu);
        tiContext = const_cast<TreeItem*>(tiContext->FindBestItem(sPath).first.get()); // TODO: make result FindBestItem non-const
        switch (detail_page_type)
        {
        case ActiveDetailPage::STATISTICS:
            MainWindow::TheOne()->showStatisticsDirectly(tiContext);
            return;
        case ActiveDetailPage::VALUE_INFO:
            if (IsDataItem(tiContext))
                MainWindow::TheOne()->showValueInfo(AsDataItem(tiContext), recNo);
            return;
        default:
            m_active_detail_page = detail_page_type;
            if (tiContext)
                MainWindow::TheOne()->setCurrentTreeItem(tiContext);
            scheduleDrawPageImpl(100);
            return;
        }
    }
}

struct link_info
{
    bool is_valid = false;
    size_t start = 0;
    size_t stop = 0;
    size_t endline = 0;
    std::string filename = "";
    std::string line = "";
    std::string col = "";
};

auto getLinkFromErrorMessage(std::string_view error_message, unsigned int lineNumber = 0) -> link_info
{
    std::string html_error_message = "";
    //auto error_message_text = std::string(error_message->Why().c_str());
    std::size_t currPos = 0, currLineNumber = 0;
    link_info lastFoundLink;
    while (currPos < error_message.size())
    {
        auto currLineEnd = error_message.find_first_of('\n', currPos);
        if (currLineEnd == std::string::npos)
            currLineEnd = error_message.size();

        auto lineView = std::string_view(&error_message[currPos], currLineEnd - currPos);
        auto round_bracked_open_pos = lineView.find_first_of('(');
        auto comma_pos = lineView.find_first_of(',');
        auto round_bracked_close_pos = lineView.find_first_of(')');

        if (round_bracked_open_pos < comma_pos && comma_pos < round_bracked_close_pos && round_bracked_close_pos != std::string::npos)
        {
            auto filename = lineView.substr(0, round_bracked_open_pos);
            auto line_number = lineView.substr(round_bracked_open_pos + 1, comma_pos - (round_bracked_open_pos + 1));
            auto col_number = lineView.substr(comma_pos + 1, round_bracked_close_pos - (comma_pos + 1));

            lastFoundLink = link_info(true, currPos, currPos + round_bracked_close_pos, currLineEnd, std::string(filename), std::string(line_number), std::string(col_number));
        }
        if (lineNumber <= currLineNumber && lastFoundLink.is_valid)
            break;

        currPos = currLineEnd + 1;
        currLineNumber++;
    }

    return lastFoundLink;
}

#include <QDesktopServices>
void DmsDetailPages::onAnchorClicked(const QUrl& link)
{
    try {
        auto linkStr = link.toString().toUtf8();

        // log link action
#if defined(_DEBUG)
        MainWindow::TheOne()->m_eventlog_model->addText(
            SeverityTypeID::ST_MajorTrace, MsgCategory::other, GetThreadID(), StreamableDateTime(), linkStr.data()
        );
#endif

        auto* current_item = MainWindow::TheOne()->getCurrentTreeItem();

        auto realm = Realm(linkStr);
        if (realm.size() == 16 && !strnicmp(realm.begin(), "editConfigSource", 16))
        {
            auto clicked_error_link = link.toString().toStdString().substr(17);
            auto parsed_clicked_error_link = getLinkFromErrorMessage(clicked_error_link);
            MainWindow::TheOne()->openConfigSourceDirectly(parsed_clicked_error_link.filename, parsed_clicked_error_link.line);
            return;
        }

        if (IsPostRequest(link))
        {
            auto queryStr = link.query().toUtf8();
            DMS_ProcessPostData(const_cast<TreeItem*>(current_item), queryStr.data(), queryStr.size());
            return;
        }
        if (!ShowInDetailPage(linkStr))
        {
            auto raw_string = SharedStr(linkStr.begin(), linkStr.end());
            ReplaceSpecificDelimiters(raw_string.GetAsMutableRange(), '\\');
            auto linkCStr = ConvertDosFileName(raw_string); // obtain zero-termination and non-const access
            QDesktopServices::openUrl(QUrl(linkCStr.c_str(), QUrl::TolerantMode));
            //StartChildProcess(nullptr, linkCStr.begin());
            return;
        }

        if (linkStr.contains(".adms"))
        {
            auto queryResult = ProcessADMS(current_item, linkStr.data());
            setHtml(queryResult.c_str());
            return;
        }
        auto sPrefix = Realm(linkStr);
        if (!strncmp(sPrefix.begin(), "dms", sPrefix.size()))
        {
            auto sAction = CharPtrRange(linkStr.begin() + 4, linkStr.end());
            DoViewAction(current_item, sAction);
            return;
        }
    }
    catch (...)
    {
        auto errMsg = catchAndReportException();
    }
}

void DmsDetailPages::connectDetailPagesAnchorClicked()
{
    setOpenLinks(false);
    setOpenExternalLinks(false);
    connect(this, &QTextBrowser::anchorClicked, this, &DmsDetailPages::onAnchorClicked);
}
