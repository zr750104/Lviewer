// Lviewer - command execution
#include "Viewer.h"
#include "I18n.h"
#include "Settings.h"
#include "Form.h"
#include <commdlg.h>
#include "Updater.h"
#include <cmath>

namespace lv {

void Viewer::Modified() {
    modified_ = true;
    InvalidateCache();
    ClampScroll();
    if (fitRule_) ApplyDisplayOption(false);
    UpdateScrollBars(); UpdateTitle(); UpdateStatus(); Redraw();
}

void Viewer::PushUndo() {
    if (g_set.undoMax <= 0 || !img_.valid()) return;
    undo_.push_back(img_);
    while (int(undo_.size()) > g_set.undoMax) undo_.erase(undo_.begin());
}

void Viewer::Undo() {
    if (undo_.empty()) return;
    img_ = std::move(undo_.back());
    undo_.pop_back();
    sel_ = Rect{};
    Modified();
    if (undo_.empty() && untitled_.empty()) modified_ = false, UpdateTitle();
}

Rect Viewer::Sel() const { return (selMode_ && !sel_.empty()) ? sel_ : Rect{}; }

void Viewer::Rotate(bool cw) {
    Edit([&](Image& im) { im = Rotate90(im, cw); });
    sel_ = Rect{};
}

bool Viewer::IsEnabled(int cmd) const {
    bool img = img_.valid();
    bool file = list_.Current() && !list_.InArchive() && untitled_.empty();
    switch (cmd) {
        case CmdReload: case CmdRename: case CmdMove: case CmdCopyFile: case CmdDelete: case CmdDeleteDirect:
        case CmdNewWndCurrent: case CmdCopyShortPath:
            return file;
        case CmdSave: return img && modified_ && file;
        case CmdSaveAs: case CmdPrint: case CmdNewWndSendClip: case CmdCopy: case CmdCut: case CmdImageInfo:
        case CmdResize: case CmdCanvasSize: case CmdRotL: case CmdRotR: case CmdFlipV: case CmdFlipH: case CmdRotAngle:
        case CmdAutoContrast: case CmdAutoLevels: case CmdLevels: case CmdBrightness: case CmdHueSat: case CmdNegative:
        case CmdGrayscale: case CmdSharpen: case CmdSharpenStrong: case CmdSharpenEdge: case CmdUnsharp: case CmdBlurN:
        case CmdBlurStrong: case CmdBlurSoft: case CmdGaussian: case CmdMaskGlassV: case CmdMaskGlassH: case CmdMaskNega:
        case CmdMaskXor: case CmdMaskRGB: case CmdMaskQ0: case CmdMaskWin: case CmdColorDepth: case CmdSwapRG: case CmdSwapRB:
        case CmdSwapGB: case CmdSwapRot: case CmdSwapRotRev: case CmdCloseImage: case CmdZoomIn: case CmdZoomOut:
        case CmdZoomDialog: case CmdOriginal: case CmdFitWindow: case CmdSelectDialog:
        case CmdWallCenter: case CmdWallTile: case CmdWallStretch: case CmdWallFitW: case CmdWallFitH:
            return img;
        case CmdCropSel: return img && selMode_ && !sel_.empty();
        case CmdUndo: return !undo_.empty();
        case CmdPaste: case CmdPasteNew: case CmdNewWndPasteClip: return ClipboardHasImage();
        case CmdCopyName: case CmdCopyFullPath: case CmdCopyFolderName: case CmdCopyFolderPath: return list_.Current() != nullptr;
        case CmdPrevPage: case CmdNextPage: return pages_ > 1;
        case CmdComicBack1: case CmdComicFwd1: return g_set.comic;
        case CmdWallDelete: return !g_set.wallpaperFile.empty() && FileExists(g_set.wallpaperFile);
        default: return true;
    }
}

bool Viewer::IsChecked(int cmd) const {
    switch (cmd) {
        case CmdSelectMode: return selMode_;
        case CmdSelUnitFree: return g_set.selUnit <= 1;
        case CmdSelUnit2: return g_set.selUnit == 2;
        case CmdSelUnit4: return g_set.selUnit == 4;
        case CmdSelUnit8: return g_set.selUnit == 8;
        case CmdSelUnit16: return g_set.selUnit == 16;
        case CmdSelUnit32: return g_set.selUnit == 32;
        case CmdSelUnitCustom: return g_set.selUnit > 1 && g_set.selUnit != 2 && g_set.selUnit != 4 && g_set.selUnit != 8 && g_set.selUnit != 16 && g_set.selUnit != 32;
        case CmdSlide: return sliding_;
        case CmdSortName: return g_set.sortKey == SortName;
        case CmdSortDate: return g_set.sortKey == SortDate;
        case CmdSortSize: return g_set.sortKey == SortSize;
        case CmdSortExt: return g_set.sortKey == SortExt;
        case CmdSortRandom: return g_set.sortKey == SortRandom;
        case CmdSortReverse: return g_set.sortReverse;
        case CmdSortNumeric: return g_set.sortNumeric;
        case CmdFullScreen: return full_;
        case CmdWndTopMost: return g_set.topMost;
        case CmdWndCaption: return g_set.caption;
        case CmdWndOuterFrame: return g_set.outerFrame;
        case CmdWndInnerFrame: return g_set.innerFrame;
        case CmdWndMenuBar: return g_set.menuBar;
        case CmdWndToolBar: return g_set.toolBar;
        case CmdWndStatusBar: return g_set.statusBar;
        case CmdWndScrollBar: return g_set.scrollBar;
        case CmdDisp1: return (full_ ? g_set.dispOptFull : g_set.dispOpt) == DispWndToImg;
        case CmdDisp2: return (full_ ? g_set.dispOptFull : g_set.dispOpt) == DispImgToWnd;
        case CmdDisp3: return (full_ ? g_set.dispOptFull : g_set.dispOpt) == DispBigToWnd;
        case CmdDisp4: return (full_ ? g_set.dispOptFull : g_set.dispOpt) == DispImgToDesk;
        case CmdDisp5: return (full_ ? g_set.dispOptFull : g_set.dispOpt) == DispBigToDesk;
        case CmdDisp0: return (full_ ? g_set.dispOptFull : g_set.dispOpt) == DispAsIs;
        case CmdFitInside: return g_set.fitOpt == FitInside;
        case CmdFitWidth: return g_set.fitOpt == FitWidth;
        case CmdFitHeight: return g_set.fitOpt == FitHeight;
        case CmdFitEither: return g_set.fitOpt == FitEither;
        case CmdRotOptNone: return g_set.rotOpt == RotNone;
        case CmdRotOptL: return g_set.rotOpt == RotLeft;
        case CmdRotOptR: return g_set.rotOpt == RotRight;
        case CmdRotOptPortL: return g_set.rotOpt == RotPortraitL;
        case CmdRotOptPortR: return g_set.rotOpt == RotPortraitR;
        case CmdRotOptLandL: return g_set.rotOpt == RotLandscapeL;
        case CmdRotOptLandR: return g_set.rotOpt == RotLandscapeR;
        case CmdRotOpt180: return g_set.rotOpt == Rot180;
        case CmdZoomLock: return g_set.zoomLock;
        case CmdOriginal: return img_.valid() && std::fabs(zoom_ - 1.0) < 1e-9 && !fitRule_;
        case CmdFitWindow: return fitRule_ != 0;
        case CmdHoverScroll: return g_set.hoverScroll;
        case CmdComic: return g_set.comic;
        case CmdComicRtoL: return g_set.comicRtoL;
        case CmdComicLtoR: return !g_set.comicRtoL;
        case CmdComicLandSingle: return g_set.comicLandscapeSingle;
        case CmdLangJa: return CurrentLanguage() == L"ja";
        case CmdLangEn: return CurrentLanguage() == L"en";
        case CmdLangTh: return CurrentLanguage() == L"th";
        default: return false;
    }
}

void Viewer::SetLanguage(const std::wstring& code) {
    g_set.lang = code;
    I18nLoad(code);
    RebuildUi();
    g_set.Save();
}

void Viewer::Execute(int cmd) {
    auto relayout = [&] { ApplyDisplayOption(true); UpdateScrollBars(); UpdateTitle(); UpdateStatus(); Redraw(); };
    auto setDisp = [&](int d) { (full_ ? g_set.dispOptFull : g_set.dispOpt) = d; relayout(); };
    auto reloadRot = [&](int r) { g_set.rotOpt = r; if (list_.Current() && untitled_.empty()) LoadCurrent(page_); };
    auto resort = [&] { list_.Sort(); UpdateStatus(); };
    auto toggleWnd = [&](bool& flag) { flag = !flag; if (!full_) ApplyWindowStyles(); };
    switch (cmd) {
        // ---------------------------------------------------------- file
        case CmdOpen: {
            if (!ConfirmDiscard()) return;
            Com<IFileOpenDialog> dlg;
            if (FAILED(CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(dlg.put())))) return;
            std::wstring spec;
            for (auto& e : ImageExtensions()) { if (!spec.empty()) spec += L";"; spec += L"*." + e; }
            spec += L";*.zip;*.cbz";
            std::wstring allName = T("dlg.imagefiles"), any = T("dlg.allfiles");
            COMDLG_FILTERSPEC fs[2] = {{allName.c_str(), spec.c_str()}, {any.c_str(), L"*.*"}};
            dlg->SetFileTypes(2, fs);
            if (list_.Current() && !list_.InArchive()) {
                Com<IShellItem> folder;
                if (SUCCEEDED(SHCreateItemFromParsingName(list_.Folder().c_str(), nullptr, IID_PPV_ARGS(folder.put())))) dlg->SetFolder(folder.get());
            }
            if (dlg->Show(hwnd_) != S_OK) return;
            Com<IShellItem> it;
            if (FAILED(dlg->GetResult(it.put()))) return;
            wchar_t* p = nullptr;
            if (SUCCEEDED(it->GetDisplayName(SIGDN_FILESYSPATH, &p))) { std::wstring s = p; CoTaskMemFree(p); OpenPath(s); }
            return;
        }
        case CmdReload:
            if (!ConfirmDiscard()) return;
            list_.Refresh();
            if (list_.Current()) LoadCurrent(page_);
            return;
        case CmdRename: DoRename(); return;
        case CmdMove: DoMoveCopy(true); return;
        case CmdCopyFile: DoMoveCopy(false); return;
        case CmdDelete: DoDelete(false); return;
        case CmdDeleteDirect: DoDelete(true); return;
        case CmdSave: DoSave(); return;
        case CmdSaveAs: DoSaveAs(); return;
        case CmdPrint: DoPrint(); return;
        case CmdNewWndBlank: DoNewWnd(0); return;
        case CmdNewWndCurrent: DoNewWnd(1); return;
        case CmdNewWndSendClip: DoNewWnd(2); return;
        case CmdNewWndPasteClip: DoNewWnd(3); return;
        case CmdApp1: case CmdApp2: case CmdApp3: case CmdApp4: case CmdApp5: case CmdApp6: case CmdApp7: case CmdApp8: case CmdApp9:
            DoRunApp(cmd - CmdApp1); return;
        case CmdAppSettings: DlgApps(); return;
        case CmdExit: PostMessageW(hwnd_, WM_CLOSE, 0, 0); return;
        // ---------------------------------------------------------- edit
        case CmdUndo: Undo(); return;
        case CmdSelectMode: selMode_ = !selMode_; if (!selMode_) sel_ = Rect{}; Redraw(); UpdateStatus(); return;
        case CmdSelectDialog: DlgSelect(); return;
        case CmdSelUnitFree: g_set.selUnit = 1; return;
        case CmdSelUnit2: g_set.selUnit = 2; return;
        case CmdSelUnit4: g_set.selUnit = 4; return;
        case CmdSelUnit8: g_set.selUnit = 8; return;
        case CmdSelUnit16: g_set.selUnit = 16; return;
        case CmdSelUnit32: g_set.selUnit = 32; return;
        case CmdSelUnitCustom: DlgSelUnit(); return;
        case CmdCut: DoCopy(true); return;
        case CmdCopy: DoCopy(false); return;
        case CmdPaste: DoPaste(false); return;
        case CmdPasteNew: DoPaste(true); return;
        case CmdCropSel:
            if (!Sel().empty()) { Rect s = Sel(); Edit([&](Image& im) { im = Crop(im, s); }); sel_ = Rect{}; Redraw(); }
            return;
        case CmdNewImage: DoNewImage(); return;
        case CmdCloseImage:
            if (!ConfirmDiscard()) return;
            StopAnimation(); ReleaseSource();
            img_ = Image(); untitled_.clear(); error_.clear(); modified_ = false; undo_.clear(); pages_ = 1; page_ = 0;
            UpdateScrollBars(); UpdateTitle(); UpdateStatus(); Redraw();
            return;
        case CmdCopyName: DoCopyName(0); return;
        case CmdCopyFullPath: DoCopyName(1); return;
        case CmdCopyFolderName: DoCopyName(2); return;
        case CmdCopyFolderPath: DoCopyName(3); return;
        case CmdCopyShortPath: DoCopyName(4); return;
        // ---------------------------------------------------------- image
        case CmdImageInfo: DoInfo(); return;
        case CmdResize: DlgResize(); return;
        case CmdCanvasSize: DlgCanvas(); return;
        case CmdRotL: Rotate(false); return;
        case CmdRotR: Rotate(true); return;
        case CmdFlipV: { Rect s = Sel(); Edit([&](Image& im) { FlipV(im, s); }); return; }
        case CmdFlipH: { Rect s = Sel(); Edit([&](Image& im) { FlipH(im, s); }); return; }
        case CmdRotAngle: DlgRotAngle(); return;
        case CmdAutoContrast: { Rect s = Sel(); Edit([&](Image& im) { AutoContrast(im, s); }); return; }
        case CmdAutoLevels: { Rect s = Sel(); Edit([&](Image& im) { AutoLevels(im, s); }); return; }
        case CmdLevels: DlgLevels(); return;
        case CmdBrightness: DlgBrightness(); return;
        case CmdHueSat: DlgHueSat(); return;
        case CmdNegative: case CmdMaskNega: { Rect s = Sel(); Edit([&](Image& im) { Invert(im, s); }); return; }
        case CmdGrayscale: { Rect s = Sel(); Edit([&](Image& im) { Grayscale(im, s); }); return; }
        case CmdSharpen: { Rect s = Sel(); Edit([&](Image& im) { Sharpen(im, 1, s); }); return; }
        case CmdSharpenStrong: { Rect s = Sel(); Edit([&](Image& im) { Sharpen(im, 2, s); }); return; }
        case CmdSharpenEdge: { Rect s = Sel(); Edit([&](Image& im) { SharpenEdges(im, s); }); return; }
        case CmdUnsharp: DlgUnsharp(); return;
        case CmdBlurN: { Rect s = Sel(); Edit([&](Image& im) { Blur(im, 1, s); }); return; }
        case CmdBlurStrong: { Rect s = Sel(); Edit([&](Image& im) { Blur(im, 2, s); }); return; }
        case CmdBlurSoft: { Rect s = Sel(); Edit([&](Image& im) { BlurSoft(im, s); }); return; }
        case CmdGaussian: DlgGaussian(); return;
        case CmdMaskGlassV: { Rect s = Sel(); Edit([&](Image& im) { GlassV(im, g_set.glassSize, g_set.glassRemainder, s); }); return; }
        case CmdMaskGlassH: { Rect s = Sel(); Edit([&](Image& im) { GlassH(im, g_set.glassSize, g_set.glassRemainder, s); }); return; }
        case CmdMaskXor: { Rect s = Sel(); Edit([&](Image& im) { Xor80(im, 7, s); }); return; }
        case CmdMaskRGB: { Rect s = Sel(); Edit([&](Image& im) { SwapChannels(im, 1, s); }); return; }
        case CmdMaskQ0: {
            Rect s = Sel();
            Q0Options o; o.glassV = g_set.q0GlassV; o.glassH = g_set.q0GlassH; o.nega = g_set.q0Nega; o.xor80 = g_set.q0Xor;
            o.flipV = g_set.q0FlipV; o.flipH = g_set.q0FlipH;
            Edit([&](Image& im) { Q0Mask(im, g_set.glassSize, o, s); });
            return;
        }
        case CmdMaskWin: { Rect s = Sel(); Edit([&](Image& im) { WinMask(im, s); }); return; }
        case CmdMaskOptions: DlgMaskOptions(); return;
        case CmdColorDepth: DlgColorDepth(); return;
        case CmdSwapRG: case CmdSwapRB: case CmdSwapGB: case CmdSwapRot: case CmdSwapRotRev: {
            Rect s = Sel(); int mode = cmd - CmdSwapRG;
            Edit([&](Image& im) { SwapChannels(im, mode, s); });
            return;
        }
        // ---------------------------------------------------------- list
        case CmdPrevFile: Navigate(-1); return;
        case CmdNextFile: Navigate(+1); return;
        case CmdDirPrev: Navigate(g_set.comic && g_set.comicRtoL ? +1 : -1); return;
        case CmdDirNext: Navigate(g_set.comic && g_set.comicRtoL ? -1 : +1); return;
        case CmdSkipPrev: Navigate(-std::max(2, g_set.skipCount), false); return;
        case CmdSkipNext: Navigate(+std::max(2, g_set.skipCount), false); return;
        case CmdFirstFile: GoTo(0); return;
        case CmdLastFile: GoTo(list_.Count() - 1); return;
        case CmdPrevPage: if (pages_ > 1 && page_ > 0) { bool keep = g_set.pagesInNav; g_set.pagesInNav = true; Navigate(-1); g_set.pagesInNav = keep; } return;
        case CmdNextPage: if (pages_ > 1 && page_ + 1 < pages_) { bool keep = g_set.pagesInNav; g_set.pagesInNav = true; Navigate(+1); g_set.pagesInNav = keep; } return;
        case CmdPrevFolder: if (ConfirmDiscard()) OpenFolder(list_.NextFolder(false)); return;
        case CmdNextFolder: if (ConfirmDiscard()) OpenFolder(list_.NextFolder(true)); return;
        case CmdPrevSibling: if (ConfirmDiscard()) OpenFolder(list_.SiblingFolder(false)); return;
        case CmdNextSibling: if (ConfirmDiscard()) OpenFolder(list_.SiblingFolder(true)); return;
        case CmdParentFolder: {
            std::wstring p = list_.ParentFolder();
            if (!p.empty() && FileList::FolderHasImages(p) && ConfirmDiscard()) OpenFolder(p);
            else if (g_set.beepAtEnd || p.empty()) MessageBeep(MB_OK);
            return;
        }
        case CmdJump: DlgJump(); return;
        case CmdSlide: if (sliding_) StartSlide(false); else DlgSlide(); return;
        case CmdSlideToggle: StartSlide(!sliding_); return;
        case CmdRandomJump:
            if (list_.Count() > 1) {
                unsigned r = GetTickCount() * 2654435761u;
                int i = int(r % unsigned(list_.Count() - 1));
                if (i >= list_.Index()) ++i;          // never the current file
                GoTo(i);
            }
            return;
        case CmdRefresh: list_.Refresh(); UpdateStatus(); return;
        case CmdSortName: g_set.sortKey = SortName; resort(); return;
        case CmdSortDate: g_set.sortKey = SortDate; resort(); return;
        case CmdSortSize: g_set.sortKey = SortSize; resort(); return;
        case CmdSortExt: g_set.sortKey = SortExt; resort(); return;
        case CmdSortRandom: g_set.sortKey = SortRandom; resort(); return;
        case CmdSortReverse: g_set.sortReverse = !g_set.sortReverse; resort(); return;
        case CmdSortNumeric: g_set.sortNumeric = !g_set.sortNumeric; resort(); return;
        // ---------------------------------------------------------- view
        case CmdFullScreen: ToggleFullScreen(); return;
        case CmdWndRestore: if (full_) ToggleFullScreen(); ShowWindow(hwnd_, SW_RESTORE); return;
        case CmdWndMinimize: ShowWindow(hwnd_, SW_MINIMIZE); return;
        case CmdWndMaximize: if (full_) ToggleFullScreen(); ShowWindow(hwnd_, IsZoomed(hwnd_) ? SW_RESTORE : SW_MAXIMIZE); return;
        case CmdWndTopMost: SetTopMost(!g_set.topMost); return;
        case CmdWndCaption: toggleWnd(g_set.caption); return;
        case CmdWndOuterFrame: toggleWnd(g_set.outerFrame); return;
        case CmdWndInnerFrame: toggleWnd(g_set.innerFrame); return;
        case CmdWndMenuBar: toggleWnd(g_set.menuBar); return;
        case CmdWndToolBar: g_set.toolBar = !g_set.toolBar; Layout(); return;
        case CmdWndStatusBar: g_set.statusBar = !g_set.statusBar; Layout(); UpdateStatus(); return;
        case CmdWndScrollBar: g_set.scrollBar = !g_set.scrollBar; UpdateScrollBars(); return;
        case CmdDisp1: setDisp(DispWndToImg); return;
        case CmdDisp2: setDisp(DispImgToWnd); return;
        case CmdDisp3: setDisp(DispBigToWnd); return;
        case CmdDisp4: setDisp(DispImgToDesk); return;
        case CmdDisp5: setDisp(DispBigToDesk); return;
        case CmdDisp0: setDisp(DispAsIs); return;
        case CmdFitInside: g_set.fitOpt = FitInside; relayout(); return;
        case CmdFitWidth: g_set.fitOpt = FitWidth; relayout(); return;
        case CmdFitHeight: g_set.fitOpt = FitHeight; relayout(); return;
        case CmdFitEither: g_set.fitOpt = FitEither; relayout(); return;
        case CmdRotOptNone: reloadRot(RotNone); return;
        case CmdRotOptL: reloadRot(RotLeft); return;
        case CmdRotOptR: reloadRot(RotRight); return;
        case CmdRotOptPortL: reloadRot(RotPortraitL); return;
        case CmdRotOptPortR: reloadRot(RotPortraitR); return;
        case CmdRotOptLandL: reloadRot(RotLandscapeL); return;
        case CmdRotOptLandR: reloadRot(RotLandscapeR); return;
        case CmdRotOpt180: reloadRot(Rot180); return;
        case CmdZoomIn: ZoomStep(+1); return;
        case CmdZoomOut: ZoomStep(-1); return;
        case CmdZoomDialog: DlgZoom(); return;
        case CmdZoomLock: g_set.zoomLock = !g_set.zoomLock; return;
        case CmdOriginal: SetZoom(1.0); return;
        case CmdFitWindow: {
            RECT r = ViewRect();
            if (GetWindowLongPtrW(view_, GWL_STYLE) & WS_VSCROLL) r.right += GetSystemMetrics(SM_CXVSCROLL);
            if (GetWindowLongPtrW(view_, GWL_STYLE) & WS_HSCROLL) r.bottom += GetSystemMetrics(SM_CYHSCROLL);
            fitRule_ = 1; zoom_ = FitZoom(r.right, r.bottom); sx_ = sy_ = 0;
            InvalidateCache(); ClampScroll(); UpdateScrollBars(); UpdateTitle(); UpdateStatus(); Redraw();
            return;
        }
        case CmdToggleOrigFit: case CmdToggleFitOrig:
            if (fitRule_ || std::fabs(zoom_ - 1.0) > 1e-9) Execute(CmdOriginal); else Execute(CmdFitWindow);
            return;
        case CmdHoverScroll: g_set.hoverScroll = !g_set.hoverScroll; return;
        case CmdComic: g_set.comic = !g_set.comic; if (list_.Current() && untitled_.empty() && ConfirmDiscard()) LoadCurrent(page_); return;
        case CmdComicBack1: if (g_set.comic) Navigate(-1, false); return;
        case CmdComicFwd1:
            if (g_set.comic && list_.Index() + 1 < list_.Count()) { list_.SetIndex(list_.Index() + 1); LoadCurrent(0, true); }
            return;
        case CmdComicRtoL: g_set.comicRtoL = true; if (spreadCount_ == 2) LoadCurrent(0, true); return;
        case CmdComicLtoR: g_set.comicRtoL = false; if (spreadCount_ == 2) LoadCurrent(0, true); return;
        case CmdComicLandSingle: g_set.comicLandscapeSingle = !g_set.comicLandscapeSingle; if (g_set.comic) LoadCurrent(0, true); return;
        case CmdLangJa: SetLanguage(L"ja"); return;
        case CmdLangEn: SetLanguage(L"en"); return;
        case CmdLangTh: SetLanguage(L"th"); return;
        // ---------------------------------------------------------- other
        case CmdPreferences: DlgPreferences(); return;
        case CmdKeySettings: DlgKeys(); return;
        case CmdMouseSettings: DlgMouse(); return;
        case CmdFormats: DoFormats(); return;
        case CmdAssociate: DlgAssociate(); return;
        case CmdCheckUpdate: DoCheckUpdate(true); return;
        case CmdWallCenter: DoWallpaper(WallCenter); return;
        case CmdWallTile: DoWallpaper(WallTile); return;
        case CmdWallStretch: DoWallpaper(WallStretch); return;
        case CmdWallFitW: DoWallpaper(WallFitW); return;
        case CmdWallFitH: DoWallpaper(WallFitH); return;
        case CmdWallDelete: DoWallDelete(); return;
        case CmdHelp: DoHelp(); return;
        case CmdAbout: DoAbout(); return;
        // ---------------------------------------------------------- popups / scroll
        case CmdPopupMenu: { POINT p; GetCursorPos(&p); RECT r; GetWindowRect(view_, &r); if (!PtInRect(&r, p)) p = {-1, -1}; ShowPopup(nullptr, p); return; }
        case CmdPopupApps: { POINT p; GetCursorPos(&p); ShowPopup("menu.apps", p); return; }
        case CmdPopupMask: { POINT p; GetCursorPos(&p); ShowPopup("menu.mask", p); return; }
        case CmdPopupSwap: { POINT p; GetCursorPos(&p); ShowPopup("menu.swap", p); return; }
        case CmdPopupDisp: { POINT p; GetCursorPos(&p); ShowPopup("menu.disp", p); return; }
        case CmdPopupWall: { POINT p; GetCursorPos(&p); ShowPopup("menu.wall", p); return; }
        case CmdScrollLeftS: ScrollBy(-Scale(g_set.wheelScroll, dpi_), 0); return;
        case CmdScrollRightS: ScrollBy(Scale(g_set.wheelScroll, dpi_), 0); return;
        case CmdScrollUpS: ScrollBy(0, -Scale(g_set.wheelScroll, dpi_)); return;
        case CmdScrollDownS: ScrollBy(0, Scale(g_set.wheelScroll, dpi_)); return;
        case CmdScrollLeftL: ScrollBy(-int(ViewRect().right / 2), 0); return;
        case CmdScrollRightL: ScrollBy(int(ViewRect().right / 2), 0); return;
        case CmdScrollUpL: ScrollBy(0, -int(ViewRect().bottom / 2)); return;
        case CmdScrollDownL: ScrollBy(0, int(ViewRect().bottom / 2)); return;
        case CmdScrollLeftE: sx_ = 0; ClampScroll(); UpdateScrollBars(); Redraw(); return;
        case CmdScrollRightE: sx_ = 1 << 30; ClampScroll(); UpdateScrollBars(); Redraw(); return;
        case CmdScrollUpE: sy_ = 0; ClampScroll(); UpdateScrollBars(); Redraw(); return;
        case CmdScrollDownE: sy_ = 1 << 30; ClampScroll(); UpdateScrollBars(); Redraw(); return;
        case CmdScrollPos7: ScrollTo(0, 0); return;
        case CmdScrollPos8: ScrollTo(0.5, 0); return;
        case CmdScrollPos9: ScrollTo(1, 0); return;
        case CmdScrollPos4: ScrollTo(0, 0.5); return;
        case CmdScrollPos5: ScrollTo(0.5, 0.5); return;
        case CmdScrollPos6: ScrollTo(1, 0.5); return;
        case CmdScrollPos1: ScrollTo(0, 1); return;
        case CmdScrollPos2: ScrollTo(0.5, 1); return;
        case CmdScrollPos3: ScrollTo(1, 1); return;
    }
}

