# SPDX-License-Identifier: GPL-3.0-or-later
# BRenBot Funsounds integration, inspired by upstream Funsounds 1.02 (idea: zunnie).
# The original keyword/WAV mapping is fetched from the pinned upstream archive.
package funsounds;
use strict;
use warnings;
use POE;
use JSON::PP qw(decode_json);
use Time::HiRes qw(time);
use NovaraLocal;
our ($plugin_name, %config, %additional_events);
my (%sounds, %muted, %last);
my $global=0;
sub start {
    open(my $in, '<:raw', 'funsounds.json') or die "funsounds.json: $!";
    local $/; my $data=decode_json(<$in>); close($in);
    while (my ($key,$file)=each %$data) {
        die 'Invalid funsound mapping' unless length($key)<=40 && $key !~ /[\r\n]/
            && $file =~ /\A[A-Za-z0-9_-][A-Za-z0-9_.-]{0,120}\.wav\z/i && $file !~ /\.\./;
        $sounds{lc $key}=$file;
    }
    print '[Funsounds] Loaded '.scalar(keys %sounds)." chat/taunt sounds.\n";
}
sub stop {}
sub forget { my ($id)=@_; delete $muted{$id}; delete $last{$id}; }
sub play {
    my ($args,$key,$explicit)=@_;
    my $id=$args->{id};
    return unless exists $sounds{$key} && !$muted{$id};
    my $now=time();
    if ($now-($last{$id}//0)<$config{player_cooldown} || $now-$global<$config{global_cooldown}) {
        NovaraLocal::page($id,'Sound cooldown active.') if $explicit;
        return;
    }
    for my $to (values %NovaraLocal::players) {
        next if $muted{$to->{id}};
        next if $args->{messagetype} eq 'team' && $to->{team}!=$NovaraLocal::players{$id}->{team};
        NovaraLocal::sound($to->{id},$sounds{$key});
    }
    $last{$id}=$now; $global=$now;
}
sub text {
    my $args=$_[ARG0]; my $text=lc $args->{message}; my $id=$args->{id};
    return unless $args->{messagetype} eq 'public' || $args->{messagetype} eq 'team';
    if ($text =~ /^!(?:sounds|taunts)(?:\s+(\S+))?\s*$/) {
        my $arg=$1//1;
        if ($arg eq 'off' || $arg eq 'on') { $muted{$id}=$arg eq 'off'; NovaraLocal::page($id,"Fun sounds $arg for you."); return; }
        if ($arg !~ /^\d{1,4}$/ || $arg<1) { NovaraLocal::page($id,'Use !sounds [page|on|off].'); return; }
        my @keys=sort keys %sounds; my $pages=int((@keys+11)/12)||1; $arg=$pages if $arg>$pages;
        my $end=$arg*12-1; $end=$#keys if $end>$#keys;
        NovaraLocal::page($id,"Sounds $arg/$pages: ".join(', ',@keys[($arg-1)*12..$end]));
        NovaraLocal::page($id,'!taunt <keyword> plays a sound. Chat keywords also work. !sounds off/on.');
    } elsif ($text =~ /^!(?:disablesounds|enablesounds)\s*$/) {
        $muted{$id}=$text =~ /disable/ ? 1:0;
        NovaraLocal::page($id,$muted{$id} ? 'Fun sounds off for you.' : 'Fun sounds on.');
    } elsif ($text =~ /^!(?:taunt|sound)\s+(.+?)\s*$/) {
        if (exists $sounds{$1}) { play($args,$1,1); }
        else { NovaraLocal::page($id,'Unknown sound. !sounds lists the keywords.'); }
    } elsif ($text !~ /^!/) {
        return if $args->{messagetype} eq 'public' && !$config{trigger_f2chat};
        return if $args->{messagetype} eq 'team' && !$config{trigger_f3chat};
        for my $key (sort {length($b)<=>length($a) || $a cmp $b} keys %sounds) {
            if ($text =~ /(?<![a-z0-9_])\Q$key\E(?![a-z0-9_])/) { play($args,$key,0); last; }
        }
    }
}
1;
