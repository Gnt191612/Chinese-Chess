# 参与贡献

感谢关注“中国象棋”。提交修改前，请先确认改动范围清晰，并完成与改动相匹配的测试。

## 开发要求

- 桌面程序使用 Visual Studio 2022、MSVC v143 和 EasyX。
- 核心逻辑测试位于 `tests/`。
- 训练辅助工具使用 Python 3.9 及以上版本，仅依赖标准库。
- 不要把教师引擎、网络权重、原始棋谱、训练标签、模型产物或未经审查的个人经验库提交到源码主分支。
- 欢迎自愿贡献经验库：通过“经验库贡献”Issue附件提交候选ZIP并说明使用许可，流程见 [经验贡献说明](docs/EXPERIENCE_CONTRIBUTIONS.md)。这与禁止误提交运行数据不冲突；投稿不会自动合并或直接影响用户对局。
- 不要提交学校账号、服务器地址、SSH密钥、Slurm账户名或绝对路径。

## 提交前检查

1. 运行 `tests/build_tests.bat`。
2. 运行 `tests/LogicTests.exe`。
3. 需要验证时间限制时，再运行 `tests/TimingTest.exe`。
4. 运行 `python tools/training/test_tools.py`。
5. 运行 `python tools/training/preflight.py --repo .`。
6. 查看 `git status --short`，确认没有生成文件或敏感信息。

涉及棋力的修改还应在相同硬件、相同思考时间和相同测试局面下比较，不能只凭少量对局判断提升。
