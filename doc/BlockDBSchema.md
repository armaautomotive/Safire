# BlockDB Schema

Safire stores blocks in LevelDB. Block identity is the block hash, not the
time-slot block number. Multiple candidate blocks may exist for the same block
number.

## Primary Keys

- `block:<hash>` stores the serialized block JSON.
- `slot:<number>:<hash>` records that a block hash exists for a time slot.
- `canonical:<number>` stores the selected canonical hash for a block number.
- `next_block_<number>` stores the canonical next block number for compatibility
  with existing chain scans.
- `first_block_id` and `latest_block_id` store canonical chain slot numbers for
  compatibility with existing commands and APIs.

## Compatibility Keys

- `b_<number>` mirrors the canonical block JSON for older code paths.
- Legacy `bf_<number>_<hash>` fork entries may still be read by repair/scanning
  code during the testnet transition.

## Repair Rules

`repairchain` rebuilds canonical indexes from stored block variants. It chooses:

1. the branch with the most connected valid blocks,
2. then the branch with the later block number,
3. then a deterministic hash tie-breaker for same-slot block variants.

Blocks with impossible parent links, such as `previous_block_id >= number`, are
rejected or ignored.
