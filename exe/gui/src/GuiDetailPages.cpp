#include <imgui.h>
#include <imgui_internal.h>
#include "GuiDetailPages.h"
#include "TicInterface.h"
#include "ClcInterface.h"
#include "Xml/XMLOut.h"
#include "Explain.h"

#include "TreeItemProps.h"
#include "TicPropDefConst.h"

/*
DMS_NumericDataItem_GetStatistics
DMS_ParseResult_GetFailReason
DMS_ParseResult_GetAsSLispExpr
DMS_OperatorGroup_GetName
DMS_GetLastErrorMsg
DMS_ValueType_GetName
DMS_TreeItem_GetAssociatedFilename
DMS_TreeItem_GetAssociatedFiletype
DMS_TreeItem_GetExpr


uDMSTreeviewFunctions.pas line 247:
case dp of
DP_AllProps: Result := DMS_TreeItem_XML_DumpAllProps(ti, xmlOut, bShowHidden);
DP_General:  Result := DMS_TreeItem_XML_DumpGeneral (ti, xmlOut, bShowHidden);
DP_Explore:  DMS_TreeItem_XML_DumpExplore (ti, xmlOut, bShowHidden);
DP_Config:   DMS_TreeItem_XML_Dump(ti, xmlOut);
DP_ValueInfo:
case funcID of
    1: Result := DMS_DataItem_ExplainAttrValueToXML(ti, xmlOut, p0, sExtraInfo, bShowHidden);
    2: Result := DMS_DataItem_ExplainGridValueToXML(ti, xmlOut, p1, p2, sExtraInfo, bShowHidden);
end;

*/
std::string Tag::href = "";

HTMLGuiComponentFactory::HTMLGuiComponentFactory() 
{}

HTMLGuiComponentFactory::~HTMLGuiComponentFactory(){}
void HTMLGuiComponentFactory::WriteBytes(const Byte* data, streamsize_t size)
{
    m_Buff.insert(m_Buff.end(), data, data + size);
}

bool HTMLGuiComponentFactory::ReplaceStringInString(std::string& str, const std::string& from, const std::string& to)
{
    while (true)
    {
        size_t start_pos = str.find(from);
        if (start_pos == std::string::npos)
            break;
        str.replace(start_pos, from.length(), to);
    }

    return true;
}
// CODE REVIEW; en &amp; ? Consider integrating with HtmlDecode in XmlParser.cpp, 
// enventueel laten we die functie op een char range werken teneinde voor zowel std::string als SharedStr te laten werken.
// HtmlDecode en helpers kunnen we verplaatsen naar Encodes.cpp voor een generieke set van consistente encoding en decoding functies.

std::string HTMLGuiComponentFactory::CleanStringFromHtmlEncoding(std::string text)
{
    ReplaceStringInString(text, "&apos;", "\'"); //&apos;
    ReplaceStringInString(text, "&quot;", "\""); //&quot;
    ReplaceStringInString(text, "&lt;", "<"); // &lt;
    ReplaceStringInString(text, "&gt;", ">"); // &gt;
    return text;
}

std::string HTMLGuiComponentFactory::GetHrefFromTag()
{
    std::string href = "";

    if (m_Tag.text.substr(9, 14) == "dms:dp.general")
        href = m_Tag.text.substr(24, m_Tag.text.length() - 26);
        
    return href;

}

