/*
 * ESH command section fragment.
 * Include this inside the consuming firmware linker script's SECTIONS block.
 */
.esh_cmd ALIGN(4) :
{
    __esh_cmd_start = .;
    KEEP(*(SORT_BY_NAME(.esh_cmd*)))
    __esh_cmd_end = .;
}
