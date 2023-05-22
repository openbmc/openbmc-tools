#!/usr/bin/python3

from typing import TypedDict

UserChanges = TypedDict('User', {'name': str, 'email': str, 'changes': list[int]})
def changes_factory():
    return {"name": None, "email": None, "changes": list()}

UserComments = TypedDict('User', {'name': str, 'email': str, 'comments': int})
def comments_factory():
    return {"name": None, "email": None, "comments": 0}
