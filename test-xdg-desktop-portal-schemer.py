import datetime as dt
import importlib
import os.path
import sys
import unittest

import pyparsing


# adapted from https://docs.python.org/3/library/importlib.html#importing-a-source-file-directly
def import_from_path(module_name, file_path):
    file_path = os.path.abspath(file_path)
    loader = importlib.machinery.SourceFileLoader(module_name, file_path)
    spec = importlib.util.spec_from_file_location(
        module_name, file_path, loader=loader)
    module = importlib.util.module_from_spec(spec)
    sys.modules[module_name] = module
    spec.loader.exec_module(module)
    return module


class TestParser(unittest.TestCase):
    maxDiff = None

    _tz = dt.timezone.utc

    def _time(self, hour, min=0, sec=0, us=0):
        return m.WrapTime(hour, min, sec, us, self._tz)

    @property
    def _time_min(self):
        return m.WrapTime.min(tzinfo=self._tz)

    @property
    def _time_max(self):
        return m.WrapTime.max(tzinfo=self._tz)

    def _dt(self, day, hour, min=0, sec=0, us=0):
        return dt.datetime.combine(day, dt.time(hour, min, sec, us, self._tz))

    def _timerange(self, t1, t2):
        return m.Range(self._time(*t1), self._time(*t2))

    def _linearPoints(self, *args):
        return tuple(
            m.LinearRule.Point(self._timerange(t1, t2), m.ShortDate(*d))
            for t1, t2, d in args)

    def testRangeNormal(self):
        r = m.Range(3, 5)
        for i, expect in enumerate((
                False, False, False, True, True, True, False)):
            with self.subTest(i=i, expect=expect):
                self.assertEqual(expect, i in r)

    def testRangeWrap(self):
        r = m.Range(5, 3)
        for i, expect in enumerate((
                True, True, True, True, False, True, True)):
            with self.subTest(i=i, expect=expect):
                self.assertEqual(expect, i in r)

    def testRangeMin(self):
        self.assertEqual(1, m.Range(1, 4).min(0))
        self.assertEqual(0, m.Range(4, 1).min(0))

    def testRangeMax(self):
        self.assertEqual(4, m.Range(1, 4).max(9))
        self.assertEqual(9, m.Range(4, 1).max(9))

    def testRangeSplit(self):
        r = m.Range(5, 8)
        self.assertEqual([r], r.split(0, 9))
        r = m.Range(6, 3)
        expect = [m.Range(0, 3), m.Range(6, 9)]
        self.assertEqual(expect, r.split(0, 9))

    def testRangeDiff(self):
        r = m.Range(4, 7)
        for other, expect in ((m.Range(3, 8), []),
                              (m.Range(4, 7), []),
                              (m.Range(2, 5), [m.Range(5, 7)]),
                              (m.Range(4, 5), [m.Range(5, 7)]),
                              (m.Range(5, 9), [m.Range(4, 5)]),
                              (m.Range(5, 7), [m.Range(4, 5)]),
                              (m.Range(5, 6), [m.Range(4, 5), m.Range(6, 7)])):
            with self.subTest(other=other, expect=expect):
                self.assertEqual(expect, r._diff(other))

    def testRangeDifference(self):
        others = []
        res = m.Range(80, 20).difference(others, 0, 99)
        expect = [m.Range(0, 20), m.Range(80, 99)]
        self.assertEqual(expect, res)

        others += expect
        res = m.Range(70, 30).difference(others, 0, 99)
        expect = [m.Range(20, 30), m.Range(70, 80)]
        self.assertEqual(expect, res)

        others += expect
        res = m.Range(40, 50).difference(others, 0, 99)
        expect = [m.Range(40, 50)]
        self.assertEqual(expect, res)

        others += expect
        res = m.Range(0, 99).difference(others, 0, 99)
        expect = [m.Range(30, 40), m.Range(50, 70)]
        self.assertEqual(expect, res)

        others += expect
        res = m.Range(0, 99).difference(others, 0, 99)
        expect = []
        self.assertEqual(expect, res)

    def testWrapTime(self):
        h1 = self._time(1)
        h6 = self._time(6)
        h23 = self._time(23)
        self.assertEqual(dt.timedelta(hours=-5), h1.delta12(h6))
        self.assertEqual(dt.timedelta(hours=2), h1.delta12(h23))
        self.assertEqual(dt.timedelta(hours=5), h6.delta12(h1))
        self.assertEqual(dt.timedelta(hours=7), h6.delta12(h23))
        self.assertEqual(dt.timedelta(hours=-2), h23.delta12(h1))
        self.assertEqual(dt.timedelta(hours=-7), h23.delta12(h6))
        self.assertEqual(h1, h6 + dt.timedelta(hours=-5))
        self.assertEqual(h1, h6 + dt.timedelta(hours=19))
        self.assertEqual(h1, h23 + dt.timedelta(hours=2))
        self.assertEqual(h1, h23 + dt.timedelta(hours=-22))
        self.assertEqual(h6, h1 + dt.timedelta(hours=5))
        self.assertEqual(h6, h1 + dt.timedelta(hours=29))
        self.assertEqual(h6, h23 + dt.timedelta(hours=-17))
        self.assertEqual(h6, h23 + dt.timedelta(hours=7))
        self.assertEqual(h23, h6 + dt.timedelta(hours=-7))
        self.assertEqual(h23, h6 + dt.timedelta(hours=17))

    def testShortDate(self):
        jan1 = m.ShortDate(1, 1)
        jan2 = m.ShortDate(1, 2)
        feb1 = m.ShortDate(2, 1)
        jan2_d = dt.date(2001, 1, 2)
        feb1_dt = dt.datetime(2002, 2, 1)

        self.assertEqual(jan1, m.ShortDate(1, 1))
        self.assertNotEqual(jan1, jan2)
        self.assertNotEqual(jan1, feb1)
        self.assertLess(jan1, jan2)
        self.assertLess(jan2, feb1)

        self.assertLess(jan1, jan2_d)
        self.assertNotEqual(jan1, jan2_d)
        self.assertEqual(jan2, jan2_d)
        self.assertGreater(feb1, jan2_d)
        self.assertNotEqual(feb1, jan2_d)

        self.assertLess(jan1, feb1_dt)
        self.assertNotEqual(jan1, feb1_dt)
        self.assertLess(jan2, feb1_dt)
        self.assertNotEqual(jan2, feb1_dt)
        self.assertEqual(feb1, feb1_dt)

        self.assertEqual(jan2_d, jan2.date(2001))
        self.assertEqual(feb1_dt.date(), feb1.date(2002))

    def testBaseRule(self):
        r = m.Rule('default', self._tz)
        expect = m.Range(self._time_min, self._time_max)
        self.assertEqual(expect, r.match(None))

    def testSimpleRule(self):
        times = self._timerange((6,), (8,))
        wdays = m.Range(0, 4)
        dates = m.Range(m.ShortDate(1, 1), m.ShortDate(1, 31))
        jan2fri = dt.date(2026, 1, 2)
        jan3sat = dt.date(2026, 1, 3)
        feb1sun = dt.date(2026, 2, 1)
        feb2mon = dt.date(2026, 2, 2)
        r = m.SimpleRule('default', self._tz, times)
        self.assertEqual(times, r.match(None))
        r = m.SimpleRule('default', self._tz, times, weekdays=wdays)
        self.assertEqual(times, r.match(jan2fri))
        self.assertIsNone(r.match(jan3sat))
        self.assertIsNone(r.match(feb1sun))
        self.assertEqual(times, r.match(feb2mon))
        r = m.SimpleRule('default', self._tz, times, dates=dates)
        self.assertEqual(times, r.match(jan2fri))
        self.assertEqual(times, r.match(jan3sat))
        self.assertIsNone(r.match(feb1sun))
        self.assertIsNone(r.match(feb2mon))
        r = m.SimpleRule('default', self._tz, times,
                         weekdays=wdays, dates=dates)
        self.assertEqual(times, r.match(jan2fri))
        self.assertIsNone(r.match(jan3sat))
        self.assertIsNone(r.match(feb1sun))
        self.assertIsNone(r.match(feb2mon))

    def testLinearRuleNormal(self):
        r = m.LinearRule('default', self._tz, self._linearPoints(
            ((7, 0), (20, 0), (3, 21)),
            ((6, 30), (20, 30), (4, 21)),
            ((6, 0), (21, 15), (5, 21)),
            ((5, 45), (21, 30), (6, 21)),
            ((6, 0), (21, 15), (7, 21)),
            ((6, 30), (20, 30), (8, 21)),
            ((7, 0), (20, 0), (9, 21))))
        expect = self._timerange((7, 0), (20, 0))
        self.assertEqual(expect, r.match(dt.date(2000, 1, 1)))
        expect = self._timerange((5, 45), (21, 30))
        self.assertEqual(expect, r.match(dt.date(2000, 6, 21)))
        expect = self._timerange((6, 20), (20, 45))
        self.assertEqual(expect, r.match(dt.date(2000, 5, 1)))
        expect = self._timerange((5, 50), (21, 25))
        self.assertEqual(expect, r.match(dt.date(2000, 7, 1)))

    def testLinearRuleWrap(self):
        r = m.LinearRule('default', self._tz, self._linearPoints(
            ((9, 0), (23, 0), (4, 1)),
            ((11, 0), (1, 0), (5, 1)),
            ((11, 0), (1, 0), (9, 1)),
            ((9, 0), (23, 0), (10, 1))))
        expect = self._timerange((9, 0), (23, 0))
        self.assertEqual(expect, r.match(dt.date(2020, 2, 29)))
        expect = self._timerange((11, 0), (1, 0))
        self.assertEqual(expect, r.match(dt.date(2020, 6, 21)))
        expect = self._timerange((9, 16), (23, 16))
        self.assertEqual(expect, r.match(dt.date(2000, 4, 5)))
        expect = self._timerange((10, 32), (0, 32))
        self.assertEqual(expect, r.match(dt.date(2000, 9, 8)))

    def testRuleSetSimple1(self):
        rules = m.RuleSet(tzinfo=self._tz, rules=(
            m.SimpleRule('dark', self._tz, self._timerange((21,), (6,))),))
        day = dt.date(2000, 1, 1)
        next_day = dt.date(2000, 1, 2)
        expect = (('dark', self._timerange((21,), (6,))),
                  ('default', m.Range(self._time_min, self._time_max)))
        mr = rules._mode_ranges_for_day(day)
        self.assertEqual(expect, mr)
        expect = (('dark', self._time_min),
                  ('default', self._time(6, us=1)),
                  ('dark', self._time(21)))
        self.assertEqual(expect, rules._transitions_for_mode_ranges(mr))

        for query, exp_mode, exp_dt in (
                (self._dt(day, 4), 'dark', self._dt(day, 6, us=1)),
                (self._dt(day, 6), 'dark', self._dt(day, 6, us=1)),
                (self._dt(day, 6, us=1), 'default', self._dt(day, 21)),
                (self._dt(day, 10), 'default', self._dt(day, 21)),
                (self._dt(day, 21), 'dark', self._dt(next_day, 0)),
                (self._dt(day, 22), 'dark', self._dt(next_day, 0))):
            with self.subTest(query=query):
                self.assertEqual((exp_mode, exp_dt), rules.lookup(query))

    def testRuleSetSimple2(self):
        rules = m.RuleSet(tzinfo=self._tz, rules=(
            m.SimpleRule('light', self._tz, self._timerange((6,), (17,))),
            m.SimpleRule('dark', self._tz, self._timerange((20,), (6,))),))
        day = dt.date(2004, 2, 28)
        next_day = dt.date(2004, 2, 29)
        expect = (('light', self._timerange((6,), (17,))),
                  ('dark', self._timerange((20,), (6,))),
                  ('default', m.Range(self._time_min, self._time_max)))
        mr = rules._mode_ranges_for_day(day)
        self.assertEqual(expect, mr)
        expect = (('dark', self._time_min),
                  ('light', self._time(6)),
                  ('default', self._time(17, us=1)),
                  ('dark', self._time(20)))
        self.assertEqual(expect, rules._transitions_for_mode_ranges(mr))

        for query, exp_mode, exp_dt in (
                (self._dt(day, 4), 'dark', self._dt(day, 6)),
                (self._dt(day, 6), 'light', self._dt(day, 17, us=1)),
                (self._dt(day, 10), 'light', self._dt(day, 17, us=1)),
                (self._dt(day, 17), 'light', self._dt(day, 17, us=1)),
                (self._dt(day, 17, us=1), 'default', self._dt(day, 20)),
                (self._dt(day, 20), 'dark', self._dt(next_day, 0)),
                (self._dt(day, 22), 'dark', self._dt(next_day, 0))):
            with self.subTest(query=query):
                self.assertEqual((exp_mode, exp_dt), rules.lookup(query))

    def testRuleSetNotSoSimple(self):
        rules = m.RuleSet(tzinfo=self._tz, rules=(
            m.SimpleRule('light', self._tz, self._timerange((5,), (22,)),
                         dates=m.Range(m.ShortDate(6, 7), m.ShortDate(7, 5))),
            m.SimpleRule('dark', self._tz, self._timerange((19,), (6,)),
                         dates=m.Range(m.ShortDate(10, 1), m.ShortDate(3, 1))),
            m.SimpleRule('light', self._tz, self._timerange((6,), (21,)),
                         weekdays=m.Range(4, 5)),
            m.SimpleRule('light', self._tz, self._timerange((8,), (19,))),
            m.SimpleRule('dark', self._tz, self._timerange((20,), (5,))),))
        feb13 = dt.date(2020, 2, 13)
        feb14 = dt.date(2020, 2, 14)
        may7 = dt.date(2020, 5, 7)
        may8 = dt.date(2020, 5, 8)
        jun20 = dt.date(2020, 6, 20)
        jun21 = dt.date(2020, 6, 21)

        for day, exp_mode_ranges, exp_transitions in (
                (feb14, (
                    ('dark', self._timerange((19,), (6,))),
                    ('light', self._timerange((6,), (21,))),
                    ('default', m.Range(self._time_min, self._time_max)),
                ), (
                    ('dark', self._time_min),
                    ('light', self._time(6, us=1)),
                    ('dark', self._time(19)),
                )),
                (may7, (
                    ('light', self._timerange((8,), (19,))),
                    ('dark', self._timerange((20,), (5,))),
                    ('default', m.Range(self._time_min, self._time_max)),
                ), (
                    ('dark', self._time_min),
                    ('default', self._time(5, us=1)),
                    ('light', self._time(8)),
                    ('default', self._time(19, us=1)),
                    ('dark', self._time(20)),
                )),
                (jun20, (
                    ('light', self._timerange((5,), (22,))),
                    ('dark', self._timerange((20,), (5,))),
                    ('default', m.Range(self._time_min, self._time_max)),
                ), (
                    ('dark', self._time_min),
                    ('light', self._time(5)),
                    ('dark', self._time(22, us=1)),
                ))):
            with self.subTest(day=day):
                mr = rules._mode_ranges_for_day(day)
                self.assertEqual(exp_mode_ranges, mr)
                tr = rules._transitions_for_mode_ranges(mr)
                self.assertEqual(exp_transitions, tr)

        for query, exp_mode, exp_dt in (
                (self._dt(feb13, 6), 'dark', self._dt(feb13, 6, us=1)),
                (self._dt(feb13, 6, us=1), 'default', self._dt(feb13, 8)),
                (self._dt(feb13, 8), 'light', self._dt(feb13, 19)),
                (self._dt(feb13, 19, us=1), 'dark', self._dt(feb14, 0)),
                (self._dt(feb14, 6), 'dark', self._dt(feb14, 6, us=1)),
                (self._dt(feb14, 6, us=1), 'light', self._dt(feb14, 19)),
                (self._dt(may7, 5), 'dark', self._dt(may7, 5, us=1)),
                (self._dt(may7, 5, us=1), 'default', self._dt(may7, 8)),
                (self._dt(may7, 8), 'light', self._dt(may7, 19, us=1)),
                (self._dt(may7, 19, us=1), 'default', self._dt(may7, 20)),
                (self._dt(may7, 20, us=1), 'dark', self._dt(may8, 0)),
                (self._dt(may8, 5), 'dark', self._dt(may8, 5, us=1)),
                (self._dt(may8, 5, us=1), 'default', self._dt(may8, 6)),
                (self._dt(may8, 6), 'light', self._dt(may8, 21, us=1)),
                (self._dt(jun20, 5), 'light', self._dt(jun20, 22, us=1)),
                (self._dt(jun20, 22, us=1), 'dark', self._dt(jun21, 0)),
                (self._dt(jun21, 5), 'light', self._dt(jun21, 22, us=1))):
            with self.subTest(query=query):
                self.assertEqual((exp_mode, exp_dt), rules.lookup(query))

    def testParserModeSpecifier(self):
        spec = m.Parser(self._tz)._get_mode_specifier()
        for want, aliases in (('default', ['def']),
                              ('dark', ['night']),
                              ('light', ['day'])):
            expect = [m.Rule(want, self._tz)]
            for mode in [want] + aliases:
                for fn in (mode.lower, mode.capitalize, mode.upper):
                    with self.subTest(fn()):
                        actual = spec.parse_string(fn(), parse_all=True)
                        self.assertListEqual(expect, actual.as_list())

    def testParserTimeSpecifier(self):
        spec = m.Parser(self._tz)._get_time_specifier()
        for expect, src in ((self._time(0), '0'),
                            (self._time(0), '12Am'),
                            (self._time(0, 0), '0:00'),
                            (self._time(1), '1am'),
                            (self._time(1), '1:00 AM'),
                            (self._time(12), '12 pM'),
                            (self._time(13, 52), '13:52'),
                            (self._time(13, 52), '1:52PM'),
                            (self._time(23), '23'),
                            (self._time(23), '11 pm'),
                            (self._time(23, 59), '23:59'),
                            (self._time(23, 59), '11:59 pm')):
            with self.subTest(src):
                actual = spec.parse_string(src, parse_all=True)
                self.assertListEqual([expect], actual.as_list())
        for ts in ('-1', '24', '100', '1:1', '1:-1', '1:60', '1:100'):
            for suf in ('', 'am'):
                src = ts + suf
                with self.subTest(src):
                    with self.assertRaises(pyparsing.ParseException):
                        spec.parse_string(src, parse_all=True)

    def testParserWeekdaySpecifier(self):
        spec = m.Parser(self._tz)._get_weekday_specifier()
        wdays = 'Monday Tuesday Wednesday Thursday Friday Saturday Sunday'
        for expect, name in enumerate(wdays.split()):
            for wday in (name, name[:3]):
                for fn in (wday.lower, wday.capitalize, wday.upper):
                    with self.subTest(fn()):
                        actual = spec.parse_string(fn(), parse_all=True)
                        self.assertListEqual([expect], actual.as_list())
        for src in ('Mo', 'Tues', 'Wednesdayy'):
            with self.subTest(src):
                with self.assertRaises(pyparsing.ParseException):
                    spec.parse_string(src, parse_all=True)

    def testParserDateSpecifier(self):
        p = m.Parser(self._tz)
        spec = p._get_date_specifier()
        monthday_tmpls = ('%s 1', '%s1', '1 %s', '1%s')
        months = 'January February March April May June ' + \
            'July August September October November December'
        for month_num, name in enumerate(months.split(), start=1):
            expect = [m.ShortDate(month_num, 1)]
            for month in (name, name[:3]):
                for fn in (month.lower, month.capitalize, month.upper):
                    for tmpl in monthday_tmpls:
                        src = tmpl % fn()
                        with self.subTest(src):
                            actual = spec.parse_string(src, parse_all=True)
                            self.assertListEqual(expect, actual.as_list())
        expect = [m.ShortDate(2, 1)]
        for src, dm in (('1/2', True), ('2/1', False), ('2/1', None),
                        (' 1/ 2', True), ('2 \t / 1 ', False)):
            p._day_month = dm
            with self.subTest(src):
                actual = spec.parse_string(src, parse_all=True)
                self.assertListEqual(expect, actual.as_list())
        p._day_month = False
        for src in ('0/1', '1/0', '13/1', '1/32', '2/29', '4/31',
                    '001/1', '100/1', 'Smarch 1', 'Janu 1', 'Juney 1'):
            with self.subTest(src):
                with self.assertRaises(pyparsing.ParseException):
                    spec.parse_string(src, parse_all=True)

    def testParserSettingsSection(self):
        p = m.Parser(self._tz)
        spec = p._get_settings_section()
        p._reset()
        self.assertTrue(p._day_month)
        src = 'set date format = month / day'
        spec.parse_string(src, parse_all=True)
        self.assertFalse(p._day_month)
        src = 'Set Date Format=Day/Month'
        spec.parse_string(src, parse_all=True)
        self.assertTrue(p._day_month)

    def testParseEmpty(self):
        expect = m.RuleSet((), self._tz)
        for conf in ('', '  \n\t ', '#foo', '\n  #bar\n\n\t'):
            with self.subTest(conf):
                self.assertEqual(expect, m.Parser(self._tz).loads(conf))

    def testParseTrivial(self):
        src = 'USE Dark Mode FROM 9Pm TO 6Am'
        expect = m.RuleSet(tzinfo=self._tz, rules=(
            m.SimpleRule('dark', self._tz, self._timerange((21,), (6,))),))
        self.assertEqual(expect.rules, m.Parser(self._tz).loads(src).rules)

    def testParseSimple(self):
        src = '''Set Date Format = Month/Day
        Use Light Mode From 9 PM To 6 AM, Monday Through Friday, May1-1Sep
        use Light mode from 20:30 To 6am monday through friday
        USE Default MODE FROM 21:30 To 6 5 / 1 - 1 September
        use default mode from 21 to 6
        use dark mode'''
        expect = m.RuleSet(tzinfo=self._tz, rules=(
            m.SimpleRule('light', self._tz, self._timerange((21,), (6,)),
                         dates=m.Range(m.ShortDate(5, 1), m.ShortDate(9, 1)),
                         weekdays=m.Range(0, 4)),
            m.SimpleRule('light', self._tz, self._timerange((20, 30), (6,)),
                         weekdays=m.Range(0, 4)),
            m.SimpleRule('default', self._tz, self._timerange((21, 30), (6,)),
                         dates=m.Range(m.ShortDate(5, 1), m.ShortDate(9, 1))),
            m.SimpleRule('default', self._tz, self._timerange((21,), (6,))),
            m.Rule('dark', self._tz)))
        self.assertEqual(expect.rules, m.Parser(self._tz).loads(src).rules)

    def testParseLinear(self):
        src = '''set date format = month/day
        use light mode linear 7 to 8pm on 3/21, 6 to 9:15pm on 5/21,
        5:45 to 9:30pm on 6/21, 6 to 9:15pm on 7/21, 7 to 8pm on 9/21
        use dark mode'''
        expect = m.RuleSet(tzinfo=self._tz, rules=(
            m.LinearRule('light', self._tz, self._linearPoints(
                ((7, 0), (20, 0), (3, 21)),
                ((6, 0), (21, 15), (5, 21)),
                ((5, 45), (21, 30), (6, 21)),
                ((6, 0), (21, 15), (7, 21)),
                ((7, 0), (20, 0), (9, 21)))),
            m.Rule('dark', self._tz)))
        self.assertEqual(expect.rules, m.Parser(self._tz).loads(src).rules)

    def testLookup(self):
        src = '''set date format = month/day
        use dark mode from 8pm to 6am fri through sat
        use light mode linear 8 to 7pm on 3/21
                              7 to 8pm on 5/21
                              7 to 8pm on 7/21
                              8 to 7pm on 9/21
        use dark mode linear  21:30 to 5:45 on Jun 21,
                              21:15 to 6:00 on May 21,
                              20:30 to 6:30 on Apr 21,
                              20:00 to 7:00 on Mar 21,
                              20:00 to 7:00 on Sep 21,
                              20:30 to 6:30 on Aug 21,
                              21:15 to 6:00 on Jul 21
        '''
        rules = m.Parser(self._tz).loads(src)
        jan1 = dt.date(2000, 1, 1)
        for query, exp_mode, exp_tm in (
                (self._dt(jan1, 20, 1), 'dark', None),
                (self._dt(jan1, 12), 'light', self._time(19, us=1)),
                (self._dt(jan1, 19, 1), 'default', self._time(20))):
            if exp_tm is not None:
                exp_dt = dt.datetime.combine(query.date(), exp_tm)
            else:
                exp_dt = dt.datetime.combine(query.date(), self._time_min) + \
                    dt.timedelta(days=1)
            with self.subTest(query=query):
                self.assertEqual((exp_mode, exp_dt), rules.lookup(query))


if __name__ == '__main__':
    m = import_from_path('m', 'xdg-desktop-portal-schemer')
    unittest.util._MAX_LENGTH = 2000
    unittest.main()
