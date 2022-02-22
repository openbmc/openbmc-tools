#!/usr/bin/python3

import argparse
import json
import os
import math


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

        for user in sorted(set(commits.keys()).union(reviews.keys())):
            user_commits = len(commits.get(user, []))
            user_reviews = reviews.get(user, {})
            reviews_by_others = len(user_reviews.get("from_others", []))
            this_user_reviews = len(user_reviews.get("for_others", []))

            points = user_commits * 3 + this_user_reviews
            print(user, points, user_commits, this_user_reviews)

            qualified = points >= 15

            if reviews_by_others == 0:
                ratio = 0.0
            else:
                ratio = this_user_reviews / reviews_by_others

            results[user] = {
                "qualified": qualified,
                "points": points,
                "commits": user_commits,
                "reviews": this_user_reviews,
                "reviews_by_others": reviews_by_others,
                "review_ratio": round(ratio, 2),
            }

        total_users = len(results)
        value_list = sorted(
            results.items(), key=lambda x: x[1]["points"], reverse=True
        )

        prev_value = {"points": math.inf}
        for (rank, (user, values)) in enumerate(value_list):
            percent = int((total_users - rank) / total_users * 100)
            results[user]["percentile"] = percent

            # if the last users points was the same as yours, assume their rank
            if prev_value["points"] == values["points"]:
                results[user]["rank"] = prev_value["rank"]
            else:
                results[user]["rank"] = rank + 1
            prev_value = values

        with open(os.path.join(args.dir, "report.json"), "w") as outfile:
            outfile.write(json.dumps(results, indent=4))

        return 0
