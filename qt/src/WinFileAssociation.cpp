#ifdef _WIN32
#include "WinFileAssociation.hpp"

#include <QString>
#include <windows.h>

namespace
{
    const wchar_t *kProgId = L"Snes9xRD.ROM";

    std::wstring exePath()
    {
        wchar_t path[MAX_PATH] = {};
        GetModuleFileNameW(nullptr, path, MAX_PATH);
        return path;
    }

    bool setStringValue(HKEY root, const std::wstring &subkey, const wchar_t *name, const std::wstring &value)
    {
        HKEY key;
        if (RegCreateKeyExW(root, subkey.c_str(), 0, nullptr, REG_OPTION_NON_VOLATILE,
                             KEY_SET_VALUE, nullptr, &key, nullptr) != ERROR_SUCCESS)
            return false;

        auto size = static_cast<DWORD>((value.size() + 1) * sizeof(wchar_t));
        LONG result = RegSetValueExW(key, name, 0, REG_SZ, reinterpret_cast<const BYTE *>(value.c_str()), size);
        RegCloseKey(key);
        return result == ERROR_SUCCESS;
    }

    void registerProgid()
    {
        auto exe = exePath();
        setStringValue(HKEY_CURRENT_USER, L"Software\\Classes\\" + std::wstring(kProgId), nullptr, L"Snes9x ROM");
        setStringValue(HKEY_CURRENT_USER, L"Software\\Classes\\" + std::wstring(kProgId) + L"\\DefaultIcon", nullptr, exe + L",0");
        setStringValue(HKEY_CURRENT_USER, L"Software\\Classes\\" + std::wstring(kProgId) + L"\\shell\\open\\command",
                        nullptr, L"\"" + exe + L"\" \"%1\"");
    }

    void registerExtension(const std::wstring &ext)
    {
        // OpenWithProgids is the modern, non-destructive association: it adds
        // Snes9x to Explorer's "Open with" list without touching whatever the
        // user already has set as default for the extension.
        setStringValue(HKEY_CURRENT_USER, L"Software\\Classes\\." + ext + L"\\OpenWithProgids", kProgId, L"");
    }

    void unregisterExtension(const std::wstring &ext)
    {
        HKEY key;
        if (RegOpenKeyExW(HKEY_CURRENT_USER, (L"Software\\Classes\\." + ext + L"\\OpenWithProgids").c_str(),
                           0, KEY_SET_VALUE, &key) != ERROR_SUCCESS)
            return;
        RegDeleteValueW(key, kProgId);
        RegCloseKey(key);
    }
}

bool WinFileAssociation::apply(bool enable, const std::vector<std::string> &extensions)
{
    if (enable)
    {
        registerProgid();
        for (auto &ext : extensions)
            registerExtension(QString::fromStdString(ext).toStdWString());
    }
    else
    {
        RegDeleteTreeW(HKEY_CURRENT_USER, (L"Software\\Classes\\" + std::wstring(kProgId)).c_str());
        for (auto &ext : extensions)
            unregisterExtension(QString::fromStdString(ext).toStdWString());
    }

    return true;
}

#endif
