# Safire Notes

## Project Goal

Safire's main purpose is to build an elegant peer-to-peer payment system for real-world transactions: fast, reliable, and prestigious enough to feel trustworthy in serious use.

The system should avoid mining if possible, and should deliberately address the common downsides and criticisms of digital currencies. The goal is not only to move value, but to make a currency network that feels practical, efficient, resilient, and worthy of everyday confidence.

## Current Challenges / Blockers

### Reliable Block Creator Selection Without Mining

The current time-and-membership-hash approach is intended to avoid mining by selecting a block creator from known network members. The blocker is that network availability is not reliable or knowable in advance. A node can appear eligible, but there is no deterministic way to prove it will be online, reachable, synchronized, and ready to create the block when its slot arrives.

Bitcoin mining avoids this particular problem because the miner proves work after doing it. The network does not need to predict which miner will be available ahead of time. Anyone can attempt the work, and everyone else can verify the resulting hash cheaply. Safire needs an alternative that preserves the no-mining goal while still giving the network a reliable, verifiable way to make progress.

## Blockchain Pruning And Carry Forward Records

When Safire purges old blockchain data to save disk space, nodes should keep the genesis block and accepted `CARRY_FORWARD` records.

The goal is to let nodes discard bulky historical transaction data while still preserving enough chain-state anchors to compare correctness with other nodes. The genesis block provides the original chain identity, and carry-forward records provide consolidated account state at later points in the chain.

This design needs review before implementation details are finalized, especially around:

- how often carry-forward records are required
- which records must be retained forever
- how nodes prove a pruned state is still connected to the original genesis chain
- how peers compare state when one node is fully historical and another is pruned

## Staggered Epoch Creator Selection

Safire should select block creators from an older finalized/checkpointed view of the chain instead of the immediately previous block. This prevents a node from needing the absolute latest block before it can know whether it is scheduled to create an upcoming block.

Current proof-of-concept constants:

- `epoch_size_blocks = 100`
- `selection_lag_epochs = 2`

For a target block slot:

```text
current_epoch = (target_block - genesis_block) / epoch_size_blocks
selection_epoch = current_epoch - selection_lag_epochs
selection_boundary = end of selection_epoch
selection_checkpoint = latest accepted block at or before selection_boundary
creator_set = active heartbeat members at selection_boundary
creator = hash(selection_checkpoint.hash + target_block) over creator_set
```

The first two epochs bootstrap from the genesis checkpoint. At 15 second slots, a 100 block epoch is about 25 minutes, so a join or heartbeat generally has about one epoch of propagation time before it affects creator selection.

This design intentionally allows skipped slots. If no blocks are created for some time, the next valid creator can build on the latest accepted parent it knows, while creator selection still comes from the older staggered checkpoint. A full outage across an epoch should recover as long as nodes still share the last finalized checkpoint and reconnect to peers.

## Heartbeat Eligibility Liveness Risk

There is a potential liveness failure in using accepted `HEARTBEAT` records as the gate for block creator eligibility. If the chain reaches a state where there are no valid heartbeat entries in the selection window, the hashing function may have no eligible creator to choose.

In that case, the chain could effectively become orphaned or frozen: no node is authorized to create the next block, so no new `HEARTBEAT`, `JOIN_NETWORK`, transfer, carry-forward, or recovery transaction can be added. Existing funds would also be stuck because transactions require a future block to be accepted.

Possible causes include:

- a network outage that prevents heartbeat records from being created or propagated
- too many clients going offline or disabling creator mode
- malicious users joining, advertising availability, and then failing to produce future heartbeat continuity
- a bug or fork that excludes otherwise valid heartbeat records from the accepted chain
- an overly short heartbeat eligibility window

This needs a consensus-level recovery rule before production. Possible directions to review later include a longer eligibility window, fallback to the last known eligible creator set, emergency recovery from an older checkpoint, genesis/foundation recovery keys, or allowing a bounded recovery block type when no eligible heartbeat set exists. Any recovery rule must be deterministic and hard to abuse, because it affects who can create blocks and move the chain forward.

## Synchronous Block Slots vs Asynchronous Networking

Safire's block schedule is time-based and deterministic, but the peer-to-peer network is asynchronous. Every block creator needs the latest accepted parent block before it can safely create the next block. That creates a tight latency budget:

