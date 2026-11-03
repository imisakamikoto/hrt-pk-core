# hrt-pk-core

<img src="assets/logo.png" width="88" align="right" alt="hrt-pk-core">

[English](README.md) · [简体中文](README.zh-CN.md) · **繁體中文**

[![build](https://github.com/imisakamikoto/hrt-pk-core/actions/workflows/build.yml/badge.svg)](https://github.com/imisakamikoto/hrt-pk-core/actions/workflows/build.yml)
[![MIT](https://img.shields.io/badge/license-MIT-blue.svg)](LICENSE)
[![ABI](https://img.shields.io/badge/ABI-2-informational)](docs/API.md)
[![platforms](https://img.shields.io/badge/Android%20·%20iOS%20·%20Windows%20·%20macOS%20·%20Linux-lightgrey)](#install)

雌二醇與睪固酮 HRT 的離線藥代動力學引擎，打包成一個體積很小的原生函式庫，
透過 JSON-over-C 的 ABI 對外提供服務。

不連網、不讀時鐘、不碰檔案系統、沒有全域狀態。你把一段用藥歷史交給它，它把
曲線、統計、方案或警告以 JSON 交還。同一份程式碼執行在 Android、iOS、Windows、
macOS 與 Linux 上，匯出的是純 C 符號，因此任何支援 FFI 的語言都能呼叫。

```
┌─────────────┐   JSON    ┌──────────────────────────────────────────┐
│ 你的應用程式 │ ────────▶ │ hrt_core                                 │
│ Dart / Py   │           │  model → engine → calibration            │
│ C# / C / …  │ ◀──────── │  stats · planning · reminders · safety   │
└─────────────┘   JSON    └──────────────────────────────────────────┘
```

- **ABI 2**，函式庫版本 `2.0.0`，匯出 19 個函式。
- 依平台不同 1.5 MB–21 MB；除 libc 外沒有執行期相依。
- 確定性：同樣的輸入在任何平台、任何一次執行都得到同樣的輸出。
- panic 在邊界被攔截，以錯誤物件回傳，不會讓呼叫端行程崩潰。

> 這是一個模型，不是醫療建議。它只根據你告訴它的內容估算濃度，無法知道任何
> 沒有被記錄的事情。

---

## 它能回答什麼

| 問題 | 函式 |
|---|---|
| 我現在的濃度是多少？下週二呢？ | `hrt_simulate` |
| 我的峰值、谷值、平均值、在範圍內的時間是多少？ | `hrt_stats` |
| 檢驗結果和模型對不上，差多少？ | `hrt_calibrate` |
| 如果每 7 天注射一次，最後會穩定在哪裡？ | `hrt_steady_state` |
| 什麼劑量能把谷值維持在 120 pg/mL？ | `hrt_plan_dose` |
| 應用程式該在什麼時候提醒我用藥？ | `hrt_reminders` |
| 我實際上有沒有照計畫用藥？ | `hrt_adherence` |
| 這瓶藥還能用多久？ | `hrt_supply` |
| 150 pg/mL 等於多少 pmol/L？ | `hrt_convert` |
| 有哪些酯類、給藥途徑與單位？ | `hrt_catalog` |
| 舌下含服要含多久？ | `hrt_sublingual` |
| 這次撕下來的貼片是貼的哪一張？ | `hrt_patches` |
| 這份紀錄裡有沒有模型會忽略的東西？ | `hrt_safety` |
| 結論對哪一個估計值最敏感？ | `hrt_sensitivity` |
| 以上全部，但只跨一次 FFI 邊界。 | `hrt_batch` |

## 快速開始

### C

```c
#include "hrt_core.h"

char *out = hrt_simulate(
    "{\"events\":[{\"id\":\"a\",\"route\":\"injection\",\"timeH\":0,"
    "\"doseMG\":4,\"ester\":\"EV\"}],\"bodyWeightKg\":70,\"nowH\":72}");
if (out) {
    puts(out);          /* {"ok":true,"result":{"timeH":[…],…}} */
    hrt_free(out);      /* 每個回傳的字串都必須釋放 */
}
```

### Python

```python
from hrt_core import HrtCore

core = HrtCore()                     # 或 HrtCore("/path/to/libhrt_core.so")
curve = core.simulate(
    events=[{"id": "a", "route": "injection", "timeH": 0, "doseMG": 4, "ester": "EV"}],
    bodyWeightKg=70,
    nowH=72,
)
print(max(curve["e2PgMl"]), "pg/mL")
```

### Flutter / Dart

```dart
final core = HrtCore.open();
final curve = core.simulate({
  'events': [
    {'id': 'a', 'route': 'injection', 'timeH': 0, 'doseMG': 4, 'ester': 'EV'},
  ],
  'bodyWeightKg': 70,
  'nowH': 72,
});
```

### C#

```csharp
var core = new HrtCore();
var curve = core.Simulate(new {
    events = new[] { new { id = "a", route = "injection", timeH = 0.0, doseMG = 4.0, ester = "EV" } },
    bodyWeightKg = 70.0,
    nowH = 72.0,
});
```

### Rust

```toml
[dependencies]
hrt-pk-core = { path = "…" }   # 或 git = "https://github.com/imisakamikoto/hrt-pk-core"
```

```rust
use hrt_core::engine::run_simulation;
use hrt_core::model::{DoseEvent, Ester, PkParams, Route};

let event = DoseEvent {
    id: "a".into(), route: Route::Injection, time_h: 0.0,
    dose_mg: 4.0, ester: Ester::EV, extras: Default::default(), updated_at: None,
};
let sim = run_simulation(&[event], 70.0, None, &PkParams::default(), 72.0, 1.0).unwrap();
```

更多內容：[`docs/API.md`](docs/API.md) 是每個欄位的完整說明，
[`docs/INTEGRATION.md`](docs/INTEGRATION.md) 是各平台的整合方式，
[`docs/MODEL.md`](docs/MODEL.md) 是數學部分，
[`docs/DEVELOPING.md`](docs/DEVELOPING.md) 是建置與發佈流程。

## 安裝

### 預編譯二進位檔

每個打了標籤的發佈都附帶：

| 檔案 | 平台 |
|---|---|
| `libhrt_core.so`（arm64-v8a、armeabi-v7a、x86_64） | Android 7+ |
| `libhrt_core.a`（實機 arm64、模擬器 universal） | iOS 15+ |
| `hrt_core.dll` + 匯入函式庫 | Windows 10 1809+ x64 |
| `libhrt_core.so` | Linux x86_64 |
| `libhrt_core.dylib`（universal） | macOS 11+ |
| `hrt_core.h` | 所有平台 |

### 從原始碼建置

```bash
cargo build --release              # 宿主平台函式庫，輸出到 target/release/
cargo test                         # 32 個單元測試 + 1 個文件測試
bash scripts/build-all.sh          # 所有交叉編譯目標（需裝好對應工具鏈）
```

## 設計

**邊界上是 JSON，內部是型別。** 若用結構體做 C ABI，就要為每個平台維護一份
結構體版本，並在每種語言裡手寫一遍 marshal。現在是一個 JSON 物件進、一個 JSON
物件出，同樣的六行綁定程式碼在任何語言裡都能用。代價是解析——與動輒取樣幾千個
點的模擬相比，只需微秒級。

**解析解，而不是數值積分。** 每次給藥都是一串一階過程（吸收 → 酯水解 → 消除），
直接求出閉式解；速率常數重合處以均差（divided differences）處理。兩個房室是
Bateman 表達式，三個房室是 `e^(−sτ)` 的二階均差。沒有步長可調、沒有漂移、
十年病史也不會累積誤差。

**曲線是唯一事實來源。** 峰值、谷值、AUC、在範圍內時間、提醒時刻全部由畫圖所
用的同一批取樣點導出，所以摘要不可能和旁邊的曲線對不上。

**沒有隱藏行為。** 模型不會使用的劑量由 `hrt_safety` 回報，而不是悄悄以 0
計入；某個途徑忽略紀錄劑量的情況會明講；一個裝不進目標範圍的方案會回傳衝突，
而不是給出一個看起來合理的劑量。

## 模型概要

- 雌二醇與睪固酮共用一個結構：`k1` 吸收，可選的 `k2` 酯水解，`k3` 消除；雙相
  釋放的酯類再分快、慢兩個成分。
- 分布體積：2 L/kg（E2）、1 L/kg（T）、14 L/kg（CPA）。
- 途徑：注射、口服、舌下（含服時間決定黏膜吸收比例）、凝膠（部位決定生體可用
  率）、貼片（一階釋放，或依標示釋放速率的零階輸入，與撕除事件配對）。
- 清除率隨年齡相對於 54 歲變化，並可由校準得到的個人係數縮放。
- 校準：`mipd`（清除率網格 + 閉式振幅）、`ekf`（對數殘差的純量卡爾曼）、
  `ou_kalman`（隨時間衰減的加權），或 `off`。

完整常數與適用範圍見 [`docs/MODEL.md`](docs/MODEL.md)。

## 相容性

JSON 欄位名稱與 [Oyama's HRT Tracker](https://github.com/xunxunProjects/Oyama-s-HRT-Tracker)
的儲存與備份格式一致，包括大寫字母的 `doseMG`，因此備份可以直接回放，不需要
轉換層。當某個欄位的意義改變時，`ABI_VERSION` 會加一。

## 相關專案

- **[Misaka hrt记录器](https://github.com/imisakamikoto/misaka-hrt-tracker)** —
  這個函式庫被抽出來的 Android / iOS / Windows 應用程式。它內嵌核心，資料只留在
  裝置上，JSON 的形狀也來自它。
- **[Oyama's HRT Tracker](https://github.com/xunxunProjects/Oyama-s-HRT-Tracker)** —
  模型移植自的瀏覽器應用程式。

## 參與貢獻

函式庫很小，職責很窄。[CONTRIBUTING.md](CONTRIBUTING.md) 說明了什麼在範圍內，以及
程式碼遵守的五條規則 —— 不讀時鐘、不做 I/O、panic 不跨越邊界、數學保持解析式、
摘要永不與曲線自相矛盾。

- **提問與想法** → [Discussions](https://github.com/imisakamikoto/hrt-pk-core/discussions)，兩個管道怎麼選見 [SUPPORT.md](SUPPORT.md)。
- **Bug** → [issues](https://github.com/imisakamikoto/hrt-pk-core/issues/new?template=bug_report.yml)，最好附上能重現的請求 JSON。
- **安全性問題** → 私密回報，見 [SECURITY.md](SECURITY.md)，不要開公開 issue。
- **行為準則** → [CODE_OF_CONDUCT.md](CODE_OF_CONDUCT.md)。

如果這個函式庫出現在你的論文或專案裡，[CITATION.cff](CITATION.cff) 裡有可直接引用的條目。

## 授權

MIT。藥代動力學模型移植自 Joseph Smirnova Oyama 的 Oyama's HRT Tracker；完整
署名與變更清單見 [`NOTICE.md`](NOTICE.md)。
