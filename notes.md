# Safire Notes

## Blockchain Pruning And Carry Forward Records

When Safire purges old blockchain data to save disk space, nodes should keep the genesis block and accepted `CARRY_FORWARD` records.

The goal is to let nodes discard bulky historical transaction data while still preserving enough chain-state anchors to compare correctness with other nodes. The genesis block provides the original chain identity, and carry-forward records provide consolidated account state at later points in the chain.

This design needs review before implementation details are finalized, especially around:

- how often carry-forward records are required
- which records must be retained forever
- how nodes prove a pruned state is still connected to the original genesis chain
- how peers compare state when one node is fully historical and another is pruned

## Block Creation Requires Parent-State Consensus

Safire block creator selection must be based on the accepted chain state after applying the parent block.

This matters because the latest block can change the set of eligible creators. For example, a block may include `JOIN_NETWORK`, `HEARTBEAT`, or other membership/status records. If a node has not received that block yet, it may calculate the next block creator from stale membership data.

A block should be valid only if:

- its `previous_hash` references the accepted parent block
- its creator was selected from the active member set after applying that parent block
- the selector input includes the parent block hash and the new block number

Conceptually:

```text
parent = accepted previous block
state = ledger and membership state after parent
expected_creator = hash(parent.hash + next_block_number) over state.active_members
block.creator must equal expected_creator
```

A node should not create a block merely because it appears selected from its local tip. It should first have sync confidence that its local tip is the best known parent. At minimum for the proof of concept, block creation should require:

- local genesis matches configured genesis
- local chain validates
- local latest block matches the best known peer tip/hash
- network time offset is sane
- this wallet is selected using the parent-state rule

If peers disagree about the latest parent hash, or if the node knows it is behind, it should wait and sync instead of creating a block.