// ======================================================================= save
bool Viewer::SaveTo(const std::wstring& path, bool ask) {
    if (ask && g_set.confirmOverwrite && FileExists(path)) {
        if (MsgBox(hwnd_, Fmt(T("msg.overwrite").c_str(), PathFileName(path).c_str()), MB_YESNO | MB_ICONQUESTION) != IDYES) return false;
    }
    FILETIME ct = {}, at = {}, mt = {};
    bool keep = g_set.keepTimestamp && FileExists(path);
    if (keep) {
        HANDLE h = CreateFileW(LongPath(path).c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, 0, nullptr);
        if (h != INVALID_HANDLE_VALUE) { GetFileTime(h, &ct, &at, &mt); CloseHandle(h); } else keep = false;
    }
    // the decoder may still hold the file
    if (list_.Current() && lstrcmpiW(list_.Current()->path.c_str(), path.c_str()) == 0) ReleaseSource();
    SaveOptions o; o.jpegQuality = g_set.jpegQuality; o.tiffCompression = g_set.tiffCompression;
    std::wstring err;
    HCURSOR oc = SetCursor(LoadCursor(nullptr, IDC_WAIT));
    bool ok = SaveImage(img_, path, o, &err);
    SetCursor(oc);
    if (!ok) { ShowError(T("err.save") + L"\n" + err); return false; }
    if (keep) {
        HANDLE h = CreateFileW(LongPath(path).c_str(), FILE_WRITE_ATTRIBUTES, FILE_SHARE_READ, nullptr, OPEN_EXISTING, 0, nullptr);
        if (h != INVALID_HANDLE_VALUE) { SetFileTime(h, &ct, &at, &mt); CloseHandle(h); }
    }
    modified_ = false;
    UpdateTitle();
    return true;
}

