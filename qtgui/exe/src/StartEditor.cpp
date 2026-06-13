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

QString findNotepadExe()
{
    QString sysRoot = qEnvironmentVariable("SystemRoot");
    QStringList candidates = {
        sysRoot + "/System32/notepad.exe",
        sysRoot + "/notepad.exe",
    };
    QString found = findExeInCandidates(candidates);
    return found.isEmpty() ? "notepad.exe" : found; // fallback to PATH
}

// ---------------------------------------------------------------------------
// Editor defaults offered by the options dialog
//
// Each supported editor is an application finder plus a parameter template. The template
// uses the same placeholder vocabulary as a hand-written command line: %F (filename),
// %L (line), %C (column) and %projDir% (the workspace root, which AbstrStorageManager::
// Expand resolves to the project directory). The dialog only fills the Application and
// Parameters fields from these; the actual editor is always launched from the stored,
// user-editable command line (registry key DmsEditor), never from a remembered editor type.
// ---------------------------------------------------------------------------

namespace {

std::string quoted(const QString& path)
{
    return "\"" + path.toStdString() + "\"";
}

// Build a choice for a named editor: located path when found, else a best-effort bare exe
// name so the Application field still shows an editable hint.
EditorChoice namedEditor(const char* label, const QString& exe, const char* bareExe, const char* params)
{
    bool found = !exe.isEmpty();
    return { label, quoted(found ? exe : QString(bareExe)), params, found };
}

} // namespace

std::vector<EditorChoice> getEditorChoices()
{
    std::vector<EditorChoice> choices;

    choices.push_back(namedEditor("Visual Studio",      findDevenvExe(),  "devenv.exe",
                                  "/edit \"%F\" /command \"edit.goto %L\""));
    // findVSCodeExe falls back to "code" on PATH, so Visual Studio Code is always available.
    choices.push_back(namedEditor("Visual Studio Code", findVSCodeExe(),  "code",
                                  "\"%projDir%\" --goto \"%F:%L:%C\""));

    QString npp = findNotepadPlusPlusExe();
    choices.push_back(namedEditor("Notepad++",          npp,              "notepad++.exe",
                                  "\"%F\" -n%L"));

    // Plain Notepad is only useful as a fallback when Notepad++ is not installed.
    if (npp.isEmpty())
        choices.push_back(namedEditor("Notepad",        findNotepadExe(), "notepad.exe",
                                      "\"%F\""));

    return choices;
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
