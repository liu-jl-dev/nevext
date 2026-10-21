# Nevext C++ 实现开发说明

> 适用范围：要改 C++ 侧代码的人（库本体 `cpp/nevext/`、测试 `cpp/test/`、示例 `cpp/demo/`）。
>
> 相关文档：
> - 语法规范：[docs/zh-cn/overview.md](../../docs/zh-cn/overview.md)
> - 使用方式（集成方看这篇）：[docs/zh-cn/guide-cpp.md](../../docs/zh-cn/guide-cpp.md)
> - C++ 私有测试套件：[cpp/docs/test-plan.md](./test-plan.md)
> - 跨实现通用语料：[docs/zh-cn/test-plan.md](../../docs/zh-cn/test-plan.md)
>
> 最后更新：2026-10-21（新增第 1 节「代码格式化」）

**本文档的层级**：仓库里 `docs/zh-cn/` 放跨实现 / 使用方视角的文档，`<实现>/docs/`
放该实现维护者自己的文档。格式化目前只对 C++ 起作用（也只有 C++ 有配置文件），
所以留在 `cpp/docs/`。将来 GDScript 侧要写自己的开发约定，对称放在
`gdscript/docs/`，不要塞进本文档。

## 1. 代码格式化

### 1.1 配置放在哪

| 文件 | 作用 |
| --- | --- |
| `cpp/.clang-format` | 风格本体：Chromium 基底 + 本项目的显式覆盖 |
| `.vscode/settings.json` | `C_Cpp.clang_format_style: "file"` 指向上面那个文件；cpp / c 保存时格式化 |
| `.vscode/extensions.json` | 推荐 `ms-vscode.cpptools` 与 `ms-vscode.cmake-tools` |

几个要点：

- **风格写进仓库，不写进本机用户设置。** 原先 `"C_Cpp.clang_format_style": "Chromium"`
  写在用户级 `settings.json`，那样只在自己一台机器上生效，别人克隆仓库拿到的是另一套
  格式，而且会连带影响其它工程。现已迁出，**请勿改回**。
- **配置文件放在 `cpp/` 而不是仓库根**：clang-format 从被格式化的文件所在目录逐级向上找
  `.clang-format`，放在 `cpp/` 才能保证「把整个仓库当工作区打开」和「单独把 `cpp/` 当
  工作区打开」两种情况都能命中。
- `.vscode/settings.json` 还设置了 `files.eol: "\n"` 与 `files.insertFinalNewline`。
  这不是洁癖：仓库有 `.gitattributes`（`* text=auto`），索引里一律 LF，而 Windows 上
  VSCode 默认存 CRLF，之前出过「文件看起来被整篇重写」的事故，这两条是把口子堵住。

### 1.2 怎么用

- **VSCode（常用）**：打开 `cpp/` 下任意 `.hpp` / `.cpp`，保存即格式化；手动触发是
  `Shift+Alt+F`。没装扩展会提示安装 cpptools。
- **命令行**：

  ```bash
  clang-format -i -style=file cpp/nevext/*.hpp
  ```

- **没单独装 clang-format 时**，可以用 VSCode cpptools 扩展自带的那个：

  ```bash
  ~/.vscode/extensions/ms-vscode.cpptools-*/LLVM/bin/clang-format.exe
  ```

- **只检查不改文件**：用 `-output-replacements-xml` 或与原文 diff：

  ```bash
  clang-format -style=file path/to/file.hpp | diff -u --strip-trailing-cr path/to/file.hpp -
  ```

### 1.3 关键配置为什么这么写

| 配置 | 值 | 理由 |
| --- | --- | --- |
| `BasedOnStyle` | `Chromium` | 起点，其余用显式值覆盖 |
| `ColumnLimit` | `80` | 现有代码就是按 80 折的，实测核心库 0 行差异；放宽到 100 / 120 反而差异更多（会把已折好的行再合并回去） |
| `IndentWidth` / `TabWidth` / `UseTab` | `2` / `2` / `Never` | 现有代码两空格缩进，不用 tab |
| `NamespaceIndentation` | `None` | 命名空间 `Nevext` 不缩进，与现有代码一致 |
| `AccessModifierOffset` | `-1` | 沿用 Chromium |
| `DerivePointerAlignment` | `false` | 不让工具按文件内容猜，避免同一份代码得出不同写法 |
| `PointerAlignment` | `Left` | `char16_t* p`，贴在类型一侧 |
| `SortIncludes` | `false` | header-only 库，include 顺序是按依赖手工排的（先底层再上层），不能被重排 |
| `IncludeBlocks` | `Preserve` | 保留手工分组的空行 |
| `Standard` | `c++20` | 见下一节 |

一句话概括这套配置的来历：**它是照着 `cpp/nevext/*.hpp` 现有风格反过来提炼的，
不是拿某个现成风格去套。** 判据就是按它格式化核心库得到 0 行差异。

### 1.4 版本坑：`Standard` 的取值写法

`Standard` 的写法随 clang-format 版本变化：

