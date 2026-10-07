"""验证配置助手不会绕过许可、覆盖配置或解压不安全路径。"""
import hashlib
import sys
import tempfile
import unittest
import zipfile
from pathlib import Path
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools/training'))
import setup_teacher
import prepare_dataset


class TrainingSetupTests(unittest.TestCase):
    def test_decline_and_existing_config(self):
        with tempfile.TemporaryDirectory() as directory:
            repo = Path(directory)
            with patch('builtins.input', return_value='NO'):
                with self.assertRaises(ValueError):
                    setup_teacher.prepare(repo)
            self.assertEqual(list(repo.iterdir()), [])
            config = repo / 'training/config/local.json'
            config.parent.mkdir(parents=True)
            config.write_text('私人配置', encoding='utf-8')
            with patch('builtins.input', return_value='YES'):
                with self.assertRaises(ValueError):
                    setup_teacher.prepare(repo)
            self.assertEqual(config.read_text(encoding='utf-8'), '私人配置')

    def test_corrupt_teacher(self):
        with tempfile.TemporaryDirectory() as directory:
            folder = Path(directory)
            (folder / next(iter(setup_teacher.FILES))).write_bytes('错误文件'.encode('utf-8'))
            with self.assertRaises(ValueError):
                setup_teacher.verify(folder)

    def test_zip_traversal(self):
        with tempfile.TemporaryDirectory() as directory:
            repo = Path(directory)
            archive = repo / 'training/engines/pikafish-2026-09-06/CCPD-source.zip'
            archive.parent.mkdir(parents=True)
            with zipfile.ZipFile(archive, 'w') as output:
                output.writestr('root/../../escape.pgn', '恶意路径')
            archive_hash = hashlib.sha256(archive.read_bytes()).hexdigest()
            with patch.object(prepare_dataset, 'SHA', archive_hash):
                with self.assertRaises(ValueError):
                    prepare_dataset.prepare(repo, full=True)
            self.assertFalse((repo / 'training/escape.pgn').exists())


if __name__ == '__main__':
    unittest.main()
