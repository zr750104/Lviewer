# Lviewer

TIFF を**プラグインなし**で開ける、軽快な単画面型の画像ビューワ（Windows 10/11, 64bit）。
MassiGra 0.44 の操作体系（既定キー・メニュー構成・表示オプション）を手本に新規実装しています。解析内容と機能対応表は [docs/spec.md](docs/spec.md)。

- **TIFF を標準対応**：マルチページ、LZW / ZIP / PackBits / JPEG / CCITT G3・G4、16bit、透明。Windows 標準の WIC を使うので追加 DLL 不要
- JPEG・PNG・GIF（アニメ）・BMP・ICO・JPEG XR、コーデックがあれば WebP・HEIF・RAW も
- ZIP / CBZ 内の画像を直接閲覧、漫画モード（見開き・右綴じ/左綴じ）
- 表示言語：**日本語 / English / ภาษาไทย** を再起動なしで切替（表示 > 表示言語、または 設定 > 全般）
- 単体 exe・インストール不要・レジストリを汚さない（関連付けをした場合のみ HKCU に登録）

## 使い方（主な操作）

| 操作 | キー / マウス |
|---|---|
| 次 / 前のファイル（マルチページ TIFF はページ） | Space / BackSpace、ホイール下 / 上 |
| TIFF のページ移動 | Ctrl + → / ← |
| 元のサイズ / ウィンドウに合わせる | G / F（X2 ボタンで切替） |
| ズーム | テンキー + / −、Ctrl + ホイール、X1 ボタンを押して左右ドラッグ |
| 全画面 | Enter、ダブルクリック |
| 画像の情報（TIFF の圧縮方式など） | I |
| 回転 | L / R |
| 名前を付けて保存 / 上書き保存 | S / Ctrl+S |
| 削除（ごみ箱）/ 直接削除 | Delete / Shift+Delete |
| メニュー | 右クリック、Apps キー |
| 終了 | Esc、ホイールクリック |
| 全キー一覧 | F1 |

キーとマウスの割り当ては「その他 > キー設定 / マウス設定」で変更できます。

## 設定ファイル

`Lviewer.exe` と同じフォルダの `Lviewer.ini`（書き込めない場所なら `%APPDATA%\Lviewer\Lviewer.ini`）。
起動オプション：`/slide` `/getclip` `/blank` `/newwnd` `/user=名前`。

翻訳の修正は、`res/lang/<言語>.ini` を exe と同じフォルダの `lang\` にコピーして編集すれば再ビルドなしで反映されます。

## ビルド

### Windows（Visual Studio 2022）
```bat
cmake -S . -B build -A x64
cmake --build build --config Release
```
`build\Release\Lviewer.exe` ができます。

### Linux / macOS からのクロスビルド（MinGW-w64）
```bash
cmake -S . -B build-mingw -DCMAKE_TOOLCHAIN_FILE=cmake/mingw-w64-x86_64.cmake -DCMAKE_BUILD_TYPE=Release
cmake --build build-mingw -j
```

### 単体テスト（画像処理・自然順ソート・言語ファイルの整合）
```bash
cmake -S . -B build-test -DLVIEWER_TESTS=ON
cmake --build build-test && (cd build-test && ctest --output-on-failure)
```

### 翻訳ファイルの再生成
文言は `tools/gen_lang.py` の表で一元管理しています（3 言語でキーが揃うことを保証）。
```bash
python3 tools/gen_lang.py
```

## ライセンス表記
ZIP の読み込みに [miniz](https://github.com/richgel999/miniz)（MIT License）を使用しています（`third_party/miniz/LICENSE`）。
