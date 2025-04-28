import unittest
import math
import sys
import nodel as nd

import logging 
logging.basicConfig(level=logging.DEBUG, stream=sys.stdout)


class TestObject(unittest.TestCase):
    def test_ctor(self):
        examples = [None, True, 7, 3.14, 'tea', [1, 2, 3]]
        for example in examples:
            obj = nd.Object(example)
            self.assertEqual(obj, example)
            self.assertEqual(str(obj), str(example))

        example = {'tea': 'Assam'}
        obj = nd.Object(example)
        self.assertEqual(obj, example)
        self.assertEqual(str(obj), '{"tea": "Assam"}')
        
    def test_nil(self):
        obj = nd.Object(None)
        self.assertTrue(nd.is_nil(obj))
        self.assertEqual(str(obj), "None")

    def test_nil_to_native(self):
        obj = nd.Object(None)
        pyo = nd.native(obj)
        self.assertIs(pyo, None)
        
    def test_int(self):
        obj = nd.Object(1)
        self.assertTrue(nd.is_int(obj))
        self.assertEqual(str(obj), "1")
        obj = nd.Object(-1)
        self.assertTrue(nd.is_int(obj))
        self.assertEqual(str(obj), "-1")
    
    def test_int_to_native(self):
        obj = nd.Object(1)
        pyo = nd.native(obj)
        self.assertIs(type(pyo), int)
        
    def test_max_mach_int(self):
        obj = nd.Object(sys.maxsize)
        self.assertTrue(nd.is_int(obj))
        self.assertEqual(str(obj), str(sys.maxsize))
        obj = nd.Object(-sys.maxsize)
        self.assertTrue(nd.is_int(obj))
        self.assertEqual(str(obj), str(-sys.maxsize))

    def test_int_arith(self):
        x, y = nd.Object(7), nd.Object(3)
        # avoid testing richcompare, here
        self.assertEqual(x + y, 10)
        self.assertEqual(x - y, 4)
        self.assertEqual(x * y, 21)
        self.assertEqual(x / y, nd.native(x) / nd.native(y))
        self.assertEqual(x // y, 2)
        self.assertEqual(x % y, 1)
        q, m = divmod(x, y)
        self.assertEqual(q, 2)
        self.assertEqual(m, 1)
        self.assertEqual(-x, -7)
        self.assertEqual(+x, 7)
        self.assertEqual(x ** y, nd.native(x) ** nd.native(y))
        self.assertEqual(pow(x, 3), pow(nd.native(x), 3))
        self.assertEqual(pow(7, y), pow(7, nd.native(y)))
        self.assertEqual(pow(x, y), pow(nd.native(x), nd.native(y)))
        self.assertEqual(pow(x, y, 2), pow(nd.native(x), nd.native(y), 2))
        self.assertEqual(pow(x, y, nd.Object(2)), pow(nd.native(x), nd.native(y), 2))

    def test_int_logic(self):
        x, y = nd.Object(7), nd.Object(3)
        # avoid testing richcompare, here
        self.assertEqual(x & y, 3)
        self.assertEqual(x | y, 7)
        self.assertEqual(x ^ y, 4)
        self.assertEqual(~x, -8)
        
    def test_int_inplace_arith(self):
        x, y = nd.Object(7), nd.Object(3)
        # avoid testing richcompare, here
        x += y
        self.assertTrue(y, 3)
        self.assertTrue(nd.is_int(x))
        self.assertEqual(x, 10)
        x = nd.Object(7)
        x -= y
        self.assertTrue(y, 3)
        self.assertTrue(nd.is_int(x))
        self.assertEqual(x, 4)
        x = nd.Object(7)
        x *= y
        self.assertTrue(y, 3)
        self.assertTrue(nd.is_int(x))
        self.assertEqual(x, 21)
        x = nd.Object(7)
        x /= y
        self.assertTrue(y, 3)
        self.assertTrue(nd.is_float(x))
        self.assertEqual(x, 7 / 3)
        x = nd.Object(7)
        x //= y
        self.assertTrue(y, 3)
        self.assertTrue(nd.is_int(x))
        self.assertEqual(x, 2)
        x = nd.Object(7)
        x %= y
        self.assertTrue(y, 3)
        self.assertTrue(nd.is_int(x))
        self.assertEqual(x, 1)
        x = nd.Object(7)
        x **= y
        self.assertTrue(y, 3)
        self.assertTrue(nd.is_int(x))
        self.assertEqual(x, 343)
        
    def test_int_inplace_logic(self):
        x, y = nd.Object(7), nd.Object(3)
        # avoid testing richcompare, here
        x &= y
        self.assertEqual(x, 3)
        x = nd.Object(7)
        x |= y
        self.assertEqual(x, 7)
        x = nd.Object(7)
        x ^= y
        self.assertEqual(x, 4)
        x = nd.Object(7)
        
    def test_index_conversion(self):
        self.assertEqual([1, 'bing!', 3][nd.Object(1)], 'bing!')
        
    def test_float(self):
        obj = nd.Object(math.pi)
        self.assertTrue(nd.is_float(obj))
        self.assertEqual(str(obj), str(math.pi))
    
    def test_float_to_native(self):
        obj = nd.Object(math.e)
        pyo = nd.native(obj)
        self.assertIs(type(pyo), float)
        
    def test_string_basic(self):
        obj = nd.Object("tea")
        self.assertTrue(nd.is_str(obj))
        self.assertEqual(str(obj), "tea")
        self.assertEqual(repr(obj), '"tea"')
    
    def test_string_with_nulls(self):
        obj = nd.Object("tea\0time")
        self.assertTrue(nd.is_str(obj))
        self.assertEqual(str(obj), "tea\x00time")
        self.assertEqual(repr(obj), '"tea\x00time"')
        
    @unittest.expectedFailure
    def test_string_slice(self):
        obj = nd.Object("teatime")
        self.assertEqual(obj[:3], "tea")
        
    def test_string_to_native(self):
        obj = nd.Object("teatime")
        s = nd.native(obj)
        self.assertIsInstance(s, str)
        self.assertEqual(s[:3], "tea")

    def test_float_arith(self):
        x, y = nd.Object(7.0), nd.Object(3.0)
        # avoid testing richcompare, here
        self.assertEqual(x + y, 10.0)
        self.assertEqual(x - y, 4.0)
        self.assertEqual(x * y, 21.0)
        self.assertEqual(x / y, nd.native(x) / nd.native(y))
        self.assertEqual(x // y, 2.0)
        self.assertEqual(x % y, 1.0)
        q, m = divmod(x, y)
        self.assertEqual(q, 2.0)
        self.assertEqual(m, 1.0)
        self.assertEqual(x ** y, nd.native(x) ** nd.native(y))
        self.assertEqual(-x, -7.0)
        self.assertEqual(+x, 7.0)

    def test_list_len(self):
        self.assertEqual(len(nd.Object([1, 2, 3])), 3)
            
    def test_list_subscript(self):
        list = nd.Object([1, 2, 3])
        self.assertEqual((list[0], list[1], list[2]), (1, 2, 3))
        self.assertEqual(list[:2], [1, 2])
        self.assertEqual(list[:-1], [1, 2])
        self.assertEqual(list[1:], [2, 3])
        self.assertEqual(list[-1:], [3])
        self.assertEqual(list[::2], [1, 3])
        self.assertEqual(list[:], [1, 2, 3])
        self.assertEqual(list[::], [1, 2, 3])
        self.assertEqual(list[-1::-1], [3, 2, 1])
        self.assertEqual(list[-1:-1:-1], [])
        
    def test_list_iter(self):
        list = nd.Object([1, 2, 3])
        it = iter(list)
        self.assertEqual(next(it), 1)
        self.assertEqual(next(it), 2)
        self.assertEqual(next(it), 3)

    def test_list_with_opaque(self):
        class X:
            pass
        x = X()
        list = nd.Object([x, x])
        self.assertIs(nd.native(list[0]), x)
        self.assertIs(nd.native(list[1]), x)

    def test_map_len(self):
        self.assertEqual(len(nd.Object({'x': 1, 'y': 2, 'z': 3})), 3)
        
    def test_map_subscript(self):
        map = nd.Object({'x': 1, 'y': 2, 'z': 3})
        self.assertEqual(map['x'], 1)
        self.assertEqual(map['y'], 2)
        self.assertEqual(map['z'], 3)
        
    @unittest.expectedFailure
    def test_map_object_subscript(self):
        self.assertEqual(map[nd.Object('z')], 3)
        
    def test_map_attr_subscript(self):
        map = nd.Object({'x': 1, 'y': 2, 'z': 3})
        self.assertEqual(map.x, 1)
        self.assertEqual(map.y, 2)
        self.assertEqual(map.z, 3)
        
    def test_map_with_opaque(self):
        class X:
            pass
        x = X()
        map = nd.Object({'x': x})
        self.assertIs(nd.native(map.x), x)

        
if __name__ == '__main__':
    unittest.main()
