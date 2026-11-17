// Lviewer - self update from GitHub Releases
#pragma once
#include "Win.h"

namespace lv {

struct ReleaseInfo {
    std::wstring tag;       // e.g. "v1.1.0"
    std::wstring pageUrl;   // release page
    std::wstring zipUrl;    // *-win64.zip asset
    std::wstring notes;     // release body
};

extern const wchar_t* kUpdateRepo;        // "owner/repo"

// Ask GitHub for the latest release (anonymous API; uses the system proxy settings).
bool FetchLatestRelease(ReleaseInfo& out, std::wstring* err);
// -1 / 0 / 1 for "v1.2.3"-style versions (leading 'v' ignored)
int CompareVersions(const std::wstring& a, const std::wstring& b);
bool DownloadToFile(const std::wstring& url, const std::wstring& path, std::wstring* err);
// Extracts the release ZIP next to the running exe; the running exe is renamed to *.old.
bool InstallUpdate(const std::wstring& zipPath, std::wstring* err);
// Removes the *.old file left by a previous update (call at start-up).
void CleanupAfterUpdate();
int TodayNumber();                        // days since 1601, for "once a day" checks

} // namespace lv
