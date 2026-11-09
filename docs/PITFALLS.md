# LightGit 开发踩坑记录

> 记录本项目从零开发过程中踩过的坑与解决方案，供后续维护者/后来人参考。
> 时间：2026-11（首版开发）。

---

## 一、工具链 / 环境

### 1. VS2022 与 Windows SDK 装在非默认盘符
- 现象：`C:\Program Files\Microsoft Visual Studio` 下只有 `Installer`，找不到 cl.exe / SDK。
- 实际位置：
  - VS2022：`D:\Visual Studio 2022\Visual Studio 2022 IDE`
  - Windows SDK：`D:\Windows Kits\10`（10.0.22621.0）
  - CMake：`<VS>\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe`
- 经验：用 `vswhere.exe`（`C:\Program Files (x86)\Microsoft Visual Studio\Installer\vswhere.exe`）定位 VS；用注册表 `HKLM\SOFTWARE\Microsoft\Windows Kits\Installed Roots` 的 `KitsRoot10` 定位 SDK。

### 2. 无 Ninja，用 VS 生成器
- 本机没有 ninja.exe，CMake 用 `-G "Visual Studio 17 2022" -A x64`（走 MSBuild）最稳。

### 3. 在 git-bash 里调 cl/vcvars 的引号坑
- `cmd //c "call \"...vcvars64.bat\" && cl ..."` 的转义极易失败（`\"` 被原样传给 cmd）。
- 更稳的做法：写一个 `.bat`（注意中文路径要用 GBK 编码写 .bat），或用 CMake 统一构建，别在 bash 里手拼 cl 命令。

### 4. 中文路径（`D:\C语言程序库\...`）
- MSBuild/cl 能处理中文路径，但控制台输出会是 GBK 乱码，属正常现象，不影响编译。
- 写 `.bat` 文件时中文路径有编码风险，尽量避免。

---

## 二、第三方依赖（miniz）

### 5. miniz 的 git checkout 缺 `miniz_export.h`
- miniz 官方 README 明确说“不要直接用 git checkout，要用 release 的合并单文件”。
- 仓库里 `miniz.h` 无条件 `#include "miniz_export.h"`，但 git checkout 里没有这个文件 → 编译报错。
- 解决：自己补一个 `miniz_export.h`，静态链接时 `#define MINIZ_EXPORT` 为空即可。

### 6. miniz 只需要 3 个文件，ZIP API 要关掉
- 我们只需要 zlib 风格的 deflate/inflate，即：
  - `miniz.c`（zlib 风格 API）
  - `miniz_tdef.c`（压缩）
  - `miniz_tinfl.c`（解压）
- `miniz_zip.c`（ZIP 归档）会 `#include <sys/stat.h>`，MSVC 编不过，且我们用不到 → 不编译它。
- 编译时定义 `MINIZ_NO_ARCHIVE_APIS`、`MINIZ_NO_ARCHIVE_WRITING_APIS`。
- 注意：`miniz.h` 仍会 include `miniz_zip.h`，所以 `miniz_zip.h` 文件要保留（头文件无依赖，无害）。

### 7. Windows 没有原生 zlib
- `RtlCompressBuffer` 只支持 LZNT1/XPRESS/XPRESS_HUFF，MSZIP 只能解不能压、且不是 zlib 容器。
- 所以生成合法 Git packfile 必须引入 zlib 兼容实现（本项目选 miniz）。

---

## 三、Git 引擎

### 8. Git 对象类型枚举值必须与协议一致
- Git 线上对象类型码：`commit=1, tree=2, blob=3, tag=4`。
- 我最初定义成 `blob=0, tree=1, commit=2, tag=3`，导致 packfile 头里 type 写错，
  `git index-pack` 报 `unknown object type 0`。
- 教训：枚举直接用 Git 的类型码，别用自增顺序。

