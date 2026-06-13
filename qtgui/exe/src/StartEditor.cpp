#include "StartEditor.h"

#include "RtcInterface.h"
#include "dbg/Check.h"
#include "dbg/SeverityType.h"

#include <QFileInfo>
#include <QProcess>
#include <QStringList>

#include <cctype>

// ---------------------------------------------------------------------------
// Internal helpers
// ---------------------------------------------------------------------------

static QString findExeInCandidates(const QStringList& candidates)
{
    for (const QString& c : candidates)
        if (QFileInfo::exists(c))
            return c;
    return {};
}

static void launchDetached(const QString& program, const QStringList& args, const char* logLabel)
{
    QProcess process;
    process.setProgram(program);
    process.setArguments(args);
    QString full = program + " " + args.join(" ");
    if (process.startDetached())
        reportF(MsgCategory::commands, SeverityTypeID::ST_MajorTrace, full.toStdString().c_str());
    else
        reportF(MsgCategory::commands, SeverityTypeID::ST_Warning,
                "Unable to start %s: %s", logLabel, full.toStdString().c_str());
}

// ---------------------------------------------------------------------------
// Exe finders
// ---------------------------------------------------------------------------

QString findVSCodeExe()
{
    QString localAppData = qEnvironmentVariable("LOCALAPPDATA");
    QString programFiles = qEnvironmentVariable("ProgramFiles");
    QStringList candidates = {
        localAppData + "/Programs/Microsoft VS Code/Code.exe",
        programFiles  + "/Microsoft VS Code/Code.exe",
    };
    QString found = findExeInCandidates(candidates);
    return found.isEmpty() ? "code" : found; // fallback to PATH
}

QString findDevenvExe()
{
    QString programFiles = qEnvironmentVariable("ProgramFiles");
    QStringList candidates = {
        programFiles + "/Microsoft Visual Studio/18/Enterprise/Common7/IDE/devenv.exe",
        programFiles + "/Microsoft Visual Studio/18/Professional/Common7/IDE/devenv.exe",
        programFiles + "/Microsoft Visual Studio/18/Community/Common7/IDE/devenv.exe",
        programFiles + "/Microsoft Visual Studio/2022/Enterprise/Common7/IDE/devenv.exe",
        programFiles + "/Microsoft Visual Studio/2022/Professional/Common7/IDE/devenv.exe",
        programFiles + "/Microsoft Visual Studio/2022/Community/Common7/IDE/devenv.exe",
        programFiles + "/Microsoft Visual Studio/2019/Enterprise/Common7/IDE/devenv.exe",
        programFiles + "/Microsoft Visual Studio/2019/Professional/Common7/IDE/devenv.exe",
        programFiles + "/Microsoft Visual Studio/2019/Community/Common7/IDE/devenv.exe",
    };
    return findExeInCandidates(candidates);
}

QString findNotepadPlusPlusExe()
{
    QString programFiles   = qEnvironmentVariable("ProgramFiles");
    QString programFiles86 = qEnvironmentVariable("ProgramFiles(x86)");
    QString localAppData   = qEnvironmentVariable("LOCALAPPDATA");
    QStringList candidates = {
        programFiles   + "/Notepad++/notepad++.exe",
        programFiles86 + "/Notepad++/notepad++.exe",
        localAppData   + "/Programs/Notepad++/notepad++.exe",
    };
    return findExeInCandidates(candidates);
}

// ---------------------------------------------------------------------------
// Editor presets
//
// Every preset is expressed as an application finder plus a command-line parameter
// template. The template uses the same placeholder vocabulary as the custom command
// line: %F (filename), %L (line), %C (column) and %projDir% (the workspace root, which
// AbstrStorageManager::Expand resolves to the project directory). This keeps the named
// editors transparent ("what gets executed") and as configurable as the custom option.
// ---------------------------------------------------------------------------

struct EditorPreset
{
    const char* key;                 // value stored under registry key DmsEditorPreset
    QString   (*findApplication)();  // locates the editor executable
    const char* paramTemplate;       // parameters appended after the quoted application
};

static const EditorPreset s_editorPresets[] = {
    { "vscode",       &findVSCodeExe,          "\"%projDir%\" --goto \"%F:%L:%C\"" },
    { "visualstudio", &findDevenvExe,          "/edit \"%F\"" },
    { "notepadpp",    &findNotepadPlusPlusExe, "\"%F\" -n%L" },
};

std::string buildEditorCommandLineTemplate(const std::string& preset, const std::string& customCmd)
{
    for (const auto& p : s_editorPresets)
    {
        if (preset != p.key)
            continue;

        QString exe = p.findApplication();
        if (exe.isEmpty())
        {
            reportF(MsgCategory::commands, SeverityTypeID::ST_Warning,
                    "Editor application for preset '%s' not found.", preset.c_str());
            return {};
        }
        // Quote the application and append the parameter template, mirroring the way a
        // custom command line is composed (quoted exe followed by its arguments).
        return "\"" + exe.toStdString() + "\" " + p.paramTemplate;
    }

    // "custom" or empty (legacy): use the user-configured command verbatim.
    return customCmd;
}

// ---------------------------------------------------------------------------
// Placeholder substitution
// ---------------------------------------------------------------------------

// A %X token is only treated as one of our F/L/C placeholders when the character that
// follows the two-character token is neither alphanumeric nor a '%'. This is how %F, %L
// and %C were substituted before #1125, and it prevents collisions with longer
// placeholders such as %LocalDataDir% (which AbstrStorageManager::Expand resolves later).
static bool isPlaceholderBoundary(char c)
{
    return !std::isalnum(static_cast<unsigned char>(c)) && c != '%';
}

std::string fillOpenConfigSourceCommand(const std::string& command,
                                        const std::string& filename,
                                        const std::string& line,
                                        const std::string& column)
{
    std::string result;
    result.reserve(command.size() + filename.size());

    for (std::size_t i = 0; i < command.size(); )
    {
        if (command[i] == '%' && i + 1 < command.size())
        {
            char code = command[i + 1];
            char next = (i + 2 < command.size()) ? command[i + 2] : '\0'; // end-of-string is a boundary
            if ((code == 'F' || code == 'L' || code == 'C') && isPlaceholderBoundary(next))
            {
                switch (code)
                {
                case 'F': result += filename; break;
                case 'L': result += line;     break;
                case 'C': result += column;   break;
                }
                i += 2;
                continue;
            }
        }
        result += command[i++];
    }
    return result;
}

// ---------------------------------------------------------------------------
// Launch
// ---------------------------------------------------------------------------

void launchEditorCommandLine(const std::string& expandedCmd)
{
    if (expandedCmd.empty())
        return;
    QStringList args = QProcess::splitCommand(QString::fromStdString(expandedCmd));
    if (args.isEmpty())
        return;
    QString prog = args.takeFirst();
    launchDetached(prog, args, "editor");
}
