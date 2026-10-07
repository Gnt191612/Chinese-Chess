# 离线训练工作区

该目录只负责生成可追溯的教师标签和保存训练产物，不参与桌面程序运行。大文件默认被Git忽略。

第一版已经选定的数据与教师组合见 [训练来源与教师选型](../docs/TRAINING_SOURCES.md)。

开发者安装版dev.2附固定教师与数据档案；配置与批次运行命令见[训练快速开始](../docs/TRAINING_QUICKSTART.md)。安装不会自动训练。

## 输入格式

清洗后的局面使用UTF-8 JSONL，每行一个对象：

```json
{"id":"game000001-ply042","fen":"2bakab2/9/4c4/p1p1p1p1p/9/2P6/P3P1P1P/2N1C1N2/9/2BAKAB2 w - - 0 1","human_move":"h2e2","game_id":"game000001","ply":42,"split":"train"}
```

必填字段只有 `id` 和 `fen`。`id` 必须在完整数据集中唯一。`human_move` 应使用与教师引擎一致的坐标表示；无法确认时留空，不要猜测转换。

训练集、验证集和测试集必须按 `game_id` 整局划分，不能把同一盘棋的相邻局面随机分到不同集合。

## 输出格式

标注器为每个输入局面写入一行JSON，保留输入ID并记录：教师名称、配置摘要、耗时、最佳着、候选主变化和分数。将死分数与普通 `cp` 分数分开保存，后续不得直接混合平均。

## 本地小样本流程

```powershell
Copy-Item training/config/teacher.example.json training/config/local.json
# 编辑local.json中的引擎路径后：
python tools/training/preflight.py --repo . --config training/config/local.json
python tools/training/split_jsonl.py --input training/data/processed/positions.jsonl --output-dir training/data/processed/shards --shards 4
python tools/training/annotate_uci.py --config training/config/local.json --input training/data/processed/shards/part-00000.jsonl --output training/data/labels/part-00000.jsonl --resume
python tools/training/validate_labels.py --input training/data/labels/part-00000.jsonl
```

标注器当前面向支持UCI及 `MultiPV` 的教师引擎。其他协议或不支持 `MultiPV` 的引擎应先单独完成适配测试，不能只修改配置名称便假定兼容。

## 服务器流程

1. 先在计算节点完成1万局面基准，不在登录节点运行引擎。
2. 根据吞吐量决定分片数和并发数。
3. 复制示例配置为 `training/config/local.json`；该文件不会进入Git。
4. 修改 `training/slurm/annotate_array.sbatch` 中带 `TODO` 的资源和路径。
5. 使用任务数组提交，每个数组任务只写自己的输出分片。
6. 下载前运行 `validate_labels.py` 检查每个分片。

## 尚未包含

- CCPD包含变化分支、无法唯一解析的中文记谱及其他编码的覆盖：当前转换器支持UTF-8/Big5主线PGN，逐着调用本项目棋规校验；遇到歧义拒绝整盘。
- ElephantArt、ElephantEye等非统一协议适配器。
- Human Prior训练和系统棋力对照：当前只实现XQEV1十个特征对原有评估的有界线性修正，尚未进行教师正式训练或宣称棋力提高。
- MIT源码树不附教师、权重或原始数据；dev.2开发Release隔离附固定第三方档案。最终训练模型仍未发布。

这些项目必须在来源、版本和许可证确定后实现，避免围绕错误格式提前编写不可验证的转换代码。

## 已连接的轻量训练流程

```powershell
tools/training/build_probe.bat
python tools/training/convert_pgn.py --input training/data/raw/样本目录 --probe build/training-probe/PositionProbe.exe --output training/data/processed/positions.jsonl
python tools/training/annotate_uci.py --config training/config/local.json --input training/data/processed/positions.jsonl --output training/data/labels/labels.jsonl
python tools/training/fit_light.py --input training/data/labels/labels.jsonl --output training/models/试验001
```

转换按原文件SHA-256整盘划分train/validation/test并去重；输出基线红方评分和C++共享特征，标注器保留这些元数据。拟合仅使用主候选的精确cp分数，按轮到谁走转换为红方视角，跳过将死与界限分数；使用岭回归拟合残差，每个修正权重限制在±50。报告记录各集合RMSE，不将评分误差减少等同实战棋力提升。CP分数尺度需按固定教师版本验证，不支持把多教师原始评分直接混合。

将生成的 `evaluation.xqweights` 放在新版程序旁并重启，程序启动后首次评估加载；文件缺失或格式无效时使用原评估。十个特征依次为红黑士、象、马、车、炮、兵数量差，以及中路兵、三七路兵、兵推进格数、过河兵差。它只是传统评估的小幅修正，不是NNUE或棋风插件。激活修正后的搜索深度与棋力须另行对照测试。

已用CCPD一份Big5中局样本回放31个合法局面，尚未证明全数据集兼容；该样本不随源码分发。单元测试使用构造标签验证拟合与泄漏拒绝，不代表已完成真实教师训练。来源仍须按TRAINING_SOURCES固定提交、校验值与署名，正式模型发布须单独审查许可。
