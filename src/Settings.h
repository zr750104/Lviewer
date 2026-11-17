#pragma once
#include "Win.h"
#include <map>

namespace lv {

enum DispOpt { DispWndToImg = 0, DispImgToWnd = 1, DispBigToWnd = 2, DispImgToDesk = 3, DispBigToDesk = 4, DispAsIs = 5 };
enum FitOpt { FitInside = 0, FitWidth = 1, FitHeight = 2, FitEither = 3 };
enum RotOpt { RotNone = 0, RotLeft, RotRight, RotPortraitL, RotPortraitR, RotLandscapeL, RotLandscapeR, Rot180 };
enum SortKey { SortName = 0, SortDate = 1, SortSize = 2, SortExt = 3, SortRandom = 4 };
enum WallStyle { WallCenter = 0, WallTile = 1, WallStretch = 2, WallFitW = 3, WallFitH = 4 };

struct OtherApp { std::wstring name, path, args; };

struct Settings {
    // general
    std::wstring lang;              // "", ja, en, th
    bool multiInstance = true;
    bool restoreLast = false;
    std::wstring lastFile;
    // window
    int wx = CW_USEDEFAULT, wy = CW_USEDEFAULT, ww = 960, wh = 720;
    bool maximized = false, topMost = false;
    bool caption = true, outerFrame = true, innerFrame = false;
    bool menuBar = false, toolBar = true, statusBar = true, scrollBar = true;
    bool centerWindow = false, fitWindowToToolbar = true;
    // background
    COLORREF bgColor1 = RGB(32, 32, 32), bgColor2 = RGB(48, 48, 48);
    bool bgChecker = false;            // checkerboard for transparent images / background
    int checkerSize = 8;
    COLORREF fsBgColor = RGB(0, 0, 0);
    // display
    int dispOpt = DispBigToWnd, dispOptFull = DispBigToWnd;
    int fitOpt = FitInside, rotOpt = RotNone;
    int maxFitZoom = 400;             // % (upper bound when enlarging to fit)
    bool zoomLock = false;
    int quality = 2;                  // 0 nearest, 1 bilinear, 2 high (area/bicubic)
    bool smoothZoomIn = true;         // bilinear when >100%
    bool hoverScroll = false;
    int hoverMargin = 10;             // %
    bool exifRotate = true;
    bool showTransparency = true;
    bool fsHideCursor = true;
    // navigation
    int sortKey = SortName;
    bool sortReverse = false, sortNumeric = true;
    bool loopList = true, beepAtEnd = false;
    bool pagesInNav = true;           // Next/Prev also steps through TIFF pages
    int skipCount = 10;
    bool includeHidden = false;
    // comic
    bool comic = false, comicRtoL = true, comicLandscapeSingle = true;
    // slide show
    int slideMs = 5000; bool slideLoop = true, slideRandom = false, slideFull = false, slideNextDir = false;
    // mouse
    int wheelScroll = 40, dragMargin = 4;
    int dragScrollRate = 1;           // multiplier for drag scrolling
    int dragZoomSpeed = 1;            // 0 slow, 1 medium, 2 fast
    int dragZoomDir = 0;              // 0 left->right, 1 right->left, 2 bottom->top, 3 top->bottom (direction that zooms in)
    int hoverMinMove = 0;             // minimum image pixels per mouse pixel in hover scroll (0 = absolute mapping)
    // updates
    bool updateCheckAtStart = true;
    int lastUpdateCheckDay = 0;
    bool wheelDull = false, activateNoClick = false;
    // file ops
    bool confirmDelete = true, confirmOverwrite = true, moveNextAfterDelete = true;
    std::wstring lastSaveDir, lastCopyDir, lastMoveDir;
    int jpegQuality = 90, pngCompression = 6, tiffCompression = 0;   // 0 auto, 1 none, 2 LZW, 3 ZIP, 4 CCITT G4, 5 PackBits
    bool keepTimestamp = false;
    // editing
    int undoMax = 5;
    int selUnit = 1;
    int glassSize = 8; bool glassRemainder = false;
    bool q0GlassV = true, q0GlassH = true, q0Nega = true, q0Xor = false, q0FlipV = false, q0FlipH = false;
    // MRU
    std::vector<std::wstring> mruDirs, mruFiles;
    // other apps
    OtherApp apps[9];
    // key/mouse bindings: command name -> "Ctrl+O,Num0"
    std::map<std::wstring, std::wstring> keys;
    // wallpaper
    std::wstring wallpaperFile;

    std::wstring iniPath;
    void Load(const std::wstring& path);
    void Save() const;
};

extern Settings g_set;
std::wstring ResolveIniPath(const std::wstring& user, bool userGiven);

void PushMru(std::vector<std::wstring>& v, const std::wstring& s, size_t max = 10);

} // namespace lv
