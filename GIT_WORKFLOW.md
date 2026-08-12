# 项目 Git / GitLab 开发管理规范与避坑手册

> 适用仓库：`smart_camera_v2` 公司 GitLab 仓库
>
> 适用人员：开发、测试、代码审核和版本维护人员
>
> 维护原则：流程变化、分支策略变化或发生新的 Git 事故后，应同步更新本文档
>
> 最近核对：2026-08-12

## 1. 目的

本文档用于统一项目的 Git 和 GitLab 协作方式，重点解决以下问题：

- 公司仓库与旧本地仓库历史不一致，避免把数百个无关提交带入公司仓库。
- 区分 Git 提交历史、文件树内容和本次发布范围，避免仅凭提交数量判断代码是否一致。
- 统一 `main`、`tfk/develop` 和功能分支的职责。
- 统一功能开发、阶段提交、Bug 修复、MR、验证、回滚和分支清理流程。
- 防止 IDE 配置、构建产物、方案数据、测试数据、备份文件和凭证误入库。
- 保留可审计的提交历史，禁止通过强推覆盖公司主线历史。

本文档中的“必须”“禁止”是团队约束；“建议”可根据实际情况调整，但不得降低主线安全性。

文中 `<feature-name>`、`<commit>`、`<target>` 等均为占位符，执行前必须替换为经过核对的真实值，禁止连同尖括号直接复制到 Shell。

### 1.1 团队红线速览

1. 公司开发以 `tfk/develop` 为基线，禁止整体合并旧本地 `develop`。
2. 大功能从 `tfk/develop` 创建 `tfk/feat/*`，普通修复创建 `tfk/fix/*`。
3. 功能先 MR 到 `tfk/develop`，集成验证通过后再 MR 到 `main`。
4. `main` 禁止直接 push、强推和重写历史；错误版本使用 revert MR 恢复。
5. 公司操作显式使用 `gitlab`，禁止把 `origin` 状态当作公司仓库状态。
6. 需要保留阶段历史时禁止 Squash；`tfk/develop` 禁止设置合并后删除。
7. 每次提交前检查分支、暂存范围和 `git diff --cached --check`。
8. IDE 用户文件、构建产物、本机路径、方案数据、备份和凭证禁止入库。
9. 合并成功页面不是最终证据；必须 fetch 后核对远程提交、祖先关系和必要的 tree。
10. 临时分支只能在远程主线验证通过后删除。

## 2. 当前仓库基线与远程约定

### 2.1 远程名称

本地仓库同时存在多个远程，必须明确区分：

| 远程 | 用途 | 当前协议 |
|---|---|---|
| `gitlab` | 公司正式仓库，所有公司 MR 和发布均以此为准 | HTTPS |
| `origin` | 旧 GitHub / 历史开发远程，不作为公司主线判断依据 | SSH |

团队统一把公司远程命名为 `gitlab`。全新克隆可直接指定远程名：

```bash
git clone -o gitlab \
  https://git.hzzh-kj.com/he/smart_camera_v2.git \
  smart_camera_v2
```

已有仓库应先查看 `git remote -v`：

- 如果 `origin` 已指向公司 GitLab，可执行 `git remote rename origin gitlab`。
- 如果 `origin` 指向旧 GitHub，保留它并使用 `git remote add gitlab <company-url>`。
- 禁止未核对 URL 就重命名或覆盖远程。

操作公司仓库时，命令必须显式写 `gitlab`，例如：

```bash
git fetch gitlab --prune
git pull --ff-only gitlab tfk/develop
git push -u gitlab tfk/feat/example
```

禁止因为本地分支显示与 `origin/develop` 一致，就认为它与公司 `gitlab/main` 或 `gitlab/tfk/develop` 一致。

开始工作前建议核对：

```bash
git remote -v
git status --short --branch
git branch -vv
```

### 2.2 公司分支基线

公司仓库当前长期分支：

```text
main                  公司稳定主线
tfk/develop           tfk 开发集成分支
tfk/feat/<name>       大功能或阶段功能分支
tfk/fix/<name>        普通 Bug 修复分支
tfk/hotfix/<name>     线上紧急修复分支
```

截至本文档最近核对时：

- `main` 和 `tfk/develop` 均指向 `70040636620b3df128c437da4cb5fb281240f1b7`。
- 标定功能的 6 个阶段提交已保留在 `main` 历史中。
- 历史错误合并 `db98559` 没有被删除，其文件变化已通过恢复提交抵消，审计链仍完整。

提交哈希仅用于记录本次处理结果。以后应以远程分支的最新状态为准，不应在脚本中永久写死这些哈希。

## 3. 三个必须分开的概念

### 3.1 提交历史

提交历史描述“代码是如何演进的”。父提交不同，即使最终文件完全相同，提交哈希也不同。

检查祖先关系：

```bash
git merge-base --is-ancestor <older> <newer>
echo $?
```

- 返回 `0`：`older` 是 `newer` 的祖先。
- 返回 `1`：不是祖先。

检查共同祖先：

```bash
git merge-base <branch-a> <branch-b>
```

没有输出并返回非零，表示两条历史没有共同祖先。

### 3.2 文件树内容

文件树描述“某个提交最终包含什么文件和内容”。两条无共同祖先的历史，也可能拥有相同或接近的文件树。

比较整个文件树：

```bash
git diff --stat <commit-a> <commit-b>
git rev-parse <commit-a>^{tree} <commit-b>^{tree}
```

两个 tree 哈希相同，表示所有被 Git 跟踪的文件、内容和文件模式逐字节一致。

### 3.3 本次发布范围

整个仓库差异很大，不等于生产源码差异很大。发布时必须按明确路径检查：

```bash
git diff --stat <base> <target> -- \
  qt_ui_test.pro src styles ui resources scripts
```