void HTMLGuiComponentFactory::InterpretTag(TableData& tableProperties)
{
    // open tags
    if (m_Tag.text.substr(0, 5) == "<BODY")
        m_OpenTags[int(HTMLTagType::BODY)]++;
    else if (m_Tag.text.substr(0, 2) == "<A")
    {
        m_OpenTags[int(HTMLTagType::LINK)]++;
        m_Tag.href = GetHrefFromTag();
    }
    else if (m_Tag.text.substr(0, 6) == "<TABLE")
    {
        m_OpenTags[int(HTMLTagType::TABLE)]++;
    }
    else if (m_Tag.text.substr(0, 3) == "<TR")
    {
        m_OpenTags[int(HTMLTagType::TABLEROW)]++;
        if (!tableProperties.back().empty())
            tableProperties.emplace_back();
    }
    else if (m_Tag.text.substr(0, 3) == "<TD")
        m_OpenTags[int(HTMLTagType::TABLEDATA)]++;
    else if (m_Tag.text.substr(0, 3) == "<H2"){}

    // close tags
    else if (m_Tag.text == "</BODY>")
        m_OpenTags[int(HTMLTagType::BODY)]--;
    else if (m_Tag.text == "</H2>")
    {
        m_Tag.href.clear();
        tableProperties.emplace_back();
        tableProperties.back().emplace_back(PET_HEADING, false, m_Text);
        m_Text.clear();
    }
    else if (m_Tag.text == "</TABLE>")
    {
        m_OpenTags[int(HTMLTagType::TABLE)]--;
    }
    else if (m_Tag.text == "</TR>")
        m_OpenTags[int(HTMLTagType::TABLEROW)]--;
    else if (m_Tag.text == "</A>")
        m_OpenTags[int(HTMLTagType::LINK)]--;
    else if (m_Tag.text == "</TD>")
    {
        if (m_OpenTags[int(HTMLTagType::TABLEDATA)] > 0)
        {
            m_OpenTags[int(HTMLTagType::TABLEDATA)]--;

            if (m_Text.empty())
                return;

            bool text_contains_failreason = m_Text.contains("FailState") || m_Text.contains("FailReason");

            if (!m_Tag.href.empty())
            {
                tableProperties.back().emplace_back(PET_LINK, text_contains_failreason, CleanStringFromHtmlEncoding(m_Text));
                m_Tag.href.clear();
            }
            else
                    tableProperties.back().emplace_back(PET_TEXT, text_contains_failreason, CleanStringFromHtmlEncoding(m_Text));
            m_Text.clear();
        }
    }
    else if (m_Tag.text == "<HR/>")
    {
        if (!tableProperties.back().empty())
            tableProperties.emplace_back();
        tableProperties.back().emplace_back(PET_SEPARATOR, false, m_Text);
    }

}
bool HTMLGuiComponentFactory::IsOpenTag(UInt32 ind)
{
    std::string tmpTag;
    while (m_Buff[ind] != '>')
    {
        tmpTag += m_Buff[ind];
        ind++;
    }
    if (tmpTag.substr(0, 4) == "BODY")
        return true;
    else if (tmpTag.substr(0, 5) == "TABLE")
        return true;
    else if (tmpTag.substr(0, 2) == "TD")
        return true;
    else if (tmpTag.substr(0, 2) == "TR")
        return true;
    else if (tmpTag.substr(0, 1) == "A")
        return true;
    else if (tmpTag.substr(0, 2) == "H2")
        return true;
    else if (tmpTag.substr(0, 5) == "/BODY")
        return true;
    else if (tmpTag.substr(0, 6) == "/TABLE")
        return true;
    else if (tmpTag.substr(0, 3) == "/TD")
        return true;
    else if (tmpTag.substr(0, 3) == "/TR")
        return true;
    else if (tmpTag.substr(0, 2) == "/A")
        return true;
    else if (tmpTag.substr(0, 3) == "/H2")
        return true;
    else if (tmpTag.substr(0, 3) == "HR/")
        return true;
    else if (tmpTag.substr(0, 2) == "BR")
        return true;

    return false;
}

auto HTMLGuiComponentFactory::InterpretBytesAsString() -> std::string
{
    return std::string(m_Buff.begin(), m_Buff.end());
}

