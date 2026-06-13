#pragma once

#include <QString>
#include <string>

// Locate editor executables in standard installation paths.
// Returns empty string if not found (except findVSCodeExe which falls back to "code").
QString findVSCodeExe();
QString findDevenvExe();
QString findNotepadPlusPlusExe();

// Build the raw, unexpanded command-line template for the editor selected by `preset`.
// For the named presets ("vscode" | "visualstudio" | "notepadpp") this is the quoted
// application path followed by a parameter template that may contain the placeholders
// %F (filename), %L (line), %C (column) and %projDir% (workspace root).
// For "custom" or "" (legacy) the user-configured `customCmd` is returned verbatim.
// Returns an empty string when the application for a named preset cannot be located.
std::string buildEditorCommandLineTemplate(const std::string& preset, const std::string& customCmd);

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
