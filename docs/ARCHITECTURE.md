# LightGit 架构

## 分层

```
GUI (src/gui)          Win32 四页 + 拖放 + 进度
  │
App (src/app)          应用状态、发布管理器（合并+推送）
  │
GitHub (src/github)    REST API 封装（auth/repository/tree）
  │
Git (src/git)          自研 Git 引擎（hash/object/pack/pktline/transport）
  │
Net (src/net)          WinHTTP HTTPS 客户端
  │
File (src/file)        目录扫描 / .gitignore / 状态
  │
Storage (src/storage)  配置 / Manifest / DPAPI 凭证
  │
Common (src/common)    十六进制 / 动态缓冲 / JSON 解析
  │
External (external)    miniz（zlib 兼容压缩，唯一第三方依赖）
```

## 数据流（一键发布）

```
本地目录扫描 → .gitignore 过滤 → blob 哈希 → 状态判定(Manifest)
→ 读远程引用(git-receive-pack info/refs) → 读远程树(REST git/trees)
→ 合并（只替换选中文件，保留远端其余） → 递归构建 Tree
→ 创建 Commit（parent=远端 HEAD） → 生成 Packfile（仅新对象）
→ POST git-receive-pack 推送 → 解析 report-status → 更新 Manifest
```

## 关键模块

| 模块 | 职责 |
|---|---|
| `git/git_hash.c` | SHA-1（BCrypt，流式） |
| `git/git_object.c` | 对象头 / 对象 ID |
| `git/git_blob.c` `git_tree.c` `git_commit.c` | 三种对象构建 |
| `git/git_pack.c` | Packfile 生成（变长头 + zlib + SHA-1 尾） |
| `git/git_pktline.c` | pkt-line 编解码 |
| `git/git_transport.c` | info/refs 发现 + git-receive-pack 推送 |
| `github/*.c` | REST API（Bearer 认证 + JSON） |
| `app/publish_manager.c` | 合并 + 增量发布主流程 |
| `storage/credential.c` | DPAPI 加密 Token |

## 线程模型

GUI 主线程只处理消息；所有网络/扫描/哈希/push 均在 `_beginthreadex`
工作线程执行，通过 `PostMessage` 回传结果。发布过程第一版为“整体阻塞 +
不确定进度”，取消/精确进度后续细化。
