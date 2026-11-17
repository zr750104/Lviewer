#include "FileList.h"
#include "Codec.h"
#include "Settings.h"
#include "core/NaturalSort.h"
#include "miniz.h"
#include <random>
#include <cstdio>

namespace lv {

FileList::FileList() {}
FileList::~FileList() { CloseArchive(); }

bool FileList::IsArchiveExt(const std::wstring& ext) {
    std::wstring e = ToLower(ext);
    return e == L"zip" || e == L"cbz";
}

void FileList::CloseArchive() {
    if (zip_) {
        mz_zip_reader_end(static_cast<mz_zip_archive*>(zip_));
        delete static_cast<mz_zip_archive*>(zip_);
        zip_ = nullptr;
    }
    if (zipFile_) { fclose(zipFile_); zipFile_ = nullptr; }
    archive_.clear();
}

void FileList::Clear() { CloseArchive(); items_.clear(); cur_ = -1; folder_.clear(); }

static bool Hidden(DWORD attr) {
    return !g_set.includeHidden && (attr & (FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_SYSTEM));
}

bool FileList::FolderHasImages(const std::wstring& dir) {
    WIN32_FIND_DATAW fd;
    HANDLE h = FindFirstFileExW(LongPath(PathJoin(dir, L"*")).c_str(), FindExInfoBasic, &fd, FindExSearchNameMatch, nullptr, FIND_FIRST_EX_LARGE_FETCH);
    if (h == INVALID_HANDLE_VALUE) return false;
    bool found = false;
    do {
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;
        if (Hidden(fd.dwFileAttributes)) continue;
        std::wstring e = PathExt(fd.cFileName);
        if (IsImageExt(e)) { found = true; break; }
    } while (FindNextFileW(h, &fd));
    FindClose(h);
    return found;
}

std::vector<std::wstring> ListSubfolders(const std::wstring& dir) {
    std::vector<std::wstring> v;
    WIN32_FIND_DATAW fd;
    HANDLE h = FindFirstFileExW(LongPath(PathJoin(dir, L"*")).c_str(), FindExInfoBasic, &fd, FindExSearchLimitToDirectories, nullptr, 0);
    if (h == INVALID_HANDLE_VALUE) return v;
    do {
        if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) continue;
        if (!wcscmp(fd.cFileName, L".") || !wcscmp(fd.cFileName, L"..")) continue;
        if (Hidden(fd.dwFileAttributes)) continue;
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) continue;
        v.push_back(PathJoin(dir, fd.cFileName));
    } while (FindNextFileW(h, &fd));
    FindClose(h);
    std::sort(v.begin(), v.end(), [](const std::wstring& a, const std::wstring& b) {
        return CompareNames(PathFileName(a), PathFileName(b), true) < 0;
    });
    return v;
}

bool FileList::ScanFolder(const std::wstring& dir) {
    items_.clear();
    WIN32_FIND_DATAW fd;
    HANDLE h = FindFirstFileExW(LongPath(PathJoin(dir, L"*")).c_str(), FindExInfoBasic, &fd, FindExSearchNameMatch, nullptr, FIND_FIRST_EX_LARGE_FETCH);
    if (h == INVALID_HANDLE_VALUE) return false;
    do {
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;
        if (Hidden(fd.dwFileAttributes)) continue;
        std::wstring e = PathExt(fd.cFileName);
        if (!IsImageExt(e)) continue;
        FileEntry fe;
        fe.name = fd.cFileName;
        fe.path = PathJoin(dir, fd.cFileName);
        fe.mtime = fd.ftLastWriteTime;
        fe.size = (unsigned long long)fd.nFileSizeHigh << 32 | fd.nFileSizeLow;
        items_.push_back(fe);
    } while (FindNextFileW(h, &fd));
    FindClose(h);
    folder_ = dir;
    return true;
}

static std::wstring DecodeZipName(const char* s, bool utf8) {
    if (utf8) return Utf8ToW(s);
    int n = MultiByteToWideChar(CP_ACP, MB_ERR_INVALID_CHARS, s, -1, nullptr, 0);
    UINT cp = n > 0 ? CP_ACP : 437;
    n = MultiByteToWideChar(cp, 0, s, -1, nullptr, 0);
    std::wstring w(n > 0 ? n : 0, 0);
    if (n > 0) MultiByteToWideChar(cp, 0, s, -1, &w[0], n);
    if (!w.empty() && w.back() == 0) w.pop_back();
    return w;
}

