# hrt-pk-core

<img src="assets/logo.png" width="88" align="right" alt="hrt-pk-core">

[English](README.md) · **简体中文** · [繁體中文](README.zh-TW.md)

[![build](https://github.com/imisakamikoto/hrt-pk-core/actions/workflows/build.yml/badge.svg)](https://github.com/imisakamikoto/hrt-pk-core/actions/workflows/build.yml)
[![MIT](https://img.shields.io/badge/license-MIT-blue.svg)](LICENSE)
[![ABI](https://img.shields.io/badge/ABI-2-informational)](docs/API.md)
[![platforms](https://img.shields.io/badge/Android%20·%20iOS%20·%20Windows%20·%20macOS%20·%20Linux-lightgrey)](#install)

雌二醇与睾酮 HRT 的离线药代动力学引擎，打包成一个体积很小的原生库，通过  
JSON-over-C 的 ABI 对外提供服务。

不联网、不读时钟、不碰文件系统、没有全局状态。你把一段用药历史交给它，它把  
曲线、统计、方案或警告以 JSON 交还。同一份代码运行在 Android、iOS、Windows、  
macOS 和 Linux 上，导出的是纯 C 符号，因此任何支持 FFI 的语言都能调用。

```
┌─────────────┐   JSON    ┌──────────────────────────────────────────┐
│ 你的应用     │ ────────▶ │ hrt_core                                 │
│ Dart / Py   │           │  model → engine → calibration            │
│ C# / C / …  │ ◀──────── │  stats · planning · reminders · safety   │
└─────────────┘   JSON    └──────────────────────────────────────────┘
```

- **ABI 2**，库版本 `2.2.0`，导出 25 个函数。
- 依平台不同 1.5 MB–21 MB；除 libc 外没有运行时依赖。
- 确定性：同样的输入在任意平台、任意一次运行都得到同样的输出。
- panic 在边界处被捕获，作为错误对象返回，不会带崩调用方进程。

> 这是一个模型，不是医疗建议。它只根据你告诉它的内容估算浓度，无法知道任何  
> 没有被记录的事情。



---

## 它能回答什么

| 问题                     | 函数                 |
| ---------------------- | ------------------ |
| 我现在的浓度是多少？下周二呢？        | `hrt_simulate`     |
| 我的峰值、谷值、均值、在范围内的时间是多少？ | `hrt_stats`        |
| 检验结果和模型对不上，差多少？        | `hrt_calibrate`    |
| 如果每 7 天注射一次，最后会稳定在哪里？  | `hrt_steady_state` |
| 什么剂量能把谷值维持在 120 pg/mL？ | `hrt_plan_dose`    |
| 应用该在什么时候提醒我吃药？         | `hrt_reminders`    |
| 我实际上有没有按计划用药？          | `hrt_adherence`    |
| 这瓶药还能用多久？              | `hrt_supply`       |
| 150 pg/mL 等于多少 pmol/L？ | `hrt_convert`      |
| 有哪些酯类、给药途径和单位？         | `hrt_catalog`      |
| 舌下含服要含多久？              | `hrt_sublingual`   |
| 这次撕下来的贴片是贴的哪一张？        | `hrt_patches`      |
| 这份记录里有没有模型会忽略的东西？      | `hrt_safety`       |
| 结论对哪一个估计值最敏感？          | `hrt_sensitivity`  |
| 螺内酯的剂量够不够，能阻断多少（测不出来）？ | `hrt_blockade` |
| 一年下来，最保守的抑制率和有效率是多少？ | `hrt_progress` |
| 每周注射还是每日凝胶——哪个更平稳，代价是多少？ | `hrt_compare` |
| 如果周四那次漏了会怎样？ | `hrt_what_if` |
| 化验在范围内吗、在上升吗、该复查了吗？ | `hrt_lab_review` |
| 上胸围 91、下胸围 76 是什么罩杯？ | `hrt_body` |
| 以上全部，但只跨一次 FFI 边界。 | `hrt_batch` |

## 文档

| | |
|---|---|
| [API 参考](docs/API.md) | 每个函数、每个字段、每个错误码 |
| [模型说明](docs/MODEL.md) | 公式、常数，以及**没有**建模的部分 |
| [集成指南](docs/INTEGRATION.md) | 各平台的打包、加载、内存与线程 |
| [开发指南](docs/DEVELOPING.md) | 构建、测试、新增函数、发布 |
| [变更记录](CHANGELOG.md) | ABI 版本之间改了什么 |

## 快速开始

### C

```c
#include "hrt_core.h"

char *out = hrt_simulate(
    "{\"events\":[{\"id\":\"a\",\"route\":\"injection\",\"timeH\":0,"
    "\"doseMG\":4,\"ester\":\"EV\"}],\"bodyWeightKg\":70,\"nowH\":72}");
if (out) {
    puts(out);          /* {"ok":true,"result":{"timeH":[…],…}} */
    hrt_free(out);      /* 每个返回的字符串都必须释放 */
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

更多内容：[`docs/API.md`](docs/API.md) 是每个字段的完整说明，  
[`docs/INTEGRATION.md`](docs/INTEGRATION.md) 是各平台的集成方式，  
[`docs/MODEL.md`](docs/MODEL.md) 是数学部分，  
[`docs/DEVELOPING.md`](docs/DEVELOPING.md) 是构建与发布流程。

## 安装

### 预编译二进制

每个打了标签的发布都附带：

| 文件                                             | 平台                   |
| ---------------------------------------------- | -------------------- |
| `libhrt_core.so`（arm64-v8a、armeabi-v7a、x86_64） | Android 7+           |
| `libhrt_core.a`（真机 arm64、模拟器 universal）        | iOS 15+              |
| `hrt_core.dll` + 导入库                           | Windows 10 1809+ x64 |
| `libhrt_core.so`                               | Linux x86_64         |
| `libhrt_core.dylib`（universal）                 | macOS 11+            |
| `hrt_core.h`                                   | 所有平台                 |

### 从源码构建

```bash
cargo build --release              # 宿主平台库，输出到 target/release/
cargo test                         # 32 个单元测试 + 1 个文档测试
bash scripts/build-all.sh          # 所有交叉编译目标（需装好对应工具链）
```

## 设计

**边界上是 JSON，内部是类型。** 如果用结构体做 C ABI，就要为每个平台维护一个  
结构体版本，并在每种语言里手写一遍 marshal。现在是一个 JSON 对象进、一个 JSON  
对象出，同样的六行绑定代码在任何语言里都能用。代价是解析——与动辄采样几千个点  
的模拟相比，只需微秒级。

**解析解，而不是数值积分。** 每次给药都是一串一阶过程（吸收 → 酯水解 → 消除），  
直接求出闭式解；速率常数重合处用均差（divided differences）处理。两个房室是  
Bateman 表达式，三个房室是 `e^(−sτ)` 的二阶均差。没有步长可调、没有漂移、  
十年病史也不会累积误差。

**曲线是唯一事实来源。** 峰值、谷值、AUC、在范围内时间、提醒时刻全部由画图所  
用的同一批采样点导出，所以摘要不可能和旁边的曲线对不上。

**没有隐藏行为。** 模型不会使用的剂量由 `hrt_safety` 报告，而不是悄悄按 0  
计入；某个途径忽略记录剂量的情况会明说；一个装不进目标范围的方案会返回冲突，  
而不是给出一个看起来合理的剂量。

## 模型概要

- 雌二醇与睾酮共用一个结构：`k1` 吸收，可选的 `k2` 酯水解，`k3` 消除；双相  
  释放的酯类再分快、慢两个组分。
- 分布容积：2 L/kg（E2）、1 L/kg（T）、14 L/kg（CPA）。
- 途径：注射、口服、舌下（含服时间决定黏膜吸收比例）、凝胶（部位决定生物利用  
  度）、贴片（一阶释放，或按标示释放速率的零阶输入，与撕除事件配对）。
- 清除率随年龄相对 54 岁变化，并可由校准得到的个人系数缩放。
- 校准：`mipd`（清除率网格 + 闭式幅度）、`ekf`（对数残差的标量卡尔曼）、  
  `ou_kalman`（随时间衰减的加权），或 `off`。

完整常数与适用范围见 [`docs/MODEL.md`](docs/MODEL.md)。

## 兼容性

JSON 字段名与 [Oyama's HRT Tracker](https://github.com/xunxunProjects/Oyama-s-HRT-Tracker)  
的存储与备份格式一致，包括大写字母的 `doseMG`，因此备份可以直接回放，不需要  
转换层。当某个字段的含义发生变化时，`ABI_VERSION` 会加一。

## 相关项目

- **[Misaka hrt记录器](https://github.com/imisakamikoto/misaka-hrt-tracker)** —
  这个库被抽出来的 Android / iOS / Windows 应用。它内嵌内核，数据只留在设备上，
  JSON 的形状也来自它。
- **[Oyama's HRT Tracker](https://github.com/xunxunProjects/Oyama-s-HRT-Tracker)** —
  模型移植自的浏览器应用。

## 参与贡献

库很小，职责很窄。[CONTRIBUTING.md](CONTRIBUTING.md) 说明了什么在范围内，以及
代码遵守的五条规则 —— 不读时钟、不做 I/O、panic 不跨越边界、数学保持解析式、
摘要永不与曲线自相矛盾。

- **提问与想法** → [Discussions](https://github.com/imisakamikoto/hrt-pk-core/discussions)，两个渠道怎么选见 [SUPPORT.md](SUPPORT.md)。
- **Bug** → [issues](https://github.com/imisakamikoto/hrt-pk-core/issues/new?template=bug_report.yml)，最好附上能复现的请求 JSON。
- **安全问题** → 私密报告，见 [SECURITY.md](SECURITY.md)，不要开公开 issue。
- **行为准则** → [CODE_OF_CONDUCT.md](CODE_OF_CONDUCT.md)。

如果这个库出现在你的论文或项目里，[CITATION.cff](CITATION.cff) 里有可直接引用的条目。

## 许可

MIT。药代动力学模型移植自 Joseph Smirnova Oyama 的 Oyama's HRT Tracker；完整  
署名与改动清单见 [`NOTICE.md`](NOTICE.md)。
