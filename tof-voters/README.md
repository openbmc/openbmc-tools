This tool is used for determining who is qualified for voting in a TOF election.
The tool will query Gerrit for commits and reviews, process them, and generate a
report of qualified individuals.

The typical use of the tool is something like this:
```
./voters dump-gerrit --after=2021-06-30
./voters analyze-commits --before "2022-01-01" --after "2021-06-30"
./voters analyze-reviews --before "2022-01-01" --after "2021-06-30"
./voters report
cat data/report.json | \
    jq "with_entries(select(.value.qualified) | .value = .value.points)"
```
