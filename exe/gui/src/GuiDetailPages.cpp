#include <imgui.h>
#include <imgui_internal.h>
#include "GuiDetailPages.h"
#include "TicInterface.h"
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

std::string HTMLGuiComponentFactory::GetHrefFromTag()
{
    std::string href = "";

    auto test = m_Tag.text.substr(9, 14);
    if (m_Tag.text.substr(9, 14) == "dms:dp.general")
        href = m_Tag.text.substr(24, m_Tag.text.length() - 26);
        
    return href;

}

void  HTMLGuiComponentFactory::InterpretTag()
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
        ImGui::BeginTable(" ", 6, ImGuiTableFlags_None, ImVec2(0.0f, ImGui::GetTextLineHeightWithSpacing() * 8));
        m_OpenTags[HTMLTagType::TABLE]++;
    }
    else if (m_Tag.text.substr(0, 3) == "<TR")
    {
        ImGui::TableNextRow();
        m_ColumnIndex = 0;
        m_OpenTags[HTMLTagType::TABLEROW]++;
    }
    else if (m_Tag.text.substr(0, 3) == "<TD")
        m_OpenTags[HTMLTagType::TABLEDATA]++;
    else if (m_Tag.text.substr(0, 3) == "<H2")
        int i = 1;

    // close tags
    else if (m_Tag.text == "</BODY>")
        m_OpenTags[HTMLTagType::BODY]--;
    else if (m_Tag.text == "</H2>")
    {
        ImGui::Text(m_Text.c_str());
        m_Text.clear();
    }
    else if (m_Tag.text == "</TABLE>")
    {
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
            ImGui::TableSetColumnIndex(m_ColumnIndex);
            if (!m_Tag.href.empty())
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
            else
            {
                ImGui::Text(m_Text.c_str());
            }
            m_Text.clear();
            m_ColumnIndex++;
        }
    }
    else if (m_Tag.text == "<HR/>")
        ImGui::Separator();
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

