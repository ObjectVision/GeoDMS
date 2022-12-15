#pragma once
#include <map>
#include <vector>
#include "GuiBase.h"
#include "ser/BaseStreamBuff.h"

enum class HTMLTagType
{
    BODY = 1,            // <BODY>
    TABLE = 2,           // <TABLE>
    TABLEROW = 3,        // <TR>
    TABLEDATA = 4,       // <TD>
    LINK = 5,            // <A>
    LINEBREAK = 6,
    HORIZONTALLINE = 7,
    HEADING = 8,         // <H1>...<H6>
    UNKNOWN = 10
};

enum class HTMLParserState
{
    TAGOPEN = 1,
    TAGCLOSE = 2,
    TEXT = 3,
    NONE = 4
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

class HTMLGuiComponentFactory : public OutStreamBuff
{
public:
    HTMLGuiComponentFactory();
    virtual ~HTMLGuiComponentFactory();

    void WriteBytes(const Byte* data, streamsize_t size) override;
    void InterpretBytes(std::vector<std::vector<PropertyEntry>>& properties);

    streamsize_t CurrPos() const override;
    bool AtEnd() const override { return false; }
    void Reset();
private:
    bool ReplaceStringInString(std::string& str, const std::string& from, const std::string& to);
    std::string CleanStringFromHtmlEncoding(std::string text_in);
    void InterpretTag(std::vector<std::vector<PropertyEntry>>& properties);
    bool IsOpenTag(UInt32 ind);
    std::string GetHrefFromTag();

    std::vector<char>             m_Buff;
    HTMLParserState               m_ParserState;
    std::map<HTMLTagType, UInt16> m_OpenTags;
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
    void DrawProperties(GuiState& state, std::vector<std::vector<PropertyEntry>> &properties);

    HTMLGuiComponentFactory                 m_Buff;
    std::vector<std::vector<PropertyEntry>> m_GeneralProperties;
    std::vector<std::vector<PropertyEntry>> m_AllProperties;
    std::vector<std::vector<PropertyEntry>> m_ExploreProperties;
    std::vector<std::vector<PropertyEntry>> m_FilteredStatistics;
    std::string                             m_Statistics;
    UInt8                                   m_ColumnIndex = 0;
};