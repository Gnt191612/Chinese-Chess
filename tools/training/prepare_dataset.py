"""核验固定CCPD压缩包，默认只提取已验证的流程样本，不启动训练。"""
import argparse
import json
import shutil
import zipfile
from pathlib import Path
from setup_teacher import digest, download

COMMIT = '368a47a947773dd8692c026e286dd19b6277b993'
SHA = '1502f88361e507fae834cc36c0cfd3c1b15bcfe09eb55227bddf5b7f768357b3'
URL = f'https://api.github.com/repos/Yvonne761/Chinese-Chess-Practical-Dataset/zipball/{COMMIT}'
SAMPLE = 'c1303638f6aeacdbae548d3f45e28f9fa317aaa45d7f2224bf66098ccfb29784'


def prepare(repo, full=False):
    archive_path = repo / 'training/engines/pikafish-2026-09-06/CCPD-source.zip'
    if not archive_path.exists():
        archive_path.parent.mkdir(parents=True, exist_ok=True)
        download(URL, archive_path, SHA)
    if digest(archive_path) != SHA:
        raise ValueError('CCPD压缩包校验失败')
    output = repo / 'training/data/raw' / ('ccpd-full' if full else 'ccpd-smoke')
    if output.exists():
        raise ValueError('数据目录已存在，不覆盖；自行检查并复用已有目录')
    with zipfile.ZipFile(archive_path) as archive:
        if archive.testzip() is not None:
            raise ValueError('CCPD压缩包完整性失败')
        output.mkdir(parents=True)
        members = archive.infolist()
        for member in members:
            relative = Path(*Path(member.filename).parts[1:])
            if relative.is_absolute() or '..' in relative.parts or ':' in str(relative):
                raise ValueError('拒绝不安全的压缩包路径')
            selected = full or str(relative).replace('\\', '/') in ('README.md', 'LICENSE', 'Dataset/中局/00000001.pgn')
            if selected and not member.is_dir():
                destination = output / relative
                destination.parent.mkdir(parents=True, exist_ok=True)
                with archive.open(member) as source, destination.open('xb') as target:
                    shutil.copyfileobj(source, target)
    if digest(output / 'Dataset/中局/00000001.pgn') != SAMPLE:
        raise ValueError('样本校验失败')
    provenance = {'dataset': 'Chinese Chess Practical Dataset (CCPD)', 'authors': 'Yu-Han Tseng and Bo-Nian Chen (2026)',
                  'source': URL, 'commit': COMMIT, 'archive_sha256': SHA,
                  'license': 'CC-BY-4.0 https://creativecommons.org/licenses/by/4.0/',
                  'changes': '未改动棋谱；默认仅提取一个中局样本与署名许可', 'full_archive_extracted': full,
                  'scope': '样本仅作价值训练流程检查，不是Human Prior，也不代表棋力提升'}
    (output / 'provenance.json').write_text(json.dumps(provenance, ensure_ascii=False, indent=2), encoding='utf-8')
    print(f'数据准备完成：{output}；未启动训练。')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--repo', type=Path, default=Path(__file__).resolve().parents[2])
    parser.add_argument('--full', action='store_true', help='提取全量原始档案；不代表全部可训练，需自行筛选真人大师完整对局')
    args = parser.parse_args()
    prepare(args.repo.resolve(), args.full)