- the previous block must propagate across the peer network
- the next creator must receive it
- the next creator must validate the parent and update local state
- the node must select valid pending records from the mempool
- the node must construct the next block, record hashes, Merkle root, and block hash
- the node must broadcast the new block quickly enough for the following creators to continue

This means a short block time can fail even if the consensus rules are correct. The network can stall simply because the selected node did not receive and validate the latest parent in time.

The protocol should make as much work asynchronous as possible while keeping block validity deterministic. Possible directions:

- continuously gossip pending records so upcoming creators already have similar mempools
- use the staggered epoch schedule to identify upcoming creators early
- prioritize direct peer connections and block relay to upcoming creators
- send compact block announcements first, followed by missing records only when needed
- allow peers to pre-build candidate block templates from known mempool records
- use creator handoff messages to push the new block directly to the next likely creators
- measure propagation delay and choose a production block interval based on real network data

The final block cannot be fully asynchronous because it must commit to exactly one parent hash and one canonical state transition. The asynchronous layer should therefore be treated as acceleration and coordination, not authority. A node should only publish a block after validating the parent chain and should reject any block whose parent, creator, records, or hashes do not match deterministic consensus rules.

## Block Creation Requires Checkpoint-State Consensus

Safire block creator selection must be based on the accepted chain state at the staggered selection checkpoint.

This matters because recent blocks can change the set of eligible creators. For example, a block may include `JOIN_NETWORK`, `HEARTBEAT`, or other membership/status records. If those records affected the very next slot, nodes that have not received the latest block yet may calculate different creators. The staggered checkpoint gives the network time to propagate and agree before membership changes affect consensus.

A block should be valid only if:

- its `previous_hash` references a known accepted parent block
- its creator was selected from the active member set at the selection checkpoint
- the selector input includes the selection checkpoint hash and the new block number

Conceptually:

```text
selection_checkpoint = latest accepted block at or before the staggered epoch boundary
state = ledger and membership state at selection_checkpoint
expected_creator = hash(selection_checkpoint.hash + target_block_number) over state.active_members
block.creator must equal expected_creator
```

A node should not create a block merely because it appears selected from its local tip. It should first have sync confidence that its local selection checkpoint is on the configured chain and that it is not knowingly behind peers. At minimum for the proof of concept, block creation should require:

- local genesis matches configured genesis
- local chain validates
- local selection checkpoint matches peers
- network time offset is sane
- this wallet is selected using the staggered checkpoint rule

If peers disagree about the selection checkpoint hash, or if the node knows it is behind the checkpoint needed for the current epoch, it should wait and sync instead of creating a block.

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

## Rolling Validation Checkpoints

A years-old chain should not require every recovery path to scan and validate from genesis. Nodes should maintain a buried validated checkpoint, then treat recovery as:

- prove the checkpoint block/hash still matches the canonical local index
- validate block envelopes and parent links from the checkpoint to the tip
- fall back to a full rebuild from genesis if the checkpoint is missing, stale, forked, or inconsistent

This does not replace consensus validation for new blocks. It is a performance anchor for finalized local history, so common auto-recovery scales with recent history instead of total chain age. Later production work should add signed/state-root checkpoints so pruned nodes can prove balances, membership, and accepted records without retaining every historical block.

## Local Simulation Harness

A local simulation harness can run many Safire processes in isolated directories under `sim/<name>/`. Each virtual node has its own wallet, queue, peer cache, config, and block database while sharing the same compiled binary.

The first harness is script-driven with `scripts/sim-network.sh`. It supports line, star, ring, mesh, and partition topologies, starts a fresh genesis node, copies the generated chain identity to the other nodes, joins wallets, monitors convergence, submits random payments, and deletes the temporary simulation directory for `run` mode.

This style tests the real process, HTTP API, LevelDB locking, peer sync, block production, and wallet behavior. Later production-grade tests should add deterministic seeds, repeatable traffic scripts, latency/drop simulation, partition healing, and machine-readable pass/fail assertions.

## Database-Backed Mempool

Pending records should live in the block database instead of only in `queue.dat`. The first implementation stores records in LevelDB with ordered `mempool:record:<sequence>:<hash>` keys and a `mempool:hash:<hash>` index for duplicate suppression.

`queue.dat` is now a legacy import/fallback path. Nodes still read it so older pending records are not lost after an upgrade, and block building clears it after consuming pending records. When LevelDB opens normally, new pending records should be written to the database-backed mempool.