### 9. 悬垂指针（栈数组地址进结构体）——踩了两次
- 现象：tree 条目 `ent.sha1 = sha_bin`，而 `sha_bin` 是循环体内的**栈数组**，循环结束后失效；
  之后 `lg_tree_build` 再读 `entries[i].sha1` 时读到的是被复用/覆盖的栈内存 →
  所有条目 sha 变成同一个值，`git fsck` 报 `blob, not a tree`。
- 出现位置：`tools/lg_push.c` 的 LgBuildTree、`src/app/publish_manager.c` 的 LgBuildTreeLevel。
- 教训：任何“指针指向局部栈变量后又被保存到数组/结构体”的写法都要警惕，
  要么每条目 `malloc` 独立拷贝，要么结构体内联数组。

### 10. Commit 的 email 要带尖括号
- Git commit 对象里 email 是 `<name@example.com>`（带尖括号），
  而我的 `lg_commit_build` 最初直接拼 `author %s %s ...`（不带尖括号），
  导致 commit 内容/哈希与官方 git 不一致。
- 解决：构建时用 `author %s <%s> ...`，调用方 email 传“不带尖括号”的裸邮箱。

### 11. `**/` 匹配“零个或多个目录”
- gitignore 的 `**/*.tmp` 应同时匹配 `a.tmp`（零层）与 `sub/a.tmp`。
- 单纯把 `**` 当“匹配任意字符含 `/`”会让 `**/*.tmp` 匹配不到根目录文件。
- 解决：遇 `**/` 单独处理——“在当前位置或任意一层 `/` 之后尝试匹配”。
- 另一个坑：C 注释里写 `**/` 会把注释提前闭合（`*/` 是注释结束符），写注释要避开。

### 12. gitignore 锚定规则
- 以 `/` 开头、或中间含 `/` 的模式是“锚定到 .gitignore 所在目录”；
  不含 `/` 的模式匹配任意层级文件名。
- 我最初判断“锚定”时先把前导 `/` 剥掉再找 `/`，导致 `/root.txt` 被判成非锚定。
- 正确：`s..end` 内只要还剩任意 `/`（含前导）就是锚定。

### 13. Manifest 解析别把整行当字符串
- 最初 `lg_manifest_set(sp+1, line)` 直接把“行内指针”当 C 字符串传，
  但 `line` 和 `sp+1` 都没有 `\0` 结尾（行间是 `\n`），导致存进多余内容。
- 正确：截取 40 字符哈希、截取路径，各自补 `\0` 后再用。

### 14. Packfile 验证：别只信 `git index-pack`
- 本地 git 2.54.0.windows.1 的 `git index-pack --strict` 在索引完成后会**段错误**（git 自身 bug）。
- 更可靠的验证：`git unpack-objects`（解包成功=格式合法）+ `git fsck --strict`（对象合法）。
- 最终以“真实推送到 GitHub 被接受”为准（GitHub 服务端用自己的 index-pack 成功解包）。

### 15. 只发送“新对象”，复用远程已有对象
- 增量发布时：未变化的子树复用远程 tree sha、未变化文件复用远程 blob sha，
  packfile 里只放【新 blob + 新 tree + 新 commit】。这样服务端才能正确解析引用链。

---

## 四、GUI / 编码

### 16. `GetDlgItemTextA` 在 Unicode 控件上不可靠
- 用 `GetDlgItemTextA` 读 Unicode 输入框的 token 会导致 401（读到空/乱码）。
- 正确：`GetDlgItemTextW` 读宽字符，再 `WideCharToMultiByte(CP_UTF8)` 转 UTF-8。

### 17. 单行编辑框粘贴遇换行会截断
- token 从聊天里复制时折行，剪贴板带 `\n`，单行 EDIT 粘贴到换行处就截断（93 字符变 65），
  GitHub 返回 401 `Bad credentials`。
- 解决：token 框改成 `ES_MULTILINE`，且读入后**剔除所有空白字符**（token 本身不含空白）。