本次历史整理中，`43c1dc4` 与旧本地 `958a106` 没有共同祖先，整个仓库相差数百个文档、测试、方案和备份文件；排除本地资料后，正式 C++、UI 和 QSS 源码实际上相同。此前把“整个仓库差异”直接解释为“生产源码差异”属于错误判断。

结论：判断代码关系时，必须同时给出以下三类证据，禁止只给其中一种：

1. 祖先关系或共同基点。
2. 文件树或限定路径的内容差异。
3. 本次 MR 实际允许进入仓库的路径范围。

## 4. 分支职责与命名

### 4.1 `main`

`main` 是公司稳定主线：

- 禁止直接推送。
- 禁止强制推送。
- 只能通过 GitLab MR 更新。
- 合并前必须通过构建、测试、评审和冲突检查。
- 发生问题时使用 `revert` MR 回滚，禁止改写已发布历史。

### 4.2 `tfk/develop`

`tfk/develop` 是面向公司仓库的长期开发集成基线：

- 所有新功能和普通 Bug 分支都从它创建。
- 不直接承载未经隔离的大功能开发。
- 功能 MR 合入后，在该分支完成集成构建和回归。
- 验证通过后，通过 MR 合入 `main`。
- `main` 合并完成后，必须将 `tfk/develop` 快进到最新 `main`。

旧本地 `develop` 含有大量历史提交，只作为旧开发历史保留。禁止执行：

```bash
git merge develop
```

将旧 `develop` 整体合入任何公司分支会重新带入无关历史和内容。

### 4.3 功能和修复分支

推荐命名：

```text
tfk/feat/calibration-v2
tfk/feat/registered-classification
tfk/fix/camera-reconnect
tfk/hotfix/startup-crash
tfk/docs/git-workflow
```

本文命令以当前维护者前缀 `tfk` 为例。其他开发人员应使用团队登记的稳定短前缀，例如 `<owner>/feat/<name>`，避免多人创建同名分支。多人共同维护且明确指定负责人时，才使用团队共享的 `feature/<name>`；不得同时混用多套命名而无人负责。

规则：

- 使用小写英文和连字符，禁止空格。
- 名称应表达业务目的，禁止 `test1`、`new`、`tmp-final` 等无意义名称。
- `tfk/feat/*` 用于大功能。
- `tfk/fix/*` 用于普通 Bug，基于 `tfk/develop`。
- `tfk/hotfix/*` 用于线上紧急 Bug，基于 `main`。
- `tfk/docs/*` 用于团队文档和规范。

创建分支应使用：

```bash
git switch -c tfk/feat/<feature-name>
```

`git branch -b` 是错误语法。旧语法可使用 `git checkout -b`，但团队统一优先使用 `git switch -c`。

## 5. 标准功能开发流程

### 5.1 更新开发基线

```bash
git switch tfk/develop
git status --short --branch
git fetch gitlab --prune
git pull --ff-only gitlab tfk/develop
```

作用：

- 确认没有未提交内容。
- 获取公司仓库最新引用。
- 只允许快进更新，避免 `git pull` 自动生成意外合并提交。

工作区不干净时禁止直接切分支、拉取或合并。先明确改动归属，再提交到正确分支或使用命名明确的临时保存方案。

### 5.2 创建功能分支

```bash
git switch -c tfk/feat/<feature-name>
git status --short --branch
```

复杂功能启动前，应明确：

- 目标和不做的内容。
- 涉及的源码、UI、配置和公共接口。
- 正常路径、异常路径和回归范围。
- 构建及测试证据。

### 5.3 阶段提交

每个提交应是可解释、可评审的逻辑单元：

```bash
git status --short
git diff -- <paths>
git add -- <paths>
git diff --cached --stat
git diff --cached --check
git commit -m "feat(module): complete one logical stage"
```

要求：

- 禁止使用 `git add .` 无检查地暂存整个工作区。
- 优先显式列出文件或目录。
- `git diff --cached --check` 必须无新增空白错误；命令成功且没有输出表示检查通过。
- 不要把多个无关功能、格式化和调试代码混入同一提交。
- 功能相关 Bug 可以在同一功能分支修复，但应独立提交。
- 阶段未完成但需要远程备份时，可推送分支；MR 应标记为 Draft，不得提前合入。

推荐提交信息：

```text
feat(calibration): add N-point trajectory configuration
fix(calibration): keep imported nine-point mode unchanged
refactor(tool-engine): isolate calibration result mapping
test(calibration): cover invalid XML configuration
docs(git): define GitLab merge workflow
build(qmake): register calibration source files
revert: restore initial clean source version
```

### 5.4 长期功能分支同步基线

开发周期较长时，`tfk/develop` 可能已合入其他功能。功能分支应定期同步：

```bash
git status --short --branch
git fetch gitlab --prune
git switch tfk/feat/<feature-name>
git merge --no-edit gitlab/tfk/develop
```

解决冲突后重新执行构建和相关回归。

- 尚未推送、只有本人使用的本地分支，可以在明确理解影响时 rebase。
- 已推送或多人使用的分支，默认使用 merge 同步，禁止未经协商 rebase 后强推。
- 即使确需重写个人远程功能分支，也只能在确认无人依赖后使用 `--force-with-lease`，禁止使用裸 `--force`。
- `main` 和 `tfk/develop` 永远不允许通过 rebase + force push 重写。

### 5.5 推送功能分支

首次推送：

```bash
git push --dry-run gitlab HEAD:refs/heads/tfk/feat/<feature-name>
git push -u gitlab HEAD:refs/heads/tfk/feat/<feature-name>
```

后续推送：

```bash
git push
```

公司 GitLab 使用 HTTPS，不要求为了推送改用 SSH。协议选择应服从公司网络和权限策略，不能把 SSH 当作 HTTPS 故障的必然解决方案。