bool FileList::ScanArchive(const std::wstring& path) {
    CloseArchive();
    items_.clear();
    zipFile_ = _wfopen(LongPath(path).c_str(), L"rb");
    if (!zipFile_) return false;
    _fseeki64(zipFile_, 0, SEEK_END);
    long long sz = _ftelli64(zipFile_);
    _fseeki64(zipFile_, 0, SEEK_SET);
    auto* z = new mz_zip_archive();
    if (!mz_zip_reader_init_cfile(z, zipFile_, mz_uint64(sz), 0)) { delete z; fclose(zipFile_); zipFile_ = nullptr; return false; }
    zip_ = z;
    archive_ = path;
    folder_ = path;
    mz_uint n = mz_zip_reader_get_num_files(z);
    for (mz_uint i = 0; i < n; ++i) {
        mz_zip_archive_file_stat st;
        if (!mz_zip_reader_file_stat(z, i, &st) || st.m_is_directory) continue;
        std::wstring inner = DecodeZipName(st.m_filename, (st.m_bit_flag & (1 << 11)) != 0);
        for (auto& c : inner) if (c == L'/') c = L'\\';
        if (!IsImageExt(PathExt(inner))) continue;
        FileEntry fe;
        fe.path = inner;
        fe.name = inner;
        fe.size = st.m_uncomp_size;
        fe.zipIndex = int(i);
        // MS-DOS time from miniz is a time_t
        long long t = (long long)st.m_time * 10000000LL + 116444736000000000LL;
        fe.mtime.dwLowDateTime = DWORD(t); fe.mtime.dwHighDateTime = DWORD(t >> 32);
        items_.push_back(fe);
    }
    return true;
}

bool FileList::ReadArchiveEntry(int i, std::vector<uint8_t>& out) const {
    if (!zip_ || i < 0 || i >= Count()) return false;
    auto* z = static_cast<mz_zip_archive*>(zip_);
    size_t sz = 0;
    void* p = mz_zip_reader_extract_to_heap(z, mz_uint(items_[i].zipIndex), &sz, 0);
    if (!p) return false;
    out.assign(static_cast<uint8_t*>(p), static_cast<uint8_t*>(p) + sz);
    mz_free(p);
    return true;
}

void FileList::Sort() {
    std::wstring keep = Current() ? Current()->path : L"";
    bool num = g_set.sortNumeric;
    auto byName = [num](const FileEntry& a, const FileEntry& b) {
        if (a.zipIndex >= 0) {   // archive: group by folder first
            int c = CompareNames(PathDir(a.name), PathDir(b.name), num);
            if (c) return c < 0;
            return CompareNames(PathFileName(a.name), PathFileName(b.name), num) < 0;
        }
        return CompareNames(a.name, b.name, num) < 0;
    };
    switch (g_set.sortKey) {
        case SortDate:
            std::stable_sort(items_.begin(), items_.end(), [&](const FileEntry& a, const FileEntry& b) {
                int c = CompareFileTime(&a.mtime, &b.mtime); return c ? c < 0 : byName(a, b); });
            break;
        case SortSize:
            std::stable_sort(items_.begin(), items_.end(), [&](const FileEntry& a, const FileEntry& b) {
                return a.size != b.size ? a.size < b.size : byName(a, b); });
            break;
        case SortExt:
            std::stable_sort(items_.begin(), items_.end(), [&](const FileEntry& a, const FileEntry& b) {
                int c = lstrcmpiW(PathExt(a.name).c_str(), PathExt(b.name).c_str()); return c ? c < 0 : byName(a, b); });
            break;
        case SortRandom: {
            std::mt19937 rng(GetTickCount());
            std::shuffle(items_.begin(), items_.end(), rng);
        } break;
        default: std::stable_sort(items_.begin(), items_.end(), byName); break;
    }
    if (g_set.sortReverse && g_set.sortKey != SortRandom) std::reverse(items_.begin(), items_.end());
    cur_ = items_.empty() ? -1 : 0;
    for (int i = 0; i < Count(); ++i) if (items_[i].path == keep) { cur_ = i; break; }
}

bool FileList::Open(const std::wstring& path0) {
    std::wstring path = path0;
    while (path.size() > 3 && (path.back() == L'\\' || path.back() == L'/')) path.pop_back();
    // "archive.zip\inner\name.jpg" form
    std::wstring innerSel;
    if (!FileExists(path) && !DirExists(path)) {
        std::wstring low = ToLower(path);
        for (const wchar_t* ext : {L".zip\\", L".cbz\\"}) {
            size_t k = low.find(ext);
            if (k != std::wstring::npos) { innerSel = path.substr(k + 5); path = path.substr(0, k + 4); break; }
        }
    }
    if (DirExists(path)) {
        CloseArchive();
        if (!ScanFolder(path)) return false;
        Sort();
        cur_ = items_.empty() ? -1 : 0;
        return !items_.empty();
    }
    if (!FileExists(path)) return false;
    if (IsArchiveExt(PathExt(path))) {
        if (!ScanArchive(path)) return false;
        Sort();
        cur_ = items_.empty() ? -1 : 0;
        if (!innerSel.empty())
            for (int i = 0; i < Count(); ++i) if (lstrcmpiW(items_[i].path.c_str(), innerSel.c_str()) == 0) { cur_ = i; break; }
        return !items_.empty();
    }
    CloseArchive();
    std::wstring dir = PathDir(path);
    ScanFolder(dir);
    // the file may have an unregistered extension; still try to show it
    bool present = false;
    for (auto& e : items_) if (lstrcmpiW(e.path.c_str(), path.c_str()) == 0) { present = true; break; }
    if (!present) {
        WIN32_FILE_ATTRIBUTE_DATA ad = {};
        GetFileAttributesExW(LongPath(path).c_str(), GetFileExInfoStandard, &ad);
        FileEntry fe; fe.path = path; fe.name = PathFileName(path); fe.mtime = ad.ftLastWriteTime;
        fe.size = (unsigned long long)ad.nFileSizeHigh << 32 | ad.nFileSizeLow;
        items_.push_back(fe);
    }
    Sort();
    for (int i = 0; i < Count(); ++i) if (lstrcmpiW(items_[i].path.c_str(), path.c_str()) == 0) { cur_ = i; break; }
    return cur_ >= 0;
}

