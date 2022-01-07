#!/usr/bin/python3
from enum import Enum
from datetime import datetime, timezone


class TimeOfDay(Enum):
    AM = 0
    PM = 1


def timestamp(date: str, time: TimeOfDay) -> int:
    [year, month, day] = [int(x) for x in date.split("-")]

    if time == TimeOfDay.AM:
        [hour, minute, second] = [00, 00, 00]
    else:
        [hour, minute, second] = [23, 59, 59]

    return int(
        datetime(
            year, month, day, hour, minute, second, tzinfo=timezone.utc
        ).timestamp()
    )
