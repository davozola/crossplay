/* The wallpaper pack, carried in the firmware where there is room for it.
 *
 * The Sticky has one 15.9 MB app slot and takes the pack whole; the X4 Pro's
 * two slots are too full for it, and there FONDOS reads /fondos.pack from the
 * card instead. See FondosActivity.cpp.
 */
#if defined(FONDOS_EMBEDDED)
    .section .rodata.fondos_pack,"a"
    .balign 4
    .global fondos_pack_start
fondos_pack_start:
    .incbin "src/apps_local/fondos/fondos.pack"
    .global fondos_pack_end
fondos_pack_end:
    .balign 4
#endif
