cmake_minimum_required(VERSION 3.10)

project(Nevext)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(MSVC)
    add_compile_options(/utf-8)
endif()

# 演示程序
add_executable(demo01
    demo/demo01.cpp
)

# ---------------------------------------------------------------------------
# 单元测试
#
#   cmake -B build && cmake --build build
#   cd build && ctest -C Debug --output-on-failure
#
# 也可直接运行可执行文件并按用例名筛选：
#   nevext_test Lexer          # 运行名字含 Lexer 的用例
#   nevext_test -v             # 详细输出
#   nevext_test --dump         # 打印跨实现通用语料的实际结果，供与 GDScript 对拍
#
# 测试零第三方依赖，与 nevext 核心库（header-only）保持同样的可移植性。
# 通用语料（跨 C++ / GDScript 两套实现）见 docs/zh-cn/test-plan.md，
# C++ 私有用例见 cpp/docs/test-plan.md。
# ---------------------------------------------------------------------------
option(NEVEXT_BUILD_TESTS "构建 nevext 单元测试" ON)

if(NEVEXT_BUILD_TESTS)
    enable_testing()

    add_executable(nevext_test
        test/test_main.cpp
        test/test_lexer.cpp
        test/test_parser.cpp
        test/test_error.cpp
        test/test_conformance.cpp
    )

    # 核心库与测试辅助均位于这些目录下，头文件由此可按相对或裸名引用
    target_include_directories(nevext_test PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/nevext
        ${CMAKE_CURRENT_SOURCE_DIR}/test
    )

    # MSVC 多配置生成器会把产物放到 build/<Config>/ 下；
    # 统一输出到 build/test/ 便于命令行直接调用，ctest 则不受影响。
    set_target_properties(nevext_test PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/test
    )

    if(MSVC)
        target_compile_options(nevext_test PRIVATE /W3)
    else()
        target_compile_options(nevext_test PRIVATE -Wall -Wextra)
    endif()

    add_test(NAME nevext_test COMMAND nevext_test)
endif()
