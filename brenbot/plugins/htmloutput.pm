# SPDX-License-Identifier: GPL-3.0-or-later
# Local-only adaptation of danpaul88's BRenBot HTML Output plugin.
package htmloutput;
use strict;
use warnings;
use POE;
use File::Path qw(make_path);
use POSIX qw(strftime);
use NovaraLocal;
our ($plugin_name, %config, %additional_events);
sub escape_html {
    my $s=shift//'';
    $s =~ s/&/&amp;/g; $s =~ s/</&lt;/g; $s =~ s/>/&gt;/g; $s =~ s/"/&quot;/g; $s =~ s/'/&#39;/g;
    return $s;
}
sub record {
    my $message=shift;
    my $file='HTML_Game_Logs/'.strftime('%Y-%m-%d',gmtime()).'.html';
    return if -e $file && -s $file > 5*1024*1024;
    my $new=!-e $file;
    open(my $out,'>>:encoding(UTF-8)',$file) or die "HTML log: $!";
    print $out "<!doctype html><meta charset=\"utf-8\"><title>Novara Coop events (UTC)</title>\n" if $new;
    print $out '<p>'.escape_html(strftime('%H:%M:%S',gmtime()).' ['.$NovaraLocal::map.'] '.$message)."</p>\n";
    close($out);
}
sub start { make_path('HTML_Game_Logs'); print "[HTML Output] Private volume logs enabled; public chat only.\n"; }
sub stop {}
sub playerjoin { record($_[ARG0]->{nick}.' joined.'); }
sub playerleave { record($_[ARG0]->{nick}.' left.'); }
sub mapload { record('Map loaded.'); }
sub text {
    my $a=$_[ARG0];
    return unless $a->{messagetype} eq 'public';
    return if $a->{message}=~/^!/; # Do not retain player commands or possible credentials.
    record($a->{nick}.': '.$a->{message});
}
1;
