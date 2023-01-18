#pragma once
#include <map>
#include <vector>
#include "GuiBase.h"
#include "ser/BaseStreamBuff.h"

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
        int i = 1;
    }
};

enum PropertyEntryType
{
    PET_SEPARATOR,
    PET_TEXT,
    PET_LINK,
    PET_HEADING
};

struct PropertyEntry
{
    PropertyEntryType   type;
    std::string         text;
};

using RowData = std::vector<PropertyEntry>;
using TableData = std::vector<RowData>;

class HTMLGuiComponentFactory : public OutStreamBuff
{
public:

    HTMLGuiComponentFactory();
    virtual ~HTMLGuiComponentFactory();

    void WriteBytes(const Byte* data, streamsize_t size) override;
    void InterpretBytes(TableData& tableProperties);

    streamsize_t CurrPos() const override;
    bool AtEnd() const override { return false; }
    void Reset();
private:
    bool ReplaceStringInString(std::string& str, const std::string& from, const std::string& to);
    std::string CleanStringFromHtmlEncoding(std::string text_in);
    void InterpretTag(TableData& tableProperties);
    bool IsOpenTag(UInt32 ind);
    std::string GetHrefFromTag();

    std::vector<char>             m_Buff;
    HTMLParserState               m_ParserState = HTMLParserState::NONE;
    UInt16                        m_OpenTags[int(HTMLTagType::COUNT)] = {};
    Tag                           m_Tag;
    std::string                   m_Text;
};

class GuiDetailPages : GuiBaseComponent
{
public:
	void Update(bool* p_open, GuiState& state);
private:
    void UpdateGeneralProperties(GuiState& state);
    void UpdateAllProperties(GuiState& state);
    void UpdateExploreProperties(GuiState& state);
    void UpdateStatistics(GuiState& state);
    void FilterStatistics();
    void DrawProperties(GuiState& state, TableData& properties);

    HTMLGuiComponentFactory m_Buff;
    TableData m_GeneralProperties;
    TableData m_AllProperties;
    TableData m_ExploreProperties;
    TableData m_FilteredStatistics;
    std::string m_Statistics;
    std::string m_Configuration;
    std::string m_SourceDescription;
    std::string m_Metadata;
};