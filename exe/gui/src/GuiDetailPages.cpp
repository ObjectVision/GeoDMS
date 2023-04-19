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

GuiOutStreamBuff::GuiOutStreamBuff()
{}

GuiOutStreamBuff::~GuiOutStreamBuff(){}
void GuiOutStreamBuff::WriteBytes(const Byte* data, streamsize_t size)
{
    m_Buff.insert(m_Buff.end(), data, data + size);
}

bool GuiOutStreamBuff::ReplaceStringInString(std::string& str, const std::string& from, const std::string& to)
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

std::string GuiOutStreamBuff::CleanStringFromHtmlEncoding(std::string text)
{
    ReplaceStringInString(text, "&apos;", "\'"); //&apos;
    ReplaceStringInString(text, "&quot;", "\""); //&quot;
    ReplaceStringInString(text, "&lt;", "<"); // &lt;
    ReplaceStringInString(text, "&gt;", ">"); // &gt;
    return text;
}

std::string GuiOutStreamBuff::GetHrefFromTag()
{
    std::string href = "";

    if (m_Tag.text.size() >= 23 && std::string_view(m_Tag.text.data()+9, 14) == "dms:dp.general")
        href = m_Tag.text.substr(24, m_Tag.text.length() - 26);
        
    return href;

}

