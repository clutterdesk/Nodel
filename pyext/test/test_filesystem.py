import math
import os
import shutil
import sys
import unittest
import urllib.parse
import nodel as nd

import logging 
logging.basicConfig(level=logging.DEBUG, stream=sys.stdout)


class TestFilesystem(unittest.TestCase):
    def test_bind(self):
        path = urllib.parse.quote(os.path.dirname(__file__))
        wd = nd.bind(f'file://?path={path}')
        self.assertTrue('test_module.py' in wd)
        
    def test_url_path_conflict(self):
        self.assertRaises(RuntimeError, nd.bind, f'file:///?path=.')  

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
        
    def test_bind_non_existing(self):
        wd = nd.bind('file://?path=FDKJSKJSD0023998378613984')
        try:
            list(wd)
            self.fail()
        except:
            pass

        
if __name__ == '__main__':
    unittest.main()
