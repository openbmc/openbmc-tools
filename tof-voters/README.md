This tool is used for determining who is qualified for voting in a TOF election,
based on the [requirements](https://github.com/openbmc/docs/blob/master/tof/membership-and-voting.md#metrics)
set out by the TOF.

The tool will query Gerrit for commits and reviews, process them, and generate a
report of qualified individuals.

The typical use of the tool is something like this:
```sh
./voters dump-gerrit --after=2021-06-30
./voters analyze-commits --before "2022-01-01" --after "2021-06-30"
./voters analyze-reviews --before "2022-01-01" --after "2021-06-30"
./voters report
cat data/report.json | \
    jq "with_entries(select(.value.qualified) | .value = .value.points)"
```

The above will yield a JSON dictionary of "users:points" where 'qualified' is
set in the users' dictionary from `report.json` like:
```json
{
    "user1": 16,
    "user2": 19,
    ...
}
```
