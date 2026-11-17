#pragma once
#include "Win.h"

namespace lv {

struct FileEntry {
    std::wstring path;      // full path (filesystem) or inner path (archive)
    std::wstring name;      // file name for display/sort
    FILETIME mtime = {};
    unsigned long long size = 0;
    int zipIndex = -1;
};

class FileList {
public:
    FileList();
    ~FileList();
    // Open a file or folder; returns false if nothing displayable is there.
    bool Open(const std::wstring& path);
    void Clear();
    void Refresh();                        // re-scan keeping current file
    void Sort();

    bool InArchive() const { return zip_ != nullptr; }
    const std::wstring& ArchivePath() const { return archive_; }
    const std::wstring& Folder() const { return folder_; }   // folder (or archive path)
    int Count() const { return int(items_.size()); }
    int Index() const { return cur_; }
    void SetIndex(int i) { if (i >= 0 && i < Count()) cur_ = i; }
    const FileEntry* Current() const { return cur_ >= 0 && cur_ < Count() ? &items_[cur_] : nullptr; }
    const FileEntry& At(int i) const { return items_[i]; }
    std::wstring CurrentDisplayPath() const;
    bool ReadArchiveEntry(int i, std::vector<uint8_t>& out) const;
    void RemoveCurrent();
    void RenameCurrent(const std::wstring& newPath);

    // Folder navigation: returns target folder path (empty when none)
    std::wstring NextFolder(bool forward) const;     // pre-order traversal of folders containing images
    std::wstring SiblingFolder(bool forward) const;  // adjacent folder under the same parent
    std::wstring ParentFolder() const;

    static bool IsArchiveExt(const std::wstring& ext);
    static bool FolderHasImages(const std::wstring& dir);

private:
    bool ScanFolder(const std::wstring& dir);
    bool ScanArchive(const std::wstring& path);
    void CloseArchive();
    std::wstring BaseFolder() const;       // filesystem folder that contains current list

    std::vector<FileEntry> items_;
    int cur_ = -1;
    std::wstring folder_, archive_;
    void* zip_ = nullptr;                  // mz_zip_archive*
    FILE* zipFile_ = nullptr;
};

std::vector<std::wstring> ListSubfolders(const std::wstring& dir);

} // namespace lv
