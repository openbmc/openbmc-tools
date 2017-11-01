#!/usr/bin/env perl

# Contributors Listed Below - COPYRIGHT 2017
# [+] International Business Machines Corp.
#
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or
# implied. See the License for the specific language governing
# permissions and limitations under the License.

use strict;
use File::Basename qw/basename/;

my $metas_to_search = "meta-phosphor meta-openbmc-machines meta-openbmc-bsp";
my $master_project = "openbmc";
my $server = "https://gerrit.openbmc-project.xyz";

system("cd $master_project && git fetch origin && git checkout origin/master");

open(FILES, "cd $master_project && git grep -l -e \"_URI\" --and -e \"github\" -- $metas_to_search |");

my @to_update = ();

while(my $file = <FILES>)
{
    chomp $file;

    my $entry = {};
    $entry->{FILE} = "$file";
    $entry->{BRANCH} = "master";

    open(FILE, "$master_project/$entry->{FILE}");
    while(my $line = <FILE>)
    {
        chomp $line;

        if ($line =~ m/SRCREV ?.*=/)
        {
            if ($line =~ m/"([0-9a-f]*)"/)
            {
                $entry->{SRCREV} = $1;
            }
        }
        elsif ($line =~ m/_URI/ and $line =~ m/github.com\/$master_project\//)
        {
            $line =~ s/.*$master_project\//$master_project\//;
            $line =~ s/"//g;
            $line =~ s/\.git$//;
            $entry->{SRC_URI} = $line;
            print "$file : $line\n";
        }
    }
    close FILE;

    if (exists $entry->{SRC_URI} and exists $entry->{SRCREV})
    {
        push @to_update, $entry;
    }
}

foreach my $entry (@to_update)
{
    my $project = $entry->{SRC_URI};
    $project =~ s/\//%2F/g;
    my $revision =
        `curl -s $server/projects/$project/branches/$entry->{BRANCH}  | \
         grep revision`;

    if (not $revision =~ m/revision/)
    {
        next;
    }
    if ($revision =~ m/$entry->{SRCREV}/)
    {
        print "$entry->{SRC_URI} is up to date @ $entry->{SRCREV}\n";
        next;
    }

    $revision =~ m/"([0-9a-f]*)"/;
    $revision = $1;

    print "$entry->{SRC_URI} needs to be updated\n";
    print "\t$entry->{SRCREV} -> $revision\n";

    my $changeId = `echo autobump $entry->{FILE} $entry->{SRCREV} $revision | git hash-object -t blob --stdin`;
    chomp $changeId;
    $changeId =~ s/[ \t]*//;
    $changeId = "I$changeId";

    my $change =
        `curl -s $server/changes/$master_project%2F$master_project~$entry->{BRANCH}~$changeId | \
         grep change_id`;

    if ($change =~ m/$changeId/)
    {
        print "\t$changeId already present.\n";
        next;
    }

    system("cd $master_project && git checkout origin/master --force &&".
            " sed -i \"s/$entry->{SRCREV}/$revision/\" $entry->{FILE} &&".
            " git add $entry->{FILE}");

    open(COMMIT, "| cd $master_project && git commit -s -F -");
    print COMMIT (basename $entry->{FILE}).": bump version\n";
    print COMMIT "\n";
    print COMMIT "Change-Id: $changeId\n";
    close(COMMIT);

    system("cd $master_project && git push origin HEAD:refs/for/master/autobump");

}