void HTMLGuiComponentFactory::InterpretBytes(TableData& tableProperties)
{
    m_ParserState = HTMLParserState::NONE;
    UInt32 ind = 0;
    for (auto &chr : m_Buff)
    {
        ind++;
        if (chr == '\n')
            continue;

        // set parser state
        if (chr == '<' && IsOpenTag(ind))
        {
            m_ParserState = HTMLParserState::TAGOPEN;
        }
        else if (chr == '>' && m_ParserState == HTMLParserState::TAGOPEN)
            m_ParserState = HTMLParserState::TAGCLOSE;
        else if (m_ParserState != HTMLParserState::TAGOPEN)
        {
            m_Tag.clear();
            m_ParserState = HTMLParserState::TEXT;
        }

        switch (m_ParserState)
        {
        case HTMLParserState::TAGOPEN:
        {
            m_Tag.text += chr;
            break;
        }
        case HTMLParserState::TAGCLOSE:
        {
            m_Tag.text += chr;
            InterpretTag(tableProperties);
            m_Tag.text.clear();
            break;
        }
        case HTMLParserState::TEXT:
        {
            if (chr == '%') // to overcome formatting interpretation of %
                m_Text += chr;
            m_Text += chr;
            break;
        }
        }
    }
}

streamsize_t HTMLGuiComponentFactory::CurrPos() const
{
    return m_Buff.size();
}

void HTMLGuiComponentFactory::Reset()
{
    m_Buff.clear();
    m_Tag.clear();
    m_Text.clear();
}

auto GuiDetailPages::ClearSpecificDetailPages(bool general, bool all_properties, bool explore_properties, bool statistics, bool value_info, bool source_description, bool configuration) -> void
{
    if (general)
        m_GeneralProperties.clear();

    if (all_properties)
        m_AllProperties.clear();

    if (explore_properties)
        m_ExploreProperties.clear();

    if (statistics)
        m_Statistics.clear();

    if (value_info)
        m_ValueInfo.clear();

    if (source_description)
        m_SourceDescription.clear();

    if (configuration)
        m_Configuration.clear();
}

void GuiDetailPages::UpdateGeneralProperties(GuiState& state)
{
    clear();
    InterestPtr<TreeItem*> tmpInterest = state.GetCurrentItem()->IsFailed() || state.GetCurrentItem()->WasFailed() ? nullptr : state.GetCurrentItem();
    auto xmlOut = (std::unique_ptr<OutStreamBase>)XML_OutStream_Create(&m_Buff, OutStreamBase::ST_HTM, "", NULL);
    auto result = DMS_TreeItem_XML_DumpGeneral(state.GetCurrentItem(), xmlOut.get(), true);
    m_Buff.InterpretBytes(m_GeneralProperties); // Create detail page from html stream
    m_Buff.Reset();
}

auto GuiDetailPages::clear() -> void
{
    DMS_ExplainValue_Clear();
    ClearSpecificDetailPages(true, true, true, true, true, true, true);
}

void GuiDetailPages::UpdateAllProperties(GuiState& state)
{
    clear();
    InterestPtr<TreeItem*> tmpInterest = state.GetCurrentItem()->IsFailed() || state.GetCurrentItem()->WasFailed() ? nullptr : state.GetCurrentItem();
    auto xmlOut = (std::unique_ptr<OutStreamBase>)XML_OutStream_Create(&m_Buff, OutStreamBase::ST_HTM, "", NULL);
    auto result = DMS_TreeItem_XML_DumpAllProps(state.GetCurrentItem(), xmlOut.get(), false);
    m_Buff.InterpretBytes(m_AllProperties); // Create detail page from html stream
    m_Buff.Reset();
}

void GuiDetailPages::UpdateExploreProperties(GuiState& state)
{
    clear();
    InterestPtr<TreeItem*> tmpInterest = state.GetCurrentItem()->IsFailed() || state.GetCurrentItem()->WasFailed() ? nullptr : state.GetCurrentItem();
    auto xmlOut = (std::unique_ptr<OutStreamBase>)XML_OutStream_Create(&m_Buff, OutStreamBase::ST_HTM, "", NULL);
    DMS_TreeItem_XML_DumpExplore(state.GetCurrentItem(), xmlOut.get(), true);
    m_Buff.InterpretBytes(m_ExploreProperties); // Create detail page from html stream
    m_Buff.Reset();
}