### 5.6 远程分支改名

Git 没有“原地重命名远程分支”的命令。`git branch -m` 只改本地名称，原远程分支及 upstream 不会自动变化。

标准流程：

```bash
git branch -m <new-name>
git status --short --branch

git push -u gitlab HEAD:refs/heads/<new-name>

git ls-remote --heads gitlab \
  refs/heads/<new-name> \
  refs/heads/<old-name>

git push gitlab --delete <old-name>
```

删除前必须确认新旧远程分支指向预期提交。删除后再次执行 `git ls-remote`，确保新分支存在、旧分支消失。

### 5.7 功能 MR 合入 `tfk/develop`

在 GitLab 创建：

```text
tfk/feat/<feature-name> -> tfk/develop
```

MR 必须包含：

- 变更目标和范围。
- 明确不包含的内容。
- 配置或接口兼容性说明。
- 构建命令和结果。
- 冒烟、回归和异常输入验证结果。
- 已知限制和后续事项。
- 必要的截图、日志摘要或复现步骤。

选项约束：

- 阶段提交有价值时，不勾选 `Squash commits`。
- 功能分支在 MR 合并并完成远程验证前，不勾选自动删除。
- `tfk/develop` 是长期分支，任何 MR 都不得设置合并后删除它。
- 作者不应是唯一审核者；正式团队流程至少需要一名非作者审核人。

MR 是 GitLab 远程概念。本地可以模拟合并和构建，但禁止先在本地把功能合入 `tfk/develop`，再直接推送绕过 MR。

### 5.8 集成验证

功能 MR 合入后：

```bash
git switch tfk/develop
git fetch gitlab --prune
git pull --ff-only gitlab tfk/develop
git status --short --branch
```

至少完成：

- 主工程 qmake。
- 主工程 make。
- 对应功能冒烟测试。
- 相关公共链路回归。
- 空图像、无效 ROI、配置缺失、运行环境异常等异常路径。
- `git diff --check`。
- 确认没有构建产物和本地数据进入提交。

### 5.9 `tfk/develop` 合入 `main`

集成验证通过后创建：

```text
tfk/develop -> main
```

要求：

- 禁止直接 push `main`。
- 默认不压缩有意义的提交历史。
- 不删除 `tfk/develop`。
- 合并前确认目标确实是公司 `main`，不是 `origin/main` 或本地同名分支。
- GitLab 显示无冲突并不替代构建和测试。

合并完成后同步长期开发分支：

```bash
git fetch gitlab --prune
git switch tfk/develop
git merge --ff-only gitlab/main
git push gitlab tfk/develop
```

最终核对：

```bash
git status --short --branch
git rev-parse HEAD gitlab/tfk/develop gitlab/main
```

三个哈希应一致。

## 6. Bug 和 Hotfix 流程

### 6.1 当前大功能内部 Bug

属于正在开发的大功能，直接在对应 `tfk/feat/*` 分支修复，并使用独立 `fix(...)` 提交。

### 6.2 普通 Bug

基于 `tfk/develop` 创建：

```bash
git switch tfk/develop
git pull --ff-only gitlab tfk/develop
git switch -c tfk/fix/<bug-name>
```

完成后：

```text
tfk/fix/<bug-name> -> MR -> tfk/develop -> 验证 -> MR -> main
```

### 6.3 线上紧急 Bug

基于公司最新 `main` 创建：

```bash
git fetch gitlab --prune
git switch -c tfk/hotfix/<bug-name> gitlab/main
```

完成后：

1. `tfk/hotfix/<bug-name> -> main` 创建紧急 MR。
2. 完成必要的最小构建和回归。
3. 合入 `main` 后，将最新 `main` 快进或合并回 `tfk/develop`。
4. 禁止只修 `main` 而遗漏开发分支，否则下次发布可能重新引入旧 Bug。

## 7. 仓库内容边界

### 7.1 必须入库

- 生产源码：`src/`。
- Qt Designer 文件：`ui/`。
- 样式：`styles/`。
- 构建入口和确定性的构建配置：`qt_ui_test.pro`、`qmake/`。
- 程序运行所需资源：`resources/`。
- 可移植脚本及模板：`scripts/*.sh`、`local_paths.pri.example`。
- 经团队确认的根级治理文档，例如 `README.md`、`INSTALLATION.md`、`GIT_WORKFLOW.md`。

### 7.2 禁止入库

- Qt Creator 用户文件：`.qtcreator/`、`*.pro.user`、`*.user*`。
- VS Code 用户配置：`.vscode/`，除非团队明确批准共享配置。
- 构建产物：`build/`、`Makefile`、`*.o`、`moc_*.cpp`、`ui_*.h`、可执行文件和日志。
- 本机路径或依赖环境：`local_paths.pri`、`scripts/dependencies.env`。
- 手工备份：`*.bak`、`*.bak_*`。
- 本地方案和客户数据：`projects/scheme_1/` 及其他 `projects/` 内容，除非脱敏后经专项批准。
- 训练数据、测试图片、临时截图和大体积素材，除非明确制定 Git LFS 或制品库方案。
- 密码、Token、License、私钥、访问地址中的敏感参数。
- 工作树目录：`.worktrees/` 或任何把另一个 Git worktree 放进仓库内部的路径。

### 7.3 当前 `.gitignore` 的特殊约束

当前公司仓库忽略根目录 `AGENTS.*`，以及整个 `docs/`、`smoke/`、`tests/` 和 `projects/`。因此：

