#include "StartEditor.h"

#include "RtcInterface.h"
#include "dbg/Check.h"
#include "dbg/SeverityType.h"

#include <QDir>
#include <QFileInfo>
#include <QProcess>
#include <QStringList>

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
// Project root detection
// ---------------------------------------------------------------------------

QString findVSCodeProjectRoot(const QString& filePath)
{
    QDir dir = QFileInfo(filePath).absoluteDir();
    while (true)
    {
        if (dir.dirName().compare("cfg", Qt::CaseInsensitive) == 0)
        {
            QDir parent = dir;
            if (parent.cdUp())
                return parent.absolutePath();
        }
        if (!dir.cdUp())
            break;
    }
    return QFileInfo(filePath).absoluteDir().absolutePath();
}

// ---------------------------------------------------------------------------
// Command template substitution
// ---------------------------------------------------------------------------

std::string fillOpenConfigSourceCommand(const std::string& command,
                                        const std::string& filename,
                                        const std::string& line)
{
    std::string result = command;
    auto fnp_pos = result.find("%F");
    if (fnp_pos == std::string::npos)
        return result;
    result.replace(fnp_pos, 2, filename);

    auto lnp_pos = result.find("%L");
    if (lnp_pos == std::string::npos)
        return result;
    result.replace(lnp_pos, 2, line);
    return result;
}

// ---------------------------------------------------------------------------
// Main entry point
// ---------------------------------------------------------------------------

void startEditorForFile(const std::string& preset,
                        const QString&     filePath,
                        std::string_view   line,
                        const std::string& expandedCustomCmd)
{
    if (preset == "vscode")
    {
        QString exe  = findVSCodeExe();
        QString root = findVSCodeProjectRoot(filePath);
        launchDetached(exe,
            { root, "--goto", filePath + ":" + QString::fromStdString(std::string(line)) },
            "VS Code");
        return;
    }
    if (preset == "visualstudio")
    {
        QString exe = findDevenvExe();
        if (exe.isEmpty())
        {
            reportF(MsgCategory::commands, SeverityTypeID::ST_Warning,
                    "Visual Studio (devenv.exe) not found.");
            return;
        }
        launchDetached(exe, { "/edit", filePath }, "Visual Studio");
        return;
    }
    if (preset == "notepadpp")
    {
        QString exe = findNotepadPlusPlusExe();
        if (exe.isEmpty())
        {
            reportF(MsgCategory::commands, SeverityTypeID::ST_Warning,
                    "Notepad++ not found in standard locations.");
            return;
        }
        launchDetached(exe,
            { filePath, "-n" + QString::fromStdString(std::string(line)) },
            "Notepad++");
        return;
    }

    // "custom" or empty (legacy): use the pre-expanded command string
    if (expandedCustomCmd.empty())
        return;
    QStringList args = QProcess::splitCommand(QString::fromStdString(expandedCustomCmd));
    if (args.isEmpty())
        return;
    QString prog = args.takeFirst();
    launchDetached(prog, args, "editor");
}
