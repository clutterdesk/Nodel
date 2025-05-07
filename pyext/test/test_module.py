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
    def test_bind(self):
        path = urllib.parse.quote(os.path.dirname(__file__))
        wd = nd.bind(f'file://?path={path}')
        self.assertTrue('test_module.py' in wd)
        
    def test_url_path_conflict(self):
        self.assertRaises(RuntimeError, nd.bind, f'file:///?path=.')  

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
        obj = nd.from_json("{'x': [{'u': 1}, {'u': 2}], 'y': [{'u': 3}, {'u': 4}]}")
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
        
    def test_reset(self):
        path = urllib.parse.quote(os.path.dirname(__file__))
        wd = nd.bind(f'file://?path={path}&perm=rw')
        self.assertTrue('test_module.py' in wd)
        nd.clear(wd)
        self.assertTrue('test_module.py' not in wd)
        nd.reset(wd)
        self.assertTrue('test_module.py' in wd)

    def test_reset_key(self):
        # TODO
        pass

    def test_save_deep_1(self):
        try:
            path = urllib.parse.quote(os.path.dirname(__file__))
            wd = nd.bind(f'file://?path={path}&perm=rw')
            wd.tmp = {}
            nd.save(wd)
            wd = nd.bind(f'file://?path={path}&perm=r')
            self.assertTrue(nd.is_map(wd.tmp))
        finally:
            shutil.rmtree(os.path.join(path, 'tmp'))                               
    
    def test_save_deep_2(self):
        try:
            path = urllib.parse.quote(os.path.dirname(__file__))
            wd = nd.bind(f'file://?path={path}&perm=rw')
            wd.tmp = {}
            wd.tmp['tea.txt'] = 'FTGFOP'
            nd.save(wd)
            wd = nd.bind(f'file://?path={path}&perm=r')
            self.assertEqual(wd.tmp['tea.txt'], 'FTGFOP')
        finally:
            shutil.rmtree(os.path.join(path, 'tmp'))                               

    def test_save_deeper(self):
        try:
            path = urllib.parse.quote(os.path.dirname(__file__))
            wd = nd.bind(f'file://?path={path}&perm=rw')
            wd.tmp = {
                'culinary': {
                    'tea.json': {'country': 'India', 'region': 'Assam', 'designation': 'FTGFOP'},
                    'vegetable.txt': 'beets'
                 },
            }
            nd.save(wd)
            wd = nd.bind(f'file://?path={path}&perm=r')
            self.assertTrue(nd.is_map(wd.tmp))
            self.assertEqual(wd.tmp.culinary['vegetable.txt'], 'beets')
            self.assertEqual(wd.tmp.culinary['tea.json'].country, 'India')
        finally:
            shutil.rmtree(os.path.join(path, 'tmp'))                               

    def test_fpath(self):
        path = urllib.parse.quote(os.path.dirname(__file__))
        wd = nd.bind(f'file://?path={path}&perm=rw')
        self.assertEqual(nd.fpath(wd), path)

    def test_is_bound(self):
        path = urllib.parse.quote(os.path.dirname(__file__))
        wd = nd.bind(f'file://?path={path}&perm=rw')
        self.assertTrue(nd.is_bound(wd))
        
    def test_is_same(self):
        obj = nd.Object({})
        self.assertTrue(nd.is_same(obj, obj))
        
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

    def test_bind_non_existing(self):
        wd = nd.bind('file://?path=FDKJSKJSD0023998378613984')
        try:
            list(wd)
            self.fail()
        except:
            pass

        
if __name__ == '__main__':
    unittest.main()