### 18. `%hs` 会把 UTF-8 当 GBK 显示 → 中文乱码
- 注释/中文存成 UTF-8 后，用 `swprintf(L"%hs", utf8)` 显示，`%hs` 按 ANSI(GBK) 解释 → 乱码。
- 正确：`MultiByteToWideChar(CP_UTF8, ...)` 显式转宽字符，再用 `%s`。
- 排查技巧：`xxd` 看文件字节即可判断“存错”还是“显示错”。

### 19. JSON 文件加 UTF-8 BOM
- 中文 Windows 记事本默认可能按 GBK 打开无 BOM 的 UTF-8 文件导致乱码。
- 写 JSON 时前置 `EF BB BF`，解析时跳过 BOM。

### 20. 文件夹输入框别设 `ES_READONLY`
- 用户要能手动粘贴/输入路径；去掉只读，并子类化处理回车触发扫描。

### 21. 勾选必须真正传给发布逻辑
- 最初“勾选”只是 UI 状态，发布时仍按“全部变化”上传。
- 正确：发布前从 ListView 收集勾选项，传给 `lg_publish` 的 `selected_paths`。

### 22. 未选仓库就发布 → 404
- 发布前 `repo_owner/repo_name` 为空时，URL 变成 `/.git/info/refs` → 404。
- 正确：Publish 页显示“目标仓库”，发布前校验为空则明确提示。

### 23. 长任务必须在工作线程
- 连接/拉仓库列表/扫描/发布都是网络或 IO，放 GUI 主线程会卡界面。
- 用 `_beginthreadex` + `PostMessage` 回传结果；发布第一版用跑马灯进度条。

---

## 五、网络 / GitHub

### 24. 错误要“带原因”往上抛
- 最初 `Failed to discover remote refs` 不说明原因，用户无法定位。
- 正确：WinHTTP 失败时用 `GetLastError()` 映射友好描述（Timeout/DNS/Cannot connect/TLS 握手），
  HTTP 非 200 时带上状态码；一路透传到界面。

### 25. 401/403 的友好提示
- 401 → “PAT 无效或已过期”；403 → “PAT 权限不足”；网络失败 → 具体网络原因。

### 26. 单文件 > 100 MiB 会被 GitHub 硬拒绝
- 上传前按实际字节数检查，> 100 MiB 阻止并提示；50~100 MiB 提示警告；不实现 Git LFS。

---

## 六、安全

### 27. PAT 一旦在聊天/日志出现就视为泄露
- 开发时 PAT 只通过环境变量传递，绝不写进源码、日志、提交。
- 曾发现用户自己放在 `build/Release/pat.txt` 的明文 token：`build/` 虽被 gitignore，
  但明文落盘仍是风险 → 建议删除并**撤销重新生成**。

### 28. 持久化凭证用 DPAPI
- `CryptProtectData/CryptUnprotectData`（CRYPTPROTECT_UI_FORBIDDEN），
  密文存 exe 同目录 `data/credentials/`，不写注册表。

### 29. 多个 PAT 的凭证库
- 每个 PAT 独立 DPAPI 加密，base64 后存 `pat_vault.json`，含注释/用户名，可切换/删除。

---

## 七、自举（用 LightGit 发布自己）

### 30. gitignore 的“目录规则”对目录里的文件不生效（曾导致 build/ 漏排除）
- 现象：`.gitignore` 里写 `/build/`、`/data/`（以 `/` 结尾的目录规则），但发布时只对“文件”做匹配，
  目录规则 `dir_only && !is_dir` 直接返回 0 → `build/Release/xxx`、`data/credentials/pat_vault.json` 这些文件会被扫进去发布！
- 正确语义：目录规则应匹配“该目录本身 + 其下所有内容”（Git 是遇到被忽略目录就不下钻）。
- 解决：目录规则匹配“文件”时，逐个检查其祖先目录是否命中。
- 教训：自举发布前，务必确认 `.gitignore` 的目录规则对文件也生效，否则会泄露 `build/` 里的加密凭证库与编译产物。
