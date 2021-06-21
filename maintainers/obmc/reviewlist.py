from typing import List, NamedTuple

class ReviewList(NamedTuple):
    maintainers: List[str] = list()
    reviewers: List[str] = list()
