#!/usr/bin/env perl
use strict;
use warnings;
use v5.10;
use Cwd qw(getcwd);

my $GITURL = "https://github.com/dagurval";
my $ROOTDIR = getcwd();

sub check_repo {
    my $repo = shift or die;
    my $dst = shift or die;

    # Already cloned.
    if (-e "$ROOTDIR/$dst") {
        chdir "$ROOTDIR/$dst"
            or die "chdir $dst: $!";
        say "Pulling $repo in $ROOTDIR/$dst";

        my $cmd = "git checkout master";
        system($cmd) == 0
            or die "Failed to call '$cmd'";

        $cmd = "git pull";
        system($cmd) == 0
            or die "Failed to call '$cmd'";
        chdir $ROOTDIR;
        return;
    }

    # Not cloned yet.
    say "Cloning repo $repo";
    my $cmd = "git clone $GITURL/$repo $dst";
    system($cmd) == 0
        or die "Failed to call '$cmd'";
}

check_repo("bitcoinxt-$_.git", $_) for qw(contrib depends doc)
