import math
import os
import sys
import unittest
import urllib.parse
import nodel as nd

import logging 
logging.basicConfig(level=logging.DEBUG, stream=sys.stdout)


class TestModule(unittest.TestCase):
    def test_bind(self):
        path = urllib.parse.quote(os.path.dirname(__file__))
        wd = nd.bind(f'file://?path={path}')
        self.assertTrue('test_module.py' in wd)
        
    def test_url_path_conflict(self):
        self.assertRaises(RuntimeError, nd.bind, f'file:///?path=.')  

    def test_from_json(self):
        d = nd.from_json('{"drink": "tea", "optional_drink": null, "cups": 2, "grams": 3.5}')
        self.assertEqual(d.drink, 'tea')
        self.assertIsNone(d.optional_drink)
        self.assertEqual(d.cups, 2)
        self.assertEqual(d.grams, 3.5)
        
    def test_clear_map(self):
        d = nd.from_json('{"drink": "tea"}')
        nd.clear(d)
        self.assertEqual(len(d), 0)

    def test_clear_list(self):
        l = nd.Object(['tea', 'tisane'])
        nd.clear(l)
        self.assertEqual(len(l), 0)
    
    def test_clear_string(self):
        s = nd.Object('tea')
        nd.clear(s)
        self.assertEqual(s, '')

    def test_clear_wrong_type(self):
        self.assertRaises(RuntimeError, nd.clear, nd.Object(5))
    
    def test_root(self):
        d = nd.from_json("{'x': {'y': {'z': 'yep, tea'}}}")
        self.assertEqual(nd.root(d.x.y.z).x.y.z, 'yep, tea')
        
    def test_parent(self):
        d = nd.from_json("{'x': [{'u': 'oops'}, {'u': 'doh'}]}")
        self.assertTrue(nd.is_same(nd.parent(d.x), d))
        self.assertTrue(nd.is_same(nd.parent(d.x[1]), d.x))
        self.assertTrue(nd.is_same(nd.parent(d.x[1].u), d.x[1]))
        
    def test_is_same(self):
        d = nd.from_json("{'drink': 'tea'}")
        self.assertTrue(nd.is_same(nd.root(d.drink), d))
        self.assertIsNot(nd.root(d.drink), d)  # NOTE this

    def test_list_iter_keys(self):
        l = nd.from_json("[1, 2]")
        self.assertEqual(list(nd.iter_keys(l)), [0, 1])
        
    def test_list_iter_values(self):
        l = nd.from_json("[1, 2]")
        self.assertEqual(list(nd.iter_values(l)), [1, 2])
        
    def test_list_iter_items(self):
        l = nd.from_json("[1, 2]")
        self.assertEqual(list(nd.iter_items(l)), [(0, 1), (1, 2)])

    def test_map_iter_keys(self):
        d = nd.from_json("{'x': 1, 'y': 2}")
        self.assertEqual(list(nd.iter_keys(d)), ['x', 'y'])
        
    def test_map_iter_values(self):
        d = nd.from_json("{'x': 1, 'y': 2}")
        self.assertEqual(list(nd.iter_values(d)), [1, 2])
        
    def test_map_iter_items(self):
        d = nd.from_json("{'x': 1, 'y': 2}")
        self.assertEqual(list(nd.iter_items(d)), [('x', 1), ('y', 2)])

    def test_map_iter_tree(self):
        d = nd.from_json("{'x': [{'u': 1}, {'u': 2}], 'y': [{'u': 3}, {'u': 4}]}")
        expect = [d, d.x, d.y, d.x[0], d.x[1], d.y[0], d.y[1], d.x[0].u, d.x[1].u, d.y[0].u, d.y[1].u]
        for actual, result in zip(nd.iter_tree(d), expect):
            self.assertTrue(nd.is_same(actual, result))
        
    def test_key(self):
        d = nd.from_json("{'x': [1, 2, 3]}")
        self.assertEqual(nd.key(d.x), 'x')
        
    def test_reset(self):
        path = urllib.parse.quote(os.path.dirname(__file__))
        wd = nd.bind(f'file://?path={path}&perm=rw')
        self.assertTrue('test_module.py' in wd)
        nd.clear(wd)
        self.assertTrue('test_module.py' not in wd)
        nd.reset(wd)
        self.assertTrue('test_module.py' in wd)
        
        
if __name__ == '__main__':
    unittest.main()