void Viewer::DoSave() {
    const FileEntry* e = list_.Current();
    if (!img_.valid()) return;
    if (!e || list_.InArchive() || !untitled_.empty() || IsEqualGUID(ContainerForExt(PathExt(e->path)), GUID_NULL)) { DoSaveAs(); return; }
    if (pages_ > 1 && MsgBox(hwnd_, T("msg.multipagesave"), MB_OKCANCEL | MB_ICONWARNING) != IDOK) return;
    std::wstring path = e->path;
    if (SaveTo(path, true)) { list_.Refresh(); UpdateStatus(); }
}

void Viewer::DoSaveAs() {
    if (!img_.valid()) return;
    Com<IFileSaveDialog> dlg;
    if (FAILED(CoCreateInstance(CLSID_FileSaveDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(dlg.put())))) return;
    struct Ty { const wchar_t* name; const wchar_t* spec; const wchar_t* ext; };
    static const Ty types[] = {{L"PNG", L"*.png", L"png"}, {L"JPEG", L"*.jpg;*.jpeg", L"jpg"}, {L"TIFF", L"*.tif;*.tiff", L"tif"},
                               {L"BMP", L"*.bmp", L"bmp"}, {L"GIF", L"*.gif", L"gif"}, {L"JPEG XR", L"*.jxr;*.wdp", L"jxr"}};
    COMDLG_FILTERSPEC fs[6];
    for (int i = 0; i < 6; ++i) fs[i] = {types[i].name, types[i].spec};
    dlg->SetFileTypes(6, fs);
    std::wstring base = untitled_.empty() && list_.Current() ? PathFileName(list_.Current()->path) : std::wstring(L"untitled.png");
    std::wstring ext = PathExt(base);
    int ti = 0;
    for (int i = 0; i < 6; ++i) if (ext == types[i].ext || (ext == L"jpeg" && i == 1) || (ext == L"tiff" && i == 2)) ti = i;
    if (IsEqualGUID(ContainerForExt(ext), GUID_NULL)) { ti = 0; base = base.substr(0, base.size() - ext.size()) + L"png"; }
    dlg->SetFileTypeIndex(UINT(ti + 1));
    dlg->SetDefaultExtension(types[ti].ext);
    dlg->SetFileName(base.c_str());
    std::wstring dir = !g_set.lastSaveDir.empty() ? g_set.lastSaveDir : (list_.Current() && !list_.InArchive() ? list_.Folder() : L"");
    if (!dir.empty()) {
        Com<IShellItem> f;
        if (SUCCEEDED(SHCreateItemFromParsingName(dir.c_str(), nullptr, IID_PPV_ARGS(f.put())))) dlg->SetFolder(f.get());
    }
    DWORD o = 0; dlg->GetOptions(&o); dlg->SetOptions(o | FOS_FORCEFILESYSTEM | FOS_OVERWRITEPROMPT);
    if (dlg->Show(hwnd_) != S_OK) return;
    Com<IShellItem> it;
    if (FAILED(dlg->GetResult(it.put()))) return;
    wchar_t* p = nullptr;
    if (FAILED(it->GetDisplayName(SIGDN_FILESYSPATH, &p))) return;
    std::wstring path = p; CoTaskMemFree(p);
    std::wstring e = PathExt(path);
    // format options
    GUID cf = ContainerForExt(e);
    if (IsEqualGUID(cf, GUID_ContainerFormatJpeg)) {
        Form f(T("dlg.saveopt"));
        f.Slider(T("dlg.jpegquality"), &g_set.jpegQuality, 1, 100);
        if (!f.Show(hwnd_)) return;
    } else if (IsEqualGUID(cf, GUID_ContainerFormatTiff)) {
        Form f(T("dlg.saveopt"));
        f.Combo(T("dlg.tiffcomp"), &g_set.tiffCompression,
                {T("tiff.auto"), T("tiff.none"), L"LZW", L"ZIP (Deflate)", L"CCITT Group 4", L"PackBits"});
        f.Label(T("dlg.tiffnote"));
        if (!f.Show(hwnd_)) return;
    }
    if (!SaveTo(path, false)) return;
    g_set.lastSaveDir = PathDir(path);
    if (!list_.InArchive() && list_.Current() && lstrcmpiW(PathDir(path).c_str(), list_.Folder().c_str()) == 0) {
        list_.Refresh();
        if (!untitled_.empty()) {
            // the saved file becomes the current item
            list_.Open(path); untitled_.clear();
        }
    } else if (!untitled_.empty()) {
        list_.Open(path); untitled_.clear();
    }
    UpdateTitle(); UpdateStatus();
}

