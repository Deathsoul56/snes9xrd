#pragma once
#ifdef _WIN32

#include <string>
#include <vector>

// Registers/unregisters snes9xrd-qt.exe as an "Open with" handler (and lets the
// user set it as default via Explorer) for the given ROM extensions, scoped
// to HKEY_CURRENT_USER so no elevation is required. extensions are given
// without the leading dot (e.g. "smc", "sfc").
namespace WinFileAssociation
{
    bool apply(bool enable, const std::vector<std::string> &extensions);
}

#endif
