# Safire Admin Console

Safire nodes serve a small web admin page at:

```text
http://host:port/admin
```

The page shows node status, wallet balance, supply, users, and peer health using the existing read-only JSON endpoints.

## Command Access

The command panel is disabled unless `settings.dat` contains an admin key:

```text
admin_api_key:change-this-secret
```

With the key configured, the page can call:

```text
POST /api/admin/command
```

Allowed commands are intentionally limited:

```text
status
network
sync
users
mempool
blockchain
peers
exchange
accounts
```

The endpoint does not execute arbitrary shell or console input.

`sync` triggers one local peer sync pass and returns before/after block heights
for each configured peer.

## Remote Server Use

For a public server, prefer an SSH tunnel instead of exposing admin access directly:

```text
ssh -L 4890:127.0.0.1:4888 root@safire.org
```

Then open:

```text
http://127.0.0.1:4890/admin
```

If the admin console is exposed through a public reverse proxy later, add HTTPS and proxy-level authentication before enabling the command API.
