#!/usr/bin/python3

from typing import TypedDict

UserChanges = TypedDict('User', {'name': str, 'email': str, 'changes': list[int]})
changes_factory = lambda: {"name": None, "email": None, "changes": list()}

UserComments = TypedDict('User', {'name': str, 'email': str, 'comments': int})
comments_factory = lambda: {"name": None, "email": None, "comments": 0}
