# ============================================================================
# LightGit — 轻量级 GitHub 文件发布客户端
#
# 构建系统：CMake + Visual Studio 2022 (MSVC) + Windows SDK
#
# 约束（详见《项目书.md》）：
#   - 核心逻辑纯 C，GUI 纯 Win32 API
#   - 不依赖 Git CLI / libgit2 / libcurl / 大型框架
#   - 不自动下载任何依赖（唯一第三方依赖 miniz 已本地置入 external/miniz）
#   - 最终产物为独立的 Windows x64 EXE（静态链接 CRT，仅依赖 Windows 系统 DLL）
# ============================================================================

cmake_minimum_required(VERSION 3.20)

project(LightGit
    VERSION 0.1.0
    DESCRIPTION "A simple, lightweight GitHub file publisher for Windows"
    LANGUAGES C)

# ----------------------------------------------------------------------------
# 全局编译选项
# ----------------------------------------------------------------------------
# 使用 MSVC 时静态链接 CRT（/MT、/MTd），避免运行时依赖 vcruntime*.dll，
# 使最终 EXE 真正“单文件、免安装”。
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

add_compile_definitions(
    WIN32_LEAN_AND_MEAN   # 精简 windows.h
    NOMINMAX              # 不让 windows.h 定义 min/max 宏
    UNICODE
    _UNICODE
)

if(MSVC)
    add_compile_options(
        /utf-8   # 源码按 UTF-8 解释（源码内含中文注释）
        /W4      # 高警告级别
        /MP      # 并行编译
        /Zi      # 生成调试信息
    )
    # 关闭“未使用参数/局部变量”等噪音警告，保留真实警告
    add_compile_options(/wd4100 /wd4189 /wd4201 /wd4127)
endif()

# ----------------------------------------------------------------------------
# 第三方依赖：miniz（zlib 兼容 DEFLATE 压缩，静态编入核心库）
# ----------------------------------------------------------------------------
set(MINIZ_SOURCES
    external/miniz/miniz.c
    external/miniz/miniz_tdef.c
    external/miniz/miniz_tinfl.c
)

# ----------------------------------------------------------------------------
# 核心库 lgcore：common + git 引擎 + miniz（GUI 与单元测试共享）
# ----------------------------------------------------------------------------
add_library(lgcore STATIC
    src/common/hex.c
    src/common/buffer.c
    src/common/json.c
    src/common/base64.c
    src/i18n.c
    src/git/git_hash.c
    src/git/git_object.c
    src/git/git_blob.c
    src/git/git_tree.c
    src/git/git_commit.c
    src/git/git_pack.c
    src/git/git_pktline.c
    src/git/git_transport.c
    src/net/http.c
    src/github/github_api.c
    src/github/github_auth.c
    src/github/github_repository.c
    src/file/gitignore.c
    src/file/file_scanner.c
    src/file/file_status.c
    src/storage/manifest.c
    src/storage/credential.c
    src/storage/config.c
    src/storage/pat_store.c
    src/github/github_tree.c
    src/app/publish_manager.c
    ${MINIZ_SOURCES}
)

target_include_directories(lgcore PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/external/miniz
)

# miniz 裁剪：关闭 ZIP 归档 API（packfile 只需要 zlib 风格压缩）
target_compile_definitions(lgcore PUBLIC
    MINIZ_NO_ARCHIVE_APIS
    MINIZ_NO_ARCHIVE_WRITING_APIS
)

# 核心库依赖：BCrypt（SHA-1）、WinHTTP（HTTPS）、crypt32（DPAPI）
target_link_libraries(lgcore PUBLIC bcrypt winhttp crypt32)

# ----------------------------------------------------------------------------
# 目标：LightGit.exe（Win32 GUI）
# ----------------------------------------------------------------------------
add_executable(LightGit WIN32
    src/main.c
    src/gui/window.c
    src/gui/account_page.c
    src/gui/repository_page.c
    src/gui/publish_page.c
    src/gui/settings_page.c
    src/app/app.c
)

target_include_directories(LightGit PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# GUI 依赖核心库 + Windows 系统库（均为 Windows 自带）
target_link_libraries(LightGit PRIVATE
    lgcore
    user32      # 窗口、控件、消息
    gdi32       # 绘制、字体
    comctl32    # 通用控件（ListView/ProgressBar）
    shell32     # Shell API（拖放、路径）
    advapi32    # DPAPI 等
    ole32       # COM（IFileOpenDialog）
    oleaut32    # COM 自动化
    comdlg32    # 通用对话框
    shlwapi     # 路径辅助
    winhttp     # HTTPS 网络
    crypt32     # 证书（DPAPI 依赖）
)

# ----------------------------------------------------------------------------
# 单元测试（控制台程序，无 GUI）
# ----------------------------------------------------------------------------
enable_testing()

add_executable(LightGitTests
    tests/test_runner.c
    tests/test_hash.c
    tests/test_blob.c
    tests/test_tree.c
    tests/test_commit.c
    tests/test_pack.c
    tests/test_json.c
    tests/test_gitignore.c
    tests/test_manifest.c
    tests/test_scanner.c
)

target_include_directories(LightGitTests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/tests
)

target_link_libraries(LightGitTests PRIVATE lgcore)

add_test(NAME LightGitTests COMMAND LightGitTests)

# ----------------------------------------------------------------------------
# 命令行推送工具（开发/集成测试用，非最终 GUI 产物）
# ----------------------------------------------------------------------------
add_executable(LightGitPush
    tools/lg_push.c
)

target_link_libraries(LightGitPush PRIVATE lgcore)

# ----------------------------------------------------------------------------
# GitHub API 验证工具（开发/集成测试用）
# ----------------------------------------------------------------------------
add_executable(LightGitGithub
    tools/lg_github.c
)

target_link_libraries(LightGitGithub PRIVATE lgcore)
