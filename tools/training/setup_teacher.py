"""配置固定版本离线教师；不执行局面搜索或批量训练。"""
import argparse
import hashlib
import json
import subprocess
import urllib.request
import zipfile
from pathlib import Path

TAG = 'Pikafish-2026-09-06'
COMMIT = '4c17cee11f888ae1d48a9494f2e2239f019f0a1f'
ARCHIVE_URL = f'https://github.com/official-pikafish/Pikafish/releases/download/{TAG}/Pikafish.2026-09-06.7z'
ARCHIVE_SHA = '41952bbfe2520faceb5902c69e6ab4845cc999841d2b49a95cc1be7867a25e5b'
FILES = {
    'Pikafish-Windows-x86-64-universal.exe': '0d57d64c212e78f96cba9e5e4f140d8872f8faa42ea631a27ab6c5db08509da3',
    'pikafish.nnue': '7d13d73569a9b571ba0eb20cf1596247bc2a42738967e61afef6482b231e900e',
    'Pikafish-source.zip': '10cbb2bbcfd2c511703f8a5145b5e871d985fdc41a123051ee8df0312036b7ef',
}


def digest(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def verify(folder):
    for name, expected in FILES.items():
        if digest(folder / name) != expected:
            raise ValueError(f'教师文件校验失败：{name}')
    for name in ('Copying.txt', 'NNUE-License.md', 'README.md', 'AUTHORS'):
        if not (folder / name).is_file():
            raise ValueError(f'缺少第三方许可资料：{name}')
    with zipfile.ZipFile(folder / 'Pikafish-source.zip') as archive:
        if archive.testzip() is not None:
            raise ValueError('教师源码压缩包损坏')


def download(url, target, expected):
    temporary = target.with_suffix(target.suffix + '.partial')
    request = urllib.request.Request(url, headers={'User-Agent': 'Chinese-Chess-training-setup'})
    with urllib.request.urlopen(request, timeout=120) as response, temporary.open('xb') as output:
        while chunk := response.read(1024 * 1024):
            output.write(chunk)
    if digest(temporary) != expected:
        raise ValueError('下载校验失败，保留partial文件供诊断，不执行它')
    temporary.rename(target)


def prepare(repo, check_only=False):
    folder = repo / 'training/engines/pikafish-2026-09-06'
    config_path = repo / 'training/config/local.json'
    if check_only:
        verify(folder)
        config = json.loads(config_path.read_text(encoding='utf-8'))
        if Path(config['engine_path']).resolve() != (folder / 'Pikafish-Windows-x86-64-universal.exe').resolve():
            raise ValueError('配置不是本安装器固定的教师，请单独核验自定义配置')
        if Path(config['extra_options']['EvalFile']).resolve() != (folder / 'pikafish.nnue').resolve():
            raise ValueError('权重路径不匹配')
        return folder
    print('教师引擎为GPLv3；官方NNUE权重仅限合法用途，未经许可不得商业使用。')
    print('完整条款：https://github.com/official-pikafish/Networks#nnue-license')
    if input('阅读并同意条款后输入 YES；其他输入退出：') != 'YES':
        raise ValueError('未同意教师条款，未启用教师')
    if config_path.exists():
        raise ValueError('已有local.json，不覆盖开发者配置；请备份后自行选择是否重新配置')
    if not folder.exists():
        folder.mkdir(parents=True)
        archive = folder / 'Pikafish.2026-09-06.7z'
        download(ARCHIVE_URL, archive, ARCHIVE_SHA)
        subprocess.run(['tar.exe', '-xf', str(archive), '-C', str(folder),
                        'Pikafish-Windows-x86-64-universal.exe', 'pikafish.nnue',
                        'Copying.txt', 'NNUE-License.md', 'README.md', 'AUTHORS'], check=True)
        download(f'https://api.github.com/repos/official-pikafish/Pikafish/zipball/{COMMIT}',
                 folder / 'Pikafish-source.zip', FILES['Pikafish-source.zip'])
    verify(folder)
    print((folder / 'NNUE-License.md').read_text(encoding='utf-8'))
    config = {'teacher_name': TAG, 'engine_path': str(folder / 'Pikafish-Windows-x86-64-universal.exe'),
              'engine_args': [], 'threads': 1, 'hash_mb': 256, 'multipv': 3,
              'movetime_ms': 300, 'position_timeout_ms': 5000,
              'extra_options': {'EvalFile': str(folder / 'pikafish.nnue')}}
    config_path.write_text(json.dumps(config, ensure_ascii=False, indent=2), encoding='utf-8')
    provenance = {'teacher_tag': TAG, 'teacher_commit': COMMIT, 'archive_url': ARCHIVE_URL,
                  'archive_sha256': ARCHIVE_SHA, 'files_sha256': FILES,
                  'license': '引擎GPLv3；权重独立条款，不声明商业可用', 'training_started': False}
    (folder / 'provenance.json').write_text(json.dumps(provenance, ensure_ascii=False, indent=2), encoding='utf-8')
    return folder


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--repo', type=Path, default=Path(__file__).resolve().parents[2])
    parser.add_argument('--check-only', action='store_true')
    arguments = parser.parse_args()
    prepare(arguments.repo.resolve(), arguments.check_only)
    print('教师文件和配置校验完成；未启动训练。')
