#pragma once
#include <map>
#include <vector>
#include "GuiBase.h"
#include "ser/BaseStreamBuff.h"
#include "xml/XMLOut.h"
#include <imgui.h>
#include <imgui_internal.h>

enum class HTMLTagType
{
    UNKNOWN = 0,
    BODY = 1,            // <BODY>
    TABLE = 2,           // <TABLE>
    TABLEROW = 3,        // <TR>
    TABLEDATA = 4,       // <TD>
    LINK = 5,            // <A>
    LINEBREAK = 6,
    HORIZONTALLINE = 7,
    HEADING = 8,         // <H1>...<H6>
    COUNT = 9
};

enum class HTMLParserState
{
    NONE = 0,
    TAGOPEN = 1,
    TAGCLOSE = 2,
    TEXT = 3,
//    NONE = 4
};

class Tag
{
public:
    std::string text;
    HTMLTagType type = HTMLTagType::UNKNOWN;
    std::string bgcolor;
    static std::string href;
    int         colspan = -1;

    void clear()
    {
        // TODO: implement?
    }
};

class GuiOutStreamBuff : public OutStreamBuff
{
public:
    GuiOutStreamBuff();
    virtual ~GuiOutStreamBuff();
    void WriteBytes(const Byte* data, streamsize_t size) override;
    void InterpretBytes(TableData& tableProperties);
    auto InterpretBytesAsString() -> std::string;
    streamsize_t CurrPos() const override;
    bool AtEnd() const override { return false; }
    void Reset();
    std::vector<char>             m_Buff;

private:
    bool ReplaceStringInString(std::string& str, const std::string& from, const std::string& to);
    std::string CleanStringFromHtmlEncoding(std::string text_in);
    void InterpretTag(TableData& tableProperties);
    bool IsOpenTag(UInt32 ind);
    std::string GetHrefFromTag();
    HTMLParserState               m_ParserState = HTMLParserState::NONE;
    UInt16                        m_OpenTags[int(HTMLTagType::COUNT)] = {};
    Tag                           m_Tag;
    std::string                   m_Text;
};

enum class DetailPageActiveTab
{
    None,
    General,
    Explore,
    Properties,
    Configuration
};

class GuiMarkDownPage
{
public:
    GuiMarkDownPage(std::string_view markdown_text);
    void Update();
    void Parse();
    void Clear();

private:
    bool ParseLink();
    bool ParseTable();
    bool ParseCodeBlock();
    void ParseDropDown();
    void ParseIndentation();
    void ParseInlineHtml();
    void ParseEmphasis();
    bool IsInlineHtml();
    bool IsDropDown();
    bool IsEmphasis();
    void ParseHeading();

    void AddTable();    
    void AddRow();
    void AddElement();
    void AddElementPart();
    void CleanupLastEmptyElementpart();
    bool IsLastElementPartEmpty();
    void AddInitialEmptyElement();

    md_data m_markdown_data;
    //MarkDownLine m_line;
    //MarkDownEmphasis m_emphasis;
    //MarkDownLink m_link;

    size_t      m_index = 0;
    std::string m_markdown_text = "";
};

class GuiDetailPages
{
public:
    void clear();
    void Update(bool* p_open, GuiState& state);
    static void OnViewAction(const TreeItem* tiContext,
        CharPtr     sAction,
        Int32         nCode,
        Int32             x,
        Int32             y,
        bool   doAddHistory,
        bool          isUrl,
        bool	mustOpenDetailsPage);
private:
    void ProcessEvents(GuiState& state);
    void DrawPinButton();
    void DrawTabButton(GuiState& state, DetailPageActiveTab tab, std::string_view icon, std::string_view text);
    void DrawTabbar(GuiState& state);
    void DrawContent(GuiState& state);
    void ClearSpecificDetailPages(bool general = false, bool all_properties = false, bool explore_properties = false, bool value_info = false, bool source_description = false, bool configuration = false);
    void UpdateGeneralProperties(GuiState& state);
    void UpdateAllProperties(GuiState& state);
    void UpdateExploreProperties(GuiState& state);
    void UpdateStatistics(GuiState& state);
    bool UpdateValueInfo(GuiState& state);
    void UpdateConfiguration(GuiState& state);
    void UpdateSourceDescription(GuiState& state);
    auto GetDetailPagesDockNode(GuiState& state) -> ImGuiDockNode*;
    void Collapse(ImGuiDockNode* detail_pages_docknode);
    void Expand(DetailPageActiveTab tab, ImGuiDockNode* detail_pages_docknode);
    void CollapseOrExpand(GuiState& state, DetailPageActiveTab tab);

    GuiOutStreamBuff m_Buff;
    TableData m_GeneralProperties, m_ExploreProperties, m_AllProperties, m_ValueInfo, m_SourceDescription;
    std::string m_Configuration;
    /*md_data            m_AllProperties;
    md_data            m_ExploreProperties;
    md_data            m_ValueInfo;
    md_data            m_SourceDescription;*/

    std::unique_ptr<GuiMarkDownPage> m_GeneralProperties_MD;


    bool                    m_is_docking_initialized = false;
    bool                    m_pinned = true;
    DetailPageActiveTab     m_active_tab = DetailPageActiveTab::None;
    Float32                 m_min_size = 30.0f;
    Float32                 m_expanded_size = 500.0f;
};