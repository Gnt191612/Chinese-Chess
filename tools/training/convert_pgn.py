"""将CCPD中文着法PGN转换为局面；使用本项目原生棋规验证着法。"""
import argparse
import hashlib
import json
import re
import subprocess
from pathlib import Path

INITIAL = 'rnbakabnr/9/1c5c1/p1p1p1p1p/9/9/P1P1P1P1P/1C5C1/9/RNBAKABNR w - - 0 1'
NUMBERS = dict(zip('一二三四五六七八九', range(1, 10)))
NUMBERS.update({str(i): i for i in range(1, 10)})
NUMBERS.update(dict(zip('１２３４５６７８９', range(1, 10))))
NAMES = dict(zip('車俥车馬傌马炮砲象相士仕將将帥帅兵卒', 'rrrnnnccbb aakkkkpp'.replace(' ', '')))


class Probe:
    def __init__(self, path):
        self.process = subprocess.Popen([str(path)], stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                                        text=True, encoding='ascii')

    def query(self, fen):
        self.process.stdin.write(fen + '\n')
        self.process.stdin.flush()
        line = self.process.stdout.readline().strip()
        if not line or line == 'ERROR':
            raise ValueError('棋规工具拒绝局面')
        base, features, moves = line.split('|')
        return int(base), [float(value) for value in features.split(',') if value], moves.rstrip(',').split(',') if moves else []

    def close(self):
        self.process.stdin.close()
        self.process.wait(timeout=10)


def grid(fen):
    rows = []
    for rank in fen.split()[0].split('/'):
        row = []
        for piece in rank:
            row.extend(['.'] * int(piece) if piece.isdigit() else [piece])
        if len(row) != 9:
            raise ValueError('FEN行宽错误')
        rows.append(row)
    if len(rows) != 10:
        raise ValueError('FEN行数错误')
    return rows


def coordinates(move):
    return ord(move[0]) - 97, 9 - int(move[1]), ord(move[2]) - 97, 9 - int(move[3])


def apply(fen, move):
    rows = grid(fen)
    x, y, tx, ty = coordinates(move)
    rows[ty][tx], rows[y][x] = rows[y][x], '.'
    ranks = []
    for row in rows:
        text, empty = '', 0
        for piece in row + ['!']:
            if piece == '.':
                empty += 1
            else:
                if empty:
                    text += str(empty)
                empty = 0
                if piece != '!':
                    text += piece
        ranks.append(text)
    side = 'b' if fen.split()[1] == 'w' else 'w'
    return '/'.join(ranks) + f' {side} - - 0 1'


def matches(token, move, fen):
    if len(token) != 4 or token[2] not in '進进退平' or token[3] not in NUMBERS:
        return token == move
    rows = grid(fen)
    red = fen.split()[1] == 'w'
    x, y, tx, ty = coordinates(move)
    piece = rows[y][x].lower().replace('h', 'n').replace('e', 'b')
    if token[0] in '前後后中':
        if NAMES.get(token[1]) != piece:
            return False
        same = [rank for rank in range(10) if rows[rank][x] == rows[y][x]]
        same.sort(reverse=not red)
        selected = same[0] if token[0] == '前' else same[-1] if token[0] in '後后' else same[len(same) // 2]
        if len(same) < 2 or y != selected or (token[0] == '中' and len(same) != 3):
            return False
    elif NAMES.get(token[0]) != piece or NUMBERS.get(token[1]) != (9 - x if red else x + 1):
        return False
    action = token[2]
    destination = NUMBERS[token[3]]
    target_file = 9 - tx if red else tx + 1
    if action == '平':
        return ty == y and target_file == destination
    forward = (y - ty) if red else (ty - y)
    if (action in '進进' and forward <= 0) or (action == '退' and forward >= 0):
        return False
    return target_file == destination if piece in 'nba' else abs(ty - y) == destination


def convert(path, probe):
    raw = path.read_bytes()
    try:
        text = raw.decode('utf-8-sig')
    except UnicodeDecodeError:
        text = raw.decode('big5')
    tags = dict(re.findall(r'^\[(\w+) "(.*?)"\]\s*$', text, re.M))
    fen = tags.get('FEN', INITIAL)
    body = re.sub(r'^\[.*?\]\s*$', '', text, flags=re.M)
    body = re.sub(r'\{[^}]*\}|;[^\n]*', '', body)
    if '(' in body or ')' in body:
        raise ValueError('含变化分支，第一版拒绝而不猜测主线')
    body = re.sub(r'\b\d+\.+', '', body)
    tokens = [word for word in body.split() if word not in ('1-0', '0-1', '1/2-1/2', '*')]
    game_id = hashlib.sha256(raw).hexdigest()
    split_key = int(game_id[:8], 16) % 10
    split = 'test' if split_key == 0 else 'validation' if split_key == 1 else 'train'
    records = []
    for ply, token in enumerate(tokens):
        base, features, legal = probe.query(fen)
        matching = [move for move in legal if matches(token, move, fen)]
        if len(matching) != 1:
            raise ValueError(f'第{ply + 1}步“{token}”不存在唯一合法着法')
        move = matching[0]
        records.append({'id': f'{game_id}-{ply}', 'game_id': game_id, 'ply': ply,
                        'fen': fen, 'human_move': move, 'split': split,
                        'features': features, 'base_score_red': base,
                        'source_file': path.name, 'source_sha256': game_id,
                        'source_license': 'CC-BY-4.0', 'feature_schema': 'XQEV1'})
        fen = apply(fen, move)
    return records


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--input', type=Path, required=True)
    parser.add_argument('--probe', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    if args.output.exists():
        parser.error('输出已存在，拒绝覆盖')
    paths = sorted(args.input.rglob('*.pgn')) if args.input.is_dir() else [args.input]
    seen, count = set(), 0
    args.output.parent.mkdir(parents=True, exist_ok=True)
    temporary = args.output.with_suffix(args.output.suffix + '.tmp')
    probe = Probe(args.probe.resolve())
    try:
        with temporary.open('x', encoding='utf-8') as output:
            for path in paths:
                game = convert(path, probe)
                if game and game[0]['game_id'] not in seen:
                    for record in game:
                        output.write(json.dumps(record, ensure_ascii=False) + '\n')
                        count += 1
                    seen.add(game[0]['game_id'])
    finally:
        probe.close()
    temporary.rename(args.output)
    print(f'转换完成：{len(seen)}盘，{count}个合法局面')


if __name__ == '__main__':
    main()
