# 社区经验批次登记

`contributors/<公开标识>/<批次编号>/manifest.json`：审核后获准公开的批次元数据。先复制模板再填写，不提交占位模板作为真实投稿。

`releases/<库版本>/manifest.json`：正式库版本的输入批次、文件校验值和测试结果，尚无正式社区库版本。

`announcements/`：批次撤销与库修正公告。

`incoming/`、`quarantine/`、`generated/`：仅维护者本地使用，Git忽略，不上传原始投稿、污染文件或聚合产物。

贡献入口为“经验库贡献”Issue模板。审核、授权与撤销规则见 `docs/EXPERIENCE_CONTRIBUTIONS.md`。目录模板不自动执行审核或合并。
