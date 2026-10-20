# SPDX-License-Identifier: GPL-3.0-or-later
package novara_coop;
use strict;
use warnings;
use POE;
use NovaraLocal;
our ($plugin_name, %config, %additional_events);
sub start { print "[Novara] Unified command help loaded.\n"; }
sub stop {}
sub text {
    my $a=$_[ARG0]; my $id=$a->{id}; my $text=lc $a->{message};
    if ($text =~ /^!(?:help|commands|coophelp)\s*$/) {
        NovaraLocal::page($id,'Coop: !buy, !heal, !cp, !goto <player>, !skins, !donate <player> <credits>, !tdonate <credits>.');
        NovaraLocal::page($id,'Fun: !sounds, !taunt <keyword>, !sounds off/on. Info: !rotation, !version, !scripts. Admin: !admin, !adminhelp.');
    } elsif ($text =~ /^!extras\s*$/) {
        NovaraLocal::page($id,'!skins [search] [page], !skin <ID|model>, !skin default, !sounds, !taunt <keyword>, !sounds off/on.');
    } elsif ($text =~ /^!rotation\s*$/) {
        NovaraLocal::page($id,'Rotation: M01 -> M02 -> M03 -> M04 -> M05 -> M06 -> M07 -> M08 -> M09 -> M10 -> M11 -> M01. Current: '.$NovaraLocal::map);
    } elsif ($text =~ /^!scripts\s*$/) {
        NovaraLocal::page($id,'TT 4.8.4 / Dragonade 1.11.1 / CoopBeta 4.2.4 + Novara campaign plugin. BRenBot: funsounds, novara_coop.');
    }
}
1;