- 本文档使用根目录 `GIT_WORKFLOW.md`，不能依赖 `git add -f docs/...` 绕过团队策略。
- 当前 `AGENTS.md` 仅作为个人本地参考，不是公司仓库中的团队规则载体；需要团队共同执行的约束必须写入已跟踪的治理文档，或通过专项治理 MR 调整忽略策略后再纳入版本控制。
- 功能文档、smoke 源码或测试源码是否纳入公司仓库，需要单独形成治理 MR，明确修改 `.gitignore` 和目录规范。
- 历史 `.gitignore` 曾出现“测试源码入库”的注释与目录级忽略规则相反的问题；本次只修正误导注释，没有改变忽略行为。
- 后续如要启用测试源码入库，应通过专门治理 MR 修改实际规则，禁止在普通功能 MR 中临时强制添加。

#### 7.3.1 忽略规则不等于文件保护

`.gitignore` 只决定未跟踪文件是否出现在默认状态和暂存候选中；它不影响已跟踪文件，也不保证文件在切换分支时保留。

如果旧分支跟踪了 `AGENTS.md`、`docs/`、`tests/` 或 `smoke/`，而目标 `tfk/develop` 的提交树不包含这些路径，那么执行 `git switch tfk/develop` 时，Git 会按照目标提交树移除它们。即使目标分支的 `.gitignore` 同时忽略这些路径，也不会阻止移除。

因此，不能把“已经写入 `.gitignore`”理解为本地资料已经备份或受到保护。切换分支前必须先确认这些资料在另一个 worktree、受控备份或其他可恢复位置中仍然存在。

#### 7.3.2 本地参考资料的安全迁移顺序

从旧历史迁移到公司基线时，必须遵守以下顺序：

1. 保留旧分支和旧 worktree，不在唯一保存资料的工作树上直接切换基线。
2. 在独立 worktree 或新克隆中先检出并确认公司 `tfk/develop` 基线；分支已被其他 worktree 占用时，先用 `git worktree list` 定位，禁止强制绕过。
3. 基线检出完成后，再从旧 worktree 按审核白名单复制 `AGENTS.md`、`docs/`、`tests/`、`smoke/` 等确需本地保留的资料。
4. 不使用 `rsync --delete` 覆盖含有额外本地资料的目录；复制后同时验证文件存在、未被追踪并命中忽略规则。
5. 禁止使用 `git add -f` 把本地参考资料塞入普通功能 MR；需要团队共享时必须另开治理 MR。

复制后的验证命令：

```bash
git ls-files -- AGENTS.md docs tests smoke
git check-ignore -v -- AGENTS.md docs tests smoke
git status --ignored --short -- AGENTS.md docs tests smoke
```

其中 `git ls-files` 应无输出，`git status --ignored` 应显示 `!!`。若路径不存在或未命中忽略规则，必须先处理，不能继续开发或清理旧 worktree。

### 7.4 提交前卫生检查

```bash
git status --short
git diff --cached --name-status
git diff --cached --check
git ls-files | grep -E '(\.user|\.bak|Makefile$|\.o$|\.log$)' || true
```

发现误暂存文件时：

```bash
git restore --staged -- <path>
```

已被历史版本跟踪、但现在应忽略的文件，需要专项清理：

```bash
git rm --cached -- <path>
```

禁止在未核对路径时使用宽泛递归删除命令。

## 8. 构建与测试门禁

### 8.1 影子构建

禁止在源码目录散落构建产物。推荐：

```bash
mkdir -p build/verification/main
cd build/verification/main
"$QT_ROOT/bin/qmake" ../../../qt_ui_test.pro
make -j"$(nproc)"
```

如果当前公司仓库的构建输出布局与上述路径不同，应以 `README.md` 和实际 qmake 配置为准，但必须保持影子构建原则。

### 8.2 功能验证证据

MR 描述中应记录：

- 执行的完整命令。
- 退出状态或明确的 PASS 信息。
- 环境版本：Qt、HALCON、OpenCV。
- 预期警告与真正失败的区别。
- 未执行的测试及原因。

例如，Qt offscreen 环境中的以下信息不一定是失败：

```text
This plugin does not support propagateSizeHints()
```

OpenCV 对测试中故意不存在的图片给出读取警告，也不一定是失败。最终应以测试设计和明确的 `PASS` / 非零退出码判断，不能只看日志中是否出现 `WARN`。

### 8.3 测试路径

从不同构建目录执行 qmake 时，相对路径不同。遇到 `Cannot find file`，先检查：

```bash
pwd
find .. -name '<test-name>.pro' -print
```

禁止通过反复猜测 `../../`、`../../../` 解决路径问题。

## 9. 旧历史迁移与提交重放

### 9.1 为什么不能直接推送旧功能分支

原始功能分支可能只有 6 个功能提交，但它的父链可能包含上百个旧提交。直接推送或合并会把全部可达祖先带到公司仓库历史。

检查功能分支相对基线的提交：

```bash
git log --oneline <base>..<feature>
git rev-list --count <base>..<feature>
```

`git log --oneline` 不带范围会显示当前分支的全部祖先，不能用来回答“功能分支新增了几个提交”。

### 9.2 原哈希与阶段历史不能同时强求

Git 提交哈希包含父提交。把旧提交移到新的公司基线上后：

- 可以保留作者、作者时间、提交说明和阶段划分。
- 不能保留原提交哈希。
- 如果强行保留原哈希，就必须保留原父链，从而带入全部旧历史。

### 9.3 只重放批准路径

当旧提交混有文档、测试数据、IDE 文件或其他功能时，推荐逐段重放批准路径：

```bash
git diff --binary <old-parent> <old-commit> -- \
  qt_ui_test.pro src styles ui \
  | git apply --index -

git status --short
git --no-pager diff --cached --stat
git diff --cached --check
git commit -C <old-commit>
```

要求：

- 每次只处理一个旧提交。
- 每次应用后都检查文件范围和空白错误。
- `git commit -C` 复用原作者、作者时间和提交说明，但会生成新哈希。
- 禁止把多段提交一次性压成一个提交，除非团队明确决定放弃阶段历史。
- 禁止机械循环自动提交而跳过每一步范围检查。