void GuiOutStreamBuff::InterpretTag(TableData& tableProperties)
{
    // open tags
    if (m_Tag.text.size() >= 5 && std::string_view(m_Tag.text.data(), 5) == "<BODY")
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

bool GuiOutStreamBuff::IsOpenTag(UInt32 ind)
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

auto GuiOutStreamBuff::InterpretBytesAsString() -> std::string
{
    return std::string(m_Buff.begin(), m_Buff.end());
}

void GuiOutStreamBuff::InterpretBytes(TableData& tableProperties)
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

streamsize_t GuiOutStreamBuff::CurrPos() const
{
    return m_Buff.size();
}

void GuiOutStreamBuff::Reset()
{
    m_Buff.clear();
    m_Tag.clear();
    m_Text.clear();
}

GuiMarkDownPage::GuiMarkDownPage(std::string_view markdown_text)
{
    m_markdown_text = markdown_text;
    Parse();
}

void GuiMarkDownPage::CleanupLastEmptyElementpart()
{
    if (m_markdown_data.tables.empty())
        return;
    if (m_markdown_data.tables.back().rows.empty())
        return;
    if (m_markdown_data.tables.back().rows.back().elements.empty())
        return;
    if (m_markdown_data.tables.back().rows.back().elements.back().parts.empty())
        return;
    if (IsLastElementPartEmpty())
        m_markdown_data.tables.back().rows.back().elements.back().parts.pop_back();
}

void GuiMarkDownPage::AddElementPart()
{
    m_markdown_data.tables.back().rows.back().elements.back().parts.emplace_back();
}

void GuiMarkDownPage::AddElement()
{
    m_markdown_data.tables.back().rows.back().elements.emplace_back();
    AddElementPart();
}

void GuiMarkDownPage::AddRow()
{
    m_markdown_data.tables.back().rows.emplace_back();
    AddElement();
}

void GuiMarkDownPage::AddTable()
{
    CleanupLastEmptyElementpart();
    m_markdown_data.tables.emplace_back();
    AddRow();
}


void GuiMarkDownPage::AddInitialEmptyElement()
{
    if (m_markdown_data.tables.empty())
        AddTable();

    if (m_markdown_data.tables.back().rows.empty())
        AddRow();

    if (m_markdown_data.tables.back().rows.back().elements.empty())
        AddElement();
}

bool GuiMarkDownPage::IsLastElementPartEmpty()
{
    AddInitialEmptyElement();
    if (m_markdown_data.tables.back().rows.back().elements.back().parts.back().type == element_type::NONE &&
        m_markdown_data.tables.back().rows.back().elements.back().parts.back().text.empty())
    {
        return true;
    }
    return false;
}

bool GuiMarkDownPage::ParseLink()
{
    auto starting_index = m_index;
    UInt8 bracket_open_counter = 1;
    UInt8 curly_open_counter = 0;
    bool link_text_just_closed = false;

    md_element_part link_element_part;
    link_element_part.type = element_type::LINK;

    while (m_index < m_markdown_text.size())
    {
        auto chr = m_markdown_text.at(m_index);
        m_index++;
        if (chr == '\n')
            break;

        switch (chr)
        {
        case '[': // link text
        {
            bracket_open_counter++;
            // TODO: add internal link here later
            break;
        }
        case ']':
        {
            bracket_open_counter--;
            
            if (bracket_open_counter==0)
                link_text_just_closed = true;

            break;
        }
        case '(': // link url
        {
            if (!link_text_just_closed)
            {
                m_index = starting_index++; // not a link
                return false;
            }

            curly_open_counter++;
            break;
        }
        case ')':
        {
            curly_open_counter--;
            AddInitialEmptyElement(); // at least needs an empty element
            m_markdown_data.tables.back().rows.back().elements.back().parts.push_back(std::move(link_element_part));
            return true;
        }
        default:
        {
            if (bracket_open_counter)
                link_element_part.text += chr;
            else if (curly_open_counter)
                link_element_part.link += chr;

            break;
        }
        }
    }

    m_index = starting_index++; // link unsuccessfully closed, return
    return false;
}

bool GuiMarkDownPage::ParseTable()
{
    // TODO: test markdown code on real-world situations

    // supported md table format:
    // |text|text|
    // |----|----|
    // |val1|val2|
    // |val3|val4|

    auto starting_index = m_index;
    bool element_open = false;
    UInt8 number_of_columns = 0;
    size_t table_row_index = 0;
    size_t table_col_index = 0;
    AddTable();

    m_markdown_data;
    while (m_index < m_markdown_text.size())
    {
        auto chr = m_markdown_text.at(m_index);
        m_index++; 

        auto test_string = m_markdown_text.substr(m_index);


        switch (chr)
        {
        case '|':
        {
            if (table_row_index == 0)
            {
                number_of_columns++;
            }
            

            // new table element
            if (table_col_index != 0 && table_row_index!=1 && m_index < m_markdown_text.size() && !(m_markdown_text.at(m_index)=='\n'))
            {
                AddElement();
            }
            table_col_index++;
            break;
        }
        case '[':
        {
            ParseLink();
            break;
        }
        case '\n':
        {
            if (table_row_index!=1) // don't draw second markdown table row
                AddRow();
            
            // end of table conditions
            if (m_index == m_markdown_text.size())
                return true;

            if (m_markdown_text.at(m_index) != '|')
                return true;

            table_col_index = 0;
            table_row_index++;
            break;
        }
        default:
        {
            if (table_row_index != 1)
                m_markdown_data.tables.back().rows.back().elements.back().parts.back().text += chr;
            break;
        }
        }
    }
    return true;
}

bool GuiMarkDownPage::ParseCodeBlock()
{
    return true;
}

void GuiMarkDownPage::ParseDropDown()
{

}

void GuiMarkDownPage::ParseIndentation()
{

}

void GuiMarkDownPage::ParseInlineHtml()
{

}

void GuiMarkDownPage::ParseEmphasis()
{

}

bool GuiMarkDownPage::IsInlineHtml()
{
    return false;
}

bool GuiMarkDownPage::IsDropDown()
{
    return false;
}

bool GuiMarkDownPage::IsEmphasis()
{
    return false;
}

void GuiMarkDownPage::ParseHeading()
{
    bool in_heading_counting = true;
    UInt8 heading_counter = 0;

    while (m_index < m_markdown_text.size())
    {
        auto chr = m_markdown_text.at(m_index);
        m_index++;

        auto test_string = m_markdown_text.substr(m_index);

        if (in_heading_counting && heading_counter && chr != '#')
        {
            //AddInitialEmptyElement();
            // add header indicator for table row
            if (!IsLastElementPartEmpty())
                m_markdown_data.tables.back().rows.back().elements.back().parts.emplace_back(heading_counter == 1 ? element_type::HEADING_1 : element_type::HEADING_2, false, false, 0, "", "");
            else
            {
                m_markdown_data.tables.back().rows.back().elements.back().parts.back().type = heading_counter == 1 ? element_type::HEADING_1 : element_type::HEADING_2;
            }

            in_heading_counting = false;
        }

        if (chr == '\n')
            break;

        switch (chr)
        {
        case '#':
        {
            heading_counter++;
            continue;
        }
        case '[': // link in header
        {
            auto link_is_valid = ParseLink();
            if (link_is_valid)
            {
                m_markdown_data.tables.back().rows.back().elements.back().parts.emplace_back();
                m_index--;
            }
            else
                m_markdown_data.tables.back().rows.back().elements.back().parts.back().text += chr;
            break;
        }
        }
    }
}

void GuiMarkDownPage::Parse()
{
    // TODO: work with begins and ends
    m_markdown_data.tables.clear();
    m_index = 0;
    bool new_line = true;
    AddTable();

    for (m_index; m_index < m_markdown_text.size(); m_index++)
    {
        auto chr = m_markdown_text.at(m_index);
        
        auto test_string = m_markdown_text.substr(m_index);

        switch (chr) 
        {
        case '#': // header
        { 
            if (new_line)
            {
                ParseHeading();
                continue;
            }
            break;
        } 
        case '`': { ParseCodeBlock(); continue; } // code block
        case ' ': // indentation
        { 
            if (new_line)
            {
                ParseIndentation();
                continue;
            }
            break;
        } 
        case '*': // emphasis or strong emphasis (bold)
        { 
            if (IsEmphasis())
                ParseEmphasis(); 
            continue; 
        } 
        case '|': // table
        { 
            ParseTable();
            continue;
        } 
        case '<': // dropdown or inline html
        { 
            if (IsDropDown())
                ParseDropDown();
            else if (IsInlineHtml())
                ParseInlineHtml();
            
            break; 
        }
        case '[': // link
        {
            break;
        }
        case '!': // image
        {
            break;
        }
        case '\n':
        {
            new_line = true;
            continue;
        }
        default: { break; }
        }
        new_line = false;

        if (m_markdown_data.tables.empty())
            AddTable();

        if (m_markdown_data.tables.back().rows.empty())
            AddRow();

        if (m_markdown_data.tables.back().rows.back().elements.empty())
            AddElement();

        if (m_markdown_data.tables.back().rows.back().elements.back().parts.empty())
            AddElementPart();

        m_markdown_data.tables.back().rows.back().elements.back().parts.back().text += chr;
    }
}

void GuiDetailPages::ClearSpecificDetailPages(bool general, bool all_properties, bool explore_properties, bool value_info, bool source_description, bool configuration)
{
    if (general)
        m_GeneralProperties.clear();

    /*if (all_properties)
        m_AllProperties.clear();

    if (explore_properties)
        m_ExploreProperties.clear();

    if (value_info)
        m_ValueInfo.clear();

    if (source_description)
        m_SourceDescription.clear();

    if (configuration)
        m_Configuration.clear();*/
}

void GuiDetailPages::UpdateGeneralProperties(GuiState& state)
{
    clear();
    SuspendTrigger::Resume();
    InterestPtr<TreeItem*> tmpInterest = state.GetCurrentItem()->IsFailed() || state.GetCurrentItem()->WasFailed() ? nullptr : state.GetCurrentItem();
    auto xmlOut = (std::unique_ptr<OutStreamBase>)XML_OutStream_Create(&m_Buff, OutStreamBase::ST_HTM, "", NULL);
    auto result = DMS_TreeItem_XML_DumpGeneral(state.GetCurrentItem(), xmlOut.get(), true); // TODO: use result
    m_Buff.InterpretBytes(m_GeneralProperties); // Create detail page from html stream*/
    
    /*// MD
    //auto mdOut = (std::unique_ptr<OutStreamBase>)XML_OutStream_Create(&m_Buff, OutStreamBase::ST_MD, "", NULL);
    auto mdOut = std::unique_ptr<OutStreamBase>( XML_OutStream_Create(&m_Buff, OutStreamBase::ST_MD, "", NULL) );
    auto result = DMS_TreeItem_XML_DumpGeneral(state.GetCurrentItem(), mdOut.get(), true); // TODO: use result
    auto general_string = m_Buff.InterpretBytesAsString();

    m_GeneralProperties_MD.release();
    m_GeneralProperties_MD = std::make_unique<GuiMarkDownPage>(general_string);

    //StringToTable(general_string, m_GeneralProperties);
    m_Buff.Reset();*/
}

void GuiDetailPages::clear()
{
    DMS_ExplainValue_Clear();
    ClearSpecificDetailPages(true, true, true, true, true, true);
}

void GuiDetailPages::UpdateAllProperties(GuiState& state)
{
    clear();
    InterestPtr<TreeItem*> tmpInterest = state.GetCurrentItem()->IsFailed() || state.GetCurrentItem()->WasFailed() ? nullptr : state.GetCurrentItem();
    auto xmlOut = (std::unique_ptr<OutStreamBase>)XML_OutStream_Create(&m_Buff, OutStreamBase::ST_HTM, "", NULL);
    auto result = DMS_TreeItem_XML_DumpAllProps(state.GetCurrentItem(), xmlOut.get(), false);
    //m_Buff.InterpretBytes(m_AllProperties); // Create detail page from html stream
    m_Buff.Reset();
}

void GuiDetailPages::UpdateExploreProperties(GuiState& state)
{
    clear();
    InterestPtr<TreeItem*> tmpInterest = state.GetCurrentItem()->IsFailed() || state.GetCurrentItem()->WasFailed() ? nullptr : state.GetCurrentItem();
    auto xmlOut = (std::unique_ptr<OutStreamBase>)XML_OutStream_Create(&m_Buff, OutStreamBase::ST_HTM, "", NULL);
    DMS_TreeItem_XML_DumpExplore(state.GetCurrentItem(), xmlOut.get(), true);
    //m_Buff.InterpretBytes(m_ExploreProperties); // Create detail page from html stream
    m_Buff.Reset();
}

void GuiDetailPages::UpdateStatistics(GuiState& state)
{
    /*clear();
    InterestPtr<TreeItem*> tmpInterest = state.GetCurrentItem()->IsFailed() || state.GetCurrentItem()->WasFailed() ? nullptr : state.GetCurrentItem();
    std::string statistics_string = DMS_NumericDataItem_GetStatistics(state.GetCurrentItem(), nullptr);
    StringToTable(statistics_string, m_Statistics, ":");*/
}

auto GetIndexFromDPVIsActionString(const std::string &dpvi_str) -> UInt64
{
    return std::stoll(dpvi_str.substr(11, dpvi_str.size()));
}

bool GuiDetailPages::UpdateValueInfo(GuiState& state)
{
    clear();
    std::string result_string = "Omitted in alpha version.";
    //StringToTable(result_string, m_ValueInfo);
    return true;


    auto current_view_action = *state.TreeItemHistoryList.GetCurrentIterator();
    if (!current_view_action.sAction.contains("dp.vi.attr"))
        return true;


    InterestPtr<TreeItem*> tmpInterest = current_view_action.tiContext->IsFailed() || current_view_action.tiContext->WasFailed() ? nullptr : current_view_action.tiContext;
    auto xmlOut = (std::unique_ptr<OutStreamBase>)XML_OutStream_Create(&m_Buff, OutStreamBase::ST_HTM, "", NULL);
    auto dpvi_index = GetIndexFromDPVIsActionString(current_view_action.sAction);
    auto result = DMS_DataItem_ExplainAttrValueToXML(AsDataItem(current_view_action.tiContext), xmlOut.get(), dpvi_index, NULL, true);

    if (!result)
        return true;

    //m_Buff.InterpretBytes(m_ValueInfo); // Create detail page from html stream
    m_Buff.Reset();
    // TODO: See uDMSTreeViewFunctions.pas line 254
//DP_ValueInfo:
//    case funcID of
//    1: Result: = DMS_DataItem_ExplainAttrValueToXML(ti, xmlOut, p0, sExtraInfo, bShowHidden);
//    2: Result: = DMS_DataItem_ExplainGridValueToXML(ti, xmlOut, p1, p2, sExtraInfo, bShowHidden);

}

void GuiDetailPages::UpdateConfiguration(GuiState& state)
{
    clear();
    InterestPtr<TreeItem*> tmpInterest = state.GetCurrentItem()->IsFailed() || state.GetCurrentItem()->WasFailed() ? nullptr : state.GetCurrentItem();
    auto xmlOut = (std::unique_ptr<OutStreamBase>)XML_OutStream_Create(&m_Buff, OutStreamBase::ST_DMS, "DMS", NULL);
    DMS_TreeItem_XML_Dump(state.GetCurrentItem(), xmlOut.get());
    auto conf_str = m_Buff.InterpretBytesAsString();
    //StringToTable(conf_str, m_Configuration);
    m_Buff.Reset();
}

void GuiDetailPages::UpdateSourceDescription(GuiState& state)
{
    clear();
    InterestPtr<TreeItem*> tmpInterest = state.GetCurrentItem()->IsFailed() || state.GetCurrentItem()->WasFailed() ? nullptr : state.GetCurrentItem();
    std::string source_descr_string = TreeItem_GetSourceDescr(state.GetCurrentItem(), state.SourceDescrMode, true).c_str();
    //StringToTable(source_descr_string, m_SourceDescription);
    auto test = std::string(DMS_TreeItem_GetExpr(state.GetCurrentItem()));
}

void GuiDetailPages::OnViewAction(  const TreeItem* tiContext,
                    CharPtr     sAction,
                    Int32         nCode,
                    Int32             x,
                    Int32             y,
                    bool   doAddHistory,
                    bool          isUrl,
                    bool	mustOpenDetailsPage)
{
    PostEmptyEventToGLFWContext();

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

auto GuiDetailPages::GetDetailPagesDockNode(GuiState& state) -> ImGuiDockNode*
{
    auto ctx = ImGui::GetCurrentContext();
    ImGuiDockContext* dc = &ctx->DockContext;
    auto dockspace_docknode = (ImGuiDockNode*)dc->Nodes.GetVoidPtr(state.dockspace_id);
    ImGuiDockNode* detail_pages_docknode = nullptr;
    if (dockspace_docknode->ChildNodes[0])
        detail_pages_docknode = dockspace_docknode->ChildNodes[0]->ChildNodes[1]->ChildNodes[1];

    return detail_pages_docknode;
}

void GuiDetailPages::Collapse(ImGuiDockNode* detail_pages_docknode)
{
    if (!detail_pages_docknode)
        return;

    m_active_tab = DetailPageActiveTab::None;
    auto window_size = ImGui::GetWindowSize();
    ImGui::DockBuilderSetNodeSize(detail_pages_docknode->ID, ImVec2(m_min_size, window_size.y));
}

void GuiDetailPages::Expand(DetailPageActiveTab tab, ImGuiDockNode* detail_pages_docknode)
{
    m_active_tab = tab;
    auto window_size = ImGui::GetWindowSize();
    ImGui::DockBuilderSetNodeSize(detail_pages_docknode->ID, ImVec2(m_expanded_size, window_size.y));
}

void GuiDetailPages::CollapseOrExpand(GuiState& state, DetailPageActiveTab tab)
{
    ImGuiDockNode* detail_pages_docknode = GetDetailPagesDockNode(state);
    if (!detail_pages_docknode)
        return;

    if (m_active_tab == tab)
        Collapse(detail_pages_docknode);
    else
        Expand(tab, detail_pages_docknode);

}

void SetButtonColor(DetailPageActiveTab active, DetailPageActiveTab current)
{
    if (active == current)
        ImGui::PushStyleColor(ImGuiCol_Button, IM_COL32(66, 150, 250, 79));
    else
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.f, 0.f, 0.f, 0.f));
    return;
}

