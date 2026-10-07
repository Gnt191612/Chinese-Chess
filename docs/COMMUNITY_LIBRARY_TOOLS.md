# 社区经验管理工具

工具需要Python 3.11及以上。初始库为空，不纳入维护者个人经验。以下命令由维护者执行，玩家数据不自动上传。

```powershell
python tools/community_experience.py validate 投稿文件.dat
python tools/community_experience.py register 投稿文件.dat --contributor user1 --batch batch001 --issue "投稿Issue地址" --version "0.1.0-rc.1" --permission "投稿者明确许可声明"
python tools/community_experience.py review --contributor user1 --batch batch001 --status accepted --evidence "来源、许可及人工复核记录"
python tools/community_experience.py rebuild --output community-experience/generated/版本001
```

登记保留最后一条累计统计，不把追加日志重复累加；同一源文件拒绝重复登记。待审和拒绝批次不参与重建。审核后的规范记录可随源码公开，投稿原始文件不直接提交。已公开的累计快照不能再次作为新的独立对局来源，应由人工审核检查跨批次重叠；当前工具不能从哈希和计数证明对局来源。

出现问题：

```powershell
python tools/community_experience.py revoke --contributor user1 --batch batch001 --reason "可复现的问题依据"
python tools/community_experience.py rebuild --output community-experience/generated/版本002
```

撤销将规范记录移入Git忽略的quarantine目录，保留批次元数据，生成公告；有效库从剩余批次重新计算。不自动删除Git历史或他人副本，公告不无依据地指控恶意行为。新库分发仍由维护者发布，不自动更新玩家文件。

生成的 `community-experience.dat` 放在新版程序旁时只读加载，用于根节点排序；个人经验仍写 `experience.dat`，不会混入公共计数。旧版v0.1.0-rc.1尚不支持该独立公共文件，需要重新构建新源码。缺少公共文件时按原有个人经验运行，搜索和棋规不变。

结构校验检查格式、长度、坐标、计数、容量与累计倒退。它不验证合法棋局、真实胜负或排除所有恶意污染；接受批次必须另行核查许可、来源、程序版本及对局证据。
