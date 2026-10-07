# 开发与训练安装版

`v0.2.0-dev.2`附项目源码、训练工具、Pikafish 2026-09-06 Windows x64通用引擎、该发布包匹配的NNUE、对应GPL源码ZIP、原始许可与作者资料、固定提交的CCPD档案ZIP。它不附VS、EasyX、Python安装程序；这些开发依赖由官方配置入口联网安装并由开发者确认。

请安装到新工作区，避免旧安装器“不覆盖源码”策略留下旧版本文件。开始菜单先运行“配置开发环境”，再运行“配置皮卡鱼训练环境”；第二步展示独立的教师权重使用条款，只有阅读并输入YES才启用教师。配置助手不搜索棋局，不自动训练，也不上传任何个人记录。

配置后执行（工作目录为安装后的源码根目录）：

```powershell
# 31个已验证中局，仅用于检查流程；不代表正式训练或Human Prior。
.\tools\RunTraining.ps1 -PgnPath .\training\data\raw\ccpd-smoke\Dataset\中局\00000001.pgn -RunName smoke001
# 可选：提取完整档案，随后自行筛选、清洗，不要把所有内容直接投入Human Prior。
python tools/training/prepare_dataset.py --full
# 对筛选过的真人大师完整PGN目录显式启动自己的批次：
.\tools\RunTraining.ps1 -PgnPath D:\我的精选PGN -RunName experiment001
```

默认教师Threads=1、Hash=256MiB、每局面300ms、MultiPV=3，配置在`training/config/local.json`，可按计算资源修改。需要暂停长任务时终止进程，保留批次诊断；分片标注与断点续跑使用training/README.md中独立工具命令，而非重复覆盖已有批次。

输出在`training/models/批次名`：转换局面、标签、`fit/evaluation.xqweights`及`fit/report.json`。训练不会自动更新游戏，尤其样本的验证/测试集合可能为空，报告中的误差下降不能作为棋力提升证据。正式发布需独立测试、性能对照、模型卡和许可审查。本轮不在维护者电脑运行正式训练。

## 可复现来源与许可

- Pikafish源码提交：`4c17cee11f888ae1d48a9494f2e2239f019f0a1f`；[官方发布](https://github.com/official-pikafish/Pikafish/releases/tag/Pikafish-2026-09-06)。随包GPLv3 Copying.txt、AUTHORS、README和对应完整源码ZIP；引擎独立运行，不与本项目链接。
- NNUE来自该发布包，SHA-256 `7d13d73569a9b571ba0eb20cf1596247bc2a42738967e61afef6482b231e900e`，并非master-net最新权重。独立条款见随包NNUE-License.md；未经许可不声明商业可用。
- CCPD：Yu-Han Tseng and Bo-Nian Chen (2026)，[Chinese Chess Practical Dataset](https://github.com/Yvonne761/Chinese-Chess-Practical-Dataset)，固定提交`368a47a947773dd8692c026e286dd19b6277b993`，CC BY 4.0，保留原LICENSE和README。原棋谱未修改，样本提取及JSONL转换应记录为处理。档案包含非真人完整比赛内容，必须筛选，不暗示全量兼容。
- 这些第三方文件仅在开发Release中隔离提供，不进入MIT源码树，不随玩家安装版提供。源码直接下载者运行同一助手可从官方地址获取固定文件；校验失败会停止，不自动换版本或关闭TLS验证。

在全新Windows上，VS/EasyX/Python安装分支仍待独立验证；已验证本机开发环境、固定文件校验、样本转换和配置入口。Linux服务器仍使用原Slurm模板及Linux教师，Windows包不是Linux环境安装器。
