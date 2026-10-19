"""Set the LAN default in this exact TT 4.8.4 FDS executable.

The original FDS compiles out [Settings]/Port loading. GameSpy mode would start
TT's separate broadcaster. EA cGameData's constructor sets Port (offset 0x210)
to 4848. Change only that immediate to retain LAN mode with Dragonade's query
socket. Refuse every other binary; do not use pattern scanning on future FDS
versions. No client executable is modified.

EA reference: Code/Commando/gamedata.cpp, cGameData constructor and Load_From_Ini.
Instruction at file offset 0x7128f: C7 85 10 02 00 00 F0 12 00 00
                                mov dword ptr [ebp+210h], 4848
"""
import hashlib
import struct

ORIGINAL_SHA256 = 'a31bddf29d07c5cc00203f5edf5d0a09c0269604eb1476050076202d7c7970b3'


def set_lan_port(data, port=35000):
    if hashlib.sha256(data).hexdigest() != ORIGINAL_SHA256:
        raise ValueError('Unsupported FDS executable; refusing port patch')
    if not 1024 <= port <= 65535:
        raise ValueError('Invalid game port')
    offset = 0x7128f
    if data[offset:offset + 10] != bytes.fromhex('c7 85 10 02 00 00 f0 12 00 00'):
        raise ValueError('Unexpected constructor instruction')
    result = bytearray(data)
    struct.pack_into('<I', result, offset + 6, port)
    return bytes(result)
