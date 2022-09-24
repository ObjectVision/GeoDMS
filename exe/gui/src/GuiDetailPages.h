#pragma once
#include <map>
#include <vector>
#include "GuiBase.h"
#include "ser/BaseStreamBuff.h"

enum HTMLTagType
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

enum HTMLParserState
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
    HTMLTagType type;
    std::string bgcolor;
    static std::string href;
    int         colspan = -1;

    void clear()
    {
        int i = 1;
    }
};

enum PropertyNames
{
    P_NAME_NAME = 0,               // "name"
    P_FULLNAME_NAME = 1,        // "FullName"      
    P_STORAGENAME_NAME = 2,      // "StorageName"
    P_STORAGETYPE_NAME = 3,   // "StorageType"
    P_STORAGEREADONLY_NAME = 4,   // "StorageReadOnly"
    P_STORAGEOPTIONS_NAME = 5,  // "StorageOptions"
    P_DISABLESTORAGE_NAME = 6,    // "DisableStorage"
    P_KEEPDATA_NAME = 7,           // "KeepData"
    P_FREEDATA_NAME = 8,          // "FreeData"
    P_ISHIDDEN_NAME = 9,       // "IsHidden"
    P_INHIDDEN_NAME = 10,       // "InHidden"
    P_STOREDATA_NAME = 11,       // "StoreData"
    P_SYNCMODE_NAME = 12,      // "SyncMode"
    P_SOURCE_NAME = 13,       // "Source"
    P_FULL_SOURCE_NAME = 14,       // "FullSource"
    P_READ_ONLY_SM_NAME = 15,   // "ReadOnlyStorageManagers"
    P_WRITE_ONLY_SM_NAME = 16,   // "NonReadOnlyStorageManagers"
    P_ALL_SM_NAME = 17,    // "AllStorageManagers"
    P_EXPR_NAME = 18,    // "Expr"
    P_DESCR_NAME = 19,           // "Descr"
    P_ICHECK_NAME = 20,          // "IntegrityCheck"
    P_LABEL_NAME = 21,         // "Label"
    P_NRSUBITEMS_NAME = 22,     // "NrSubItems"
    P_ISCALCULABLE_NAME = 23,     // "HasCalculator"
    P_ISLOADABLE_NAME = 24,     // "IsLoadable"
    P_ISSTORABLE_NAME = 25,     // "IsStorable"
    P_DIALOGTYPE_NAME = 26,     // "DialogType"
    P_DIALOGDATA_NAME = 27,     // "DialogData"
    P_PARAMTYPE_NAME = 28,     // "ParamType"
    P_PARAMDATA_NAME = 29,      // "ParamData"
    P_CDF_NAME = 30,      // "cdf"
    P_URL_NAME = 31,           // "url"
    P_VIEW_ACTION_NAME = 32,        // "ViewAction"
    P_VIEW_DATA_NAME = 33,    // "ViewData"
    P_USERNAME_NAME = 34,     // "UserName"
    P_PASSWORD_NAME = 35,      // "Password"
    P_SQLSTRING_NAME = 36,      // "SqlString"
    P_TABLETYPE_NAME = 37,     // "TableType"
    P_USING_NAME = 38,     // "Using"
    P_EXPLICITSUPPLIERS_NAME = 39,  // "ExplicitSuppliers"
    P_ISTEMPLATE_NAME = 40,   // "IsTemplate"
    P_INTEMPLATE_NAME = 41,     // "InTemplate"
    P_ISENDOGENOUS_NAME = 42,    // "IsEndogeous"
    IP_SPASSOR_NAME = 43,  // "IsPassor"
    P_INENDOGENOUS_NAME = 44,	    // "InEndogeous"
    P_HASMISSINGVALUES_NAME = 45,  // "HasMissingValues"
    P_CONFIGSTORE_NAME = 46,  // "ConfigStore"
    P_CONFIGFILELINENR_NAME = 47,   // "ConfigFileLineNr"
    P_CONFIGFILECOLNR_NAME = 48,  // "ConfigFileColNr"
    P_CONFIGFILENAME_NAME = 49,  // "ConfigFileName"
    P_CASEDIR_NAME = 50,    // "CaseDir"
    P_FORMAT_NAME = 51,         // "Format"
    P_METRIC_NAME = 52,         // "Metric"
    P_PROJECTION_NAME = 53,         // "Projection"
    P_VALUETYPE_NAME = 54,     // "ValueType"
    P_ISPASSOR_NAME = 55   // "IsPassor"
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
    void InterpretBytes(bool direct_draw, std::vector<std::vector<PropertyEntry>>& properties);

    streamsize_t CurrPos() const override;
    bool AtEnd() const override { return false; }
    void Reset();
private:
    bool ReplaceStringInString(std::string& str, const std::string& from, const std::string& to);
    std::string CleanStringFromHtmlEncoding(std::string text_in);
    void InterpretTag(bool direct_draw, std::vector<std::vector<PropertyEntry>>& properties);
    bool IsOpenTag(UInt32 ind);
    std::string GetHrefFromTag();

    std::vector<char>             m_Buff;
    int                           m_refIndex;
    HTMLParserState               m_ParserState;
    std::map<HTMLTagType, UInt16> m_OpenTags;
    Tag                           m_Tag;
    std::string                   m_Text;
    UInt16                        m_ColumnIndex;
    GuiState                      m_State;
};

class GuiDetailPages : GuiBaseComponent
{
public:
	void Update(bool* p_open);
private:
    //std::string PropertyTypeToPropertyName(PropertyTypes pt);
    //void AddProperty(PropertyTypes pt);
    void UpdateGeneralProperties();
	GuiState                                m_State;
    HTMLGuiComponentFactory                 m_GeneralBuff;
    std::vector<std::vector<PropertyEntry>> m_GeneralProperties;
    UInt16                                  m_ColumnIndex;
};