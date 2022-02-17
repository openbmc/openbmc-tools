#!/usr/bin/python3

import argparse
import json
import os


class subcmd:
    def __init__(self, parser: argparse._SubParsersAction) -> None:
        p = parser.add_parser(
            "report", help="Create final report"
        )

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

        for user in sorted(set(commits.keys()).union(reviews.keys())):
            user_commits = len(commits.get(user, []))
            user_reviews = len(reviews.get(user, []))

            points = user_commits * 3 + user_reviews
            print(user, points, user_commits, user_reviews)

            qualified = points >= 15

            results[user] = {"qualified": qualified, "points": points,
                             "commits": user_commits, "reviews": user_reviews}

        total_users = len(results)
        for user, values in results.items():
            user_points = values["points"]
            rank = 0
            for other_user, other_values in results.items():
                if user_points >= other_values["points"]:
                    rank += 1
            results[user]["percentile"] = int(rank / total_users * 100)
            results[user]["rank"] = total_users-rank + 1

        with open(os.path.join(args.dir, "report.json"), "w") as outfile:
            outfile.write(json.dumps(results, indent=4))

        return 0