- 新版认小写字符串：`c++20`
- 旧版认枚举写法：`Cpp20`
- 写错会报 `.clang-format:<行>:<列>: error: unknown enumerated scalar`

不确定时先探一下：

```bash
clang-format -style=file -dump-config | grep Standard
```

能正常输出就说明写法被当前版本接受。

### 1.5 唯一的例外：语料表不许排版

`cpp/test/test_conformance.cpp` 第 30~127 行用 `// clang-format off` 与
`// clang-format on` 圈住。那是 55 条跨实现语料对齐成的表格，需要和
`gdscript/test/test_conformance.gd` 逐条对照，一旦被折行，对照关系就失效了。
**改这张表时不要删掉这对标记。**

### 1.6 当前状态与收敛策略

| 目录 | 按新配置重排的差异行数 | 处理方式 |
| --- | --- | --- |
| `cpp/nevext/*.hpp` | 0 | 无需处理 |
| `cpp/demo/` | 7 | 暂不动 |
| `cpp/test/` | 约 324 | 暂不动 |

策略：**不做一次性批量重排。** 数百行的纯排版 diff 会把真正的逻辑改动淹没在噪声里，
也不利于 blame 追踪。谁编辑哪个文件，保存时自然收敛到 80 列即可。

## 2. 构建与测试

完整的cmake 入口与用例说明见 [cpp/docs/test-plan.md](./test-plan.md) 第 4 节，
这里只列最常用的两条：

```bash
# CMake + ctest（推荐）
cd cpp && cmake -B build && cmake --build build && ctest -C Debug --output-on-failure

# MinGW 直编译（不依赖 CMake，改一行看结果时最快）
# cpp/test 下的源文件一起编，详见 cpp/CMakeLists.txt 里的目标定义
```

**改动 `cpp/nevext/` 之前先跑 C++ 侧的 Conformance 套件**（55 条跨实现语料）：

```bash
./build/test/nevext_test Conformance
```

## 3. 目录职责

| 路径 | 职责 | 是否随库发布 |
| --- | --- | --- |
| `cpp/nevext/` | 库本体，header-only（`common` / `lexer` / `parser` / `nevext`） | 是，只需拷这个目录 |
| `cpp/test/` | 私有测试套件，零第三方依赖 | 否 |
| `cpp/demo/` | 可运行示例，含 UTF-8 / UTF-16 转换小工具 | 否 |
| `cpp/docs/` | 给维护者看的文档（本文档 + 测试计划） | 否 |
| `cpp/CMakeLists.txt` | 构建 demo01 与 nevext_test，并注册 ctest | 否 |

## 4. 代码约定（clang-format 管不到的部分）

- **语言与形态**：C++20，全部 header-only，命名空间统一 `Nevext`。
- **字符串**：内部统一 `std::u16string` / `std::u16string_view`（UTF-16），
  只在对外输出时转 `std::string`（UTF-8）。
- **命名**：类型 PascalCase（`Token` / `Lexer` / `Parser` / `Statement`），
  公开方法 PascalCase（`NextToken` / `ParseProgram`），私有助手 camelCase
  （`skipWs` / `scanQuotedStr` / `parseStatement` / `escape`），数据成员全小写
  （`line` / `input` / `errors` / `curToken`）。枚举项也是 PascalCase。
- **依赖**：核心库不得引入任何第三方依赖；**测试代码同样不得引入 GTest / Catch2**，
  原因见测试计划第 3 节——零依赖是这个库能被轻易移植、被 LLM 重新生成的前提。
- **注释与文档用中文**。
- **源文件编码与行尾**：UTF-8 无 BOM，行尾 LF。仓库靠 `.gitattributes`
  （`* text=auto`）保证索引里是 LF；Windows 上检出时由 `core.autocrlf=true`
  转成 CRLF，所以**不要把工作区的 CRLF 当作差异提交进去**。
- **MSVC**：CMake 已加 `/utf-8`；自己搭工程时必须手动加，否则含中文的 `u"..."`
  字面量会被按 ANSI 代码页错误解码。

## 5. 提交前检查清单

1. **测试通过**：`ctest -C Debug --output-on-failure` 全绿
   （基线：C++ 109 用例 / 769 断言）。
2. **两套实现仍然一致**（改过 `cpp/nevext/` 时必做）：

   ```bash
   diff --strip-trailing-cr \
        <(./cpp/build/test/nevext_test --dump) \
        <(godot --headless --path gdscript --script res://test/test_main.gd -- --dump)
   ```

   `--strip-trailing-cr` 不能省：C++ 侧 Windows 控制台输出是 CRLF，Godot 侧是 LF。
3. **行尾没混进去**：`git ls-files --eol` 应全部 `i/lf`。
4. **没有多余的格式化改动**：`git diff --stat` 里不应出现整批与本次意图无关的
   排版换行（开着保存时格式化时最容易带进来，值得看一眼）。
5. **文档同步**：改了行为要连带更新 `docs/zh-cn/` 下的相关文档；新增了文件要更新
   README 的目录/索引，避免留下悬空引用。
