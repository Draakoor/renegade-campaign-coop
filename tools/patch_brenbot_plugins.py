"""Small, checked compatibility fixes for the pinned official social plugins."""
import re
import xml.etree.ElementTree as ET


def replace(text, before, after, count=1):
    if text.count(before) != count:
        raise ValueError('Pinned BRenBot plugin changed: ' + before[:80])
    return text.replace(before, after)


def configure(name, source, xml):
    """Retain upstream implementations and notices; expose coop-safe commands."""
    source = source.replace('\r\n', '\n')
    if name == 'joinmessages':
        source = replace(source, r'\S(.+)', r'\S.*')
        source = replace(source, 'substr( $1, 0, 249 )', 'substr( $1, 0, 160 )')
        source = replace(source, 'set_joinmessage( $args{\'nick\'}, $setjoin_msg );',
                         """if (lc($setjoin_msg) eq 'off') {
      my $name = lc($args{'nick'}); $name =~ s/'/''/g;
      plugin::execute_query("DELETE FROM join_messages WHERE LOWER(name) = '$name'", 1);
      plugin::pagePlayer($args{'nick'}, 'BRenBot', 'Your join message has been removed.');
      return;
    }
    set_joinmessage( $args{'nick'}, $setjoin_msg );""")
        allowed = {'setjoin', 'viewjoin'}
    elif name == 'seen':
        source = replace(source, 'abs(shift - shift)', 'abs(shift() - shift())')
        source = replace(source, '# Try a wildcard match\n  my @result',
                         '# Try a literal partial match\n  @result')
        # The upstream code creates plugin_seen but queries seen, and fails to
        # escape apostrophes. Keep the existing seen table if one already exists.
        source = replace(source,
                         'plugin_seen => "CREATE TABLE plugin_seen(id INT PRIMARY KEY, name CHAR(128), timestamp INT)"',
                         'seen => "CREATE TABLE seen(name TEXT PRIMARY KEY, timestamp INTEGER)"')
        source, count = re.subn(r'  \$name =~ s/";?[^;\n]+;', "  $name =~ s/'/''/g;", source)
        if count != 2:
            raise ValueError('Pinned Seen escaping changed')
        source = replace(source,
                         r'WHERE LOWER(name) LIKE \"%$name%\"',
                         "WHERE instr(LOWER(name), '$name') > 0")
        source = replace(source, '{ plugin::serverMsg($message); }',
                         "{ plugin::pagePlayer($args{'nick'}, 'BRenBot', $message); }")
        source = replace(source, '  return $string;', '  return $string || "0 seconds";')
        allowed = {'seen'}
    elif name == 'recommendations':
        source = replace(source, '  my(%args) = @_;', '  my %args = %{$_[0]};')
        source = replace(source, 'my $playerCount = (plugin::getGameStatus())[6];',
                         'my $playerCount = scalar(keys %NovaraLocal::players);')
        source = replace(source, '# Print header line\n  if ( $nPlayersWithRecs == 0 )',
                         '# Print header line\n  if ( 1 )')
        source = replace(source, '  my $count       = 0;', '  my $count       = 0;\n  my $total = 0;')
        source = replace(source, '    if ( ++$count >= 5 )', '    ++$total;\n    if ( ++$count >= 5 )')
        source = replace(source,
                         '  teamplayers_outputTeam($teamid, $string) if ( $string ne "" );',
                         '  teamplayers_outputTeam($teamid, $string) if ( $string ne "" );\n  return $total;')
        # Recommendations are a social score, never game money/admin rights.
        source = replace(source, "    my $reason = $2;", "    my $reason = substr($2, 0, 120);")
        source = replace(source, "  plugin::play_sound_for_player ( $name, 'bonus_complete.wav' );",
                         '  # No extra unmutable recommendation sound.')
        allowed = {'recommend', 'recommendations', 'teamplayers'}
    elif name == 'ping':
        # BRenBot command events provide nick; text events additionally carry id.
        source = replace(source, "sub ping\n{\n  my %args = %{$_[ ARG0 ]};",
                         """sub ping
{
  my %args = %{$_[ ARG0 ]};
  my ($found, %sender) = plugin::getPlayerData($args{'nick'});
  return unless $found == 1;
  $args{'id'} = $sender{'id'};""")
        command, own = source.split('sub report_own_ping', 1)
        command = replace(command,
                         "plugin::pagePlayer ( $player{'id'}, 'Your ping is currently '.$player{'ping'} );",
                         "plugin::pagePlayer ( $args{'id'}, 'BRenBot', $message );")
        own = replace(own,
                         "plugin::pagePlayer ( $player{'id'}, 'Your ping is currently '.$player{'ping'} );",
                         "plugin::pagePlayer ( $player{'id'}, 'BRenBot', 'Your ping is currently '.$player{'ping'} );")
        source = command + 'sub report_own_ping' + own
        allowed = {'ping'}
    elif name == 'fds_status_reporter':
        source = replace(source, 'use Net::FTP;', '# Local output only; FTP is disabled.')
        source = replace(source, '  uploadFile();', '  # Keep status in the private server volume.')
        source = replace(source, '$kernel->alarm(updateStatus => int(time()) + 15);',
                         '$kernel->alarm(updateStatus => int(time()) + 1);')
        source = replace(source, "$config{'updateInterval'}*60", "$config{'updateInterval'}")
        allowed = set()
    else:
        raise ValueError('Unknown pinned plugin')

    config = ET.fromstring(xml)
    for command in list(config.findall('command')):
        if command.get('name') not in allowed:
            config.remove(command)
            continue
        command.find('permission').set('level', '1')
        for group in list(command.findall('group')):
            if group.text != 'ingame':
                command.remove(group)
        for guard in list(command.findall('renguard')):
            command.remove(guard)
        if name == 'seen':
            command.find('syntax').set('value', '!seen <playername>')
        elif name == 'joinmessages' and command.get('name') == 'setjoin':
            command.find('syntax').set('value', '!setjoin <message|off>')
    if name == 'recommendations':
        for cvar in config.findall('./config/cvar'):
            cvar.set('value', '0')
        events = config.find('events')
        for event in list(events):
            if event.text not in ('command', 'mapload'):
                events.remove(event)
        config.remove(config.find('gamelog_regex_hooks'))
    elif name == 'ping':
        config.find('./config/cvar').set('value', '0')
    elif name == 'fds_status_reporter':
        for cvar in list(config.find('config')):
            if cvar.get('name') not in ('updateInterval', 'filename'):
                config.find('config').remove(cvar)
    return source, ET.tostring(config, encoding='utf-8', xml_declaration=True)
