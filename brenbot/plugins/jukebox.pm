# SPDX-License-Identifier: GPL-3.0-or-later
# Local coop adaptation of danpaul88's BRenBot Jukebox 1.0.
# Track names/durations are extracted from the pinned original plugin.
package jukebox;
use strict;
use warnings;
use POE;
use JSON::PP qw(decode_json);
use Time::HiRes qw(time);
use NovaraLocal;
our ($plugin_name, %config, %additional_events);
%additional_events = (tick=>'tick');
my (@tracks, %state);
sub start {
    open(my $in, '<:raw', 'jukebox.json') or die "jukebox.json: $!";
    local $/; @tracks=@{decode_json(<$in>)}; close($in);
    for my $t (@tracks) {
        die 'Invalid jukebox track' unless $t->{file} =~ /\A[a-z0-9 &_-]+\.mp3\z/
            && $t->{seconds} =~ /^\d+$/ && $t->{seconds}>0;
    }
    print '[Jukebox] Loaded '.scalar(@tracks)." original Renegade tracks; personal playback.\n";
    $_[KERNEL]->delay('tick', 1);
}
sub forget { delete $state{$_[0]}; }
sub enabled { return grep { !exists($config{$_->{file}}) || $config{$_->{file}} } @tracks; }
sub play {
    my ($id, $track, $announce)=@_;
    my $p=$NovaraLocal::players{$id} or return;
    my $s=$state{$id}//={};
    $s->{track}=$track; $s->{until}=time()+$track->{seconds}; $s->{on}=1;
    NovaraLocal::music($id,$track->{file});
    NovaraLocal::page($id,"Jukebox: $track->{id} - $track->{file}") if $announce;
}
sub next_track {
    my ($id,$announce)=@_; my @available=enabled();
    my $old=$state{$id}{track}{id}//-1;
    @available=grep { $_->{id}!=$old } @available if @available>1;
    if (!@available) { NovaraLocal::page($id,'All jukebox tracks are disabled.'); return; }
    play($id,$available[int(rand(@available))],$announce);
}
sub sync_player {
    my ($p,$old)=@_;
    if (!$old || $p->{session}!=$old->{session}) {
        forget($p->{id});
        next_track($p->{id},0) if $config{defaultState};
    } elsif (($p->{object}//0)>0 && ($p->{object}//0)!=($old->{object}//0) && $state{$p->{id}}{on}) {
        play($p->{id},$state{$p->{id}}{track},0);
    }
}
sub tick {
    for my $id (keys %state) {
        next unless $state{$id}{on} && $NovaraLocal::players{$id};
        next_track($id,0) if time()>=$state{$id}{until};
    }
    $_[KERNEL]->delay('tick',1);
}
sub stop { NovaraLocal::music($_,'off') for grep { $state{$_}{on} } keys %state; }
sub mapload { play($_,$state{$_}{track},0) for grep { $state{$_}{on} } keys %state; }
sub text {
    my $a=$_[ARG0]; my $id=$a->{id}; my $text=lc $a->{message};
    if ($text =~ /^!(?:tracks|jukebox)(?:\s+(\d+))?\s*$/) {
        my $page=$1||1; my @list=enabled(); my $pages=int((@list+4)/5)||1;
        $page=$pages if $page>$pages;
        NovaraLocal::page($id,"Tracks $page/$pages: !music on/off, !settrack <ID|name>, !nexttrack.");
        for my $i (($page-1)*5..$page*5-1) {
            last if $i>@list-1;
            NovaraLocal::page($id,"$list[$i]{id}: $list[$i]{file}");
        }
    } elsif ($text =~ /^!music(?:\s+(on|off))?\s*$/) {
        if (!defined $1) { NovaraLocal::page($id,'!music on/off; !tracks lists music; !settrack <ID|name>.'); return; }
        if ($1 eq 'off') { $state{$id}{on}=0; NovaraLocal::music($id,'off'); NovaraLocal::page($id,'Jukebox off for you.'); }
        else { next_track($id,1); }
    } elsif ($text =~ /^!(nexttrack|settrack)(?:\s+(.+?))?\s*$/) {
        my ($command,$arg)=($1,$2);
        if (time()-($state{$id}{last}//0)<($config{commandCooldown}//2)) {
            NovaraLocal::page($id,'Jukebox cooldown active.'); return;
        }
        if ($command eq 'nexttrack') { next_track($id,1); }
        elsif (defined $arg) {
            my @matches=grep { $arg =~ /^\d+$/ ? $_->{id}==$arg : index(lc($_->{file}),$arg)>=0 } enabled();
            if (@matches!=1) { NovaraLocal::page($id,'Track not found or ambiguous. Use !tracks and an exact ID.'); return; }
            play($id,$matches[0],1);
        } else { NovaraLocal::page($id,'Usage: !settrack <ID|name>.'); return; }
        $state{$id}{last}=time();
    }
}
1;