// ================================================================= file ops
static bool ShellOp(HWND owner, UINT op, const std::wstring& from, const std::wstring& to, FILEOP_FLAGS flags) {
    std::wstring f = from; f.push_back(0);
    std::wstring t = to; t.push_back(0);
    SHFILEOPSTRUCTW s = {};
    s.hwnd = owner; s.wFunc = op; s.pFrom = f.c_str(); s.pTo = to.empty() ? nullptr : t.c_str(); s.fFlags = flags;
    int r = SHFileOperationW(&s);
    return r == 0 && !s.fAnyOperationsAborted;
}

void Viewer::DoRename() {
    const FileEntry* e = list_.Current();
    if (!e || list_.InArchive()) return;
    if (!ConfirmDiscard()) return;
    std::wstring name = e->name;
    Form f(T("cmd.rename"));
    f.Edit(T("dlg.newname"), &name, 48);
    if (!f.Show(hwnd_) || name.empty() || name == e->name) return;
    if (name.find_first_of(L"\\/:*?\"<>|") != std::wstring::npos) { ShowError(T("err.badname")); return; }
    std::wstring np = PathJoin(PathDir(e->path), name);
    ReleaseSource();
    if (!MoveFileExW(LongPath(e->path).c_str(), LongPath(np).c_str(), 0)) {
        ShowError(T("err.rename") + Fmt(L" (%lu)", GetLastError()));
        return;
    }
    list_.RenameCurrent(np);
    if (g_set.sortKey == SortName || g_set.sortKey == SortExt) list_.Sort();
    UpdateTitle(); UpdateStatus();
    if (pages_ > 1) { std::wstring err; Image tmp; LoadEntry(list_.Index(), page_, tmp, src_, &err); srcOpen_ = true; }
}

