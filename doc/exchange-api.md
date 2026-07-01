# Safire Exchange API Draft

This is the first proof-of-concept API surface for services that need to run a
Safire node and integrate deposits or withdrawals.

## Read-only endpoints

`GET /api/exchange/status`

Returns node sync, chain, wallet, supply, peer, genesis, and exchange API status.

`GET /api/exchange/deposits/<address>`

Returns the latest accepted transfer records sent to `address`, limited to 500
records. Each record includes hash, block, block hash, amount, fee, sender,
recipient, and confirmation count.

`GET /api/exchange/tx/<hash>`

Looks up a transaction by record hash in the accepted chain first, then in the
local mempool. Accepted records include confirmation count. Pending records
return zero confirmations.

`GET /api/exchange/confirmations/<hash>`

Returns compact confirmation status for a transaction hash.

## Multi-account wallet endpoints

The node shares one blockchain database, but the wallet can hold multiple local
accounts/addresses.

`GET /api/wallet/accounts`

Lists local wallet accounts, the active account, public names, balances, joined
state, and heartbeat state.

`POST /api/wallet/accounts/create`

Creates a new local keypair. The new account is not joined automatically.

Parameters:

```json
{ "label": "Customer deposits" }
```

`POST /api/wallet/accounts/active`

Switches the active wallet account used by wallet status, send, join, set name,
history, receive, heartbeat, and block creation.

Parameters:

```json
{ "wallet_id": "03..." }
```

## Withdrawal endpoint

`POST /api/exchange/withdraw`

Submits a signed transfer from the node wallet. This endpoint is disabled unless
`settings.dat` contains:

```text
exchange_api_key:change-this-secret
```

Request parameters can be form encoded, query parameters, or simple JSON string
fields:

```json
{
  "api_key": "change-this-secret",
  "recipient": "03...",
  "amount": "1.25",
  "fee": "0"
}
```

The response includes the queued record hash, nonce, amount, fee, and recipient.
The record is not final until it appears in a block and reaches the exchange's
required confirmation threshold.

## Production gaps

- Use TLS and IP allowlisting in front of any public exchange node.
- Move hot-wallet signing behind a stronger approval policy.
- Add multi-address or deposit memo support before a real exchange integration.
- Add reorg/fork alerts and webhook or websocket event streaming.
- Add indexed transaction lookup so queries do not scan the connected chain.
