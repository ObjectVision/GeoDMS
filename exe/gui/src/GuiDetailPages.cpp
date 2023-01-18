#include <imgui.h>
#include <imgui_internal.h>
#include "GuiDetailPages.h"
#include "TicInterface.h"
#include "ClcInterface.h"
#include "Xml/XMLOut.h"

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
        tableProperties.back().emplace_back(PET_HEADING, m_Text);
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

            if (!m_Tag.href.empty())
            {
                tableProperties.back().emplace_back(PET_LINK, CleanStringFromHtmlEncoding(m_Text));
                m_Tag.href.clear();
            }
            else
            {
                if (!m_Text.empty())
                    tableProperties.back().emplace_back(PET_TEXT, CleanStringFromHtmlEncoding(m_Text));
            }
            m_Text.clear();
        }
    }
    else if (m_Tag.text == "<HR/>")
    {
        if (!tableProperties.back().empty())
            tableProperties.emplace_back();
        tableProperties.back().emplace_back(PET_SEPARATOR, m_Text);
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

void GuiDetailPages::UpdateGeneralProperties(GuiState& state)
{
    m_GeneralProperties.clear();
    InterestPtr<TreeItem*> tmpInterest = state.GetCurrentItem()->IsFailed() || state.GetCurrentItem()->WasFailed() ? nullptr : state.GetCurrentItem();
    auto xmlOut = (std::unique_ptr<OutStreamBase>)XML_OutStream_Create(&m_Buff, OutStreamBase::ST_HTM, "", NULL);
    auto result = DMS_TreeItem_XML_DumpGeneral(state.GetCurrentItem(), xmlOut.get(), true);
    m_Buff.InterpretBytes(m_GeneralProperties); // Create detail page from html stream
    m_Buff.Reset();
}

void GuiDetailPages::UpdateAllProperties(GuiState& state)
{
    m_AllProperties.clear();
    InterestPtr<TreeItem*> tmpInterest = state.GetCurrentItem()->IsFailed() || state.GetCurrentItem()->WasFailed() ? nullptr : state.GetCurrentItem();
    auto xmlOut = (std::unique_ptr<OutStreamBase>)XML_OutStream_Create(&m_Buff, OutStreamBase::ST_HTM, "", NULL);
    auto result = DMS_TreeItem_XML_DumpAllProps(state.GetCurrentItem(), xmlOut.get(), false);
    m_Buff.InterpretBytes(m_AllProperties); // Create detail page from html stream
    m_Buff.Reset();
}

void GuiDetailPages::UpdateExploreProperties(GuiState& state)
{
    m_ExploreProperties.clear();
    InterestPtr<TreeItem*> tmpInterest = state.GetCurrentItem()->IsFailed() || state.GetCurrentItem()->WasFailed() ? nullptr : state.GetCurrentItem();
    auto xmlOut = (std::unique_ptr<OutStreamBase>)XML_OutStream_Create(&m_Buff, OutStreamBase::ST_HTM, "", NULL);
    DMS_TreeItem_XML_DumpExplore(state.GetCurrentItem(), xmlOut.get(), true);
    m_Buff.InterpretBytes(m_ExploreProperties); // Create detail page from html stream
    m_Buff.Reset();
}

void GuiDetailPages::FilterStatistics()
{
    auto lines = DivideTreeItemFullNameIntoTreeItemNames(m_Statistics, "\n");
    for (auto& line : lines)
    {
        auto colon_separated_line = DivideTreeItemFullNameIntoTreeItemNames(line, ":");
        //properties.emplace_back();
        //properties.back().emplace_back(PET_HEADING, m_Text);
        if (!colon_separated_line.empty())
        {
            m_FilteredStatistics.emplace_back();
            for (auto& part : colon_separated_line)
            {
                m_FilteredStatistics.back().emplace_back(PET_TEXT, part);
            }
        }
    }
}

void GuiDetailPages::UpdateStatistics(GuiState& state)
{
    SuspendTrigger::Resume();
    m_FilteredStatistics.clear();
    InterestPtr<TreeItem*> tmpInterest = state.GetCurrentItem()->IsFailed() || state.GetCurrentItem()->WasFailed() ? nullptr : state.GetCurrentItem();
    m_Statistics = DMS_NumericDataItem_GetStatistics(state.GetCurrentItem(), nullptr);
    FilterStatistics();
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
                    auto unfound_part = IString::Create("");
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
        }
    }
    ImGui::EndTable();
}

void GuiDetailPages::Update(bool* p_open, GuiState& state)
{
    if (!ImGui::Begin("Detail Pages", p_open, ImGuiWindowFlags_None | ImGuiWindowFlags_NoTitleBar))
    {
        ImGui::End();
        return;
    }

    auto event_queues = GuiEventQueues::getInstance();

    if (ImGui::IsWindowHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
        SetKeyboardFocusToThisHwnd();

    if (event_queues->DetailPagesEvents.HasEvents()) // new current item
    {
        event_queues->DetailPagesEvents.Pop();
        m_GeneralProperties.clear();
        m_AllProperties.clear();
        m_ExploreProperties.clear();
        m_FilteredStatistics.clear();
        m_Statistics.clear();
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
                if (m_FilteredStatistics.empty())
                    UpdateStatistics(state);
                DrawProperties(state, m_FilteredStatistics);
            }
            ImGui::EndTabItem();
            if (ImGui::IsItemHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
                SetKeyboardFocusToThisHwnd();
        }

        if (ImGui::BeginTabItem("Value info", 0, ImGuiTabItemFlags_None))
        {
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Configuration", 0, ImGuiTabItemFlags_None))
        {
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Metadata", 0, ImGuiTabItemFlags_None))
        {
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Source descr", 0, ImGuiTabItemFlags_None))
        {
            ImGui::EndTabItem();
        }

        ImGui::EndTabBar();
    }

    ImGui::End();
}