void GuiDetailPages::StringToTable(std::string &input, TableData &result, std::string separator="")
{
    result.clear();
    auto lines = DivideTreeItemFullNameIntoTreeItemNames(input, "\n");
    for (auto& line : lines)
    {
        auto colon_separated_line = DivideTreeItemFullNameIntoTreeItemNames(line, separator);
        //properties.emplace_back();
        //properties.back().emplace_back(PET_HEADING, m_Text);
        if (!colon_separated_line.empty())
        {
            result.emplace_back();
            for (auto& part : colon_separated_line)
            {
                result.back().emplace_back(PET_TEXT, false, part);
            }
        }
    }
}

auto GuiDetailPages::UpdateStatistics(GuiState& state) -> void
{
    //SuspendTrigger::Resume(); // TODO: necessary?
    clear();
    InterestPtr<TreeItem*> tmpInterest = state.GetCurrentItem()->IsFailed() || state.GetCurrentItem()->WasFailed() ? nullptr : state.GetCurrentItem();
    std::string statistics_string = DMS_NumericDataItem_GetStatistics(state.GetCurrentItem(), nullptr);
    StringToTable(statistics_string, m_Statistics, ":");
}

auto GetIndexFromDPVIsActionString(const std::string &dpvi_str) -> UInt64
{
    return std::stoll(dpvi_str.substr(11, dpvi_str.size()));
}

auto GuiDetailPages::UpdateValueInfo(GuiState& state) -> void
{
    clear();
    auto current_view_action = *state.TreeItemHistoryList.GetCurrentIterator();
    if (!current_view_action.sAction.contains("dp.vi.attr"))
        return;


    InterestPtr<TreeItem*> tmpInterest = current_view_action.tiContext->IsFailed() || current_view_action.tiContext->WasFailed() ? nullptr : current_view_action.tiContext;
    auto xmlOut = (std::unique_ptr<OutStreamBase>)XML_OutStream_Create(&m_Buff, OutStreamBase::ST_HTM, "", NULL);
    auto dpvi_index = GetIndexFromDPVIsActionString(current_view_action.sAction);
    DMS_DataItem_ExplainAttrValueToXML(AsDataItem(current_view_action.tiContext), xmlOut.get(), dpvi_index, NULL, true);

    m_Buff.InterpretBytes(m_ValueInfo); // Create detail page from html stream
    m_Buff.Reset();
    // TODO: See uDMSTreeViewFunctions.pas line 254
//DP_ValueInfo:
//    case funcID of
//    1: Result: = DMS_DataItem_ExplainAttrValueToXML(ti, xmlOut, p0, sExtraInfo, bShowHidden);
//    2: Result: = DMS_DataItem_ExplainGridValueToXML(ti, xmlOut, p1, p2, sExtraInfo, bShowHidden);

}

auto GuiDetailPages::UpdateConfiguration(GuiState& state) -> void
{
    clear();
    InterestPtr<TreeItem*> tmpInterest = state.GetCurrentItem()->IsFailed() || state.GetCurrentItem()->WasFailed() ? nullptr : state.GetCurrentItem();
    auto xmlOut = (std::unique_ptr<OutStreamBase>)XML_OutStream_Create(&m_Buff, OutStreamBase::ST_DMS, "DMS", NULL);
    DMS_TreeItem_XML_Dump(state.GetCurrentItem(), xmlOut.get());
    auto conf_str = m_Buff.InterpretBytesAsString();
    StringToTable(conf_str, m_Configuration);
    m_Buff.Reset();
}

auto GuiDetailPages::UpdateSourceDescription(GuiState& state) -> void
{
    clear();
    std::string source_descr_string = TreeItem_GetSourceDescr(state.GetCurrentItem(), state.SourceDescrMode, true).c_str();
    StringToTable(source_descr_string, m_SourceDescription);
    auto test = std::string(DMS_TreeItem_GetExpr(state.GetCurrentItem()));
}

