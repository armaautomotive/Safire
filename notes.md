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

## Ephemeral Creator Handoff Protocol

Safire should have a live coordination layer separate from the permanent ledger. These messages are not blockchain records and do not make any block valid by themselves. They are hints that help the next creator and other peers see what is happening.

The current block creator has the best immediate view after creating a block because it knows:

- the parent block it used
- the final block contents and block hash
- the membership and heartbeat state after applying the block
- the next creator calculated from that new state

After creating and signing a block, the creator should send a `BLOCK_HANDOFF` message to peers, especially the next calculated creator.

Initial message shape:

```text
type: BLOCK_HANDOFF
block_number: generated block slot
block_hash: generated block hash
parent_hash: parent block hash
creator: current creator public key
next_slot: next slot being announced
next_creator: public key selected from state after this block
active_member_set_hash: hash of active members after this block
handoff_hash: hash of the handoff fields
signature: handoff_hash signed by current creator
```

Peers can validate the message by checking:

- the handoff hash matches the message fields
- the signature verifies against the creator public key
- if the block is known locally, the block hash, parent hash, and creator match the handoff
- once the block is available, applying it produces the claimed active member set and next creator

The handoff is advisory. The actual source of truth remains the block contents plus the parent chain state. A malicious or buggy creator can claim the wrong next creator, but peers should reject or ignore the claim after independent validation.

First implementation can use HTTP endpoints:

- `POST /api/handoff/submit` receives a handoff message
- `GET /api/handoff/latest` returns the last accepted handoff message for inspection

Later, this should become a direct P2P handoff path so the current creator can send the new block and handoff to the next creator quickly.

## Peer Discovery and Topology

For larger internet tests, the bootstrap server should act as a peer directory and first contact point, not as the authority for the chain. A new wallet should load the configured default peer, ask for known peers, verify each candidate peer against `/api/status`, and then keep a bounded set of healthy peers.

Public nodes can advertise themselves with a public URL. Peers should only cache announced nodes after checking that they are reachable and on the configured genesis chain. This keeps the graph from filling with stale, private, or wrong-chain URLs.

The node should avoid an all-to-all sync loop as the network grows. Keep a larger cache of known peers, but actively sync with the best-scored subset based on reachability, genesis match, height, and recent success. Slow or unavailable discovered peers can be purged after a few days; configured peers should stay pinned.

This topology gives us:
- discovery through the bootstrap node
- redundancy through multiple active peers
- lower latency propagation without making every client connect to every other client
- less dependence on `safire.org` once peers have discovered each other

## Security Test Backlog

Initial transfer nonce tests now cover a valid spend, duplicate nonce records in one block, skipped nonces, sequential nonces, and replayed historical transfers. The remaining test suite should cover competing blocks, peer sync/fork repair, and stale pending records received from peers.

## Transfer Nonces

New transfer records include a sender `nonce`. For new-format blocks that include `records_merkle_root`, validators require each sender's transfer nonce to be exactly one greater than the latest accepted nonce for that sender. This gives the ledger an explicit replay and double-spend guard beyond comparing record hashes.

The block builder also filters queued transfers before including them in a block. A pending transfer is ignored if it has no nonce, skips the expected nonce, reuses an accepted nonce, or would overspend the sender after earlier pending transfers in the same candidate block.

Older blocks without `records_merkle_root` remain readable with the legacy rules. For the current public test network, a chain reset is recommended so all new transfer activity starts with nonce-enforced blocks.

## Merkle Roots

First-stage Merkle support adds a `records_merkle_root` to new blocks. This root is calculated from the hashes of records in the block and lets nodes verify that the block hash commits to the record set without folding every record hash directly into the block hash.

Older blocks without `records_merkle_root` remain valid through the legacy block hash calculation. Later production work should add deterministic account state roots, checkpoint roots, and proof APIs for transactions and account state.

## Storage Profiles and Pruning

Wallets can expose storage profiles for server, desktop, and mobile devices. The carry-forward cadence is a network-level policy and must be the same for all nodes. The profiles should only control how much raw block history a node keeps locally after the shared carry-forward policy has produced enough accepted checkpoints.

Carry-forward records can now be created on a monthly period by every profile. Server nodes should still keep full history, but monthly carry-forwards help smaller devices converge on a smaller working set. Desktop and mobile profiles can target shorter retained histories, such as one year and three months, without changing what carry-forward records are valid.

Physical pruning should not delete old raw blocks until the client can rebuild balances and validation state from genesis plus accepted carry-forward/checkpoint records. The safe pruning milestone is a state-checkpoint reader that proves the post-prune state root matches the unpruned chain before deleting older records.
