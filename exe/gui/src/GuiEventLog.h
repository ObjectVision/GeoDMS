#pragma once
#include <imgui.h>
#include "GuiBase.h"

class GuiEventLog : GuiBaseComponent
{
public:
    GuiEventLog();
    ~GuiEventLog();
    static void GeoDMSMessage(ClientHandle clientHandle, SeverityTypeID st, CharPtr msg);
    static void GeoDMSExceptionMessage(CharPtr msg);
    static void GeoDMSContextMessage(ClientHandle clientHandle, CharPtr msg);
    void Update(bool* p_open);

private:
    ImColor ConvertSeverityTypeIDToColor(SeverityTypeID st);
    void ClearLog();
    bool EventFilter(SeverityTypeID st);
    static void AddLog(SeverityTypeID st, std::string msg);
    static int   Stricmp(const char* s1, const char* s2);
    static int   Strnicmp(const char* s1, const char* s2, int n);
    static char* Strdup(const char* s);
    static void  Strtrim(char* s);

    char                  InputBuf[256];
    static std::vector<std::pair<SeverityTypeID,std::string>> m_Items;
    static UInt32 m_MaxLogLines;
    ImGuiTextFilter       Filter;
    bool                  AutoScroll;
    bool                  ScrollToBottom;

    GuiState              m_State;
};