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

/*void GuiOutStreamBuff::InterpretTag(table_data& tableProperties)
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

}*/

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

/*void GuiOutStreamBuff::InterpretBytes(table_data& tableProperties)
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
            //InterpretTag(tableProperties);
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
}*/

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

// Markdown dump of DetailPages
auto GetTreeItemMarkdownPageTitle(const TreeItem* current_item) -> std::string
{
    return std::string("# ") + current_item->GetName().c_str() + "\n";
}

bool TreeItemToMarkdownPageGeneral(const TreeItem* current_item, bool showAll)
{
    std::string ti_md_page_general = "";
    ti_md_page_general += GetTreeItemMarkdownPageTitle(current_item);
    // fullname
    // progress state // optional: #if defined(MG_DEBUG)
    // failstate      // optional
    // failreason     // optional
    // PartOfTemplate // optional
    // Label          // optional
    // Descr          // optional
    // ValuesType
    // ValuesComposition
    // cdf
    return true;
}

GuiMarkDownPage::GuiMarkDownPage(std::string_view markdown_text)
{
    Parse(markdown_text);
}

void GuiMarkDownPage::Parse(std::string_view markdown_text)
{
    m_data.clear();

    for (size_t i=0; i<markdown_text.size(); i++)
    {
        auto chr = markdown_text.at(i);
        // If we're at the beginning of the line, count any spaces
        if (m_line.isLeadingSpace)
        {
            if (chr == ' ')
            {
                ++m_line.leadSpaceCount;
                continue;
            }
            else
            {
                m_line.isLeadingSpace = false;
                m_line.lastRenderPosition = i - 1;
                if ((chr == '*') && (m_line.leadSpaceCount >= 2))
                {
                    if (((int)markdown_text.size() > i + 1) && (markdown_text[i + 1] == ' '))    // space after '*'
                    {
                        m_line.isUnorderedListStart = true;
                        ++i;
                        ++m_line.lastRenderPosition;
                    }
                    // carry on processing as could be emphasis
                }
                else if (chr == '#')
                {
                    m_line.headingCount++;
                    bool bContinueChecking = true;
                    int j = i;
                    while (++j < (int)markdown_text.size() && bContinueChecking)
                    {
                        chr = markdown_text[j];
                        switch (chr)
                        {
                        case '#':
                            m_line.headingCount++;
                            break;
                        case ' ':
                            m_line.lastRenderPosition = j - 1;
                            i = j;
                            m_line.isHeading = true;
                            bContinueChecking = false;
                            break;
                        default:
                            m_line.isHeading = false;
                            bContinueChecking = false;
                            break;
                        }
                    }
                    if (m_line.isHeading)
                    {
                        // reset emphasis status, we do not support emphasis around headers for now
                        m_emphasis = MarkDownEmphasis();
                        continue;
                    }
                }
            }
        }

        // Test to see if we have a link
        switch (m_link.state)
        {
        case MarkDownLink::NO_LINK:
            if (chr == '[' && !m_line.isHeading) // we do not support headings with links for now
            {
                m_link.state = MarkDownLink::HAS_SQUARE_BRACKET_OPEN;
                m_link.text.start = i + 1;
                if (i > 0 && markdown_text[i - 1] == '!')
                {
                    m_link.isImage = true;
                }
            }
            break;
        case MarkDownLink::HAS_SQUARE_BRACKET_OPEN:
            if (chr == ']')
            {
                m_link.state = MarkDownLink::HAS_SQUARE_BRACKETS;
                m_link.text.stop = i;
            }
            break;
        case MarkDownLink::HAS_SQUARE_BRACKETS:
            if (chr == '(')
            {
                m_link.state = MarkDownLink::HAS_SQUARE_BRACKETS_ROUND_BRACKET_OPEN;
                m_link.url.start = i + 1;
                m_link.num_brackets_open = 1;
            }
            break;
        case MarkDownLink::HAS_SQUARE_BRACKETS_ROUND_BRACKET_OPEN:
            if (chr == '(')
            {
                ++m_link.num_brackets_open;
            }
            else if (chr == ')')
            {
                --m_link.num_brackets_open;
            }
            if (m_link.num_brackets_open == 0)
            {
                // reset emphasis status, we do not support emphasis around links for now
                m_emphasis = MarkDownEmphasis();
                // render previous line content
                m_line.lineEnd = m_link.text.start - (m_link.isImage ? 2 : 1);
                //RenderLine(markdown_text, m_line, textRegion, mdConfig_);
                m_line.leadSpaceCount = 0;
                m_link.url.stop = i;
                m_line.isUnorderedListStart = false;    // the following text shouldn't have bullets
                ImGui::SameLine(0.0f, 0.0f);
                if (m_link.isImage)   // it's an image, render it.
                {
                    // TODO: implement
                    /*bool drawnImage = false;
                    bool useLinkCallback = false;
                    if (mdConfig_.imageCallback)
                    {
                        MarkdownImageData imageData = mdConfig_.imageCallback({ markdown_ + link.text.start, link.text.size(), markdown_ + link.url.start, link.url.size(), mdConfig_.userData, true });
                        useLinkCallback = imageData.useLinkCallback;
                        if (imageData.isValid)
                        {
                            ImGui::Image(imageData.user_texture_id, imageData.size, imageData.uv0, imageData.uv1, imageData.tint_col, imageData.border_col);
                            drawnImage = true;
                        }
                    }
                    if (!drawnImage)
                    {
                        ImGui::Text("( Image %.*s not loaded )", link.url.size(), markdown_ + link.url.start);
                    }
                    if (ImGui::IsItemHovered())
                    {
                        if (ImGui::IsMouseReleased(0) && mdConfig_.linkCallback && useLinkCallback)
                        {
                            mdConfig_.linkCallback({ markdown_ + link.text.start, link.text.size(), markdown_ + link.url.start, link.url.size(), mdConfig_.userData, true });
                        }
                        if (link.text.size() > 0 && mdConfig_.tooltipCallback)
                        {
                            mdConfig_.tooltipCallback({ { markdown_ + link.text.start, link.text.size(), markdown_ + link.url.start, link.url.size(), mdConfig_.userData, true }, mdConfig_.linkIcon });
                        }
                    }*/
                }
                else                 // it's a link, render it.
                {
                    //textRegion.RenderLinkTextWrapped(markdown_ + link.text.start, markdown_ + link.text.start + link.text.size(), link, markdown_, mdConfig_, &linkHoverStart, false);
                }
                ImGui::SameLine(0.0f, 0.0f);
                // reset the link by reinitializing it
                m_link = MarkDownLink();
                m_line.lastRenderPosition = i;
                break;
            }
        }

        // Test to see if we have emphasis styling
        switch (m_emphasis.state)
        {
        case MarkDownEmphasis::NONE:
            if (m_link.state == MarkDownLink::NO_LINK && !m_line.isHeading)
            {
                int next = i + 1;
                int prev = i - 1;
                if ((chr == '*' || chr == '_')
                    && (i == m_line.lineStart
                        || markdown_text[prev] == ' '
                        || markdown_text[prev] == '\t') // empasis must be preceded by whitespace or line start
                    && (int)markdown_text.size() > next // emphasis must precede non-whitespace
                    && markdown_text[next] != ' '
                    && markdown_text[next] != '\n'
                    && markdown_text[next] != '\t')
                {
                    m_emphasis.state = MarkDownEmphasis::LEFT;
                    m_emphasis.sym = chr;
                    m_emphasis.text.start = i;
                    m_line.emphasisCount = 1;
                    continue;
                }
            }
            break;
        case MarkDownEmphasis::LEFT:
            if (m_emphasis.sym == chr)
            {
                ++m_line.emphasisCount;
                continue;
            }
            else
            {
                m_emphasis.text.start = i;
                m_emphasis.state = MarkDownEmphasis::MIDDLE;
            }
            break;
        case MarkDownEmphasis::MIDDLE:
            if (m_emphasis.sym == chr)
            {
                m_emphasis.state = MarkDownEmphasis::RIGHT;
                m_emphasis.text.stop = i;
                // pass through to case Emphasis::RIGHT
            }
            else
            {
                break;
            }
        case MarkDownEmphasis::RIGHT:
            if (m_emphasis.sym == chr)
            {
                if (m_line.emphasisCount < 3 && (i - m_emphasis.text.stop + 1 == m_line.emphasisCount))
                {
                    // render text up to emphasis
                    int lineEnd = m_emphasis.text.start - m_line.emphasisCount;
                    if (lineEnd > m_line.lineStart)
                    {
                        m_line.lineEnd = lineEnd;
                        //RenderLine(markdown_, line, textRegion, mdConfig_);
                        ImGui::SameLine(0.0f, 0.0f);
                        m_line.isUnorderedListStart = false;
                        m_line.leadSpaceCount = 0;
                    }
                    m_line.isEmphasis = true;
                    m_line.lastRenderPosition = m_emphasis.text.start - 1;
                    m_line.lineStart = m_emphasis.text.start;
                    m_line.lineEnd = m_emphasis.text.stop;
                    //RenderLine(markdown_, line, textRegion, mdConfig_);
                    ImGui::SameLine(0.0f, 0.0f);
                    m_line.isEmphasis = false;
                    m_line.lastRenderPosition = i;
                    m_emphasis = MarkDownEmphasis();
                }
                continue;
            }
            else
            {
                m_emphasis.state = MarkDownEmphasis::NONE;
                // render text up to here
                int start = m_emphasis.text.start - m_line.emphasisCount;
                if (start < m_line.lineStart)
                {
                    m_line.lineEnd = m_line.lineStart;
                    m_line.lineStart = start;
                    m_line.lastRenderPosition = start - 1;
                    //RenderLine(markdown_, m_line, textRegion, mdConfig_);
                    m_line.lineStart = m_line.lineEnd;
                    m_line.lastRenderPosition = m_line.lineStart - 1;
                }
            }
            break;
        }

        // handle end of line (render)
        if (chr == '\n')
        {
            // first check if the line is a horizontal rule
            m_line.lineEnd = i;
            if (m_emphasis.state == MarkDownEmphasis::MIDDLE && m_line.emphasisCount >= 3 &&
                (m_line.lineStart + m_line.emphasisCount) == i)
            {
                ImGui::Separator();
            }
            else
            {
                // render the line: multiline emphasis requires a complex implementation so not supporting
                //RenderLine(markdown_, m_line, textRegion, mdConfig_);
            }

            // reset the line and emphasis state
            m_line = MarkDownLine();
            m_emphasis = MarkDownEmphasis();

            m_line.lineStart = i + 1;
            m_line.lastRenderPosition = i;

            //textRegion.ResetIndent();

            // reset the link
            m_link = MarkDownLink();
        }
    }

    if (m_emphasis.state == MarkDownEmphasis::LEFT && m_line.emphasisCount >= 3)
    {
        ImGui::Separator();
    }
    else
    {
        // render any remaining text if last char wasn't 0
        if (markdown_text.size() && m_line.lineStart < (int)markdown_text.size() && markdown_text[m_line.lineStart] != 0)
        {
            // handle both null terminated and non null terminated strings
            m_line.lineEnd = (int)markdown_text.size();
            if (0 == markdown_text[m_line.lineEnd - 1])
            {
                --m_line.lineEnd;
            }
            //RenderLine(markdown_, m_line, textRegion, mdConfig_);
        }
    }
    
}