void Viewer::DoMoveCopy(bool move) {
    const FileEntry* e = list_.Current();
    if (!e || list_.InArchive()) return;
    if (move && !ConfirmDiscard()) return;
    std::wstring& last = move ? g_set.lastMoveDir : g_set.lastCopyDir;
    Com<IFileOpenDialog> dlg;
    if (FAILED(CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(dlg.put())))) return;
    DWORD o = 0; dlg->GetOptions(&o); dlg->SetOptions(o | FOS_PICKFOLDERS | FOS_FORCEFILESYSTEM);
    dlg->SetTitle(T(move ? "dlg.moveto" : "dlg.copyto").c_str());
    std::wstring start = last.empty() ? list_.Folder() : last;
    Com<IShellItem> fi;
    if (SUCCEEDED(SHCreateItemFromParsingName(start.c_str(), nullptr, IID_PPV_ARGS(fi.put())))) dlg->SetFolder(fi.get());
    if (dlg->Show(hwnd_) != S_OK) return;
    Com<IShellItem> it;
    if (FAILED(dlg->GetResult(it.put()))) return;
    wchar_t* p = nullptr;
    if (FAILED(it->GetDisplayName(SIGDN_FILESYSPATH, &p))) return;
    std::wstring dir = p; CoTaskMemFree(p);
    if (lstrcmpiW(dir.c_str(), list_.Folder().c_str()) == 0) return;
    last = dir;
    std::wstring src = e->path;
    if (move) ReleaseSource();
    if (!ShellOp(hwnd_, move ? FO_MOVE : FO_COPY, src, dir, FOF_ALLOWUNDO | (g_set.confirmOverwrite ? 0 : FOF_NOCONFIRMATION))) {
        if (move && pages_ > 1) LoadCurrent(page_);
        return;
    }
    if (move) {
        list_.RemoveCurrent();
        if (list_.Count()) LoadCurrent(0, true);
        else Execute(CmdCloseImage);
    }
}

