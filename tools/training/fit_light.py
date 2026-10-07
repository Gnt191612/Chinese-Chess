"""按整盘划分，拟合共享XQEV1特征对手工评估的有界修正。"""
import argparse
import json
import math
from pathlib import Path


def solve(matrix, target):
    augmented = [row[:] + [value] for row, value in zip(matrix, target)]
    for column in range(10):
        pivot = max(range(column, 10), key=lambda row: abs(augmented[row][column]))
        augmented[column], augmented[pivot] = augmented[pivot], augmented[column]
        scale = augmented[column][column]
        if abs(scale) < 1e-12:
            raise ValueError('拟合矩阵不可解')
        augmented[column] = [value / scale for value in augmented[column]]
        for row in range(10):
            if row != column:
                factor = augmented[row][column]
                augmented[row] = [a - factor * b for a, b in zip(augmented[row], augmented[column])]
    return [max(-50, min(50, row[-1])) for row in augmented]


def records(path):
    games, ids = {}, set()
    with path.open(encoding='utf-8') as source:
        for line in source:
            row = json.loads(line)
            if row['id'] in ids:
                raise ValueError('标签ID重复')
            ids.add(row['id'])
            split = row['split']
            if split not in ('train', 'validation', 'test'):
                raise ValueError('缺少合法数据划分')
            if games.setdefault(row['game_id'], split) != split:
                raise ValueError('同一盘棋发生训练/验证泄漏')
            if row.get('feature_schema') != 'XQEV1':
                raise ValueError('特征版本不匹配')
            candidates = row.get('candidates', [])
            best = next((candidate for candidate in candidates if candidate.get('multipv') == 1), None)
            if row.get('status') != 'ok' or not best or best.get('score_type') != 'cp' or best.get('bound') != 'exact':
                continue
            features = row['features']
            if len(features) != 10 or not all(isinstance(value, (float, int)) and math.isfinite(value) for value in features):
                raise ValueError('特征无效')
            base = row['base_score_red']
            teacher = best['score'] * (1 if row['fen'].split()[1] == 'w' else -1)
            if not math.isfinite(base) or not math.isfinite(teacher) or abs(teacher) > 2000:
                continue
            yield split, features, max(-800, min(800, teacher - base))


def fit(source, output, ridge=10):
    if output.exists():
        raise ValueError('输出目录已存在')
    matrix = [[ridge if i == j else 0.0 for j in range(10)] for i in range(10)]
    target, count = [0.0] * 10, 0
    for split, features, residual in records(source):
        if split != 'train':
            continue
        count += 1
        for i in range(10):
            target[i] += features[i] * residual
            for j in range(10):
                matrix[i][j] += features[i] * features[j]
    if not count:
        raise ValueError('没有可拟合的训练局面')
    weights = solve(matrix, target)
    stats = {split: {'count': 0, 'before_squared': 0.0, 'after_squared': 0.0} for split in ('train', 'validation', 'test')}
    for split, features, residual in records(source):
        correction = sum(a * b for a, b in zip(weights, features))
        stats[split]['count'] += 1
        stats[split]['before_squared'] += residual ** 2
        stats[split]['after_squared'] += (residual - correction) ** 2
    for value in stats.values():
        n = value['count']
        before = value.pop('before_squared')
        after = value.pop('after_squared')
        value['before_rmse'] = math.sqrt(before / n) if n else None
        value['after_rmse'] = math.sqrt(after / n) if n else None
    output.mkdir(parents=True)
    (output / 'evaluation.xqweights').write_text('XQEV1\n' + ' '.join(f'{weight:.12g}' for weight in weights) + '\n', encoding='ascii')
    (output / 'report.json').write_text(json.dumps({'schema': 'XQEV1', 'weights': weights, 'metrics': stats,
                                                   'commercial_use': '未审查', 'strength_claim': '需另行实战对照验证'},
                                                  ensure_ascii=False, indent=2), encoding='utf-8')
    return stats


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--input', required=True, type=Path)
    parser.add_argument('--output', required=True, type=Path)
    args = parser.parse_args()
    print(json.dumps(fit(args.input, args.output), ensure_ascii=False))