void GuiDetailPages::ClearSpecificDetailPages(bool general, bool all_properties, bool explore_properties, bool value_info, bool source_description, bool configuration)
{
    if (general)
        m_GeneralProperties.clear();

    if (all_properties)
        m_AllProperties.clear();

    if (explore_properties)
        m_ExploreProperties.clear();

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
    SuspendTrigger::Resume();
    InterestPtr<TreeItem*> tmpInterest = state.GetCurrentItem()->IsFailed() || state.GetCurrentItem()->WasFailed() ? nullptr : state.GetCurrentItem();
    /*auto xmlOut = (std::unique_ptr<OutStreamBase>)XML_OutStream_Create(&m_Buff, OutStreamBase::ST_HTM, "", NULL);
    auto result = DMS_TreeItem_XML_DumpGeneral(state.GetCurrentItem(), xmlOut.get(), true); // TODO: use result
    m_Buff.InterpretBytes(m_GeneralProperties); // Create detail page from html stream*/
    
    //auto mdOut = (std::unique_ptr<OutStreamBase>)XML_OutStream_Create(&m_Buff, OutStreamBase::ST_MD, "", NULL);
    auto mdOut = (std::unique_ptr<OutStreamBase>)XML_OutStream_Create(&m_Buff, OutStreamBase::ST_MD, "", NULL);
    auto result = DMS_TreeItem_XML_DumpGeneral(state.GetCurrentItem(), mdOut.get(), true); // TODO: use result
    auto general_string = m_Buff.InterpretBytesAsString();

    m_GeneralProperties_MD.release();
    m_GeneralProperties_MD = std::make_unique<GuiMarkDownPage>(general_string);

    //StringToTable(general_string, m_GeneralProperties);
    m_Buff.Reset();
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
            //DrawProperties(state, m_GeneralProperties);
        }
        break;
    }
    case DetailPageActiveTab::Explore:
    {
        if (state.GetCurrentItem())
        {
            if (m_ExploreProperties.empty())
                UpdateExploreProperties(state);
            //DrawProperties(state, m_ExploreProperties);
        }
        break;
    }
    case DetailPageActiveTab::Properties:
    {
        if (state.GetCurrentItem())
        {
            if (m_AllProperties.empty())
                UpdateAllProperties(state);
            //DrawProperties(state, m_AllProperties);
        }
        break;
    }
    case DetailPageActiveTab::Configuration:
    {
        if (state.GetCurrentItem())
        {
            if (m_Configuration.empty())
                UpdateConfiguration(state);
            //DrawProperties(state, m_Configuration);
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

            m_GeneralProperties.clear();
            m_AllProperties.clear();
            m_ExploreProperties.clear();
            m_Configuration.clear();
            m_ValueInfo.clear();
            m_SourceDescription.clear();
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
            Expand(DetailPageActiveTab::General, detail_pages_docknode);
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