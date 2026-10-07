"""社区经验登记、结构校验、审核及按有效批次重建。"""
import argparse
import hashlib
import json
import re
import struct
from pathlib import Path

HEADER = struct.Struct('<II')
ENTRY = struct.Struct('<QbbbbII')
MAGIC = 0x45585143
MAX_BYTES = 128 * 1024 * 1024
MAX_ENTRIES = 200000


def digest(path):
    with path.open('rb') as source:
        return hashlib.file_digest(source, 'sha256').hexdigest()


def read_book(path):
    if not 8 <= path.stat().st_size <= MAX_BYTES:
        raise ValueError('文件大小超出限制')
    records = {}
    with path.open('rb') as source:
        if HEADER.unpack(source.read(8)) != (MAGIC, 1):
            raise ValueError('经验文件格式或版本不支持')
        while data := source.read(ENTRY.size):
            if len(data) != ENTRY.size:
                raise ValueError('经验记录被截断')
            value = ENTRY.unpack(data)
            position, x, y, tx, ty, wins, losses = value
            if not (0 <= x < 9 and 0 <= tx < 9 and 0 <= y < 10 and 0 <= ty < 10):
                raise ValueError('着法坐标越界')
            if (x, y) == (tx, ty) or not 0 < wins + losses <= 1000000:
                raise ValueError('着法或统计计数异常')
            key = value[:5]
            previous = records.get(key)
            if previous and (wins < previous[0] or losses < previous[1]):
                raise ValueError('累计记录发生倒退')
            records[key] = (wins, losses)
            if len(records) > MAX_ENTRIES:
                raise ValueError('经验条数超出限制')
    return records


def save_json(path, value):
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(value, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')


def identifier(value):
    if not re.fullmatch(r'[a-zA-Z0-9][a-zA-Z0-9_-]{0,63}', value):
        raise ValueError('公开标识只能包含字母、数字、下划线和连字符')
    return value


def batch_path(root, contributor, batch):
    return root / 'contributors' / identifier(contributor) / identifier(batch)


def register(root, source, contributor, batch, issue, version, permission):
    if not all(value.strip() for value in (issue, version, permission)):
        raise ValueError('来源、版本和许可声明不能为空')
    records = read_book(source)
    source_hash = digest(source)
    for existing in root.glob('contributors/*/*/manifest.json'):
        if json.loads(existing.read_text(encoding='utf-8'))['source_sha256'] == source_hash:
            raise ValueError('同一文件已经登记，不能重复投稿')
    target = batch_path(root, contributor, batch)
    if target.exists():
        raise ValueError('批次已存在，不能覆盖')
    save_json(target / 'records.json', [list(key + counts) for key, counts in sorted(records.items())])
    save_json(target / 'manifest.json', {
        'schema_version': 1, 'contributor_public_id': contributor, 'batch_id': batch,
        'source_issue': issue, 'program_version': version, 'source_sha256': source_hash,
        'contribution_permission': permission, 'status': 'pending', 'review_evidence': [],
        'records_sha256': digest(target / 'records.json'), 'revocation': None,
    })


def review(root, contributor, batch, status, evidence):
    target = batch_path(root, contributor, batch)
    manifest = json.loads((target / 'manifest.json').read_text(encoding='utf-8'))
    if manifest['status'] != 'pending':
        raise ValueError('只允许审核待审批次')
    manifest['status'] = status
    manifest['review_evidence'] = [evidence]
    save_json(target / 'manifest.json', manifest)


def revoke(root, contributor, batch, reason):
    target = batch_path(root, contributor, batch)
    manifest = json.loads((target / 'manifest.json').read_text(encoding='utf-8'))
    if manifest['status'] != 'accepted':
        raise ValueError('只允许撤销已通过的批次')
    quarantine = root / 'quarantine' / contributor / batch
    quarantine.mkdir(parents=True, exist_ok=False)
    (target / 'records.json').rename(quarantine / 'records.json')
    manifest.update(status='revoked', revocation={'reason': reason})
    save_json(target / 'manifest.json', manifest)
    announcement = root / 'announcements' / f'{contributor}-{batch}-revoked.md'
    announcement.parent.mkdir(parents=True, exist_ok=True)
    announcement.write_text(
        f'# 经验批次撤销\n\n贡献者：{contributor}\n\n批次：{batch}\n\n依据：{reason}\n\n'
        '该批次已退出有效输入，维护者须重新构建并发布经验库；已下载的旧副本不会自动撤回。\n',
        encoding='utf-8')


def rebuild(root, output):
    if output.exists():
        raise ValueError('输出目录已存在，不能覆盖已有版本')
    merged, inputs, seen = {}, [], set()
    for path in sorted(root.glob('contributors/*/*/manifest.json')):
        metadata = json.loads(path.read_text(encoding='utf-8'))
        if metadata['status'] != 'accepted':
            continue
        if metadata['source_sha256'] in seen:
            raise ValueError('有效批次存在重复源文件')
        seen.add(metadata['source_sha256'])
        records_file = path.parent / 'records.json'
        if digest(records_file) != metadata['records_sha256']:
            raise ValueError('审核后的记录发生变化')
        for record in json.loads(records_file.read_text(encoding='utf-8')):
            key, counts = tuple(record[:5]), record[5:]
            previous = merged.get(key, (0, 0))
            total = tuple(a + b for a, b in zip(previous, counts))
            if max(total) > 0xffffffff:
                raise ValueError('聚合统计溢出')
            merged[key] = total
            if len(merged) > MAX_ENTRIES:
                raise ValueError('聚合库超出运行时容量')
        inputs.append({'manifest': path.relative_to(root).as_posix(),
                       'source_sha256': metadata['source_sha256'],
                       'records_sha256': metadata['records_sha256']})
    output.mkdir(parents=True)
    book = output / 'community-experience.dat'
    with book.open('wb') as destination:
        destination.write(HEADER.pack(MAGIC, 1))
        for key, counts in sorted(merged.items()):
            destination.write(ENTRY.pack(*(key + counts)))
    save_json(output / 'manifest.json', {'schema_version': 1, 'inputs': inputs,
                                         'entries': len(merged), 'sha256': digest(book)})


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--root', type=Path, default=Path('community-experience'))
    commands = parser.add_subparsers(dest='command', required=True)
    check = commands.add_parser('validate')
    check.add_argument('source', type=Path)
    register_parser = commands.add_parser('register')
    register_parser.add_argument('source', type=Path)
    for command in ('register', 'review', 'revoke'):
        sub = register_parser if command == 'register' else commands.add_parser(command)
        sub.add_argument('--contributor', required=True)
        sub.add_argument('--batch', required=True)
        if command == 'register':
            for field in ('issue', 'version', 'permission'):
                sub.add_argument('--' + field, required=True)
        elif command == 'review':
            sub.add_argument('--status', required=True, choices=['accepted', 'rejected'])
            sub.add_argument('--evidence', required=True)
        else:
            sub.add_argument('--reason', required=True)
    build = commands.add_parser('rebuild')
    build.add_argument('--output', required=True, type=Path)
    args = parser.parse_args()
    if args.command == 'validate':
        print(f'结构校验通过：{len(read_book(args.source))} 条；不代表真实棋谱合法性审核通过')
    elif args.command == 'register':
        register(args.root, args.source, args.contributor, args.batch, args.issue, args.version, args.permission)
    elif args.command == 'review':
        review(args.root, args.contributor, args.batch, args.status, args.evidence)
    elif args.command == 'revoke':
        revoke(args.root, args.contributor, args.batch, args.reason)
    else:
        rebuild(args.root, args.output)


if __name__ == '__main__':
    main()