void GuiDetailPages::DrawProperties(GuiState& state, TableData& properties)
{
    auto event_queues = GuiEventQueues::getInstance();
    if (ImGui::GetContentRegionAvail().y < 0) // table needs space, crashes otherwise
        return;

    int button_index = 0;
    ImGui::BeginTable(" ", 6, ImGuiTableFlags_ScrollX | ImGuiTableFlags_NoHostExtendY); // ImVec2(0.0f, ImGui::GetTextLineHeightWithSpacing() * 8)
    for (auto& row : properties)
    {
        ImGui::TableNextRow();
        UInt8 column_index = 0;
        for (auto& col : row)
        {
            ImGui::TableSetColumnIndex(column_index);
            if (col.background_is_red)
                SetTextBackgroundRed(ImVec2(ImGui::GetScrollMaxX(), ImGui::GetTextLineHeight()+1.0));// ImGui::GetWindowSize
            if (col.type == PET_HEADING)
            {
                ImGui::Text(col.text.c_str());
            }
            else if (col.type == PET_LINK)
            {
                ImGui::PushID(button_index++);
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(51.0f / 255.0f, 102.0f / 255.0f, 204.0f / 255.0f, 1.0f));
                if (ImGui::Button(col.text.c_str()))
                {
                    auto unfound_part = IString::Create(""); //TODO: replace with TreeItem_GetBest...
                    TreeItem* jumpItem = (TreeItem*)DMS_TreeItem_GetBestItemAndUnfoundPart(state.GetRoot(), col.text.c_str(), &unfound_part);
                    if (jumpItem)
                    {
                        state.SetCurrentItem(jumpItem);
                        event_queues->TreeViewEvents.Add(GuiEvents::JumpToCurrentItem);
                        event_queues->CurrentItemBarEvents.Add(GuiEvents::UpdateCurrentItem);
                        event_queues->DetailPagesEvents.Add(GuiEvents::UpdateCurrentItem);
                        event_queues->MainEvents.Add(GuiEvents::UpdateCurrentItem);
                    }
                    unfound_part->Release(unfound_part);
                }
                ImGui::PopID();
                ImGui::PopStyleColor(2);
            }
            else if (col.type == PET_TEXT)
            {
                ImGui::Text(col.text.c_str());
            }
            else if (col.type == PET_SEPARATOR)
            {
                ImGui::Separator();
                column_index++;
                ImGui::TableSetColumnIndex(column_index);
                ImGui::Separator();
            }
            if (ImGui::IsItemHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
                SetKeyboardFocusToThisHwnd();
            column_index++;
            OnItemClickItemTextTextToClipboard(col.text);
        }
    }
    ImGui::EndTable();
}

auto GuiDetailPages::OnViewAction(  const TreeItem* tiContext,
                    CharPtr     sAction,
                    Int32         nCode,
                    Int32             x,
                    Int32             y,
                    bool   doAddHistory,
                    bool          isUrl,
                    bool	mustOpenDetailsPage) -> void
{
    if (not doAddHistory)
        return;

    auto event_queues = GuiEventQueues::getInstance();
    if (!std::string(sAction).contains("dp.vi"))
        return;

    GuiState::TreeItemHistoryList.Insert({ const_cast<TreeItem*>(tiContext), sAction, nCode, x, y });

    if (!mustOpenDetailsPage)
        return;

    event_queues->DetailPagesEvents.Add(GuiEvents::FocusValueInfoTab);
}

