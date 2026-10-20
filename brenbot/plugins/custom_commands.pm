# SPDX-License-Identifier: GPL-3.0-or-later
# Reply-only coop adaptation of BRenBot's custom commands concept.
package custom_commands;
use strict;
use warnings;
use POE;
use JSON::PP qw(decode_json);
use NovaraLocal;
our ($plugin_name, %config, %additional_events);
my (%replies,%last);
sub start {
    open(my $in,'<:raw','custom_commands.json') or die "custom_commands.json: $!";
    local $/; %replies=%{decode_json(<$in>)}; close($in);
    my %reserved=map {$_=>1} qw(help commands coophelp extras rotation scripts version sounds taunts taunt sound
        disablesounds enablesounds tracks jukebox music nexttrack settrack seen setjoin viewjoin recommend rec
        recommendations recs teamplayers tp ping buy heal cp goto skins skin donate tdonate admin adminhelp
        credits givecredits vehicles vehicle spawnvehicle weapons giveweapon giveammo map changemap nextmap restartmap kick);
    for my $key (keys %replies) {
        die "Reserved or invalid custom command: $key" if $reserved{$key} || $key!~/^[a-z][a-z0-9_]{0,31}$/;
        die 'Invalid custom reply' unless ref($replies{$key}) eq 'ARRAY' && @{$replies{$key}}<=5
            && !grep {ref($_) || length($_)>220 || /[\r\n]/} @{$replies{$key}};
    }
    print "[Custom commands] Loaded reply-only server information commands.\n";
}
sub stop {}
sub text {
    my $a=$_[ARG0]; return unless lc($a->{message}) =~ /^!(\w+)\s*$/;
    my $key=$1; my $lines=$replies{$key} or return;
    return if time()-($last{$a->{id}}//0)<2;
    $last{$a->{id}}=time();
    NovaraLocal::page($a->{id},$_) for @$lines;
}
1;
