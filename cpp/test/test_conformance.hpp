/*******************************************************
MIT License

Copyright (c) 2026 nevext contributors

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY AND
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
 *******************************************************/

#pragma once

// ---------------------------------------------------------------------------
// test_conformance.hpp —— 跨实现通用语料（用例定义 + 断言驱动 + dump）
//
// 语料本体定义在 docs/zh-cn/test-plan.md 第 3 节，两侧实现必须用同一份 ID 与
// 同一份期望值。每条用例在 C++ 侧注册为 `Conformance.<ID>`，便于按 ID 前缀筛选：
//
//   ./build/test/nevext_test Conformance.C0      # 只跑转义相关的用例
//   ./build/test/nevext_test --dump              # 按统一行格式打印全部语料
//
// dump 的行格式详见 test-plan.md 第 5 节，GDScript 侧须逐字节一致。
// ---------------------------------------------------------------------------

#include <string>
#include <vector>

#include "test_framework.hpp"
#include "test_util.hpp"  // 复用其中的 Outcome / Run / ExpectStrings 与 StmtExpect

namespace nevext_test {

// 期望的一条错误：只断言位置与性质，不依赖消息措辞（两实现的措辞可能不同）
struct ErrExpect {
  int line = 0;
  const char* kind = "";  // UnclosedString / InvalidEscape / UnexpectedToken
};

struct ConformanceCase {
  const char* id = "";
  std::u16string src;
  bool pause = true;
  bool ok = true;  // true 表示期望返回 Success
  std::vector<StmtExpect> prog;
  std::vector<ErrExpect> errs;
};

// 全部通用语料（函数内静态构造，避免静态初始化顺序问题）
const std::vector<ConformanceCase>& ConformanceCases();

// 运行语料中的某一条并断言（由各 Conformance.<ID> 用例调用）
void CheckConformanceCase(Context& ctx, const char* id);

// dump 模式：按统一行格式打印全部语料的实际结果
void DumpConformance();

// 错误消息 -> 语义种类 / 行号。改消息措辞不需要改用例。
std::string ErrorKind(const std::string& msg);
int ErrorLine(const std::string& msg);

}  // namespace nevext_test