void FileList::Refresh() {
    std::wstring keep = Current() ? Current()->path : L"";
    if (InArchive()) { std::wstring a = archive_; ScanArchive(a); }
    else if (!folder_.empty()) ScanFolder(folder_);
    Sort();
    for (int i = 0; i < Count(); ++i) if (items_[i].path == keep) { cur_ = i; return; }
    if (cur_ >= Count()) cur_ = Count() - 1;
}

std::wstring FileList::CurrentDisplayPath() const {
    const FileEntry* e = Current();
    if (!e) return L"";
    return InArchive() ? PathJoin(archive_, e->path) : e->path;
}

void FileList::RemoveCurrent() {
    if (!Current()) return;
    items_.erase(items_.begin() + cur_);
    if (cur_ >= Count()) cur_ = Count() - 1;
}

void FileList::RenameCurrent(const std::wstring& np) {
    if (!Current()) return;
    items_[cur_].path = np;
    items_[cur_].name = PathFileName(np);
}

std::wstring FileList::BaseFolder() const {
    if (InArchive()) return PathDir(archive_);
    return folder_;
}

std::wstring FileList::ParentFolder() const {
    std::wstring b = InArchive() ? PathDir(archive_) : PathDir(folder_);
    if (b.empty() || b == folder_) return L"";
    return b;
}

std::wstring FileList::SiblingFolder(bool fwd) const {
    std::wstring self = InArchive() ? archive_ : folder_;
    std::wstring parent = PathDir(self);
    if (parent.empty() || parent == self) return L"";
    auto subs = ListSubfolders(parent);
    // include archives as siblings, too
    WIN32_FIND_DATAW fd;
    HANDLE h = FindFirstFileExW(LongPath(PathJoin(parent, L"*")).c_str(), FindExInfoBasic, &fd, FindExSearchNameMatch, nullptr, 0);
    if (h != INVALID_HANDLE_VALUE) {
        do { if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) && IsArchiveExt(PathExt(fd.cFileName))) subs.push_back(PathJoin(parent, fd.cFileName)); }
        while (FindNextFileW(h, &fd));
        FindClose(h);
        std::sort(subs.begin(), subs.end(), [](const std::wstring& a, const std::wstring& b) { return CompareNames(PathFileName(a), PathFileName(b), true) < 0; });
    }
    int idx = -1;
    for (int i = 0; i < int(subs.size()); ++i) if (lstrcmpiW(subs[i].c_str(), self.c_str()) == 0) { idx = i; break; }
    if (idx < 0) return L"";
    for (int i = idx + (fwd ? 1 : -1); i >= 0 && i < int(subs.size()); i += fwd ? 1 : -1) {
        if (IsArchiveExt(PathExt(subs[i])) || FolderHasImages(subs[i])) return subs[i];
    }
    return L"";
}

// Pre-order traversal over the folder tree rooted at the drive/share.
std::wstring FileList::NextFolder(bool fwd) const {
    std::wstring cur = BaseFolder();
    if (cur.empty()) return L"";
    int budget = 20000;
    auto deepestLast = [&](std::wstring d) {
        for (;;) {
            auto s = ListSubfolders(d);
            if (s.empty() || --budget < 0) return d;
            d = s.back();
        }
    };
    std::wstring node = cur;
    while (budget-- > 0) {
        std::wstring next;
        if (fwd) {
            auto kids = ListSubfolders(node);
            if (!kids.empty()) next = kids.front();
            else {
                std::wstring n = node;
                while (next.empty()) {
                    std::wstring p = PathDir(n);
                    if (p.empty() || p == n) return L"";
                    auto sib = ListSubfolders(p);
                    for (size_t i = 0; i < sib.size(); ++i)
                        if (lstrcmpiW(sib[i].c_str(), n.c_str()) == 0 && i + 1 < sib.size()) { next = sib[i + 1]; break; }
                    n = p;
                }
            }
        } else {
            std::wstring p = PathDir(node);
            if (p.empty() || p == node) return L"";
            auto sib = ListSubfolders(p);
            int idx = -1;
            for (int i = 0; i < int(sib.size()); ++i) if (lstrcmpiW(sib[i].c_str(), node.c_str()) == 0) { idx = i; break; }
            next = idx > 0 ? deepestLast(sib[idx - 1]) : p;
        }
        if (next.empty()) return L"";
        if (lstrcmpiW(next.c_str(), cur.c_str()) != 0 && FolderHasImages(next)) return next;
        node = next;
    }
    return L"";
}

} // namespace lv