void GuiDetailPages::DrawPinButton()
{
    ImGui::SetCursorPos(ImVec2(ImGui::GetWindowContentRegionMax().x - 20, ImGui::GetCursorPos().y));
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.f, 0.f, 0.f, 0.f));
    if (ImGui::Button(m_pinned ? ICON_RI_PIN_ON : ICON_RI_PIN_OFF))
        m_pinned = !m_pinned; // toggle
    ImGui::PopStyleColor();
}

void GuiDetailPages::DrawTabButton(GuiState& state, DetailPageActiveTab tab, std::string_view icon, std::string_view text)
{
    ImGui::SetCursorPos(ImVec2(ImGui::GetWindowContentRegionMax().x - 20, ImGui::GetCursorPos().y));
    SetButtonColor(m_active_tab, tab);
    if (ImGui::Button(icon.data()))
        CollapseOrExpand(state, tab);
    ImGui::PopStyleColor();
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
        ImGui::SetTooltip(text.data());
}

void GuiDetailPages::DrawTabbar(GuiState& state)
{
    auto backup_cursor_pos = ImGui::GetCursorPos();
    DrawPinButton();
    DrawTabButton(state, DetailPageActiveTab::General, ICON_RI_GENERAL, "General");
    DrawTabButton(state, DetailPageActiveTab::Explore, ICON_RI_FIND, "Explore");
    DrawTabButton(state, DetailPageActiveTab::Properties, ICON_RI_PROPERTIES, "Properties");
    DrawTabButton(state, DetailPageActiveTab::Configuration, ICON_RI_CODE, "Configuration");

    ImGui::SetCursorPos(backup_cursor_pos);
}

