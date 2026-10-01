RPM packaging
=============

Copyright 2026 Qore Technologies, s.r.o.

The canonical qore-ssh2-module.spec targets Fedora, Enterprise Linux and
openSUSE with the Qore 3.0 SDK and matching qore-rpm-macros. It packages the
native module, four compiled/source user modules, compiler metadata and
translations. Documentation is a separate package. System libssh2 and OpenSSL
provide SSH and cryptography; neither implementation is bundled.

Prepare a committed source bundle and build in an isolated target SDK::

    python3 tools/packaging.py prepare --repo ../module-ssh2 --ref COMMIT \
      --name qore-ssh2-module --version 2.0.0 --spec qore-ssh2-module.spec \
      --output work/ssh2-source
    python3 tools/build-local.py --source work/ssh2-source \
      --image TARGET_SDK_IMAGE --output results/ssh2-build --jobs 2

These commands run from qore-packaging. Default builds include all eight SSH2
suites, real ELF metadata-preservation checks, fixture tests and complete locale
validation. Repository qualification retains tests and documentation enabled.
The generated runtime dependencies require the ABI and SDK version used to
compile the modules; the SSH server and nss_wrapper are test dependencies only.

The fixture creates temporary host/client keys, account lookup files and a
private home. It starts an unprivileged OpenSSH server on a loopback port and
subscribes to its listening log event before authenticating. It continuously
drains server logs and terminates/reaps the process group on every exit path.
Existing accounts, SSH configuration, services and known-host files are not
modified. Modern per-source penalties are disabled only for this private
server because the suites deliberately attempt invalid authentication.

Installed qualification copies only test inputs and loads the exact packaged
native and compiled module paths, outside the checkout::

    python3 -B -W error rpm/run-tests.py --installed

Run this unprivileged in a disposable runtime image with networking disabled
and test dependencies installed. Normal module use requires no SSH server.
