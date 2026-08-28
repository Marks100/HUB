"""hub_bootloader_xor.py — Seed/key plugin for this project's own FBL bootloader.

Matches FBL's placeholder algorithm, used whenever no real security_calculate_key_func_p is
wired (see FBL_security_verify_key() in xCOMMON_MODULES/Src/FBL/FBL.c, and INTEGRATION_STUBS.c's
.security_calculate_key_func_p = NULL_P for this target):

    key = seed XOR 0xA5A5A5A5   (FBL_SECURITY_KEY_XOR_MASK, see FBL.h)

This is a placeholder algorithm, not a real challenge/response scheme - it exists to exercise
the seed/key exchange end to end, not to resist anyone who can read FBL.h. Replace this plugin
if/when FBL is ever wired to a real security_calculate_key_func_p.

Canonical copy of this file also lives in the CAN_FLASH tool's own seedkeyplugin/ directory;
this one is what HUB's own build bundles into its flash pack (see APP/Makefile's
FLASH_PACK_SEEDKEY and scripts/create_flash_pack) so that build has no dependency on the
separate CAN_FLASH project directory existing.
"""

FBL_SECURITY_KEY_XOR_MASK = 0xA5A5A5A5


def compute_key(seed, seed_subf):
    """seed is bytes (modern security.py call) or int (legacy call) - handle both."""
    if isinstance(seed, (bytes, bytearray, memoryview)):
        seed_int = int.from_bytes(bytes(seed), "big")
    else:
        seed_int = int(seed)

    return (seed_int ^ FBL_SECURITY_KEY_XOR_MASK) & 0xFFFFFFFF