void GuiDetailPages::DrawContent(GuiState& state)
{
    // detail page content area if necessary
    ImGui::BeginChild("DetailPageContentArea", ImVec2(-20, 0), false, ImGuiWindowFlags_NoScrollbar);

    switch (m_active_tab)
    {
    case DetailPageActiveTab::General:
    {
        if (state.GetCurrentItem())
        {
            if (m_GeneralProperties.empty())
                UpdateGeneralProperties(state);
            DrawProperties(state, m_GeneralProperties);
        }
        break;
    }
    case DetailPageActiveTab::Explore:
    {
        if (state.GetCurrentItem())
        {
            //if (m_ExploreProperties.empty())
            //    UpdateExploreProperties(state);
            ////DrawProperties(state, m_ExploreProperties);
        }
        break;
    }
    case DetailPageActiveTab::Properties:
    {
        if (state.GetCurrentItem())
        {
            //if (m_AllProperties.empty())
            //   UpdateAllProperties(state);
            ////DrawProperties(state, m_AllProperties);
        }
        break;
    }
    case DetailPageActiveTab::Configuration:
    {
        if (state.GetCurrentItem())
        {
        //   if (m_Configuration.empty())
        //       UpdateConfiguration(state);
        //   //DrawProperties(state, m_Configuration);
        }
        break;
    }
    default: { break; };
    }

    ImGui::EndChild();
}

