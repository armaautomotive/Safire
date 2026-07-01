# BlockDB Schema

Safire stores blocks in LevelDB. Block identity is the block hash, not the
time-slot block number. Multiple candidate blocks may exist for the same block
number.

## Primary Keys

- `block:<hash>` stores the serialized block JSON.
- `slot:<number>:<hash>` records that a block hash exists for a time slot.
- `canonical:<number>` stores the selected canonical hash for a block number.
- `next:<parent_hash>` stores the selected canonical child hash for an exact
  parent block hash. Chain traversal should prefer this key because block
  numbers alone are ambiguous during forks.

## Chain Metadata

- `chain:schema_version` stores the BlockDB schema version. Version `2` adds
  canonical chain metadata and hash-based child indexes while keeping the
  legacy keys below.
- `chain:first_block` stores the configured genesis slot.
- `chain:latest_block` stores the canonical connected tip slot.
- `chain:latest_hash` stores the canonical connected tip hash.
- `chain:connected_latest` caches the most recent block reachable from genesis
  by canonical child links.

## Compatibility Keys

- `b_<number>` mirrors the canonical block JSON for older code paths.
- `next_block_<number>` mirrors the canonical next block number for older chain
  scans.
- `first_block_id` and `latest_block_id` mirror the canonical chain slot numbers
  for older commands and APIs.
- Legacy `bf_<number>_<hash>` fork entries may still be read by repair/scanning
  code during the testnet transition.

## Migration Rules

When a node opens an older database, Safire copies `first_block_id` and
`latest_block_id` into `chain:first_block` and `chain:latest_block`, derives
`chain:latest_hash` from the canonical block when possible, and copies valid
legacy next links into `next:<parent_hash>`. The migration is additive and does
not delete existing chain data.

## Repair Rules

`repairchain` rebuilds canonical indexes from stored block variants. It chooses:

1. the branch with the most connected valid blocks,
2. then the branch with the later block number,
3. then a deterministic hash tie-breaker for same-slot block variants.

Blocks with impossible parent links, such as `previous_block_id >= number`, are
rejected or ignored.
