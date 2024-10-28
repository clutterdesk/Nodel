import unittest
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
        
if __name__ == '__main__':
    unittest.main()
