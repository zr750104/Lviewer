#pragma once
#include "Win.h"
#include "Codec.h"
#include "FileList.h"
#include "Commands.h"
#include "core/Filters.h"
#include <map>

namespace lv {

struct StartupOptions {
    std::wstring path;
    bool slide = false, getclip = false, blank = false, newwnd = false;
    std::wstring user; bool userGiven = false;
};

class Viewer {
public:
    bool Create(const StartupOptions& opt);
    int Run();
    static Viewer* Instance();

    // ---- used by dialogs/commands
    HWND Hwnd() const { return hwnd_; }
    void Execute(int cmd);
    bool IsChecked(int cmd) const;
    bool IsEnabled(int cmd) const;
    void OpenPath(const std::wstring& path);
    void Redraw();
    Image& Img() { return img_; }
    bool HasImage() const { return img_.valid(); }

private:
    // window procs
    static LRESULT CALLBACK MainProc(HWND, UINT, WPARAM, LPARAM);
    static LRESULT CALLBACK ViewProc(HWND, UINT, WPARAM, LPARAM);
    LRESULT OnMain(UINT, WPARAM, LPARAM);
    LRESULT OnView(UINT, WPARAM, LPARAM);
    bool PreTranslate(MSG& m);

    // UI construction
    void CreateToolbar();
    void RebuildUi();                   // menus, toolbar tips, fonts after language/DPI change
    HMENU BuildMenu(const MenuNode& n, bool popup);
    void FillDynamic(HMENU m, int kind);
    void UpdateMenuStates(HMENU m);
    void ShowPopup(const char* label, POINT pt);
    void Layout();
    void ApplyWindowStyles();
    void UpdateTitle();
    void UpdateStatus();
    void UpdateScrollBars();

    // loading / navigation
    bool ConfirmDiscard();             // ask to save modified image; false = cancel
    void LoadCurrent(int page = 0, bool fromNav = false);
    bool LoadEntry(int index, int page, Image& out, ImageSource& src, std::wstring* err);
    void ApplyAutoRotate(Image& im, int orient);
    void BuildSpread();
    void Navigate(int delta, bool usePages = true);
    void GoTo(int index, int page = 0);
    void OpenFolder(const std::wstring& dir, bool last = false);
    void ReleaseSource();
    void StartAnimation();
    void StopAnimation();
    void ShowError(const std::wstring& msg);

    // view math
    RECT ViewRect() const;
    double FitZoom(int vw, int vh) const;
    void ApplyDisplayOption(bool initial);
    void SetZoom(double z, POINT* anchor = nullptr, bool userAction = true);
    void ZoomStep(int dir, POINT* anchor = nullptr);
    void ClampScroll();
    void ScrollBy(int dx, int dy);
    void ScrollTo(double fx, double fy);   // fractional position 0..1
    int DispW() const; int DispH() const;
    POINT ImageOrigin() const;              // top-left of image in view coords
    bool ViewToImage(POINT v, double& ix, double& iy) const;
    void InvalidateCache() { cacheValid_ = false; }

    // painting
    void Paint(HDC dc);
    void RenderTo(uint32_t* dst, int vw, int vh, int stride);
    void EnsureCache();
    void DrawSelection(HDC dc);

    // editing
    void PushUndo();
    void Undo();
    Rect Sel() const;                       // current selection or empty (= whole image)
    void Modified();
    template <class F> void Edit(F f) { if (!HasImage()) return; PushUndo(); f(img_); Modified(); }
    void Rotate(bool cw);
    bool SaveTo(const std::wstring& path, bool ask);
    void DoSave(); void DoSaveAs();
    void DoRename(); void DoMoveCopy(bool move); void DoDelete(bool direct);
    void DoPrint(); void DoRunApp(int i); void DoNewWnd(int kind);
    void DoCopy(bool cut); void DoPaste(bool asNew);
    void DoNewImage();
    void DoCopyName(int kind);
    void DoInfo();
    void DoWallpaper(int style); void DoWallDelete();
    void DoHelp(); void DoAbout(); void DoFormats();
    void SetLanguage(const std::wstring& code);
    // dialogs (Dialogs.cpp)
    void DlgResize(); void DlgCanvas(); void DlgRotAngle(); void DlgLevels(); void DlgBrightness(); void DlgHueSat();
    void DlgUnsharp(); void DlgGaussian(); void DlgColorDepth(); void DlgMaskOptions(); void DlgSelect(); void DlgSelUnit();
    void DlgZoom(); void DlgJump(); void DlgSlide(); void DlgPreferences(); void DlgMouse(); void DlgApps();
    void DlgAssociate(); void DlgKeys();
    template <class F> void PreviewFilter(class Form& f, F apply);

    // fullscreen / window
    void ToggleFullScreen();
    void SetTopMost(bool on);
    void FitWindowToImage(bool desk);
    void StartSlide(bool on);

    // input helpers
    bool DispatchChord(const Chord& c);
    void HoverScroll(POINT p);

    // ---- members
    HWND hwnd_ = nullptr, view_ = nullptr, tb_ = nullptr, sb_ = nullptr;
    HMENU menuBar_ = nullptr;
    std::map<HMENU, int> dynMenus_;
    HIMAGELIST tbImages_ = nullptr;
    HFONT uiFont_ = nullptr;
    int dpi_ = 96;

    FileList list_;
    ImageSource src_;
    bool srcOpen_ = false;
    Image img_;
    std::vector<Image> anim_; std::vector<int> delays_; int animFrame_ = 0; bool animating_ = false;
    int page_ = 0, pages_ = 1;
    int spreadCount_ = 1;                 // 2 when a comic spread is shown
    bool modified_ = false;
    std::wstring untitled_;               // non-empty when showing an unsaved new/pasted image
    std::wstring error_;
    std::vector<Image> undo_;

    double zoom_ = 1.0;
    int fitRule_ = 0;                     // 0 none, 1 fit (may enlarge), 2 fit (shrink only)
    int sx_ = 0, sy_ = 0;                 // scroll offset in display pixels

    // cache of the down-scaled image
    Image cache_; bool cacheValid_ = false; double cacheZoom_ = 0;

    // selection
    bool selMode_ = false; Rect sel_; bool selecting_ = false; double selAx_ = 0, selAy_ = 0;

    // mouse state
    bool lDown_ = false, rDown_ = false, dragging_ = false, x1Down_ = false, x1Zooming_ = false;
    bool rWheelUsed_ = false, lWheelUsed_ = false, suppressClick_ = false;
    POINT downPt_ = {}, lastPt_ = {};
    int wheelAcc_ = 0, hwheelAcc_ = 0;
    double x1StartZoom_ = 1;

    // fullscreen
    bool full_ = false;
    WINDOWPLACEMENT prevPlacement_ = {sizeof(WINDOWPLACEMENT)};
    DWORD prevStyle_ = 0;
    bool sliding_ = false;
    DWORD lastMouseMove_ = 0; bool cursorHidden_ = false;

    StartupOptions opt_;
};

} // namespace lv
