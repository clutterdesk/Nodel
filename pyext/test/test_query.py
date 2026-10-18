import unittest
import nodel as nd


class TestQuery(unittest.TestCase):
    def setUp(self):
        self.d = nd.new({"x": [{"id": 1, "value": 100}, {"id": 2, "value": 200}, {"id": 3, "value": 300}],
                         "l": [1, 5, 2, 7]})

    def ids(self, spec):
        return [obj.id for obj in nd.query(self.d, spec)]

    def test_example(self):
        d = nd.new({"x": [{"id": 1, "value": 100}, {"id": 2, "value": 200}]})
        self.assertEqual(list(nd.query(d, "x(id=2)")), [])
        result = list(nd.query(d, "x.*(id=2)"))
        self.assertEqual(len(result), 1)
        self.assertTrue(nd.is_same(result[0], d.x[1]))
        self.assertEqual(result[0], {"id": 2, "value": 200})

    def test_comparisons(self):
        self.assertEqual(self.ids("x.*(id!=2)"), [1, 3])
        self.assertEqual(self.ids("x.*(1 <= id < 3)"), [1, 2])
        self.assertEqual(self.ids("x.*(id in [1, 3])"), [1, 3])
        self.assertEqual(self.ids("x.*(id not in [1, 3])"), [2])
        self.assertEqual(self.ids("x.*(id = 1 or value >= 300)"), [1, 3])
        self.assertEqual(self.ids("x.*(not id = 1 and value < 300)"), [2])

    def test_scalars(self):
        self.assertEqual(list(nd.query(self.d, "l.*(. > 3)")), [5, 7])
        self.assertEqual(list(nd.query(self.d, "x.*.id")), [1, 2, 3])

    def test_axes(self):
        x1 = self.d.x[1]
        self.assertTrue(nd.is_same(next(nd.query(x1, ":parent")), self.d.x))
        self.assertEqual(len(list(nd.query(x1, ":ancestor"))), 2)
        self.assertTrue(nd.is_same(next(nd.query(x1, ":root")), self.d))
        self.assertEqual([obj.id for obj in nd.query(self.d, ":dc(value >= 200)")], [2, 3])
        with self.assertRaises(ValueError):
            nd.query(self.d, "**")

    def test_compiled_query(self):
        q = nd.Query("x.*(value > 150)")
        self.assertEqual(repr(q), "nodel.Query('x.*(value > 150)')")
        self.assertEqual(str(q), "x.*(value > 150)")
        self.assertEqual([obj.id for obj in q.list(self.d)], [2, 3])
        self.assertEqual([obj.id for obj in q.iter(self.d)], [2, 3])
        other = nd.new({"x": [{"value": 500, "id": 9}]})
        self.assertEqual([obj.id for obj in nd.query(other, q)], [9])

    def test_lazy(self):
        big = nd.new({"x": [{"id": i} for i in range(100000)]})
        it = nd.query(big, "x.*(id >= 5)")
        self.assertEqual(next(it).id, 5)
        self.assertEqual(next(it).id, 6)

    def test_iterator_outlives_query(self):
        it = nd.Query("x.*(id=3)").iter(self.d)
        self.assertEqual([obj.id for obj in it], [3])
        self.assertEqual(list(it), [])

    def test_syntax_error(self):
        for spec in ["x.*(", "x.*(id=)", "x.*(and)", "x[abc]", "x)"]:
            with self.assertRaises(ValueError, msg=spec):
                nd.query(self.d, spec)
        with self.assertRaises(ValueError):
            nd.Query("x.*(id=")

    def test_wrong_types(self):
        with self.assertRaises(TypeError):
            nd.query(self.d, 1)
        with self.assertRaises(TypeError):
            nd.Query(1)
        with self.assertRaises(ValueError):
            nd.query({"x": 1}, "x")


if __name__ == '__main__':
    unittest.main()
