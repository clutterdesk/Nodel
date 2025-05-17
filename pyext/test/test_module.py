import math
import os
import shutil
import sys
import unittest
import urllib.parse
import nodel as nd

import logging 
logging.basicConfig(level=logging.DEBUG, stream=sys.stdout)


class TestModule(unittest.TestCase):
    def test_from_json(self):
        obj = nd.from_json('{"drink": "tea", "optional_drink": null, "cups": 2, "grams": 3.5}')
        self.assertEqual(obj.drink, 'tea')
        self.assertIsNone(obj.optional_drink)
        self.assertEqual(obj.cups, 2)
        self.assertEqual(obj.grams, 3.5)
        
    def test_clear_map(self):
        obj = nd.from_json('{"drink": "tea"}')
        nd.clear(obj)
        self.assertEqual(len(obj), 0)

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
        obj = nd.from_json("{'x': {'y': {'z': 'yep, tea'}}}")
        self.assertEqual(nd.root(obj.x.y.z).x.y.z, 'yep, tea')
        
    def test_parent(self):
        obj = nd.from_json("{'x': [{'u': 'oops'}, {'u': 'doh'}]}")
        self.assertTrue(nd.is_same(nd.parent(obj.x), obj))
        self.assertTrue(nd.is_same(nd.parent(obj.x[1]), obj.x))
        self.assertTrue(nd.is_same(nd.parent(obj.x[1].u), obj.x[1]))
        
    def test_is_same(self):
        obj = nd.from_json("{'drink': 'tea'}")
        self.assertTrue(nd.is_same(nd.root(obj.drink), obj))
        self.assertIsNot(nd.root(obj.drink), obj)  # NOTE this

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
        obj = nd.from_json("{'x': 1, 'y': 2}")
        self.assertEqual(list(nd.iter_keys(obj)), ['x', 'y'])
        
    def test_map_iter_values(self):
        obj = nd.from_json("{'x': 1, 'y': 2}")
        self.assertEqual(list(nd.iter_values(obj)), [1, 2])
        
    def test_map_iter_items(self):
        obj = nd.from_json("{'x': 1, 'y': 2}")
        self.assertEqual(list(nd.iter_items(obj)), [('x', 1), ('y', 2)])

    def test_map_iter_tree(self):
        obj = nd.from_json("{'x': [{'u': 'a'}, {'u': 'b'}], 'y': [{'u': 'c'}, {'u': 'd'}]}")
        expect = [obj, obj.x, obj.y, obj.x[0], obj.x[1], obj.y[0], obj.y[1], obj.x[0].u, obj.x[1].u, obj.y[0].u, obj.y[1].u]
        for actual, result in zip(nd.iter_tree(obj), expect):
            self.assertTrue(nd.is_same(actual, result))
        
    def test_key(self):
        obj = nd.from_json("{'x': [1, 2, 3]}")
        self.assertEqual(nd.key(obj.x), 'x')
        
    def test_get(self):
        obj = nd.Object({'x': 'X'})
        self.assertTrue(nd.is_same(nd.get(obj, 'x'), obj.x))
        self.assertTrue(nd.is_same(nd.get(obj, 'x', 'Y'), obj.x))
        self.assertEqual(nd.get(obj, 'z', 'Z'), 'Z')
        obj = nd.Object(['X'])
        self.assertTrue(nd.is_same(nd.get(obj, 0), obj[0]))
        self.assertTrue(nd.is_same(nd.get(obj, 0, 'Y'), obj[0]))
        self.assertEqual(nd.get(obj, 1, 'Z'), 'Z')
        
    def test_is_nil(self):
        obj = nd.Object(None)
        self.assertTrue(nd.is_nil(obj))
        
    def test_is_bool(self):
        obj = nd.Object(True)
        self.assertTrue(nd.is_bool(obj))
        
    def test_is_int(self):
        obj = nd.Object(sys.maxsize)
        self.assertTrue(nd.is_int(obj))

    def test_is_float(self):
        obj = nd.Object(2.18)
        self.assertTrue(nd.is_float(obj))
        
    def test_is_str(self):
        obj = nd.Object('tea')
        self.assertTrue(nd.is_str(obj))
        
    def test_is_list(self):
        obj = nd.Object([])
        self.assertTrue(nd.is_list(obj))
        
    def test_is_map(self):
        obj = nd.Object({})
        self.assertTrue(nd.is_map(obj))
        
    def test_is_native(self):
        obj = nd.Object((1, 2))
        self.assertTrue(nd.is_native(obj))

    def test_native(self):
        self.assertIs(type(nd.native(nd.Object('foo'))), str)
        self.assertEqual(nd.native(nd.Object('foo')), 'foo')
        self.assertIs(type(nd.native(nd.Object(42))), int)
        self.assertEqual(nd.native(nd.Object(42)), 42)

    def test_ref_count(self):
        obj = nd.Object([])
        self.assertEqual(nd.ref_count(obj), 1)

        
if __name__ == '__main__':
    unittest.main()