### 9.4 最终内容验证

重放完成后，确认提交数量、顺序和最终文件树：

```bash
git rev-list --count <new-base>..HEAD
git log --oneline --reverse <new-base>..HEAD
git rev-parse HEAD^{tree} <verified-version>^{tree}
```

tree 哈希相同，说明重放后的最终内容与已验证版本完全一致。

本次标定历史重放结果记录如下：

| 原提交 | 新提交 | 阶段 |
|---|---|---|
| `3fb628b` | `7eb31b1` | 9 点标定和标定转换初步验证 |
| `4d20711` | `19c9a122` | N 点标定、轨迹线、XML 检测和持久化 |
| `8dbafa7` | `15dcadd` | 标定有效区域和返回参数语义 |
| `26a92ab` | `c915452` | 9 点/12 点选择及列表交互修复 |
| `de8e7fc` | `3ebef84` | XML 1.4、参数整理和缩略图交互 |
| `a77e02a` | `62658c0` | 函数注释和接口说明 |

最终生产文件树为 `a4f2dd06f1c0b6b08819613d85c3bdc80856c162`。该哈希用于本次迁移审计，不作为未来版本的固定目标。

### 9.5 `cherry-pick` 的使用边界

普通 `cherry-pick <commit>` 会生成新提交，不会自动带入该提交的全部祖先，因此它本身并非“带入旧历史”的原因。但它会应用该提交涉及的所有路径。

- 提交范围干净且全部应进入公司仓库：可以逐个 `cherry-pick`。
- 提交夹杂无关路径：使用限定路径的 `git diff | git apply --index`。
- 合并提交：禁止不理解父节点就执行 `cherry-pick -m`。

## 10. 错误合并与安全恢复

### 10.1 禁止做法

公司 `main` 出现错误合并后，禁止：

- `git push --force` 覆盖远程主线。
- 删除历史提交后伪装成从未发生。
- 未建立备份就执行 `reset --hard`。
- 直接把本地正确分支强推为远程 `main`。

这些操作会破坏审计历史，并可能覆盖其他人的工作。

### 10.2 撤销合并提交

先只读核对合并提交父节点：

```bash
git show -s --format='commit=%H%nparents=%P%nsubject=%s' <bad-merge>
```

从公司最新主线创建恢复分支：

```bash
git fetch gitlab --prune
git switch -c tfk/recovery/<date> gitlab/main
git revert -m 1 --no-commit <bad-merge>
```

`-m 1` 表示以第一个父提交为应保留的主线。它不是固定答案；执行前必须根据父节点和预期文件树确认主父编号。

验证：

```bash
git diff --name-only --diff-filter=U
git diff --cached --check
git write-tree
git rev-parse <expected-commit>^{tree}
```

如果 `git write-tree` 与预期 tree 哈希相同，说明暂存区准确恢复到预期内容。然后创建独立恢复提交、推送恢复分支并通过 MR 合入 `main`。

如果预期基线本身已有尾随空格，`git diff --cached --check` 可能报告旧问题。此时不要在“精确恢复”提交中顺手修改，否则 tree 将不再完全一致；应在恢复完成后用单独 MR 清理格式。

### 10.3 合并后验证

```bash
git fetch gitlab --prune
git show -s --format='HEAD=%H%nPARENTS=%P%nSUBJECT=%s' gitlab/main
git rev-parse gitlab/main^{tree} <expected-commit>^{tree}
```

恢复分支只能在远程 `main` 文件树验证通过后删除。

## 11. 合并和冲突预检查

### 11.1 分叉数量

```bash
git rev-list --left-right --count <target>...<source>
```

输出：

```text
<target 独有提交数> <source 独有提交数>
```

“源分支落后目标分支 N 个提交”不一定表示源代码过期。必须继续检查这些目标提交是否真的改变了文件树。

### 11.2 共同基点和目标变化

```bash
base="$(git merge-base <target> <source>)"
git diff --stat "$base" <target>
git diff --stat "$base" <source>
```

如果目标分支相对共同基点只有历史恢复/合并提交，且最终 tree 与基点一致，源分支可能无需 rebase 也能安全合并。

### 11.3 `merge-tree` 版本差异

较新的 Git 支持：

```bash
git merge-tree --write-tree <target> <source>
```

旧版本可能只支持三参数形式：

```bash
git merge-tree "$(git merge-base <target> <source>)" <target> <source>
```

遇到 usage 提示时先执行：

```bash
git --version
git merge-tree -h
```

禁止因为某个选项不可用就把“命令版本不兼容”误判为“代码无法合并”。

### 11.4 真实合并验证

对于高风险 MR，可在独立临时分支或 worktree 中模拟合并和构建。禁止在长期分支上直接试合并后再随意 `reset --hard`。

### 11.5 冲突解决语义

冲突状态中的 `ours` / `theirs` 只是相对当前 Git 操作的两侧，不代表“公司正确版本”和“功能正确版本”。尤其在 `cherry-pick`、`rebase` 和 `revert` 中，其含义容易与直觉相反。

禁止对公共源码批量执行：

```bash
git checkout --ours -- <many-files>
git checkout --theirs -- <many-files>
```

除非已经逐文件确认整份文件应采用某一侧。标准做法是：

```bash
git status --short
git diff --name-only --diff-filter=U
git diff -- <conflicted-file>
```

逐块解决后执行 `git add <conflicted-file>`。如果某文件只是“不属于本次发布范围”，应在独立发布分支采用路径白名单，而不是在原功能分支执行 `git rm -f` 删除合法资料。

