# Verified SFTP exchange

Copyright 2026 Qore Technologies, s.r.o.

1. Add typed client publication and acquisition operations. Require an explicit endpoint agreement, absolute remote paths and an exclusive staging name. Use acknowledged writes, close, byte counts and SHA-256. Use the POSIX rename extension for replacement on OpenSSH and ordinary rename only for a hosted backend whose configured contract enforces no-replace at commit. Test success, refusal and uncertain outcomes before integrating providers.
2. Register typed publish/acquire actions and an opt-in poller mode for immutable committed directories. Bind local visibility and readiness to verification, keep discovery references stable, and leave manifest correlation to Qorus. Exercise the client, action and polling entry points.
3. Document the wire operations, endpoint setup, reconciliation and upgrade behavior. Qualify the module against isolated real endpoints, inject failures at protocol boundaries and run the debug-enabled suites and Valgrind on affected native paths. Audit each staged commit.

The companion server contract scopes the identity to the endpoint, authenticated principal and absolute final name. A private staging directory and an agreed final directory share one filesystem. The OpenSSH replacement profile additionally needs a producer digest to identify a particular version during acquisition.
