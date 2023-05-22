#!/usr/bin/python3

import argparse
import json
import os


class subcmd:
    def __init__(self, parser: argparse._SubParsersAction) -> None:
        p = parser.add_parser("report", help="Create final report")

        p.set_defaults(cmd=self)

    def run(self, args: argparse.Namespace) -> int:
        commits_fp = os.path.join(args.dir, "commits.json")
        reviews_fp = os.path.join(args.dir, "reviews.json")

        results = {}

        if not os.path.isfile(commits_fp):
            print("Missing commits.json; run analyze-commits?")
            return 1

        if not os.path.isfile(reviews_fp):
            print("Missing reviews.json; run analyze-reviews?")
            return 1

        with open(commits_fp, "r") as commits_file:
            commits = json.load(commits_file)

        with open(reviews_fp, "r") as reviews_file:
            reviews = json.load(reviews_file)

        contributions = commits | reviews

        for user in sorted(contributions.keys()):
            user_commits = len(commits.get(user, {}).get("changes", []))
            user_reviews = len(reviews.get(user, {}).get("changes", []))

            points = user_commits * 3 + user_reviews
            print(user, points, user_commits, user_reviews)

            qualified = points >= 15

            results[user] = {
                "name": contributions[user]["name"],
                "email": contributions[user]["email"],
                "qualified": qualified,
                "points": points,
                "commits": user_commits,
                "reviews": user_reviews,
            }

        with open(os.path.join(args.dir, "report.json"), "w") as outfile:
            outfile.write(json.dumps(results, indent=4))

        return 0