`git cherry-pick --abort` 或 `git merge --abort` 提示当前没有对应操作时，不代表工作区已经恢复。应先查看 `git status` 和暂存区；只有在隔离 worktree、工作区无须保留且目标提交已核对时，才允许使用 `reset --hard` 恢复。

## 12. Worktree 和备份规范

### 12.1 使用场景

以下场景建议使用独立 worktree：

- 公司干净基线与旧本地开发历史并存。
- 高风险 cherry-pick、提交重放或恢复操作。
- 两个功能需要并行构建验证。

查看：

```bash
git worktree list
```

创建前应确认目标目录不在仓库跟踪范围内。推荐放在仓库同级目录，不要放入仓库根目录后误提交。

示例：

```bash
git worktree add \
  -b tfk/feat/<feature-name> \
  ../smart_camera_v2-<feature-name> \
  gitlab/tfk/develop
```

一个分支通常只能被一个 worktree 检出。出现“分支已被其他 worktree 使用”时，先执行 `git worktree list`，禁止用强制参数绕过占用检查。

### 12.2 备份分支

在高风险合并、rebase、reset 或历史重放前创建命名明确的备份：

```bash
git branch backup/<topic>-before-<operation>-<yyyymmdd>
```

备份分支不是永久垃圾桶。操作完成、远程验证通过并确认无需恢复后，再清理。

### 12.3 `reset --hard`

仅允许在满足全部条件时使用：

- 明确的临时分支或隔离 worktree。
- `git status` 已确认没有需要保留的改动。
- 目标提交已精确核对。
- 必要时已有备份分支。
- 不影响其他 worktree 正在使用的分支。

禁止在工作区根目录、未知分支或含用户改动时照抄 `reset --hard`。

### 12.4 分支和 worktree 清理

短期分支合并并完成远程验证后，先删除 worktree，再删除分支。不能在待删除 worktree 自己的目录内执行移除。

```bash
git worktree list
git worktree remove /absolute/path/to/worktree
git worktree prune
git branch -d tfk/feat/<feature-name>
```

`git branch -d` 会阻止删除尚未合入当前历史的分支。只有在以下证据全部满足时才允许 `-D`：

- 远程正式分支已包含所需提交，或另一正式分支具有完全相同的已验证 tree。
- 不再需要该提交作为唯一恢复引用。
- 已记录待删除分支的提交哈希。

远程删除前先核对：

```bash
git ls-remote --heads gitlab refs/heads/<branch-name>
git push gitlab --delete <branch-name>
git fetch gitlab --prune
```

## 13. HTTPS、鉴权和代理

### 13.1 HTTPS 即可完成公司推送

公司仓库当前使用 HTTPS：

```text
https://git.hzzh-kj.com/he/smart_camera_v2.git
```

不需要为了推送强制改为 SSH。HTTPS 鉴权失败应优先检查用户名、密码或访问令牌、凭据缓存、证书和网络代理。

禁止把密码或 Token 写入：

- Git 远程 URL。
- Shell 历史可见命令。
- 仓库配置文件。
- MR 描述和日志。

### 13.2 TLS 被代理中断

典型错误：

```text
gnutls_handshake() failed: The TLS connection was non-properly terminated
```

本次实际根因是本机代理环境变量干扰公司 GitLab HTTPS。可对单条 Git 命令临时取消代理：

```bash
env \
  -u HTTP_PROXY -u HTTPS_PROXY \
  -u http_proxy -u https_proxy \
  -u ALL_PROXY -u all_proxy \
  git ls-remote --heads gitlab
```

推送同理。该方式只影响当前命令，不会破坏全局代理配置。

先用只读命令验证连通性：

```bash
git ls-remote --heads gitlab
```

再使用：

```bash
git push --dry-run gitlab HEAD:refs/heads/<branch>
```

最后才正式 push。

### 13.3 无输出不一定失败

`git fetch gitlab --prune` 成功且远程没有变化时可能没有任何输出。应通过退出状态或后续 `git log gitlab/main` 验证，不要把“无输出”解释为“没有执行”。

命令提示符已经重新出现后再按 `Ctrl+C`，只会中断当前空提示或下一次输入，不会撤销刚刚成功完成的 fetch。

### 13.4 终端续行和分页器

Shell 多行命令中的反斜杠必须是该行最后一个字符，后面不能有空格：

```bash
env \
  -u HTTP_PROXY -u HTTPS_PROXY \
  git fetch gitlab --prune
```

Git 长输出末尾出现 `:` 时，通常表示进入 `less` 分页器，并非命令卡死。按 `q` 退出，或者对只读统计使用：

```bash
git --no-pager diff --cached --stat
git --no-pager log --oneline -20
```

高风险操作应一次执行一个命令。多条检查命令连续粘贴时，必须能明确区分每一条命令对应的输出。

## 14. MR 设置与 GitLab 项目保护

### 14.1 建议的 GitLab 保护设置

`main`：

- Protected branch。
- 禁止普通开发者直接 push。
- 禁止 force push。
- 仅允许通过 MR 合并。
- 至少一名非作者审核者。
- 必须解决所有讨论。
- CI 成功后才允许合并。

`tfk/develop`：

- 建议禁止直接强推。
- 功能应通过 MR 合入。
- 保留长期分支，不自动删除。

### 14.2 Squash 规则

- 阶段提交清晰、有审计价值：不 Squash。
- 大量 WIP、fixup、临时提交：在功能分支尚未共享时先整理；或经评审决定 Squash。
- 已被多人使用的分支，禁止随意 rebase 后强推。
- `tfk/develop -> main` 默认不 Squash，以保留已审核的功能提交。

### 14.3 删除源分支

- 短期功能分支：合并并完成远程验证后可以删除。
- `tfk/develop`：禁止删除。
- 恢复分支：必须等 `main` tree 验证通过后再删除。
- 删除前使用 `git ls-remote` 核对目标分支和提交。

