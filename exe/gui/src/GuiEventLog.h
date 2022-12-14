#pragma once
#include <imgui.h>
#include "GuiBase.h"



struct EventLogItem
{
    SeverityTypeID m_Severity_type = SeverityTypeID::ST_Nothing;
    std::string    m_Text = "";
    std::string    m_Link = "";
};

class EventLogIterator
{
public:
    auto Init(std::list<EventLogItem>::iterator iterator, UInt32 index=0) -> void
    {
        m_Index = index;
        m_Start = iterator;
    }

    auto GetIterator() -> std::list<EventLogItem>::iterator
    {
        return m_Start;
    }

    auto SetStartPosition(UInt32 new_start) -> void
    {
        Int32 pos_dif = new_start - m_Index;
        std::advance(m_Start, pos_dif);
        m_Index = new_start;
    }

    auto CompareStartPositions(UInt32 new_start) -> bool
    {
        if (new_start == m_Index)
            return true;
        return false;
    }

private:
    std::list<EventLogItem>::iterator m_Start;
    UInt32 m_Index = 0;
};

class GuiEventLog : GuiBaseComponent
{
public:
    GuiEventLog();
    ~GuiEventLog();
    auto static GeoDMSMessage(ClientHandle clientHandle, SeverityTypeID st, CharPtr msg) -> void;
    auto static GeoDMSExceptionMessage(CharPtr msg) -> void;
    auto Update(bool* p_open, GuiState& state) -> void;

private:
    auto DrawItem(EventLogItem& item) -> void;
    auto ConvertSeverityTypeIDToColor(SeverityTypeID st) -> ImColor;
    auto ClearLog() -> void;
    auto EventFilter(SeverityTypeID st, GuiState& state) -> bool;
    
    auto static AddLog(SeverityTypeID severity_type, std::string original_message) -> void;

    char                  InputBuf[256];
    static std::list<EventLogItem> m_Items;
    EventLogIterator m_Iterator;
    static UInt32 m_MaxLogLines;
    ImGuiTextFilter       Filter;
    bool                  AutoScroll;
    bool                  ScrollToBottom;
};