void Viewer::DoDelete(bool direct) {
    const FileEntry* e = list_.Current();
    if (!e || list_.InArchive()) return;
    std::wstring path = e->path;
    if (g_set.confirmDelete) {
        std::wstring q = Fmt(T(direct ? "msg.deletedirect" : "msg.delete").c_str(), e->name.c_str());
        if (MsgBox(hwnd_, q, MB_YESNO | MB_ICONQUESTION | MB_DEFBUTTON2) != IDYES) return;
    }
    StopAnimation();
    ReleaseSource();
    FILEOP_FLAGS fl = FOF_NOCONFIRMATION | FOF_SILENT | (direct ? 0 : FOF_ALLOWUNDO);
    if (!ShellOp(hwnd_, FO_DELETE, path, L"", fl)) {
        ShowError(T("err.delete"));
        LoadCurrent(page_);
        return;
    }
    modified_ = false;
    int i = list_.Index();
    list_.RemoveCurrent();
    if (!list_.Count()) { Execute(CmdCloseImage); return; }
    list_.SetIndex(g_set.moveNextAfterDelete ? std::min(i, list_.Count() - 1) : std::max(0, i - 1));
    LoadCurrent(0, true);
}

// ================================================================== print
void Viewer::DoPrint() {
    if (!img_.valid()) return;
    PRINTDLGW pd = {sizeof(pd)};
    pd.hwndOwner = hwnd_;
    pd.Flags = PD_RETURNDC | PD_NOSELECTION | PD_NOPAGENUMS;
    if (!PrintDlgW(&pd)) return;
    HDC dc = pd.hDC;
    DOCINFOW di = {sizeof(di)};
    std::wstring name = list_.Current() ? list_.Current()->name : std::wstring(L"Lviewer");
    di.lpszDocName = name.c_str();
    if (StartDocW(dc, &di) > 0 && StartPage(dc) > 0) {
        int pw = GetDeviceCaps(dc, HORZRES), ph = GetDeviceCaps(dc, VERTRES);
        double s = std::min(double(pw) / img_.w, double(ph) / img_.h);
        // do not enlarge beyond the physical size given by the image DPI
        double natural = GetDeviceCaps(dc, LOGPIXELSX) / std::max(1.0, img_.dpiX);
        if (natural > 0 && natural < s) s = natural;
        int w = int(img_.w * s), h = int(img_.h * s);
        int x = (pw - w) / 2, y = (ph - h) / 2;
        // composite onto white
        std::vector<uint32_t> buf(img_.px.size());
        for (size_t i = 0; i < buf.size(); ++i) {
            uint32_t p = img_.px[i], a = p >> 24;
            if (a == 255) { buf[i] = p; continue; }
            uint32_t r = (((p >> 16) & 255) * a + 255 * (255 - a)) / 255, g = (((p >> 8) & 255) * a + 255 * (255 - a)) / 255,
                     b = ((p & 255) * a + 255 * (255 - a)) / 255;
            buf[i] = 0xFF000000 | (r << 16) | (g << 8) | b;
        }
        BITMAPINFO bi = {};
        bi.bmiHeader.biSize = sizeof(bi.bmiHeader); bi.bmiHeader.biWidth = img_.w; bi.bmiHeader.biHeight = -img_.h;
        bi.bmiHeader.biPlanes = 1; bi.bmiHeader.biBitCount = 32; bi.bmiHeader.biCompression = BI_RGB;
        SetStretchBltMode(dc, HALFTONE); SetBrushOrgEx(dc, 0, 0, nullptr);
        StretchDIBits(dc, x, y, w, h, 0, 0, img_.w, img_.h, buf.data(), &bi, DIB_RGB_COLORS, SRCCOPY);
        EndPage(dc);
        EndDoc(dc);
    }
    DeleteDC(dc);
    if (pd.hDevMode) GlobalFree(pd.hDevMode);
    if (pd.hDevNames) GlobalFree(pd.hDevNames);
}

// ============================================================ other apps
static std::wstring TempCopyOfEntry(FileList& list, int index) {
    std::vector<uint8_t> data;
    if (!list.ReadArchiveEntry(index, data)) return L"";
    wchar_t tmp[MAX_PATH]; GetTempPathW(MAX_PATH, tmp);
    std::wstring dir = PathJoin(tmp, L"Lviewer");
    CreateDirectoryW(dir.c_str(), nullptr);
    std::wstring p = PathJoin(dir, PathFileName(list.At(index).path));
    HANDLE h = CreateFileW(p.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, 0, nullptr);
    if (h == INVALID_HANDLE_VALUE) return L"";
    DWORD wr; WriteFile(h, data.data(), DWORD(data.size()), &wr, nullptr); CloseHandle(h);
    return p;
}

void Viewer::DoRunApp(int i) {
    if (i < 0 || i >= 9 || g_set.apps[i].path.empty()) { DlgApps(); return; }
    std::wstring file;
    if (list_.Current()) file = list_.InArchive() ? TempCopyOfEntry(list_, list_.Index()) : list_.Current()->path;
    std::wstring args = g_set.apps[i].args;
    if (args.find(L"%1") != std::wstring::npos) {
        size_t k;
        while ((k = args.find(L"%1")) != std::wstring::npos) args.replace(k, 2, file);
    } else if (!file.empty()) {
        if (!args.empty()) args += L" ";
        args += L"\"" + file + L"\"";
    }
    HINSTANCE r = ShellExecuteW(hwnd_, nullptr, g_set.apps[i].path.c_str(), args.c_str(), file.empty() ? nullptr : PathDir(file).c_str(), SW_SHOWNORMAL);
    if (INT_PTR(r) <= 32) ShowError(T("err.runapp") + L"\n" + g_set.apps[i].path);
}

void Viewer::DoNewWnd(int kind) {
    std::wstring cmd = L"\"" + ExePath() + L"\" /newwnd";
    if (!opt_.user.empty()) cmd += L" /user=\"" + opt_.user + L"\"";
    switch (kind) {
        case 0: cmd += L" /blank"; break;
        case 1: if (list_.Current()) cmd += L" \"" + list_.CurrentDisplayPath() + L"\""; break;
        case 2: if (!CopyImageToClipboard(hwnd_, img_)) return; cmd += L" /getclip"; break;
        case 3: cmd += L" /getclip"; break;
    }
    STARTUPINFOW si = {sizeof(si)};
    PROCESS_INFORMATION pi = {};
    if (CreateProcessW(nullptr, &cmd[0], nullptr, nullptr, FALSE, 0, nullptr, nullptr, &si, &pi)) {
        CloseHandle(pi.hThread); CloseHandle(pi.hProcess);
    }
}

// ============================================================= clipboard
void Viewer::DoCopy(bool cut) {
    if (!img_.valid()) return;
    Rect s = Sel();
    Image part = s.empty() ? img_ : Crop(img_, s);
    CopyImageToClipboard(hwnd_, part);
    if (cut) {
        Rect r = s.empty() ? img_.full() : s;
        Edit([&](Image& im) {
            Rect c = ClipRect(im, r);
            for (int y = c.y; y < c.bottom(); ++y) std::fill(im.row(y) + c.x, im.row(y) + c.right(), 0xFFFFFFFF);
        });
    }
}