void HTMLGuiComponentFactory::InterpretBytes()
{
    m_refIndex = 0;
    m_ParserState = HTMLParserState::NONE;
    UInt32 ind = 0;
    for (auto chr : m_Buff)
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
            InterpretTag();
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

std::string GuiDetailPages::PropertyTypeToPropertyName(PropertyTypes pt)
{
    switch (pt)
    {
    case P_NAME_NAME:               return NAME_NAME;
    case P_FULLNAME_NAME:           return FULLNAME_NAME;
    case P_STORAGENAME_NAME:        return NAME_NAME;
    case P_STORAGETYPE_NAME:        return STORAGETYPE_NAME;
    case P_STORAGEREADONLY_NAME:    return STORAGEREADONLY_NAME;
    case P_STORAGEOPTIONS_NAME:     return STORAGEOPTIONS_NAME;
    case P_DISABLESTORAGE_NAME:     return DISABLESTORAGE_NAME;
    case P_KEEPDATA_NAME:           return KEEPDATA_NAME;
    case P_FREEDATA_NAME:           return FREEDATA_NAME;
    case P_ISHIDDEN_NAME:           return ISHIDDEN_NAME;
    case P_INHIDDEN_NAME:           return INHIDDEN_NAME;
    case P_STOREDATA_NAME:          return STOREDATA_NAME;
    case P_SYNCMODE_NAME:           return SYNCMODE_NAME;
    case P_SOURCE_NAME:             return SOURCE_NAME;
    case P_FULL_SOURCE_NAME:        return FULL_SOURCE_NAME;
    case P_READ_ONLY_SM_NAME:       return READ_ONLY_SM_NAME;
    case P_WRITE_ONLY_SM_NAME:      return WRITE_ONLY_SM_NAME;
    case P_ALL_SM_NAME:             return ALL_SM_NAME;
    case P_EXPR_NAME:               return EXPR_NAME;
    case P_DESCR_NAME:              return DESCR_NAME;
    case P_ICHECK_NAME:             return ICHECK_NAME;
    case P_LABEL_NAME:              return LABEL_NAME;
    case P_NRSUBITEMS_NAME:         return NRSUBITEMS_NAME;
    case P_ISCALCULABLE_NAME:       return ISCALCULABLE_NAME;
    case P_ISLOADABLE_NAME:         return ISLOADABLE_NAME;
    case P_ISSTORABLE_NAME:         return ISSTORABLE_NAME;
    case P_DIALOGTYPE_NAME:         return DIALOGTYPE_NAME;
    case P_DIALOGDATA_NAME:         return DIALOGDATA_NAME;
    case P_PARAMTYPE_NAME:          return PARAMTYPE_NAME;
    case P_PARAMDATA_NAME:          return PARAMDATA_NAME;
    case P_CDF_NAME:                return CDF_NAME;
    case P_URL_NAME:                return URL_NAME;
    case P_VIEW_ACTION_NAME:        return VIEW_ACTION_NAME;
    case P_VIEW_DATA_NAME:          return VIEW_DATA_NAME;
    case P_USERNAME_NAME:           return USERNAME_NAME;
    case P_PASSWORD_NAME:           return PASSWORD_NAME;
    case P_SQLSTRING_NAME:          return SQLSTRING_NAME;
    case P_TABLETYPE_NAME:          return TABLETYPE_NAME;
    case P_USING_NAME:              return USING_NAME;
    case P_EXPLICITSUPPLIERS_NAME:  return EXPLICITSUPPLIERS_NAME;
    case P_ISTEMPLATE_NAME:         return ISTEMPLATE_NAME;
    case P_ISENDOGENOUS_NAME:       return ISENDOGENOUS_NAME;
    case P_ISPASSOR_NAME:     return ISPASSOR_NAME;
    case P_INENDOGENOUS_NAME: return INENDOGENOUS_NAME;
    case P_HASMISSINGVALUES_NAME:     return HASMISSINGVALUES_NAME;
    case P_CONFIGSTORE_NAME: return CONFIGSTORE_NAME;
    case P_CONFIGFILELINENR_NAME:     return CONFIGFILELINENR_NAME;
    case P_CONFIGFILECOLNR_NAME: return CONFIGFILECOLNR_NAME;
    case P_CONFIGFILENAME_NAME:     return CONFIGFILENAME_NAME;
    case P_CASEDIR_NAME: return CASEDIR_NAME;
    case P_FORMAT_NAME:     return FORMAT_NAME;
    case P_METRIC_NAME: return METRIC_NAME;
    case P_PROJECTION_NAME:     return PROJECTION_NAME;
    case P_VALUETYPE_NAME: return VALUETYPE_NAME;
    default:              return "";
    }
}


void GuiDetailPages::AddProperty(PropertyTypes pt)
{
    // PropertyTypes type;
    // std::string   name;
    // std::string   value;
    switch (pt)
    {
    case P_EXPR_NAME:
    {
        Property p = { pt, PropertyTypeToPropertyName(pt), std::string(TreeItemPropertyValue(m_State.GetCurrentItem(), exprPropDefPtr).c_str()) };
        auto test = TreeItemPropertyValue(m_State.GetCurrentItem(), storageNamePropDefPtr).c_str();
        auto test1 = TreeItemPropertyValue(m_State.GetCurrentItem(), storageTypePropDefPtr).c_str();
        auto test2 = TreeItemPropertyValue(m_State.GetCurrentItem(), syncModePropDefPtr).c_str();
        auto test3 = TreeItemPropertyValue(m_State.GetCurrentItem(), dialogDataPropDefPtr).c_str();
        
        if (m_State.GetCurrentItem())
        {
            auto expr = m_State.GetCurrentItem()->GetExpr();
            int i = 0;
        }

    }

    default: return;
    }
    //const AbstrPropDef* pd;
    //labelPropDefPtr
    //auto name  = std::string(SharedStr(pd->GetID()).c_str());
    //auto value = std::string(TreeItemPropertyValue(m_State.GetCurrentItem(), pd).c_str());
    return;
}

void GuiDetailPages::UpdateProperties()
{
    
}

void GuiDetailPages::Update(bool* p_open)
{
    if (!ImGui::Begin("Detail Pages", p_open, ImGuiWindowFlags_AlwaysAutoResize|ImGuiWindowFlags_AlwaysHorizontalScrollbar))
    {
        ImGui::End();
        return;
    }

    if (m_State.DetailPagesEvents.HasEvents()) // new current item
    {
        m_State.DetailPagesEvents.Pop();
        UpdateProperties();
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

    // TODO: generalize General and Properties tabs TicPropDefConst or search DISABLESTORAGE_NAME >>>> TreeItemProps.cpp

    //ImGui::Text("Long string: DDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDD");

    //SharedStr result = TreeItemPropertyValue(self, labelPropDefPtr);  GetID()

    if (ImGui::BeginTabBar("Tabs", ImGuiTabBarFlags_FittingPolicyResizeDown))
    {
        if (ImGui::BeginTabItem("General", 0, ImGuiTabItemFlags_None))
        {

            //AddProperty(P_EXPR_NAME);


            if (m_State.GetCurrentItem())
            {
                if (m_State.GetCurrentItem()->IsFailed())
                {
                    auto xmlOut = XML_OutStream_Create(&m_GeneralBuff, OutStreamBase::ST_HTM, "", NULL);
                    auto result = DMS_TreeItem_XML_DumpGeneral(m_State.GetCurrentItem(), xmlOut, true);

                    m_GeneralBuff.InterpretBytes(); // Create detail page from html stream
                    m_GeneralBuff.Reset();
                }
                else
                {
                    InterestPtr<TreeItem*> tmpInterest = m_State.GetCurrentItem();
                    auto xmlOut = XML_OutStream_Create(&m_GeneralBuff, OutStreamBase::ST_HTM, "", NULL);
                    auto result = DMS_TreeItem_XML_DumpGeneral(m_State.GetCurrentItem(), xmlOut, true);
                    m_GeneralBuff.InterpretBytes(); // Create detail page from html stream
                    m_GeneralBuff.Reset();
                }
            }
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }

    ImGui::End();
}