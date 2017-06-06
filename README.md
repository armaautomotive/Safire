# Safire
Peer to Peer Digital Currency

Website: http://safire.org


<h2>Overview:</h2>

Safire is a distributed digital currency. Every user can send payments to any other user with a small fee. Users are rewarded for relaying network transactions with new currency issued at a rate determined by a group preference average.

Like Bitcoin it uses blocks to confirm pending transactions and validate them for all users across the network.

Unlike Bitcoin instead of blocks being formed by the first solver of a difficulty hashing function it uses a selection function to choose one user based on the current time. Only that user can create the block for the current time period using a signed message for which they keep a public private key pair they created when they joined the network. This means that before new users can send and receive transactions they have to be granted entry into the network through a membership entry in a new block.

New currency is issued to the block creator for each generated block. The amount of currency issued is based on a network wide preference average. Accounts not used by people for commerce are discouraged through currency inflation and reward commission if they don't support the network by staying up to date. 

Each user in the system can specify preferences for network behaviour. Because all users are known the averages for each setting can be used to alter core settings like inflation rates and block times.

Constraints on the the number of transactions that can be performed by the network are the propagation time for generated blocks to be sent to other nodes in the network and the amount of disk space required to store the data for each user. 

If the transfer takes longer than the block time, then the block creator following can be skipped. This creates a time gap between block times allowing nodes to catch up. 

Transactions older than two years are accumulated in the current year for each user so that older records can be cleared requiring disk storage for only current activity. 



Key Benefits:

1) Send and receive currency with anyone over the internet even if the recipient is not running. 

2) Users vote equally on the network and currency properties adjusting to future needs.

3) Currency is issued to users evenly for relaying transactions without the use of mining. Rewards are distributed evenly to all users that are able to stay up to date with relaying transactions. In mining networks rewards are unbalanced when the distribution of hashing power is uneven.

3) Decentralization and network capacity are strained mainly by current demand as old history is purged. In the long term the requirements to run the network will not increase with the entire history but only by current demand.

4) Graceful handling of excess capacity demands on the system. This means demand peaks transactions will take longer to confirm but the throughput of the network increases to compensate and catch up.



Process:

A new user installs the software on their computer. 

They will generate a few public private key pairs, One is a wallet key pair that allows them to transfer currency with other users and another key pair is a block creation verification set used to ensure only they can create a block when they are designated by the selection function. 

They then connect to a few other users in the network to start downloading historical transaction and user data.

To participate in the network the user will submit a request to join the network. The request is  sent to all other users they are connected to and the message will be passed along until it eventually reaches the current block creator.

The block creator includes new user requests in the block and broadcasts it directly to the next block creator and additionally to all other users in the network where it is relayed to all other users. Blocks are created during a set time frame and there may be a padding transit time that allows blocks to be transmitted. 

Once the new user sees their inclusion in a valid block they are able to relay transactions across the network. They are also in rotation for creating new blocks and receive currency for doing so.



Each time a user generates a block they add a public key and a signed message to that block. The next time that particular user generates another block they must add to the block the private key from that past message as validation. Everyone knows if it is valid because they can test it. No one can fake it because they don't have the private key. That user will then also generate another public private key and message attaching the public key and message. When that user is designated to generate another block only that user can create it.
New users will always broadcast this block key pair in their application to join the network. 

Update: Rather than using a new hash each block, just use a block public signature (created on first startup) against the contents of the block. All users can verify that the block code was created with their private key. 

To create a block the user must receive the previous block from the last user. If a block creator does not receive the previous block it in time they can't create a new block and it will be up to the next designated block creator to get the latest block and carry on. Each block creator can contact the previous block creator to check their status and request the latest block. 

Over time old balances will be rebroadcast every two years to prune old transaction data. 

Users that do not stay up to date and running for a period of years need to have their balances pushed forward. Any user in the network can ping other users and request their balance if they discover that a user has transactions that have not been carried forward. Users will receive a bonus for pushing other accounts up to date and then paper wallets will be reliable. 



Currency Issuance:

Currency is issued to running nodes in the network for staying up to date as well as creating blocks. The new currency is issued with a variant of a transaction request called issuance. These. These issuance transactions don't have a sender and are validated by the network so that they adhere to rules about who can generate blocks and what is contained in them. Users in the network are chosen by hash function for each five or ten minute time period.

1) The user chosen to be block creator for a five or ten minute time period can collect (send themselves an issuance transaction) a bounty payment if they broadcast a valid block with transactions and user updates. If they are not running they do not receive the bounty.

In order for the block creator to publish a block get the bounty they have to broadcast the block to the next user as chosen by time hash before their time block expires. If they do not relay the data the block is not valid. This hand off process is necessary to ensure continuity so that there isn't a fork and double spending.

The blocks must be relayed directly from one user to the next to ensure that they can be linked together. Otherwise there would be an opportunity for state to change without the block creator knowing about it.

2) A set of runner up nodes to the block creator. I.e. chosen by user hash and time hash in order receive a transaction from the block creator if they are running and up to date. This effectively rewards a few random users for participating in the network to a lesser degree than the block creator. This is important because as the number of users in the network grows the rate each user becomes the block creator becomes less frequent and the distribution of new currency also.