## 15. 常用只读审计命令

### 当前状态

```bash
git status --short --branch
git branch --show-current
git branch -vv
git worktree list
git remote -v
```

### 功能提交范围

```bash
git log --oneline <base>..<feature>
git rev-list --count <base>..<feature>
git log --oneline --reverse <base>..<feature>
```

### 分支分叉

```bash
git rev-list --left-right --count <target>...<source>
git merge-base <target> <source>
```

### 文件范围

```bash
git diff --name-status <base>..<target>
git diff --stat <base>..<target>
git diff --check <base>..<target>
```

### 远程精确核验

```bash
git ls-remote --heads gitlab \
  refs/heads/main \
  refs/heads/tfk/develop
```

### 提交父节点和文件树

```bash
git show -s --format='HEAD=%H%nPARENTS=%P%nSUBJECT=%s' <commit>
git rev-parse <commit-a>^{tree} <commit-b>^{tree}
```

## 16. 本次实际踩坑记录

| 坑 | 现象 | 根因 | 标准处理 |
|---|---|---|---|
| 把全部日志当成功能提交 | `git log --oneline` 显示大量提交，却又说功能只有 6 个 | 未限定比较范围 | 使用 `git log develop..feature` |
| 没确认当前分支 | 以为在 `develop`，实际仍在 `feature/calibration` | 只看日志装饰或记忆 | 先执行 `git branch --show-current` 和 `git status -sb` |
| 混淆远程 | `develop...origin/develop` 被误认为公司 GitLab 状态 | `origin` 与 `gitlab` 用途不同 | 公司操作显式写 `gitlab` |
| 误解“领先 7/8” | 功能只有 6 个提交，分支却领先 7 或 8 | `--no-ff` 合并提交和后续格式修复也计入 | 用图形日志和范围日志分别解释 |
| MR 显示源分支落后目标 | 因“落后 4 个提交”准备盲目 rebase | ahead/behind 只统计提交图，目标 tree 可能仍等于共同基点 | 同时检查 merge-base、目标变化、冲突和 tree |
| 把仓库差异当源码差异 | 两个基线显示数百文件差异，误判生产代码差异巨大 | 文档、测试、方案和备份占多数 | 使用限定路径 diff，并单独核对 tree |
| 把 `.gitignore` 当成本地文件保护 | 切换到 `tfk/develop` 后 `AGENTS.md`、`docs/`、`tests/` 消失 | 这些路径在旧分支被跟踪，但目标提交树不包含；ignore 不影响已跟踪文件和分支切换 | 保留旧 worktree，先检出公司基线，再复制成本地忽略副本并验证 `git ls-files` 无输出、状态为 `!!` |
| 把相似文件树当同一历史 | `43c1dc4` 与 `958a106` 生产源码接近，就认为有父子关系 | 文件内容与提交拓扑是两件事 | 同时检查 `merge-base` 和限定路径 diff |
| 认为父提交内容仍被保留 | `db98559` 有 `43c1dc4` 作为父节点，却大量删除源码 | 合并提交的最终 tree 可以改变父节点任意内容 | 检查合并提交 tree，不根据父节点名称推断 |
| 直接 cherry-pick 合并提交 | 出现大量修改/删除和内容冲突 | 在错误基线使用 `cherry-pick -m 1` | 先核对父节点、基线和预期 tree；优先逐段重放 |
| 把 `theirs` 当成功能正确版本 | 对冲突文件整体执行 `checkout --theirs` | `ours/theirs` 只表示当前操作的两侧 | 公共文件逐块审查，不按名称猜业务含义 |
| 用 `git rm -f` 排除无关内容 | 冲突消失了，但合法文件也从发布树删除 | 混淆“不进入本次 MR”和“项目不再需要” | 使用独立发布分支和路径白名单 |
| 把其他功能资料视为垃圾 | RegisteredClassification 文档/测试出现在差异中就准备删除 | 只按当前功能名称判断文件价值 | 从本次 MR 排除，但不在原开发分支删除 |
| `abort` 提示没有操作 | `cherry-pick --abort` 无法执行 | 当前没有对应 sequencer 状态 | 先看 status 和暂存区，只在隔离 worktree 谨慎 reset |
| 压缩了需要保留的阶段历史 | 最终代码正确，但 6 个功能提交变成 1 个 | 发布时只应用最终 diff 后一次提交 | 按相邻提交逐段 `diff/apply` 并 `commit -C` |
| 想同时保留原哈希和新基线 | 希望提交仍为原哈希，又不带旧父链 | Git 哈希包含父提交 | 接受新哈希，保留作者、时间、说明和阶段 |
| 误把 `git apply` 成功当完整验证 | 补丁无输出 | 无输出只表示应用成功，不代表路径和格式正确 | 继续检查 status、stat、diff check |
| 暂存统计与提交统计不同 | commit 显示 rewrite 或行数略变 | Git 相似度和重写识别的展示方式不同 | 以 name-status、实际 diff 和 tree 哈希为准 |
| 文档尾随空格 | `git diff --check` 报 trailing whitespace | Markdown 行末空格 | 单独修复并再次检查；精确恢复时另行处理 |
| 相对测试路径错误 | qmake 报 `Cannot find file` | 当前目录与预期不同 | 先 `pwd` 和 `find`，再使用正确相对路径 |
| 把警告当失败 | offscreen / 缺失测试图片出现 WARN | 测试设计中的预期环境信息 | 结合退出码和明确 PASS 判断 |
| `merge-tree` 选项不可用 | 显示 usage | Git 版本较旧 | 查帮助并使用三参数形式 |
| HTTPS TLS 失败 | `gnutls_handshake()` 被中断 | 代理环境变量干扰公司内网 | 对单条 Git 命令取消代理，先 `ls-remote`、再 dry-run |
| 认为 HTTPS 必须改 SSH | 推送失败后准备切换协议 | 未先定位代理或鉴权问题 | HTTPS 可以正常使用，按网络、证书、代理、凭据顺序排查 |
| 本地改名后 upstream 仍是旧名称 | `git branch -m` 后 status 仍跟踪旧远程 | 本地改名不会修改远程引用 | 创建新远程、设置 upstream、核对哈希、再删旧远程 |
| 过早删除旧远程分支 | 新分支尚未核对就准备删除旧分支 | 缺少哈希验证 | 先 `ls-remote` 确认新旧指向相同提交，再删除旧分支 |
| 删除远程后本地显示“丢失” | 本地分支仍记录旧 upstream | 远程删除不会自动删除本地分支 | fetch `--prune` 后按 `-d` / 受控 `-D` 清理本地引用 |
| 过早删除恢复分支 | MR 刚合并就清理 | 尚未验证远程 main tree | fetch 后核对 tree，再删远程和本地临时分支 |
| 在长期分支直接开发 | 功能、Bug 和集成状态混在一起 | 没有功能分支隔离 | 从 `tfk/develop` 创建 `tfk/feat/*` 或 `tfk/fix/*` |
| 把 MR 当本地操作 | 想执行“本地 MR” | MR 属于 GitLab | 本地只验证，正式合并必须走远程 MR |
| 自动删除长期分支 | MR 页面默认勾选 Delete source branch | 未区分临时与长期分支 | `tfk/develop` 永不自动删除 |
| Squash 丢失历史 | MR 默认或误勾选压缩 | 未提前确定历史保留策略 | 有价值的阶段提交不 Squash |
| 本地方案数据误入库 | `projects/scheme_1` 出现在补丁或冲突中 | 将整个提交/目录作为发布范围 | 保留本地，发布时采用路径白名单 |
| IDE 用户文件冲突 | `.pro.user` 修改/删除冲突 | 本机 IDE 状态被跟踪 | `.gitignore` 排除并从索引移除 |
| 构建产物污染源码树 | Makefile、对象文件和可执行文件出现 | in-source build 或误暂存 | 使用影子构建并执行提交前卫生检查 |
| 输出末尾出现 `:` | 以为命令卡死 | Git 进入 `less` 分页器 | 按 `q` 退出或使用 `git --no-pager` |
| 多行命令续行异常 | 反斜杠后带空格或不是行尾 | Shell 将命令拆开执行 | 确保 `\` 是行尾最后一个字符 |

## 17. 团队检查清单

### 开始开发前

- [ ] 已确认当前目录和 worktree。
- [ ] 切换公司基线前，旧分支中的本地参考资料已保存在独立 worktree 或其他可恢复位置。
- [ ] 已确认当前分支不是旧本地 `develop`。
- [ ] 已确认公司远程为 `gitlab`。
- [ ] `git status` 干净。
- [ ] `tfk/develop` 已通过 `--ff-only` 更新。
- [ ] 仅在公司基线检出完成后复制本地参考资料，并确认 `git ls-files` 无输出、`git status --ignored` 显示 `!!`。
- [ ] 已创建语义清晰的功能或修复分支。

### 每次提交前

- [ ] 只暂存本次逻辑单元相关文件。
- [ ] 已检查 `git diff --cached --stat`。
- [ ] `git diff --cached --check` 无新增问题。
- [ ] 无 IDE 文件、构建产物、本地路径、方案数据和凭证。
- [ ] 提交信息能说明模块和目的。

### 功能 MR 前

- [ ] 功能分支已推送并设置正确上游。
- [ ] MR 目标是 `tfk/develop`。
- [ ] 描述包含范围、验证、限制和不包含项。
- [ ] 已完成 qmake、make 和专项测试。
- [ ] 已确认是否保留阶段提交；需要保留时未勾选 Squash。
- [ ] 未设置删除 `tfk/develop`。

### 发布到 `main` 前

- [ ] `tfk/develop` 已完成集成验证。
- [ ] MR 目标确实是公司 `main`。
- [ ] 无未解决冲突和评审意见。
- [ ] CI / 构建 / 冒烟通过。
- [ ] 至少一名非作者审核者批准。
- [ ] 已准备回滚方式和预期 tree / tag。

### 合并后

- [ ] 已 fetch 公司远程。
- [ ] 已核对 `gitlab/main` 提交和文件树。
- [ ] 已将 `tfk/develop` 快进到最新 `main`。
- [ ] 已推送更新后的 `tfk/develop`。
- [ ] 仅在验证通过后删除临时远程分支。
- [ ] 已清理无用本地临时分支和 worktree。

## 18. 版本发布和回滚建议

正式版本验证完成后，建议由维护人员在 `main` 创建带注释标签：

```bash
git fetch gitlab --prune
git tag -a v<major>.<minor>.<patch> gitlab/main \
  -m "release: v<major>.<minor>.<patch>"
git push gitlab v<major>.<minor>.<patch>
```

标签发布前必须确认：

- 标签目标就是已验证的 `gitlab/main`。
- 版本说明完整。
- 构建环境和依赖版本已记录。
- 回滚目标清晰。

线上回滚优先创建 revert 分支和 MR。禁止删除标签后强推主线来掩盖已发布版本。

## 19. 维护责任

- 分支创建者负责分支范围、提交卫生和 MR 描述。
- 审核者负责检查业务正确性、兼容性、路径范围和测试证据，不能只看 GitLab 绿色对勾。
- 维护人员负责 `main` 保护规则、版本标签、恢复操作和远程分支清理。
- 任何人发现本文档与实际仓库策略不一致，应通过 `tfk/docs/*` 分支提交修订 MR，不得口头约定后长期不更新文档。
