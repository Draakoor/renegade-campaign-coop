package NovaraNoNetwork;
use IO::Socket::INET;
use IO::Socket::IP;
BEGIN {
    no warnings 'redefine';
    *IO::Socket::INET::new = sub { die 'TEST: BRenBot attempted a network connection'; };
    *IO::Socket::IP::new = sub { die 'TEST: BRenBot attempted a network connection'; };
}
1;
