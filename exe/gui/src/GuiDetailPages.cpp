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
{
    m_OpenTags[HTMLTagType::BODY]           = 0;
    m_OpenTags[HTMLTagType::TABLE]          = 0;
    m_OpenTags[HTMLTagType::TABLEROW]       = 0;
    m_OpenTags[HTMLTagType::TABLEDATA]      = 0;
    m_OpenTags[HTMLTagType::LINK]           = 0;
    m_OpenTags[HTMLTagType::LINEBREAK]      = 0;
    m_OpenTags[HTMLTagType::HORIZONTALLINE] = 0;
    m_OpenTags[HTMLTagType::HEADING]        = 0;
}
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

void HTMLGuiComponentFactory::InterpretTag(bool direct_draw, std::vector<std::vector<PropertyEntry>> &properties)
{
    // open tags
    if (m_Tag.text.substr(0, 5) == "<BODY")
        m_OpenTags[HTMLTagType::BODY]++;
    else if (m_Tag.text.substr(0, 2) == "<A")
    {
        m_OpenTags[HTMLTagType::LINK]++;
        m_Tag.href = GetHrefFromTag();
    }
    else if (m_Tag.text.substr(0, 6) == "<TABLE")
    {
        if (direct_draw)
            ImGui::BeginTable(" ", 6, ImGuiTableFlags_None, ImVec2(0.0f, ImGui::GetTextLineHeightWithSpacing() * 8));

        m_OpenTags[HTMLTagType::TABLE]++;
    }
    else if (m_Tag.text.substr(0, 3) == "<TR")
    {
        if (direct_draw)
            ImGui::TableNextRow();

        m_ColumnIndex = 0;
        m_OpenTags[HTMLTagType::TABLEROW]++;
        if (!properties.back().empty())
            properties.emplace_back();
    }
    else if (m_Tag.text.substr(0, 3) == "<TD")
        m_OpenTags[HTMLTagType::TABLEDATA]++;
    else if (m_Tag.text.substr(0, 3) == "<H2"){}

    // close tags
    else if (m_Tag.text == "</BODY>")
        m_OpenTags[HTMLTagType::BODY]--;
    else if (m_Tag.text == "</H2>")
    {
        if (direct_draw)
            ImGui::Text(m_Text.c_str());

        m_Tag.href.clear();
        properties.emplace_back();
        properties.back().emplace_back(PET_HEADING, m_Text);
        m_Text.clear();
    }
    else if (m_Tag.text == "</TABLE>")
    {
        if (direct_draw)
            ImGui::EndTable();

        m_OpenTags[HTMLTagType::TABLE]--;
    }
    else if (m_Tag.text == "</TR>")
        m_OpenTags[HTMLTagType::TABLEROW]--;
    else if (m_Tag.text == "</A>")
        m_OpenTags[HTMLTagType::LINK]--;
    else if (m_Tag.text == "</TD>")
    {
        if (m_OpenTags[HTMLTagType::TABLEDATA] > 0)
        {
            m_OpenTags[HTMLTagType::TABLEDATA]--;
            if (direct_draw)
                ImGui::TableSetColumnIndex(m_ColumnIndex);

            if (!m_Tag.href.empty())
            {
                if (direct_draw)
                {
                    ImGui::PushID(m_refIndex++);

                    if (ImGui::Button(m_Text.c_str()))
                    {
                        auto item = SetJumpItemFullNameToOpenInTreeView(m_State.GetRoot(), DivideTreeItemFullNameIntoTreeItemNames(m_Text.c_str()));
                        if (item)
                        {
                            m_State.SetCurrentItem(item);
                            m_State.TreeViewEvents.Add(GuiEvents::JumpToCurrentItem);
                            m_State.CurrentItemBarEvents.Add(GuiEvents::UpdateCurrentItem);
                        }
                    }
                    ImGui::PopID();
                }
                properties.back().emplace_back(PET_LINK, CleanStringFromHtmlEncoding(m_Text));
                m_Tag.href.clear();
            }
            else
            {
                if (direct_draw)
                {
                    ImGui::Text(m_Text.c_str());
                }
                if (!m_Text.empty())
                    properties.back().emplace_back(PET_TEXT, CleanStringFromHtmlEncoding(m_Text));
            }
            m_Text.clear();
            m_ColumnIndex++;
        }
    }
    else if (m_Tag.text == "<HR/>")
    {
        if (direct_draw)
            ImGui::Separator();

        if (!properties.back().empty())
            properties.emplace_back();
        properties.back().emplace_back(PET_SEPARATOR, m_Text);
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

void HTMLGuiComponentFactory::InterpretBytes(bool direct_draw, std::vector<std::vector<PropertyEntry>> &properties)
{
    m_refIndex = 0;
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
            InterpretTag(direct_draw, properties);
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

void GuiDetailPages::UpdateGeneralProperties()
{
    m_GeneralProperties.clear();
    InterestPtr<TreeItem*> tmpInterest = m_State.GetCurrentItem()->IsFailed() ? nullptr : m_State.GetCurrentItem();
    auto xmlOut = (std::unique_ptr<OutStreamBase>)XML_OutStream_Create(&m_Buff, OutStreamBase::ST_HTM, "", NULL);
    auto result = DMS_TreeItem_XML_DumpGeneral(m_State.GetCurrentItem(), xmlOut.get(), true);
    m_Buff.InterpretBytes(false, m_GeneralProperties); // Create detail page from html stream
    m_Buff.Reset();
}

void GuiDetailPages::UpdateAllProperties()
{
    m_AllProperties.clear();
    InterestPtr<TreeItem*> tmpInterest = m_State.GetCurrentItem()->IsFailed() ? nullptr : m_State.GetCurrentItem();
    auto xmlOut = (std::unique_ptr<OutStreamBase>)XML_OutStream_Create(&m_Buff, OutStreamBase::ST_HTM, "", NULL);
    auto result = DMS_TreeItem_XML_DumpAllProps(m_State.GetCurrentItem(), xmlOut.get(), false);
    m_Buff.InterpretBytes(false, m_AllProperties); // Create detail page from html stream
    m_Buff.Reset();
}

void GuiDetailPages::UpdateExploreProperties()
{
    m_ExploreProperties.clear();
    InterestPtr<TreeItem*> tmpInterest = m_State.GetCurrentItem()->IsFailed() ? nullptr : m_State.GetCurrentItem();
    auto xmlOut = (std::unique_ptr<OutStreamBase>)XML_OutStream_Create(&m_Buff, OutStreamBase::ST_HTM, "", NULL);
    DMS_TreeItem_XML_DumpExplore(m_State.GetCurrentItem(), xmlOut.get(), true);
    m_Buff.InterpretBytes(false, m_ExploreProperties); // Create detail page from html stream
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

void GuiDetailPages::UpdateStatistics()
{
    SuspendTrigger::Resume();
    m_FilteredStatistics.clear();
    InterestPtr<TreeItem*> tmpInterest = m_State.GetCurrentItem()->IsFailed() ? nullptr : m_State.GetCurrentItem();
    m_Statistics = DMS_NumericDataItem_GetStatistics(m_State.GetCurrentItem(), nullptr);
    FilterStatistics();
}

void GuiDetailPages::DrawProperties(std::vector<std::vector<PropertyEntry>>& properties)
{
    int button_index = 0;
    ImGui::BeginTable(" ", 6, ImGuiTableFlags_ScrollX | ImGuiTableFlags_NoHostExtendY); // ImVec2(0.0f, ImGui::GetTextLineHeightWithSpacing() * 8)
    for (auto& row : properties)
    {
        ImGui::TableNextRow();
        m_ColumnIndex = 0;
        for (auto& col : row)
        {
            ImGui::TableSetColumnIndex(m_ColumnIndex);
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
                    auto item = SetJumpItemFullNameToOpenInTreeView(m_State.GetRoot(), DivideTreeItemFullNameIntoTreeItemNames(col.text.c_str()));
                    if (item)
                    {
                        m_State.SetCurrentItem(item);
                        m_State.TreeViewEvents.Add(GuiEvents::JumpToCurrentItem);
                        m_State.CurrentItemBarEvents.Add(GuiEvents::UpdateCurrentItem);
                        m_State.DetailPagesEvents.Add(GuiEvents::UpdateCurrentItem);
                    }
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
                m_ColumnIndex++;
                ImGui::TableSetColumnIndex(m_ColumnIndex);
                ImGui::Separator();
            }
            m_ColumnIndex++;
        }
    }
    ImGui::EndTable();
}

void GuiDetailPages::Update(bool* p_open)
{
    if (!ImGui::Begin("Detail Pages", p_open, ImGuiWindowFlags_None))
    {
        ImGui::End();
        return;
    }

    if (ImGui::IsWindowHovered() && ImGui::IsMouseReleased(ImGuiMouseButton_Left))
        SetKeyboardFocusToThisHwnd();

    if (m_State.DetailPagesEvents.HasEvents()) // new current item
    {
        m_State.DetailPagesEvents.Pop();
        m_GeneralProperties.clear();
        m_AllProperties.clear();
        m_ExploreProperties.clear();
        m_FilteredStatistics.clear();
        m_Statistics.clear();
    }

    // window specific options button
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
    ImGui::PopClipRect();

    if (ImGui::BeginTabBar("Tabs", ImGuiTabBarFlags_None))
    {
        if (ImGui::BeginTabItem("General", 0, ImGuiTabItemFlags_None))
        {
            if (m_State.GetCurrentItem())
            {
                if (m_GeneralProperties.empty())
                    UpdateGeneralProperties();
                DrawProperties(m_GeneralProperties);
            }
            //if (ImGui::IsItemActive() && ImGui::IsItemHovered() && ImGui::IsAnyMouseDown())
            //    SetKeyboardFocusToThisHwnd();
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Properties", 0, ImGuiTabItemFlags_None))
        { // HAS NON DEFAULT VALUES
            if (ImGui::IsWindowHovered() && ImGui::IsAnyMouseDown())
                SetKeyboardFocusToThisHwnd();

            if (m_State.GetCurrentItem())
            {
                if (m_AllProperties.empty())
                    UpdateAllProperties();
                DrawProperties(m_AllProperties);
            }
            if (ImGui::IsItemHovered() && ImGui::IsAnyMouseDown())
                SetKeyboardFocusToThisHwnd();
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Explore", 0, ImGuiTabItemFlags_None))
        {
            if (ImGui::IsWindowHovered() && ImGui::IsAnyMouseDown())
                SetKeyboardFocusToThisHwnd();

            if (m_State.GetCurrentItem())
            {
                if (m_ExploreProperties.empty())
                    UpdateExploreProperties();
                DrawProperties(m_ExploreProperties);
            }
            if (ImGui::IsItemHovered() && ImGui::IsAnyMouseDown())
                SetKeyboardFocusToThisHwnd();
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Statistics", 0, ImGuiTabItemFlags_None))
        {
            if (ImGui::IsWindowHovered() && ImGui::IsAnyMouseDown())
                SetKeyboardFocusToThisHwnd();

            if (m_State.GetCurrentItem())
            {
                if (m_FilteredStatistics.empty())
                    UpdateStatistics();
                //ImGui::InputTextMultiline("##statistics", const_cast<char*>(m_Statistics.c_str()), m_Statistics.size(), ImVec2(-FLT_MIN, ImGui::GetTextLineHeight() * 16));
                DrawProperties(m_FilteredStatistics);
            }

            if (ImGui::IsItemHovered() && ImGui::IsAnyMouseDown())
                SetKeyboardFocusToThisHwnd();
            ImGui::EndTabItem();
        }

        ImGui::EndTabBar();
    }

    ImGui::End();
}