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
        d = nd.Object({})
        self.assertTrue(nd.is_same(d, d))
        
    def test_is_nil(self):
        d = nd.Object(None)
        self.assertTrue(nd.is_nil(d))
        
    def test_is_bool(self):
        d = nd.Object(True)
        self.assertTrue(nd.is_bool(d))
        
    def test_is_int(self):
        d = nd.Object(sys.maxsize)
        self.assertTrue(nd.is_int(d))

    def test_is_float(self):
        d = nd.Object(2.18)
        self.assertTrue(nd.is_float(d))
        
    def test_is_str(self):
        d = nd.Object('tea')
        self.assertTrue(nd.is_str(d))
        
    def test_is_list(self):
        d = nd.Object([])
        self.assertTrue(nd.is_list(d))
        
    def test_is_map(self):
        d = nd.Object({})
        self.assertTrue(nd.is_map(d))
        
    def test_is_native(self):
        d = nd.Object((1, 2))
        self.assertTrue(nd.is_native(d))
        
    def test_ref_count(self):
        d = nd.Object([])
        self.assertEqual(nd.ref_count(d), 1)

    def test_bind_non_existing(self):
        wd = nd.bind('file://?path=FDKJSKJSD002399')
        try:
            list(wd)
            self.fail()
        except:
            pass

        
if __name__ == '__main__':
    unittest.main()
