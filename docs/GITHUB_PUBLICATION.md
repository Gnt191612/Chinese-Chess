# 首次GitHub开源准备

本次以圆形半透明“吃／将”徽章版作为 `v0.1.0-rc.1` 源码候选。没有训练模型，也不附带教师程序、EasyX库、个人棋局或经验库。所有者已授权首次提交、创建公开仓库和推送；实际操作结果以Git记录及远端页面为准，不将发布准备文档当作已经上传的证明。

## 推荐仓库资料

- 显示名称：中国象棋。
- 已创建公开仓库并推送 `main`：[Gnt191612/Chinese-Chess](https://github.com/Gnt191612/Chinese-Chess)。
- 简介：Windows本地中国象棋人机对弈，限时搜索、轻量棋理评估、常见开局与传统木质界面。
- 关键词建议：`xiangqi`、`chinese-chess`、`cpp`、`windows`、`game-ai`。
- 许可证：自有源码MIT；第三方参考资料和未来模型另行遵守各自许可。
- 源码候选可先发布；另行准备成品包，公开二进制前仍需完成扫描和再分发确认。

## GitHub搜索与Topics

按[GitHub官方规则](https://docs.github.com/en/repositories/managing-your-repositorys-settings-and-features/customizing-your-repository/classifying-your-repository-with-topics)，Topics使用小写字母、数字和连字符，每个不超过50字符，最多20个。中文关键词写进README，不直接用作Topics；不保证任何搜索排名。

已在公开仓库设置的20个Topics：

```text
xiangqi chinese-chess cpp windows easyx game-ai alpha-beta iterative-deepening opening-book learn-xiangqi student-project course-project artificial-intelligence machine-learning knowledge-distillation pikafish community-data chess-style hu-ronghua xu-yinchuan
```

其中训练、蒸馏、棋风和棋手姓名对应README明确标注的后续研究计划，不代表相关模型已存在或本人背书。以后实现并验证更多棋手插件后，再调整相关标签；不堆砌与项目无关的姓名。中文发现关键词自然纳入介绍：大学生大作业、学习象棋、人工智能训练模型、社区经验贡献、棋风模拟。

## 发布前由所有者确认

1. 所有者已确认有权公开现有源码与素材；参考材料不随包分发。
2. 已确认GitHub账号、仓库名称和公开可见性为 `Gnt191612/Chinese-Chess`。
3. 已授权并完成首次本地提交、创建远端并推送。
4. 安全问题的私密联系渠道；不要在公开Issue中接收凭据。

## 本地验证与上传内容

源码包包含 `.github/` 问题与贡献模板、源码、测试、文档、训练前置工具和工程文件。解压后的项目目录是仓库根目录；不要把外层ZIP或 `build/`、`x64/` 一并上传。`SOURCE_SHA256.txt` 为包内容的校验清单，不代表签名或安全认证。

按README安装VS2022的C++桌面开发组件与EasyX，构建Release x64。运行 `tests/build_tests.bat` 后至少执行逻辑、规则审计、评估、快捷键、展示与缓存测试；展示测试会创建短暂图形窗口，应在有桌面的环境运行。训练辅助工具可独立用Python测试，不要求教师已下载。

当前未配置Windows图形CI：EasyX依赖的获取与图形测试桌面需求尚未自动化，不提供未经验证的绿色CI标记。问题、经验贡献及PR模板已随源码上传。

## 实际发布记录

2026年10月4日创建公开仓库，首次源码提交为 `3535912`，已推送至 `main` 并设置20个Topics。尚未创建正式Release或发布EXE、模型与个人经验库，也未上传学校服务器。

## 发布说明草稿

这是早期源码候选，不宣称达到顶级引擎棋力，也未完成教师训练。“人情味”目前体现于常见开局、近似候选变化、限时响应与对局交互，真人先验训练仍是后续计划。完整赛事长捉规则、系统棋力评测等边界见README与规则说明。

版本亮点：玩家执红；AI约9秒搜索预算；168条开局候选记录；无限逐轮悔棋；长将判负；本次／上次两盘切换；传统定位纹；平稳走子；半透明单字战术徽章及音效。