void GuiDetailPages::ProcessEvents(GuiState &state)
{
    auto event_queues = GuiEventQueues::getInstance();
    if (event_queues->DetailPagesEvents.HasEvents()) // new current item
    {
        auto event = event_queues->DetailPagesEvents.Pop();
        switch (event)
        {
        case GuiEvents::UpdateCurrentItem:
        {
            if (!m_pinned)
            {
                Collapse(GetDetailPagesDockNode(state));
                m_active_tab = DetailPageActiveTab::None;
            }

            /*m_GeneralProperties.clear();
            m_AllProperties.clear();
            m_ExploreProperties.clear();
            m_Configuration.clear();
            m_ValueInfo.clear();
            m_SourceDescription.clear();*/
            break;
        }
        default:    break;
        }
    }
}

void GuiDetailPages::Update(bool* p_open, GuiState& state)
{
    if (!ImGui::Begin("Detail Pages", p_open, ImGuiWindowFlags_None | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoMove))
    {
        ImGui::End();
        return;
    }

    if (!m_is_docking_initialized)
    {
        auto detail_pages_docknode = GetDetailPagesDockNode(state);
        if (detail_pages_docknode)
        {
            AutoHideWindowDocknodeTabBar(m_is_docking_initialized);
            //Collapse(detail_pages_docknode);
            Expand(DetailPageActiveTab::General, detail_pages_docknode); // testing purposes
        }
    }

    if (ImGui::IsWindowHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
        SetKeyboardFocusToThisHwnd();

    ProcessEvents(state);

    DrawTabbar(state);
    
    if (m_active_tab != DetailPageActiveTab::None)
        DrawContent(state);

    ImGui::End();
}