void Viewer::DoPaste(bool asNew) {
    Image clip;
    if (!PasteImageFromClipboard(hwnd_, clip)) {
        // file names on the clipboard
        if (OpenClipboard(hwnd_)) {
            HANDLE h = GetClipboardData(CF_HDROP);
            std::wstring f;
            if (h) { wchar_t buf[32768]; if (DragQueryFileW(HDROP(h), 0, buf, 32768)) f = buf; }
            CloseClipboard();
            if (!f.empty() && ConfirmDiscard()) OpenPath(f);
        }
        return;
    }
    if (asNew || !img_.valid()) {
        if (!ConfirmDiscard()) return;
        StopAnimation(); ReleaseSource();
        img_ = std::move(clip); untitled_ = T("misc.untitled"); error_.clear(); modified_ = true; undo_.clear(); pages_ = 1; page_ = 0;
        sel_ = Rect{};
        ApplyDisplayOption(true); UpdateScrollBars(); UpdateTitle(); UpdateStatus(); Redraw();
        return;
    }
    Rect s = Sel();
    int ox = s.empty() ? 0 : s.x, oy = s.empty() ? 0 : s.y;
    Edit([&](Image& im) {
        for (int y = 0; y < clip.h; ++y) {
            int dy = oy + y; if (dy < 0 || dy >= im.h) continue;
            for (int x = 0; x < clip.w; ++x) {
                int dx = ox + x; if (dx < 0 || dx >= im.w) continue;
                im.at(dx, dy) = clip.at(x, y);
            }
        }
        if (clip.mode == ColorMode::Color) im.mode = ColorMode::Color;
    });
    sel_ = Rect{ox, oy, std::min(clip.w, img_.w - ox), std::min(clip.h, img_.h - oy)};
    selMode_ = true;
    Redraw();
}

void Viewer::DoNewImage() {
    if (!ConfirmDiscard()) return;
    static int w = 640, h = 480; static COLORREF col = RGB(255, 255, 255);
    Form f(T("cmd.newimage"));
    f.Int(T("dlg.width"), &w, 1, 30000, L"px");
    f.Int(T("dlg.height"), &h, 1, 30000, L"px");
    f.Color(T("dlg.color"), &col);
    if (!f.Show(hwnd_)) return;
    StopAnimation(); ReleaseSource();
    img_ = Image(w, h, ARGB(255, GetRValue(col), GetGValue(col), GetBValue(col)));
    untitled_ = T("misc.untitled"); error_.clear(); modified_ = true; undo_.clear(); pages_ = 1; page_ = 0; sel_ = Rect{};
    ApplyDisplayOption(true); UpdateScrollBars(); UpdateTitle(); UpdateStatus(); Redraw();
}

void Viewer::DoCopyName(int kind) {
    const FileEntry* e = list_.Current();
    if (!e) return;
    std::wstring full = list_.CurrentDisplayPath(), s;
    switch (kind) {
        case 0: s = PathFileName(e->path); break;
        case 1: s = full; break;
        case 2: s = PathFileName(list_.InArchive() ? list_.ArchivePath() : list_.Folder()); break;
        case 3: s = list_.InArchive() ? list_.ArchivePath() : list_.Folder(); break;
        case 4: {
            wchar_t buf[MAX_PATH] = {};
            if (GetShortPathNameW(e->path.c_str(), buf, MAX_PATH)) s = buf; else s = e->path;
        } break;
    }
    CopyTextToClipboard(hwnd_, s);
}

void Viewer::DoInfo() {
    if (!img_.valid()) return;
    std::wstring t;
    auto add = [&](const std::wstring& k, const std::wstring& v) { t += k + L": " + v + L"\n"; };
    if (const FileEntry* e = list_.Current(); e && untitled_.empty()) {
        add(T("info.path"), list_.CurrentDisplayPath());
        add(T("info.filesize"), FormatBytes(e->size) + Fmt(L" (%llu bytes)", e->size));
        add(T("info.modified"), FormatFileTime(e->mtime));
        ImageSource s; std::wstring err;
        bool ok = list_.InArchive() ? [&] { std::vector<uint8_t> d; return list_.ReadArchiveEntry(list_.Index(), d) && s.OpenMemory(std::move(d), &err); }()
                                    : s.OpenFile(e->path, &err);
        if (ok) for (auto& kv : s.Properties(page_)) add(kv.first, kv.second);
    }
    t += L"\n";
    add(T("info.display"), Fmt(L"%d x %d, %d bpp, %ls", img_.w, img_.h, img_.srcBpp,
                                img_.mode == ColorMode::BW ? T("info.mode.bw").c_str() : img_.mode == ColorMode::Gray ? T("info.mode.gray").c_str() : T("info.mode.color").c_str()));
    add(T("info.memory"), FormatBytes((unsigned long long)img_.w * img_.h * 4));
    add(T("info.transparency"), img_.hasAlpha ? T("misc.yes") : T("misc.no"));
    add(T("info.zoom"), Fmt(L"%.1f%%", zoom_ * 100));
    Form f(T("cmd.imageinfo"));
    f.Text(L"", t, 16, 70);
    f.Button(T("btn.copytext"), [this, t] { CopyTextToClipboard(hwnd_, t); });
    f.Show(hwnd_);
}

// ============================================================== wallpaper
void Viewer::DoWallpaper(int style) {
    if (!img_.valid()) return;
    int sw = GetSystemMetrics(SM_CXSCREEN), sh = GetSystemMetrics(SM_CYSCREEN);
    Image im = img_;
    if (style == WallFitW || style == WallFitH) {
        double s = style == WallFitW ? double(sw) / im.w : double(sh) / im.h;
        im = Resize(im, std::max(1, int(im.w * s)), std::max(1, int(im.h * s)), ResizeQuality::Bicubic);
    }
    for (auto& p : im.px) {   // flatten alpha onto black
        uint32_t a = p >> 24;
        if (a != 255) p = 0xFF000000 | ((((p >> 16) & 255) * a / 255) << 16) | ((((p >> 8) & 255) * a / 255) << 8) | ((p & 255) * a / 255);
    }
    im.hasAlpha = false;
    wchar_t* app = nullptr;
    std::wstring dir;
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_RoamingAppData, 0, nullptr, &app))) { dir = PathJoin(app, L"Lviewer"); CoTaskMemFree(app); }
    SHCreateDirectoryExW(nullptr, dir.c_str(), nullptr);
    std::wstring path = PathJoin(dir, L"Wallpaper.bmp");
    SaveOptions o; std::wstring err;
    if (!SaveImage(im, path, o, &err)) { ShowError(T("err.save") + L"\n" + err); return; }
    HKEY k;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Control Panel\\Desktop", 0, KEY_SET_VALUE, &k) == ERROR_SUCCESS) {
        const wchar_t* ws = L"0"; const wchar_t* tile = L"0";
        if (style == WallTile) tile = L"1";
        if (style == WallStretch) ws = L"2";
        RegSetValueExW(k, L"WallpaperStyle", 0, REG_SZ, (const BYTE*)ws, DWORD((wcslen(ws) + 1) * 2));
        RegSetValueExW(k, L"TileWallpaper", 0, REG_SZ, (const BYTE*)tile, DWORD((wcslen(tile) + 1) * 2));
        RegCloseKey(k);
    }
    SystemParametersInfoW(SPI_SETDESKWALLPAPER, 0, (void*)path.c_str(), SPIF_UPDATEINIFILE | SPIF_SENDCHANGE);
    g_set.wallpaperFile = path;
    g_set.Save();
}

