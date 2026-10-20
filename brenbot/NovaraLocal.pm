# SPDX-License-Identifier: GPL-3.0-or-later
# Local adapter for BRenBot 1.56. No IRC, RenRem or SSGM network connection.
package NovaraLocal;
use strict;
use warnings;
use POE;
use JSON::PP qw(decode_json);
use Fcntl qw(:flock SEEK_SET SEEK_END);
use File::Path qw(make_path);
use File::Basename qw(dirname);
use File::Copy qw(copy);
our %players;
our @plugins = qw(funsounds novara_coop joinmessages seen recommendations jukebox ping custom_commands htmloutput fds_status_reporter);
our $map = 'unknown';
my $maxplayers=8;
my $server = $ENV{NOVARA_SERVER} or die 'NOVARA_SERVER is required';

sub page {
    my ($id, $text) = @_;
    my $p = $players{$id} or return;
    $text =~ s/[\x00-\x1f\x7f]/ /g;
    write_command("page $p->{session} $id ".substr($text, 0, 240));
}
sub sound {
    my ($id, $file) = @_;
    my $p = $players{$id} or return;
    return unless $file =~ /\A[A-Za-z0-9_-][A-Za-z0-9_.-]{0,120}\.wav\z/i && $file !~ /\.\./;
    write_command("sound $p->{session} $id $file");
}
sub music {
    my ($id,$file)=@_;
    my $p=$players{$id} or return;
    return unless $file eq 'off' || $file =~ /\A[a-z0-9 &_-]+\.mp3\z/;
    write_command("music $p->{session} $id $file");
}
sub write_command {
    my ($line) = @_;
    die 'Invalid local command' if $line =~ /[\r\n]/;
    open(my $out, '>>:encoding(cp1252)', "$server/brenbot.in") or die "Local bridge: $!";
    flock($out, LOCK_EX) or die "Local bridge lock: $!";
    print $out "$line\n";
    close($out) or die "Local bridge write: $!";
}
sub install {
    no warnings 'redefine';
    # These overrides are installed before any BRenBot startup functions run.
    *brIRC::init = sub {};
    *brIRC::ircmsg = sub {};
    *ssgm::start = sub {};
    *renlog::init = sub {};
    *serverStatus::init = sub {};
    *plugin::getGameStatus = sub {
        my ($gdi,$nod,$gscore,$nscore)=(0,0,0,0);
        for my $p (values %players) {
            if ($p->{team}==1) { ++$gdi; $gscore+=$p->{score}//0; }
            else { ++$nod; $nscore+=$p->{score}//0; }
        }
        return ('Coop',$map,$gdi,$gscore,$nod,$nscore,scalar(keys %players),$maxplayers,'unlimited','unknown');
    };
    *modules::AutoAnnounce_Init = sub {};
    *brconfig::GetAvailableMaps = sub {};
    *renlog::check_banlist = sub { return 0; };
    *modules::check_kicks = sub { return 0; };
    *brconfig::read_server_configuration = sub {};
    *brconfig::read_ssgm_settings = sub {
        $brconfig::ssgm_version = 4.1;
        $brconfig::serverScriptsVersion = 4.8;
        $brconfig::gamelog_in_ssgm = 0;
    };
    my $check_modules = \&modules::check_modules;
    *modules::check_modules = sub {
        $check_modules->();
        brdatabase::execute_query('UPDATE modules SET status=0', 1);
        modules::load_modules();
    };
    *RenRem::RenRemCMD = sub {
        my ($command) = @_;
        # The bot cannot grant admin rights, change a map or kick players.
        if ($command =~ /^msg (.+)$/s) { page($_, $1) for keys %players; }
        elsif ($command =~ /^(?:ppage|pamsg) (\d+) (.+)$/s) { page($1, $2); }
        elsif ($command =~ /^sndp (\d+) ([\w.-]+\.wav)$/i) { sound($1, $2); }
        elsif ($command =~ /^snda ([\w.-]+\.wav)$/i) { sound($_, $1) for keys %players; }
    };
}
sub backup_social_database {
    return if brdatabase::get_global('novara_social_schema_v1');
    my $dir = "$server/../backups/brenbot-social-".time()."-$$";
    make_path($dir);
    $brdatabase::dbh->sqlite_backup_to_file("$dir/brenbot.dat")
        or die 'Could not back up BRenBot database';
    brdatabase::set_global('novara_social_schema_v1', 1);
    print "[Novara] Social plugin database backup: $dir/brenbot.dat\n";
}
sub prepare_catalogs {
    my $core=dirname(__FILE__);
    for my $name ('jukebox.json','custom_commands.json') {
        copy("$core/$name",$name) or die "Could not initialize $name: $!" unless -e $name;
    }
    return if -e 'funsounds-extra-v1.applied';
    open(my $old,'<:raw','funsounds.json') or die "funsounds.json: $!";
    my $sounds=do { local $/; decode_json(<$old>) }; close($old);
    open(my $extra,'<:raw',"$core/funsounds-extra.json") or die "Extra sounds: $!";
    my $more=do { local $/; decode_json(<$extra>) }; close($extra);
    my $backup="$server/../backups/funsounds-".time()."-$$";
    make_path($backup);
    copy('funsounds.json',"$backup/funsounds.json") or die "Funsounds backup: $!";
    for my $key (keys %$more) { $sounds->{$key}=$more->{$key} unless exists $sounds->{$key}; }
    open(my $out,'>:raw','funsounds.json.new') or die "Funsounds update: $!";
    print $out JSON::PP->new->canonical->pretty->utf8->encode($sounds); close($out) or die $!;
    rename('funsounds.json.new','funsounds.json') or die $!;
    open(my $marker,'>','funsounds-extra-v1.applied') or die $!;
    print $marker "120 additional defaults merged; operator entries preserved.\n"; close($marker);
    print "[Funsounds] Extended catalog merged; backup: $backup/funsounds.json\n";
}
sub plugin_event {
    my ($name, $p) = @_;
    my %args = (id=>$p->{id}, nick=>$p->{name}, name=>$p->{name},
                teamid=>$p->{team}, session=>$p->{session});
    for my $plugin (plugin::get_plugin_event($name)) {
        $main::poe_kernel->post("plugin_$plugin", $name, \%args);
    }
}
sub leave_player {
    my ($id) = @_;
    my $old = delete $players{$id} or return;
    plugin_event('playerleave', $old);
    playerData::clearPlayerData($id);
    funsounds::forget($id) if defined &funsounds::forget;
    jukebox::forget($id) if defined &jukebox::forget;
}
sub update_player {
    my ($p) = @_;
    return unless ref($p) eq 'HASH' && $p->{id} =~ /^\d+$/ && $p->{id} > 0 && $p->{id} < 128
        && $p->{session} =~ /^\d+$/ && defined($p->{name}) && $p->{team} =~ /^-?\d+$/;
    my $old = $players{$p->{id}};
    my $joined = !$old || $old->{session} != $p->{session} || $old->{name} ne $p->{name};
    if ($joined) {
        leave_player($p->{id}) if $old;
        funsounds::forget($p->{id}) if defined &funsounds::forget;
    }
    $players{$p->{id}} = $p;
    playerData::updatePlayerData($p->{id}, $p->{name}, $p->{score}//0, $p->{team} == 1 ? 'GDI' : 'Nod', $p->{ping}//'unknown', '127.0.0.1', 0, '00.00.00');
    playerData::setKeyValue($p->{id},'stats_kills',$p->{kills}//0);
    playerData::setKeyValue($p->{id},'stats_deaths',$p->{deaths}//0);
    jukebox::sync_player($p,$old) if defined &jukebox::sync_player;
    plugin_event('playerjoin', $p) if $joined;
    return 1;
}
sub event {
    my ($row) = @_;
    if ($row->{event} eq 'players') {
        my %present;
        for my $p (@{$row->{players}}) { $present{$p->{id}}=1 if update_player($p); }
        for my $id (keys %players) {
            next if $present{$id};
            leave_player($id);
        }
    } elsif ($row->{event} eq 'leave') {
        my $p = $players{$row->{id}};
        leave_player($row->{id}) if $p && $p->{session} == $row->{session};
    } elsif ($row->{event} eq 'join') {
        update_player($row);
    } elsif ($row->{event} eq 'map') {
        $map = $row->{map}; serverStatus::updateMap($map);
        for my $plugin (plugin::get_plugin_event('mapload')) {
            $main::poe_kernel->post("plugin_$plugin", 'mapload', {map=>$map});
        }
    } elsif ($row->{event} eq 'chat' && ($row->{scope} eq 'public' || $row->{scope} eq 'team')) {
        return if !defined($row->{message}) || length($row->{message})>300
            || $row->{message} =~ /[\x00-\x1f\x7f]/;
        return unless update_player($row);
        # The engine supplies identity separately from text; names cannot spoof logs.
        renlog::handle_chat($row->{name}, $row->{message}, $row->{scope} eq 'team');
    }
}
sub start {
    prepare_catalogs();
    if (open(my $cfg,'<',"$server/data/svrcfg_cnc.ini")) {
        while (<$cfg>) { $maxplayers=$1 if /^MaxPlayers\s*=\s*(\d+)/i; }
        close($cfg);
    }
    for my $name (@plugins) {
        die "BRenBot plugin $name failed" unless plugin::get_state($name)==1 || plugin::load_plugin($name)==1;
    }
    if (open(my $active, '<', "$server/active-map.txt")) {
        chomp($map=<$active>); close($active);
    }
    my $path = "$server/brenbot-events.jsonl";
    my $offset = -e $path ? -s $path : 0; # Never replay earlier chat on bot restart.
    POE::Session->create(inline_states => {
        _start => sub { $_[KERNEL]->yield('poll'); },
        poll => sub {
            if (open(my $in, '<:raw', $path)) {
                $offset=0 if -s $in < $offset;
                seek($in, $offset, SEEK_SET);
                my $count=0;
                while (my $line=<$in>) {
                    last unless $line =~ /\n\z/;
                    $offset=tell($in);
                    eval { event(decode_json($line)); 1 } or warn "BRenBot bridge event rejected: $@";
                    last if ++$count>=100;
                }
                close($in);
            }
            $_[KERNEL]->delay('poll', 0.2);
        }
    });
    print "[Novara] BRenBot ready: ".join(' + ', @plugins)."; local files only, IRC disabled.\n";
}
1;