void GuiDetailPages::Update(bool* p_open, GuiState& state)
{
    if (!ImGui::Begin("Detail Pages", p_open, ImGuiWindowFlags_None | ImGuiWindowFlags_NoTitleBar))
    {
        ImGui::End();
        return;
    }

    bool set_value_info_selected = false;
    auto event_queues = GuiEventQueues::getInstance();

    if (ImGui::IsWindowHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
        SetKeyboardFocusToThisHwnd();

    if (event_queues->DetailPagesEvents.HasEvents()) // new current item
    {
        auto event = event_queues->DetailPagesEvents.Pop();
        switch (event)
        {
        case GuiEvents::FocusValueInfoTab: 
        {
            set_value_info_selected = true;
            m_ValueInfo.clear();
            break;
        }
        case GuiEvents::UpdateCurrentItem: 
        {
            m_GeneralProperties.clear();
            m_AllProperties.clear();
            m_ExploreProperties.clear();
            m_Configuration.clear();
            m_Statistics.clear();
            m_ValueInfo.clear();
            m_SourceDescription.clear();
            break;
        }
        default:    break;
        }
    }

    /*// window specific options button
    auto old_cpos = SetCursorPosToOptionsIconInWindowHeader();
    SetClipRectToIncludeOptionsIconInWindowHeader();
    ImGui::Text(ICON_RI_SETTINGS);
    if (MouseHooversOptionsIconInWindowHeader())
    {        
        if (ImGui::IsMouseClicked(ImGuiMouseButton_Left))
        {
            // do something useful with options window
        }
    }
    ImGui::SetCursorPos(old_cpos);
    ImGui::PopClipRect();*/

    if (ImGui::BeginTabBar("Tabs", ImGuiTabBarFlags_FittingPolicyScroll))
    {
        if (ImGui::BeginTabItem("General", 0, ImGuiTabItemFlags_None))
        {

            if (state.GetCurrentItem())
            {
                if (m_GeneralProperties.empty())
                    UpdateGeneralProperties(state);
                DrawProperties(state, m_GeneralProperties);
            }
            ImGui::EndTabItem();
            if (ImGui::IsItemHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
                SetKeyboardFocusToThisHwnd();
        }

        if (ImGui::BeginTabItem("Properties", 0, ImGuiTabItemFlags_None))
        {
            if (state.GetCurrentItem())
            {
                if (m_AllProperties.empty())
                    UpdateAllProperties(state);
                DrawProperties(state, m_AllProperties);
            }
            ImGui::EndTabItem();
            if (ImGui::IsItemHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
                SetKeyboardFocusToThisHwnd();
        }

        if (ImGui::IsItemHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
            SetKeyboardFocusToThisHwnd();

        if (ImGui::BeginTabItem("Explore", 0, ImGuiTabItemFlags_None))
        {
            if (state.GetCurrentItem())
            {
                if (m_ExploreProperties.empty())
                    UpdateExploreProperties(state);
                DrawProperties(state, m_ExploreProperties);
            }
            ImGui::EndTabItem();
            if (ImGui::IsItemHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
                SetKeyboardFocusToThisHwnd();
        }

        if (ImGui::BeginTabItem("Statistics", 0, ImGuiTabItemFlags_None))
        {
            if (state.GetCurrentItem())
            {
                if (m_Statistics.empty())
                    UpdateStatistics(state);
                DrawProperties(state, m_Statistics);
            }
            ImGui::EndTabItem();
            if (ImGui::IsItemHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
                SetKeyboardFocusToThisHwnd();
        }

        if (ImGui::BeginTabItem("Value info", 0, set_value_info_selected?ImGuiTabItemFlags_SetSelected:ImGuiTabItemFlags_None))
        {
            if (m_ValueInfo.empty())
                UpdateValueInfo(state);
            DrawProperties(state, m_ValueInfo);

            ImGui::EndTabItem();
            if (ImGui::IsItemHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
                SetKeyboardFocusToThisHwnd();            
        }

        if (ImGui::BeginTabItem("Configuration", 0, ImGuiTabItemFlags_None))
        {
            if (state.GetCurrentItem())
            {
                if (m_Configuration.empty())
                    UpdateConfiguration(state);
                DrawProperties(state, m_Configuration);
            }
            ImGui::EndTabItem();
        }

        /*if (ImGui::BeginTabItem("Metadata", 0, ImGuiTabItemFlags_None))
        {
            TODO: think about html editable functionality like export settings, reimplement?
            ImGui::EndTabItem();
        }*/

        if (ImGui::BeginTabItem("Source descr", 0, ImGuiTabItemFlags_None))
        {
            if (state.GetCurrentItem())
            {
                if (m_SourceDescription.empty())
                    UpdateSourceDescription(state);
                DrawProperties(state, m_SourceDescription);
            }
            ImGui::EndTabItem();
        }

        ImGui::EndTabBar();
    }

    ImGui::End();
}