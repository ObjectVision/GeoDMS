#pragma once

#include <QString>
#include <string>
#include <string_view>

// Locate editor executables in standard installation paths.
// Returns empty string if not found (except findVSCodeExe which falls back to "code").
QString findVSCodeExe();
QString findDevenvExe();
QString findNotepadPlusPlusExe();

// Walk up from filePath's directory; return parent of nearest dir named 'cfg'.
// Falls back to the file's own directory if no 'cfg' ancestor found.
QString findVSCodeProjectRoot(const QString& filePath);

// Substitute %F (filename) and %L (line) placeholders in a command string.
std::string fillOpenConfigSourceCommand(const std::string& command,
                                        const std::string& filename,
                                        const std::string& line);

// Open the editor for filePath:line using the given preset.
// preset:          "vscode" | "visualstudio" | "notepadpp" | "custom" | ""
// expandedCustomCmd: full expanded command string; only used when preset is
//                    "custom" or empty (legacy). %F/%L already substituted and
//                    env-vars already expanded by the caller.
void startEditorForFile(const std::string& preset,
                        const QString&     filePath,
                        std::string_view   line,
                        const std::string& expandedCustomCmd);