void Viewer::DoWallDelete() {
    if (g_set.wallpaperFile.empty()) return;
    if (MsgBox(hwnd_, T("msg.walldelete"), MB_YESNO | MB_ICONQUESTION) != IDYES) return;
    wchar_t cur[MAX_PATH] = {};
    SystemParametersInfoW(SPI_GETDESKWALLPAPER, MAX_PATH, cur, 0);
    if (lstrcmpiW(cur, g_set.wallpaperFile.c_str()) == 0)
        SystemParametersInfoW(SPI_SETDESKWALLPAPER, 0, (void*)L"", SPIF_UPDATEINIFILE | SPIF_SENDCHANGE);
    DeleteFileW(g_set.wallpaperFile.c_str());
    g_set.wallpaperFile.clear();
    g_set.Save();
}

// ================================================================ help etc.
void Viewer::DoHelp() {
    std::wstring t = T("help.intro") + L"\n\n";
    std::function<void(const MenuNode&, int)> walk = [&](const MenuNode& n, int depth) {
        for (auto& k : n.kids) {
            if (k.cmd == -1) continue;
            if (k.label) {
                t += std::wstring(depth * 2, L' ') + L"■ " + StripMnemonic(T(k.label)) + L"\n";
                walk(k, depth + 1);
                continue;
            }
            std::wstring keys;
            for (auto& c : g_keymap.ChordsOf(k.cmd)) { if (!keys.empty()) keys += L", "; keys += ChordToDisplay(c); }
            t += std::wstring(depth * 2, L' ') + CommandLabel(k.cmd) + (keys.empty() ? L"" : L"   [" + keys + L"]") + L"\n";
        }
    };
    walk(MenuTree(), 0);
    t += L"\n■ " + T("help.hidden") + L"\n";
    for (int c : {int(CmdDeleteDirect), int(CmdRandomJump), int(CmdSlideToggle), int(CmdToggleOrigFit), int(CmdPopupMenu), int(CmdDirPrev), int(CmdDirNext)}) {
        std::wstring keys;
        for (auto& ch : g_keymap.ChordsOf(c)) { if (!keys.empty()) keys += L", "; keys += ChordToDisplay(ch); }
        t += L"  " + CommandLabel(c) + (keys.empty() ? L"" : L"   [" + keys + L"]") + L"\n";
    }
    t += L"\n" + T("help.mouse") + L"\n";
    Form f(T("cmd.help"));
    f.Text(L"", t, 24, 80);
    f.Show(hwnd_);
}

void Viewer::DoAbout() {
    std::wstring t = L"Lviewer " LV_VERSION_STR L"\n\n" + T("about.text") + L"\n\n" + T("about.license");
    MsgBox(hwnd_, t, MB_ICONINFORMATION);
}

void Viewer::DoFormats() {
    std::wstring t = T("formats.intro") + L"\n\n";
    for (auto& c : ListCodecs()) {
        std::wstring ex;
        for (auto& e : c.exts) { if (!ex.empty()) ex += L", "; ex += e; }
        t += c.name + L"  [" + (c.canWrite ? T("formats.rw") : T("formats.r")) + L"]\n    " + ex + L"\n";
    }
    t += L"\nZIP / CBZ  [" + T("formats.r") + L"]\n    zip, cbz\n";
    Form f(T("cmd.formats"));
    f.Text(L"", t, 20, 70);
    f.Show(hwnd_);
}

// ================================================================ updates
namespace {
const UINT WM_LV_UPDATE = WM_APP + 10;
struct UpdateThreadArgs { HWND hwnd; };
DWORD WINAPI UpdateThread(LPVOID p) {
    std::unique_ptr<UpdateThreadArgs> a(static_cast<UpdateThreadArgs*>(p));
    auto* r = new ReleaseInfo();
    std::wstring err;
    if (FetchLatestRelease(*r, &err) && CompareVersions(r->tag, LV_VERSION_STR) > 0 && IsWindow(a->hwnd)) {
        if (PostMessageW(a->hwnd, WM_LV_UPDATE, 0, LPARAM(r))) return 0;
    }
    delete r;
    return 0;
}
} // namespace

void Viewer::StartBackgroundUpdateCheck() {
    if (!g_set.updateCheckAtStart) return;
    int today = TodayNumber();
    if (g_set.lastUpdateCheckDay == today) return;          // at most once a day
    g_set.lastUpdateCheckDay = today;
    HANDLE h = CreateThread(nullptr, 0, UpdateThread, new UpdateThreadArgs{hwnd_}, 0, nullptr);
    if (h) CloseHandle(h);
}

void Viewer::DoCheckUpdate(bool manual) {
    (void)manual;
    HCURSOR oc = SetCursor(LoadCursor(nullptr, IDC_WAIT));
    ReleaseInfo r; std::wstring err;
    bool ok = FetchLatestRelease(r, &err);
    SetCursor(oc);
    g_set.lastUpdateCheckDay = TodayNumber();
    if (!ok) { ShowError(T("update.checkfailed") + L"\n" + err); return; }
    if (CompareVersions(r.tag, LV_VERSION_STR) <= 0) {
        MsgBox(hwnd_, Fmt(T("update.latest").c_str(), LV_VERSION_STR), MB_ICONINFORMATION);
        return;
    }
    OfferUpdate(r);
}

void Viewer::OfferUpdate(const ReleaseInfo& r) {
    std::wstring notes = r.notes.size() > 800 ? r.notes.substr(0, 800) + L"..." : r.notes;
    std::wstring q = Fmt(T("update.available").c_str(), r.tag.c_str(), LV_VERSION_STR) + L"\n\n" + notes + L"\n\n" + T("update.question");
    int ans = MsgBox(hwnd_, q, MB_YESNOCANCEL | MB_ICONQUESTION);
    if (ans == IDNO) { ShellExecuteW(hwnd_, nullptr, r.pageUrl.c_str(), nullptr, nullptr, SW_SHOWNORMAL); return; }
    if (ans != IDYES) return;
    if (!ConfirmDiscard()) return;
    wchar_t tmp[MAX_PATH]; GetTempPathW(MAX_PATH, tmp);
    std::wstring dir = PathJoin(tmp, L"Lviewer-update");
    CreateDirectoryW(dir.c_str(), nullptr);
    std::wstring zip = PathJoin(dir, L"update.zip");
    std::wstring err;
    HCURSOR oc = SetCursor(LoadCursor(nullptr, IDC_WAIT));
    bool ok = DownloadToFile(r.zipUrl, zip, &err);
    if (ok) {
        StopAnimation(); ReleaseSource();
        ok = InstallUpdate(zip, &err);
    }
    SetCursor(oc);
    DeleteFileW(zip.c_str());
    if (!ok) {
        if (MsgBox(hwnd_, T("update.failed") + L"\n" + err + L"\n\n" + T("update.openpage"), MB_YESNO | MB_ICONWARNING) == IDYES)
            ShellExecuteW(hwnd_, nullptr, r.pageUrl.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
        if (list_.Current() && untitled_.empty()) LoadCurrent(page_);
        return;
    }
    MsgBox(hwnd_, Fmt(T("update.done").c_str(), r.tag.c_str()), MB_ICONINFORMATION);
    // restart with the current file
    std::wstring cmd = L"\"" + ExePath() + L"\" /newwnd";
    if (!opt_.user.empty()) cmd += L" /user=\"" + opt_.user + L"\"";
    if (list_.Current()) cmd += L" \"" + list_.CurrentDisplayPath() + L"\"";
    g_set.Save();
    STARTUPINFOW si = {sizeof(si)};
    PROCESS_INFORMATION pi = {};
    if (CreateProcessW(nullptr, &cmd[0], nullptr, nullptr, FALSE, 0, nullptr, nullptr, &si, &pi)) {
        CloseHandle(pi.hThread); CloseHandle(pi.hProcess);
        modified_ = false;
        PostMessageW(hwnd_, WM_CLOSE, 0, 0);
    }
}

} // namespace lv
