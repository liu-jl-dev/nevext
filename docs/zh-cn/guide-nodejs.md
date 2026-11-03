# Nevext JavaScript 使用指南

本指南介绍 JavaScript 实现（Node.js）的集成方式与 API 用法。关于语言本身的语法规则，
请先阅读[语法概述](./overview.md)；行为与 C++ 参考实现逐条一致，并由 55 条跨实现
通用语料与 dump 门禁守护（见[跨实现通用测试计划](./test-plan.md)）。

## 集成

零依赖单文件，把 `nodejs/nevext/nevext.js` 拷进工程即可（ESM）：

```js
import { Lexer, Parser, TokenType, Result } from './nevext/nevext.js';

const parser = new Parser(new Lexer('windowSize 1280, 720;'));
const res = parser.parseProgram();
if (res.err === Result.Error) {
  console.error(parser.errors);
}
for (const stmt of res.val) {
  console.log(`line ${stmt.line}:`, stmt.strings);
}
// line 1: [ 'windowSize', '1280', '720' ]
```

要点：

- **Node.js 任意近期版本**；零第三方依赖，`package.json` 可有可无
  （`nodejs/` 自带一个用于 `npm test`，已声明 `"type": "module"`）；
- JS 字符串就是 UTF-16 码元序列，中文、emoji 直接可用，无需转码；
- 工程是 ESM（`package.json` 的 `"type": "module"`），拷入 CommonJS 工程时
  用 `createRequire` 引入，或改名为 `.mjs`；
- `Result.Success === 0`，可直接写 `if (res.err)`。

## API 参考

版本常量 `VERSION`（具名导出）与 [更新日志](../../CHANGELOG.md)、git tag 同步更新。

### 枚举与产物类型

```js
TokenType // { Unexpected: 0, EndOfFile: 1, RawStr: 2, QuotedStr: 3, Sep: 4 }
Result    // { Success: 0, Error: 1 }

Token     // { type, line, literal }   literal 为码元原文（QuotedStr 含转义序列原文）
Statement // { line, strings: string[] }
ParseResult // { err, val: Statement[] }
```

### Lexer（`nextToken` / `reset`）

```js
const lexer = new Lexer(input);  // 输入为 JS 字符串，值拷贝，无生命周期约束
const token = lexer.nextToken();
lexer.reset(otherInput);         // 复用对象：行号、游标与 errors 全部归零
```

一般不需要直接使用 `Lexer`，交给 `Parser` 驱动即可。

### Parser（`parseProgram` / `reset`）

```js
const parser = new Parser(lexer);
const res = parser.parseProgram(pauseWhenError = true);
parser.reset(lexer2);            // 重绑词法器并清空 errors，复用对象
```

**`parseProgram` 参数**

| 参数 | 说明 |
| --- | --- |
| `pauseWhenError` | `true`（默认）：遇到第一个错误立即停止解析；`false`：继续解析，尽量多收集错误 |

无论哪种模式，出错前已经解析出的语句都会保留在 `res.val` 中。

### 与 C++ 实现的 API 对应

| C++ | JavaScript |
| --- | --- |
| `Lexer::NextToken()` | `lexer.nextToken()` |
| `Lexer::Reset(input)` | `lexer.reset(input)` |
| `Parser::ParseProgram(prog, p)`（出参） | `parser.parseProgram(p)`（返回值） |
| `Parser::Reset(lexer)` | `parser.reset(lexer)` |

## 错误处理

与 C++ 完全同构：错误分两个容器，消息文本逐字一致（含句号后的空格）。

| 容器 | 内容 | 典型消息 |
| --- | --- | --- |
| `lexer.errors` | 词法阶段错误 | `line 1: unclosed string literal.` |
| `parser.errors` | 语法 / 转义阶段错误 | `line 1: unexpected token.`<br>`line 1: invalid escape character.` |

转义只在语法层对引号串展开，失败时保留失败点之前已展开的部分；
同一行的多处错误各记一条，不做去重。复用 `Parser` 前请调用 `parser.reset(...)`，
只有 `reset` 会清空错误容器。

## 运行示例

[`nodejs/demo/demo01.js`](../../nodejs/demo/demo01.js) 是可直接运行的完整示例，
与 C++ / GDScript / JavaScript / C# / Python 五份 demo 使用同一段输入与同一输出格式：

```bash
node nodejs/demo/demo01.js                # 内置示例
node nodejs/demo/demo01.js 某个文件.txt    # 解析该文件（UTF-8）
nodejs\demo.cmd [文件]                    # Windows 便捷入口
```

退出码：`0` 解析成功；`1` 有词法 / 语法错误（分别打印 `lexer.errors` 与 `parser.errors`
两个容器）；`2` 文件打不开。五份 demo 输出的逐字节比对见
[跨实现通用测试计划](./test-plan.md) 第 5.1 节。

## 已知限制

- **面向 Node.js**：实现本体是纯 JS（无 Node 专有 API），嵌入浏览器等环境理论上可行
  （ESM 原生可用），但未经本仓库测试守护；
- 性能介于 C++ 与 GDScript 之间（游标实现，整体 O(n)），大输入请自行评估。
