#pragma once

#include <QString>
#include <string>
#include <vector>

// Locate editor executables in standard installation paths.
// Returns empty string if not found (except findVSCodeExe which falls back to "code").
QString findVSCodeExe();
QString findDevenvExe();
QString findNotepadPlusPlusExe();
QString findNotepadExe();

// A suggested default for one of the supported editors, used to fill the Application /
// Parameters fields in the Local Machine options dialog. `application` is the quoted
// (located, or best-effort) executable path; `parameters` is the parameter template using
// the placeholders %F (filename), %L (line), %C (column) and %projDir% (workspace root).
// `found` is false when the executable could not be located on this machine.
struct EditorChoice
{
    std::string label;        // menu text, e.g. "Visual Studio Code"
    std::string application;  // quoted application path
    std::string parameters;   // parameter template
    bool        found = false;
};

// The supported editors offered by the "Set default parameters for editor" menu:
// Visual Studio, Visual Studio Code and Notepad++ are always listed (disabled when not
// installed); plain Notepad is appended only when Notepad++ is not available.
std::vector<EditorChoice> getEditorChoices();

// Substitute %F (filename), %L (line) and %C (column) in `command`, each only when the
// two-character token is followed by a non-alphanumeric, non-'%' character so that longer
// placeholders such as %LocalDataDir% or %localAppData% are left intact for the subsequent
// AbstrStorageManager::Expand pass.
std::string fillOpenConfigSourceCommand(const std::string& command,
                                        const std::string& filename,
                                        const std::string& line,
                                        const std::string& column);

// Split a fully expanded command line and start it detached.
void launchEditorCommandLine(const std::string& expandedCmd);
