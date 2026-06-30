# Safire Notes

## Blockchain Pruning And Carry Forward Records

When Safire purges old blockchain data to save disk space, nodes should keep the genesis block and accepted `CARRY_FORWARD` records.

The goal is to let nodes discard bulky historical transaction data while still preserving enough chain-state anchors to compare correctness with other nodes. The genesis block provides the original chain identity, and carry-forward records provide consolidated account state at later points in the chain.

This design needs review before implementation details are finalized, especially around:

- how often carry-forward records are required
- which records must be retained forever
- how nodes prove a pruned state is still connected to the original genesis chain
- how peers compare state when one node is fully historical and another is pruned
