# 离线训练工作区

该目录只负责生成可追溯的教师标签和保存训练产物，不参与桌面程序运行。大文件默认被Git忽略。

第一版已经选定的数据与教师组合见 [训练来源与教师选型](../docs/TRAINING_SOURCES.md)。

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

- CCPD原始格式到统一JSONL的转换器：内部字段已经冻结，仍需在首次取得数据后针对实际棋谱编码实现并验证。
- ElephantArt、ElephantEye等非统一协议适配器。
- 参数拟合程序：需要先确定最终评估特征和模型文件格式。
- 教师引擎、权重、数据集和最终模型。

这些项目必须在来源、版本和许可证确定后实现，避免围绕错误格式提前编写不可验证的转换代码。
