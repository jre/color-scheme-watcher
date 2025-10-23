#!/usr/bin/env python3

import re
import sys


man_pattern = re.compile('([a-zA-Z0-9_.-]+)\\(([0-9])\\)')
known_manpages = set(sys.argv[1:])


def fixup(match):
    page = '%s.%s' % match.groups()
    if page in known_manpages:
        return '[%s](%s.md)' % (match.group(0), page)
    return match.group(0)


for old_line in sys.stdin.readlines():
    new_line, _ = re.subn(man_pattern, fixup, old_line)
    sys.stdout.write(new_line)
