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
        // TODO: implement?
    }
};

class HTMLGuiComponentFactory : public OutStreamBuff
{
public:

    HTMLGuiComponentFactory();
    virtual ~HTMLGuiComponentFactory();

    void WriteBytes(const Byte* data, streamsize_t size) override;
    auto InterpretBytes(TableData& tableProperties) -> void;
    auto InterpretBytesAsString() -> std::string;


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

class GuiDetailPages
{
public:
    auto clear() -> void;
    auto Update(bool* p_open, GuiState& state) -> void;
    auto static OnViewAction(const TreeItem* tiContext,
        CharPtr     sAction,
        Int32         nCode,
        Int32             x,
        Int32             y,
        bool   doAddHistory,
        bool          isUrl,
        bool	mustOpenDetailsPage) -> void;
private:
    auto ClearSpecificDetailPages(bool general=false, bool all_properties=false, bool explore_properties=false, bool statistics=false, bool value_info=false, bool source_description=false, bool configuration=false) -> void;
    auto UpdateGeneralProperties(GuiState& state) -> void;
    auto UpdateAllProperties(GuiState& state) -> void;
    auto UpdateExploreProperties(GuiState& state) -> void;
    auto UpdateStatistics(GuiState& state) -> void;
    bool UpdateValueInfo(GuiState& state);
    auto UpdateConfiguration(GuiState& state) -> void;
    auto UpdateSourceDescription(GuiState& state) -> void;
    auto StringToTable(std::string& input, TableData& result, std::string separator) -> void;
    auto DrawProperties(GuiState& state, TableData& properties) -> void;

    HTMLGuiComponentFactory m_Buff;
    TableData m_GeneralProperties;
    TableData m_AllProperties;
    TableData m_ExploreProperties;
    //TableData m_Statistics;
    TableData m_ValueInfo;
    TableData m_SourceDescription;
    TableData m_Configuration;
    bool is_docking_initialized = false;
};