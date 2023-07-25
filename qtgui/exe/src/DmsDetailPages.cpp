#include <QListWidget>

#include <QObject>
#include <QDockWidget>
#include <QTextBrowser>
#include <QTimer>

#include "DmsDetailPages.h"
#include "DmsMainWindow.h"
#include "DmsEventLog.h"
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

void DmsDetailPages::setActiveDetailPage(ActiveDetailPage new_active_detail_page)
{
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

void DmsDetailPages::toggle(ActiveDetailPage new_active_detail_page)
{
    auto* detail_pages_dock = static_cast<QDockWidget*>(parent());
    if (detail_pages_dock->isHidden() || m_active_detail_page != new_active_detail_page || !isVisible())
    {
        detail_pages_dock->show();
        toggleVisualState(new_active_detail_page, true);
        setActiveDetailPage(new_active_detail_page);
        setVisible(true);
    }
    else
    {
        detail_pages_dock->hide();
        toggleVisualState(new_active_detail_page, false);
        setActiveDetailPage(ActiveDetailPage::NONE);
        setVisible(false);
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
    if (m_active_detail_page != ActiveDetailPage::SOURCEDESCR || m_SDM == SourceDescrMode::All)
    {
        m_SDM = SourceDescrMode::Configured;
        toggle(ActiveDetailPage::SOURCEDESCR);
    }
    else
    {
        reinterpret_cast<int&>(m_SDM)++;
        scheduleDrawPageImpl(1000);
    }
}

void DmsDetailPages::toggleMetaInfo()
{
    toggle(ActiveDetailPage::METADATA);
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
    if (m_DrawPageRequestPending)
        return;

    m_DrawPageRequestPending = true;
    QTimer::singleShot(milliseconds, [this]()
        {
            m_DrawPageRequestPending = false;
            this->drawPage();
        }
    );
}

void DmsDetailPages::scheduleDrawPage()
{
    scheduleDrawPageImpl(0);
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
    {
        DMS_TreeItem_XML_Dump(current_item, xmlOut.get());
        break;
    }
    case ActiveDetailPage::SOURCEDESCR:
    {
        (*xmlOut) << TreeItem_GetSourceDescr(current_item, m_SDM, true).c_str();
        break;
    }
    case ActiveDetailPage::METADATA:
    {
        auto url = FindURL(current_item);
        if (!url.empty())
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
        DMS_XML_MetaInfoRef(current_item, xmlOut.get());
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
    return QSize(500, 20);
}

void DmsDetailPages::DoViewAction(TreeItem* tiContext, CharPtrRange sAction)
{
    assert(tiContext);

    auto colonPos = std::find(sAction.begin(), sAction.end(), ':');
    auto sMenu = CharPtrRange(sAction.begin(), colonPos);
    auto sPathWithSub = CharPtrRange(colonPos == sAction.end() ? colonPos : colonPos + 1, sAction.end());
/*
    if (UpperCase(sMenu) = 'EXECUTE') then
    begin
    m_VAT : = ViewActionType::Execute;
    m_Url: = sPathWithSub;
    exit; // when called from DoOrCreate, self will be Applied Directly and Destroyed.
    end;
    */
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

    if (sMenu.size() >= 3 && !strncmp(sMenu.begin(), "dp.", 3))
    {
        sMenu.first += 3;
        auto detail_page_type = dp_FromName(sMenu);
        tiContext = const_cast<TreeItem*>(tiContext->FindBestItem(sPath).first); // TODO: make result FindBestItem non-const
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
            drawPage(); // Update
            return;
        }
    }
}

#include <QDesktopServices>
void DmsDetailPages::onAnchorClicked(const QUrl& link)
{
    auto linkStr = link.toString().toUtf8();

    // log link action
#if defined(_DEBUG)
    MainWindow::TheOne()->m_eventlog_model->addText(SeverityTypeID::ST_MajorTrace, MsgCategory::nonspecific, linkStr.data());
#endif
    auto* current_item = MainWindow::TheOne()->getCurrentTreeItem();
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

void DmsDetailPages::connectDetailPagesAnchorClicked()
{
    setOpenLinks(false);
    setOpenExternalLinks(false);
    connect(this, &QTextBrowser::anchorClicked, this, &DmsDetailPages::onAnchorClicked);
}
