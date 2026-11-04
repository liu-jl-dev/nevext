#!/usr/bin/env python3
# ---------------------------------------------------------------------------
# check_version.py —— 校验版本号在全部落点的一致性。
#
# 整个仓库共用一个版本号，落在 9 个位置（见 VERSION_SPOTS）。手工改版本号时
# 容易漏改某一处，本脚本把它们全部 grep 出来断言相等：
#
#   python tools/check_version.py            # 全部一致则退出码 0，否则 1
#   python tools/check_version.py 0.2.0      # 与期望值比较（发布前预检用）
#
# 零第三方依赖，只用标准库。新增版本落点时，在 VERSION_SPOTS 里加一行即可。
# ---------------------------------------------------------------------------

import re
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent

# (相对路径, 正则)。正则需含一个捕获组，捕获版本号本身。
VERSION_SPOTS = [
    ("cpp/CMakeLists.txt", r"^project\(Nevext VERSION (\S+)"),
    ("cpp/nevext/nevext.hpp", r'kVersion = "([^"]+)"'),
    ("nodejs/package.json", r'"version": "([^"]+)"'),
    ("nodejs/nevext/nevext.js", r"^export const VERSION = '([^']+)'"),
    ("python/nevext/nevext.py", r'^__version__ = "([^"]+)"'),
    ("lua/nevext/nevext.lua", r'^\s*VERSION = "([^"]+)",'),
    ("csharp/nevext.csproj", r"<Version>([^<]+)</Version>"),
    ("csharp/nevext/Nevext.cs", r'Version = "([^"]+)"'),
    ("gdscript/nevext/nevext.gd", r'^const VERSION := "([^"]+)"'),
    # 只匹配正式版本小节（x.y.z）；「未发布」小节不参与一致性比较
    ("CHANGELOG.md", r"^## \[(\d+\.\d+\.\d+)\]"),
]


def main() -> int:
    expected = sys.argv[1] if len(sys.argv) > 1 else None
    versions = {}  # 落点描述 -> 版本号
    ok = True

    for rel, pattern in VERSION_SPOTS:
        text = (REPO / rel).read_text(encoding="utf-8")
        m = re.search(pattern, text, re.MULTILINE)
        if m:
            versions[rel] = m.group(1)
        else:
            print(f"缺失: {rel}（模式 {pattern} 未命中）")
            ok = False

    if not versions:
        return 1

    distinct = sorted(set(versions.values()))
    for rel, ver in versions.items():
        mark = "" if (len(distinct) == 1 and (expected is None or ver == expected)) else "  <-- 不一致"
        print(f"{ver}  {rel}{mark}")

    if len(distinct) != 1:
        print(f"失败: 存在 {len(distinct)} 个不同的版本号: {distinct}")
        return 1
    if expected is not None and distinct[0] != expected:
        print(f"失败: 版本号 {distinct[0]} != 期望 {expected}")
        return 1
    if not ok:
        return 1
    print(f"一致: 全部落点均为 {distinct[0]}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
