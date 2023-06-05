#include <QListWidget>

#include <QObject>
#include <QDockWidget>
#include <QTextBrowser>

#include "DmsDetailPages.h"
#include "DmsMainWindow.h"
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
    if (m_active_detail_page != ActiveDetailPage::VALUE_INFO)
    {
        m_tiValueInfoContext = nullptr;
        DMS_ExplainValue_Clear();
    }
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
    if (!strncmp(realm.begin(), "dms", realm.size()))
        return true;
    if (!realm.empty())
        return false;
    CharPtrRange knownSuffix(".adms");
    return std::search(x.begin(), x.end(), knownSuffix.begin(), knownSuffix.end());
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
    auto* current_item = MainWindow::TheOne()->getCurrentTreeItem();
    if (!current_item)
        return;

    bool ready = true;
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
        *xmlOut << "<BODY><PRE>";
        DMS_TreeItem_XML_Dump(current_item, xmlOut.get());
        *xmlOut << "</BODY></PRE>";
        break;
    }
    case ActiveDetailPage::METADATA:
    {
        auto url = TreeItemPropertyValue(current_item, urlPropDefPtr);
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
        DMS_XML_MetaInfoRef(current_item, xmlOut.get(), url.c_str());
        break;
    }
    case ActiveDetailPage::VALUE_INFO:
    {
        assert(m_tiValueInfoContext);
        assert(IsDataItem(m_tiValueInfoContext.get_ptr()));
        SuspendTrigger::Resume();
        ready = DMS_DataItem_ExplainAttrValueToXML(AsDataItem(m_tiValueInfoContext.get_ptr()), xmlOut.get(), m_RecNo, nullptr, true);
        break;
    }
    }
    if (result)
    {
        buffer.WriteByte(0); // std::ends
        // set buff to detail page:
        setHtml(buffer.GetData());
    }
    if (!ready)
        m_Repeater.start(0);
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
        m_active_detail_page = dp_FromName(sMenu);
        tiContext = const_cast<TreeItem*>(tiContext->FindBestItem(sPath).first); // TODO: make result FindBestItem non-const
        if (m_active_detail_page == ActiveDetailPage::VALUE_INFO)
        {
            m_RecNo = recNo;
            m_tiValueInfoContext = tiContext;
            drawPage(); // Update
        }
        if (tiContext)
            MainWindow::TheOne()->setCurrentTreeItem(tiContext);
        return;
    }
}


void DmsDetailPages::onAnchorClicked(const QUrl& link)
{
    auto linkStr = link.toString().toUtf8();

    // log link action
#if defined(_DEBUG)
    MainWindow::EventLog(SeverityTypeID::ST_MajorTrace, linkStr.data());
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
        auto linkCStr = SharedStr(linkStr.begin(), linkStr.end()); // obtain zero-termination and non-const access
        StartChildProcess(nullptr, linkCStr.begin());
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
    m_Repeater.callOnTimeout(this, &DmsDetailPages::drawPage);
    m_Repeater.setSingleShot(true);
}
