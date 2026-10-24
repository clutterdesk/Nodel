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

    def test_save_inplace_str(self):
        try:
            path = urllib.parse.quote(os.path.dirname(__file__))
            wd = nd.bind(f'file://?path={path}&perm=rw')
            wd.tmp = {'tea.txt': 'FTG', 'herb.txt': 'mint', 'note.txt': 'abcdef'}
            nd.save(wd)
            wd = nd.bind(f'file://?path={path}&perm=rw')
            tea = wd.tmp['tea.txt']
            tea += 'FOP'
            self.assertTrue(nd.is_unsaved(tea))
            herb = wd.tmp['herb.txt']
            herb *= 2
            note = wd.tmp['note.txt']
            note[1:3] = 'X'
            nd.save(wd)
            wd = nd.bind(f'file://?path={path}&perm=r')
            self.assertEqual(wd.tmp['tea.txt'], 'FTGFOP')
            self.assertEqual(wd.tmp['herb.txt'], 'mintmint')
            self.assertEqual(wd.tmp['note.txt'], 'aXdef')
        finally:
            shutil.rmtree(os.path.join(path, 'tmp'))

    def test_save_inplace_list(self):
        try:
            path = urllib.parse.quote(os.path.dirname(__file__))
            wd = nd.bind(f'file://?path={path}&perm=rw')
            wd.tmp = {'teas.json': ['Assam', 'Ceylon']}
            nd.save(wd)
            wd = nd.bind(f'file://?path={path}&perm=rw')
            teas = wd.tmp['teas.json']
            teas *= 2
            self.assertTrue(nd.is_unsaved(teas))
            nd.save(wd)
            wd = nd.bind(f'file://?path={path}&perm=r')
            self.assertEqual(wd.tmp['teas.json'], ['Assam', 'Ceylon', 'Assam', 'Ceylon'])
        finally:
            shutil.rmtree(os.path.join(path, 'tmp'))

    def test_inplace_list_write_protected(self):
        try:
            path = urllib.parse.quote(os.path.dirname(__file__))
            wd = nd.bind(f'file://?path={path}&perm=rw')
            wd.tmp = {'teas.json': ['Assam']}
            nd.save(wd)
            wd = nd.bind(f'file://?path={path}&perm=r')
            teas = wd.tmp['teas.json']
            with self.assertRaises(RuntimeError):
                teas += ['Ceylon']
            with self.assertRaises(RuntimeError):
                teas *= 2
            with self.assertRaises(RuntimeError):
                teas *= 0
            with self.assertRaises(RuntimeError):
                teas.__iadd__(['Ceylon'])
            self.assertEqual(teas, ['Assam'])
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
