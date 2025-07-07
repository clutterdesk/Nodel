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
        l = nd.new(['tea', 'tisane'])
        nd.clear(l)
        self.assertEqual(len(l), 0)
    
    def test_clear_string(self):
        s = nd.new('tea')
        nd.clear(s)
        self.assertEqual(s, '')

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
        obj = nd.new({'x': 'X'})
        self.assertTrue(nd.is_same(nd.get(obj, 'x'), obj.x))
        self.assertTrue(nd.is_same(nd.get(obj, 'x', 'Y'), obj.x))
        self.assertEqual(nd.get(obj, 'z', 'Z'), 'Z')
        obj = nd.new(['X'])
        self.assertTrue(nd.is_same(nd.get(obj, 0), obj[0]))
        self.assertTrue(nd.is_same(nd.get(obj, 0, 'Y'), obj[0]))
        self.assertEqual(nd.get(obj, 1, 'Z'), 'Z')
        
    def test_is_str(self):
        obj = nd.new('tea')
        self.assertTrue(nd.is_str(obj))
        
    def test_is_list(self):
        obj = nd.new([])
        self.assertTrue(nd.is_list(obj))
        
    def test_is_map(self):
        obj = nd.new({})
        self.assertTrue(nd.is_map(obj))
        
    def test_is_str_like(self):
        self.assertTrue(nd.is_str_like(nd.new('tea')))
        self.assertTrue(nd.is_str_like('tea'))
        self.assertFalse(nd.is_str_like(nd.new([])))
        self.assertFalse(nd.is_str_like(1))

    def test_is_list_like(self):
        self.assertTrue(nd.is_list_like(nd.new([1])))
        self.assertTrue(nd.is_list_like([1]))
        self.assertFalse(nd.is_list_like(nd.new('no')))
        self.assertFalse(nd.is_list_like('no'))

    def test_is_map_like(self):
        self.assertTrue(nd.is_map_like(nd.new({})))
        self.assertTrue(nd.is_map_like({}))
        self.assertFalse(nd.is_map_like(nd.new('no')))
        self.assertFalse(nd.is_map_like('no'))
        
    def test_is_native(self):
        obj = nd.new((1, 2))
        self.assertTrue(nd.is_alien(obj))

    def test_native(self):
        self.assertIs(type(nd.native(nd.new('foo'))), str)
        self.assertEqual(nd.native(nd.new('foo')), 'foo')

    def test_ref_count(self):
        obj = nd.new([])
        self.assertEqual(nd.ref_count(obj), 1)

    def test_complete_map_with_object(self):
        root = nd.new({})
        b = nd.complete(root, ['a', 'b'], {'c': 'C'})
        self.assertEqual(b.c, 'C')
        self.assertEqual(nd.path(b.c, 'str'), 'a.b.c')

    def test_complete_map_with_type(self):
        root = nd.new({})
        b = nd.complete(root, ['a', 'b'], dict)
        self.assertTrue(nd.is_map(b))

    def test_complete_map_existing(self):
        root = nd.new({'a': {'b': 'pass'}})
        b = nd.complete(root, ['a', 'b'], 'fail')
        self.assertEqual(b, 'pass')

    def test_complete_list_with_object(self):
        root = nd.new([])
        b = nd.complete(root, [0, 0], {'c': 'C'})
        self.assertEqual(b.c, 'C')
        self.assertEqual(nd.path(b.c, 'str'), '[0][0].c')

    def test_complete_list_with_type(self):
        root = nd.new([])
        b = nd.complete(root, [0, 0], dict)
        self.assertTrue(nd.is_map(b))

    def test_complete_list_existing(self):
        root = nd.new([['pass']])
        b = nd.complete(root, [0, 0], 'fail')
        self.assertEqual(b, 'pass')
        
        
if __name__ == '__main__':
    unittest.main()
