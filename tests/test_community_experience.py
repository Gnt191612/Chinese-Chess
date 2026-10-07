"""社区批次隔离与重建测试，所有数据均为临时构造记录。"""
import json
import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools'))
import community_experience as book


class CommunityTests(unittest.TestCase):
    def test_review_rebuild_revoke(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            source = root / 'sample.dat'
            source.write_bytes(book.HEADER.pack(book.MAGIC, 1) +
                               book.ENTRY.pack(42, 1, 2, 1, 3, 1, 0) +
                               book.ENTRY.pack(42, 1, 2, 1, 3, 2, 1))
            book.register(root, source, 'tester', 'batch1', '测试来源', '0.1', '测试许可')
            book.rebuild(root, root / 'pending')
            self.assertEqual(book.read_book(root / 'pending/community-experience.dat'), {})
            with self.assertRaises(ValueError):
                book.register(root, source, 'tester2', 'batch2', '测试', '0.1', '测试')
            book.review(root, 'tester', 'batch1', 'accepted', '人工审核测试')
            book.rebuild(root, root / 'accepted')
            self.assertEqual(list(book.read_book(root / 'accepted/community-experience.dat').values()), [(2, 1)])
            book.revoke(root, 'tester', 'batch1', '测试撤销')
            book.rebuild(root, root / 'revoked')
            self.assertEqual(book.read_book(root / 'revoked/community-experience.dat'), {})
            self.assertTrue((root / 'announcements/tester-batch1-revoked.md').exists())
            self.assertTrue((root / 'quarantine/tester/batch1/records.json').exists())

    def test_corrupt_and_invalid(self):
        with tempfile.TemporaryDirectory() as temporary:
            source = Path(temporary) / 'bad.dat'
            for data in (b'bad', book.HEADER.pack(book.MAGIC, 1) + b'x',
                         book.HEADER.pack(book.MAGIC, 1) + book.ENTRY.pack(1, 9, 1, 0, 1, 1, 0),
                         book.HEADER.pack(book.MAGIC, 1) + book.ENTRY.pack(1, 0, 1, 0, 1, 1, 0)):
                source.write_bytes(data)
                with self.assertRaises(ValueError):
                    book.read_book(source)
            with self.assertRaises(ValueError):
                book.identifier('../越界')

    def test_reviewed_file_tampering(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            source = root / 'sample.dat'
            source.write_bytes(book.HEADER.pack(book.MAGIC, 1) + book.ENTRY.pack(1, 0, 1, 0, 2, 1, 0))
            book.register(root, source, 'tester', 'batch1', '测试', '0.1', '测试')
            book.review(root, 'tester', 'batch1', 'accepted', '测试')
            (root / 'contributors/tester/batch1/records.json').write_text('[]', encoding='utf-8')
            with self.assertRaises(ValueError):
                book.rebuild(root, root / 'tampered')


if __name__ == '__main__':
    unittest.main()
