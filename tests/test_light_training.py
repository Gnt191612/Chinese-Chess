"""轻量拟合和中文棋谱解析单元测试。"""
import json
import sys
import tempfile
import unittest
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools/training'))
from convert_pgn import INITIAL, matches, apply
from fit_light import fit


class LightTrainingTests(unittest.TestCase):
    def test_chinese_move_and_turn(self):
        self.assertTrue(matches('炮二平五', 'h2e2', INITIAL))
        self.assertFalse(matches('炮二平五', 'b2e2', INITIAL))
        self.assertEqual(apply(INITIAL, 'h2e2').split()[1], 'b')

    def test_fit_and_game_split(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            source = root / 'labels.jsonl'
            rows = []
            for i in range(10):
                features = [int(j == i) for j in range(10)]
                rows.append({'id': str(i), 'game_id': str(i), 'split': 'train', 'feature_schema': 'XQEV1',
                             'features': features, 'base_score_red': 0, 'fen': INITIAL, 'status': 'ok',
                             'candidates': [{'multipv': 1, 'score_type': 'cp', 'bound': 'exact', 'score': 22}]})
            source.write_text('\n'.join(json.dumps(row) for row in rows), encoding='utf-8')
            result = fit(source, root / 'model')
            self.assertLess(result['train']['after_rmse'], result['train']['before_rmse'])
            self.assertTrue((root / 'model/evaluation.xqweights').read_text().startswith('XQEV1'))
            row = dict(rows[0], id='extra', split='test')
            with source.open('a', encoding='utf-8') as destination:
                destination.write('\n' + json.dumps(row))
            with self.assertRaises(ValueError):
                fit(source, root / 'leak')


if __name__ == '__main__':
    unittest.main()