Transactions

A user can send a portion of their balance to another user on the network by composing a network message and relaying it to other users. Once the message reaches the block creator and is included the transaction is confirmed. 

A transaction includes the sender address, amount to be sent, the public key of the recipient and a signed verification hash of the transaction details. 

The sending user signs the combination of the sender address, recipient address and amount with their private key. They then attach this message to the request. Everyone can verify that the transaction message was created using the senders private key because everyone has that users public key.  

Users in the network can observe validated transactions by receiving and confirming a block. Only one block confirmation is needed to ensure the transaction is final and authentic. 

Transactions have the effect of debiting the sender and crediting the receiver. This is calculated by accumulating transactions in the block chain for each user. The presence of carry forward transactions moving balances up from past years is also included to validate the numbers are added correctly.

Having other nodes verify that their copy of the block chain matches other nodes can add extra validation but should be unnecessary because it is unlikely anyone can create a block hash message for a user private key they don't control. 

The calculation to determine which user is responsible for creating blocks in the past depends on the inclusion of users to the network and can be used to verify that non authorized users didn't add blocks when they were not supposed to. This should not be possible even if they could add an unauthorized block or transaction without the private keys.  



Pruning and Balance forward:

All accounts transfer their balance from transactions prior to two years into the current year. Balances are calculated by the accumulation of debit and credit from transactions. Balances are carried forward using a variant of the transaction network request called balance carry forward. The client software will automatically make this request to the network. Transaction data older than four years can be deleted.

Balances for users can be validated using transaction history data as well as carry forward data.

There could be a bounty for people who identify old balances with no carry forward yet to encourage others to keep everyone up to date and not loose continuity for inactive users. This would allow for people to rely on paper wallets. 

Double Spending Transactions:

The network prevents users from sending a transaction to more than one user because of the following constraints:

1) Blocks created by users in the network are validated against the historical data to ensure that the user has funds available. All users in the network receiving new blocks can validate them based on the contents such that the transfers have balance and the block creator included a content hash signature that was signed by the designated block creator.

2) Blocks are chained together so that the hash of each previous block is included in the the next block ensuring that no one can insert fake blocks at a later date.

3) Only the designated block creator can create blocks because they have a block_generation_private_key from a previous block they created or from when they signed up.


Privacy:

This system broadcasts transactions with your Keys across the network. You can create multiple account wallets to separate your identity from other wallets. Although your exposed wallet will show trades against anonymous accounts.

Very similar behaviour to Bitcoin. Extra anonymity features are not currently created. The key areas of this system are user choice on network behaviour and high transaction capacity.


Attacks and Failure Conditions:

Based on the system design and structure there are particular areas where people could attempt to manipulate the system for their advantage or disadvantage of others. Mapping out some of these areas here is useful for guarding against such attacks.

1) Prevent people from creating inactive accounts without participating or contributing to the function of the network. It takes resources by everyone on the network to store data for each user. Accounts that won't be used are discouraged by the fact that they won't receive payments if they are not up to date.

2) Adding more users means less evenly distributed currency?
If there are a million users that are up to date and blocks generated every ten minutes then on average a given user will be selected every nineteen years. Currency will also have to be issued to a set of randomly allocated users each block cycle to bring this distribution time down preferably within a few months. These payments would be of very small amounts much smaller than the block creator.

3) What happens if rouge nodes refuse to include new users? Anyone can choose the number of pending user requests to include based on resources. More data in a block means less likely to be received by the next block maker.

4) Time synchronization conflicts break communication. What happens if users have different system times then the miner selection function won't work evenly. Also timezone differences can affect what times each user sees.

6) Block creator DDOS, Because the user responsible for creating a block is know anyone can see this and initiate a network flood attack on this or upcoming users. 

7) If individuals are known in the network targeted individuals could be DDOS attacked preventing them from accessing the network?

8) What if some nodes in a region of the internet are slower and can't keep up with faster areas that generate big blocks too quickly?
An option to handle this would be to have users monitor the status of other nodes in the network and relay an up to date score to the block creator. If the network is falling behind then they can reduce the size of the block.

9) A Time attack can occur if a sufficiently large number of users in the network intentionally set the system time to the wrong value. This could be used to alter for each time change who is the block creator. This could be used to artificially set the block creator more frequently to malicious nodes that don't include transactions lowering the capacity of the network.




User Identity

Each user client node has an identity with an address key pairs, block creation key paris and  voting configuration settings. A transaction ledger of transactions between other nodes. The latest transaction and balance between other users.



Settings and Voting

There is a bell curve of settings used to vote for network settings. Higher and lower on the average are counted less. These settings affect network policy like inflation rate. If a standard deviation of users average an inflation rate of 1% then that is used to pay users in currency for validating transactions over a year. 

If some users settings are 1000% inflation and they are outside of the second or third deviation their settings are weighted much lower than a vote in the first standard deviation.

Settings are stored as key value, certain keys have effect. The hash is of the key names and values. New settings added later will have defaults so that old users will adopt default settings without invalidating their history score.

