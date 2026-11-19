// Copyright 2026 Qore Technologies, s.r.o.
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "SFTPClient.h"

#include <qore/ScopeGuard.h>
#include <openssl/evp.h>
#include <openssl/rand.h>
#include <array>
#include <climits>
#include <memory>

namespace {
std::string field(const QoreHashNode* spec, const char* key) {
    const auto* value = spec->getKeyValue(key).get<const QoreStringNode>();
    return value ? std::string(value->c_str(), value->size()) : std::string();
}

std::string hex(const unsigned char* data, size_t len) {
    static const char digits[] = "0123456789abcdef";
    std::string result(len * 2, '0');
    for (size_t i = 0; i < len; ++i) {
        result[2 * i] = digits[data[i] >> 4];
        result[2 * i + 1] = digits[data[i] & 15];
    }
    return result;
}

// A unique spelling matters because the absolute path forms part of the receipt identity.
bool absolutePath(const std::string& path) {
    return !path.empty() && path[0] == '/' && path.size() < 32768
        && path.find('\0') == path.npos && path.find("//") == path.npos
        && path.find("/./") == path.npos && path.find("/../") == path.npos
        && path.substr(path.find_last_of('/') + 1) != "."
        && path.substr(path.find_last_of('/') + 1) != "..";
}

bool isDigest(const std::string& value) {
    return value.size() == 64 && value.find_first_not_of("0123456789abcdef") == value.npos;
}
}

class SftpExchange {
public:
    SftpExchange(SFTPClient& client, InputStream* input, const QoreStringNode* local_path,
            const QoreHashNode* spec, int timeout, QoreHashNode& receipt, ExceptionSink& err)
        : client(client), input(input), local(local_path ? std::string(local_path->c_str(), local_path->size()) : ""),
          remote(field(spec, "remote_path")), directory(field(spec, "staging_dir")), profile(field(spec, "profile")),
          policy(field(spec, "collision_policy")), pin(field(spec, "host_key_sha256")),
          expected(spec->existsKeyValue("length") ? spec->getKeyValue("length").getAsBigInt() : -1),
          digest_given(!spec->getKeyValue("sha256").isNothing()), expected_digest(field(spec, "sha256")),
          timeout(timeout), receipt(receipt), err(err),
          handle(&client, "SFTP-EXCHANGE-ERROR", "SFTP exchange", timeout, &err) {
    }

    void run() {
        set("host", client.getHost());
        set("principal", client.getUser());
        receipt.setKeyValue("port", client.getPort(), &err);
        set("remote_path", remote);
        set("profile", profile);
        set("collision_policy", policy);
        receipt.setKeyValue("expected_length", expected, &err);
        if (digest_given) {
            set("expected_sha256", expected_digest);
        }
        if (!input) {
            set("local_path", local);
        }
        if (!absolutePath(remote) || remote.back() == '/' || expected < 0
                || remote.compare(remote.find_last_of('/') + 1, 12, ".qore-stage-") == 0
                || (digest_given && !isDigest(expected_digest))
                || (input && (!absolutePath(directory) || (directory != "/" && directory.back() == '/')))
                || (!input && (!q_absolute_path(local.c_str()) || local.find('\0') != local.npos))) {
            err.raiseException("SFTP-EXCHANGE-ARG", "invalid path, length or SHA-256 in exchange agreement");
            return;
        }
        if (!qualified()) {
            outcome = "unsupported";
            err.raiseException("SFTP-EXCHANGE-UNSUPPORTED", "no qualified commit contract for this profile and policy");
            return;
        }
        if (!input && policy == "replace" && !digest_given) {
            err.raiseException("SFTP-EXCHANGE-ARG", "a replaceable path requires a producer SHA-256");
            return;
        }
        if (client.exchange_reader || client.checkPollOpUnlocked("SFTP exchange", &err)) {
            if (!err) {
                err.raiseException("SFTP-EXCHANGE-BUSY", "a stream callback owns this client");
            }
            return;
        }
        if (client.sftp_session) {
            if (!checkPeer()) {
                return;
            }
        } else if (client.sftpConnectUnlocked(timeout, &err, [this]() { return checkPeer(); }, pin.empty())) {
            return;
        }
        if (pin.empty() && !client.known_host_key_matched) {
            outcome = "unsupported";
            err.raiseException("SFTP-EXCHANGE-UNSUPPORTED", "the endpoint requires an enrolled host key or a pin");
            return;
        }
        if (!digest || !EVP_DigestInit_ex(digest.get(), EVP_sha256(), nullptr)) {
            err.raiseException("SFTP-EXCHANGE-DIGEST", "could not initialize SHA-256");
            return;
        }
        unsigned char nonce[16];
        if (RAND_bytes(nonce, sizeof(nonce)) != 1) {
            err.raiseException("SFTP-EXCHANGE-STAGE", "could not generate a private staging name");
            return;
        }
        std::string suffix = ".qore-stage-" + hex(nonce, sizeof(nonce));
        BlockingHelper nonblocking(&client);
        if (input) {
            stage = (directory == "/" ? "/" : directory + "/") + suffix;
            if (stage == remote) {
                err.raiseException("SFTP-EXCHANGE-STAGE", "staging and final paths must be distinct");
                return;
            }
            set("staging_path", stage);
            publish();
        } else {
            auto pos = local.find_last_of(
#ifdef _WIN32
                "/\\"
#else
                "/"
#endif
            );
            stage = local.substr(0, pos == local.npos ? 0 : pos + 1) + suffix;
            if (stage == local) {
                err.raiseException("SFTP-EXCHANGE-STAGE", "staging and final paths must be distinct");
                return;
            }
            acquire();
        }
    }

    const char* outcome = "failed";
    int64 observed = 0;

private:
    SFTPClient& client;
    InputStream* input;
    const std::string local, remote, directory, profile, policy, pin;
    const int64 expected;
    const bool digest_given;
    const std::string expected_digest;
    const int timeout;
    QoreHashNode& receipt;
    ExceptionSink& err;
    QSftpHelper handle;
    std::string stage;
    std::unique_ptr<EVP_MD_CTX, decltype(&EVP_MD_CTX_free)> digest{EVP_MD_CTX_new(), EVP_MD_CTX_free};

    void set(const char* key, const std::string& value) {
        receipt.setKeyValue(key, new QoreStringNode(value), &err);
    }

    bool qualified() const {
        // These guarantees were qualified with the 1.11.1 close and rename implementations.
        if (LIBSSH2_VERSION_NUM < 0x010b01 || !libssh2_version(0x010b01)) {
            return false;
        }
        if (profile == "hosted-immutable" && policy == "no-replace") {
            // This selects the operator-qualified staged backend, scoped to the configured principal and roots.
            return isDigest(pin) && remote.compare(0, 9, "/inbound/") == 0
                && remote.find('/', 9) == remote.npos && (!input || directory == "/staging");
        }
#ifdef HAVE_LIBSSH2_POSIX_RENAME
        return profile == "openssh-posix" && policy == "replace";
#else
        return false;
#endif
    }

    bool qualifiedServer() const {
#ifdef HAVE_LIBSSH2_SERVER_BANNER
        const char* id = libssh2_session_banner_get(client.ssh_session);
        if (!id) {
            return false;
        }
        std::string banner(id);
        banner.erase(banner.find_last_not_of("\r\n") + 1);
        if (profile == "openssh-posix") {
            return banner.compare(0, 16, "SSH-2.0-OpenSSH_") == 0;
        }
        // The listener administrator advertises this exact token only for a staged exchange.
        // It is authenticated by the key exchange and must match the independently pinned key.
        auto pos = banner.find_last_of(" -");
        return pos != banner.npos && banner.substr(pos + 1) == "QoreStagedSFTP_1";
#else
        return false;
#endif
    }

    bool checkPeer() {
        size_t key_size;
        int key_type;
        const char* key = libssh2_session_hostkey(client.ssh_session, &key_size, &key_type);
        unsigned char key_digest[EVP_MAX_MD_SIZE];
        unsigned key_digest_size;
        if (!key || !EVP_Digest(key, key_size, key_digest, &key_digest_size, EVP_sha256(), nullptr)) {
            err.raiseException("SFTP-EXCHANGE-HOST", "could not fingerprint the negotiated host key");
            return false;
        }
        std::string actual_pin = hex(key_digest, key_digest_size);
        set("host_key_sha256", actual_pin);
        if ((!pin.empty() && actual_pin != pin) || (pin.empty() && !client.verify_host_key)) {
            outcome = "unsupported";
            err.raiseException("SFTP-EXCHANGE-UNSUPPORTED", "endpoint does not match the qualified host key agreement");
            return false;
        }
        if (!qualifiedServer()) {
            outcome = "unsupported";
            err.raiseException("SFTP-EXCHANGE-UNSUPPORTED", "the authenticated listener does not advertise the agreed profile");
            return false;
        }
        return true;
    }

    void protocolError(const char* operation, int rc) {
        err.raiseException("SFTP-EXCHANGE-IO", "%s failed (libssh2 %d, SFTP %lu)", operation, rc,
            client.sftp_session ? libssh2_sftp_last_error(client.sftp_session) : 0UL);
    }

    bool open(const std::string& path, unsigned long flags) {
        do {
            handle.assign(libssh2_sftp_open(client.sftp_session, path.c_str(), flags, 0600));
            if (handle) {
                return true;
            }
            int rc = libssh2_session_last_errno(client.ssh_session);
            if (rc != LIBSSH2_ERROR_EAGAIN) {
                protocolError("open", rc);
                return false;
            }
        } while (!handle.waitSocket());
        return false;
    }

    bool close() {
        int rc = handle.close();
        if (rc && !err) {
            protocolError("close", rc);
        }
        return !rc && !err;
    }

    bool update(const char* bytes, size_t size) {
        if (size > static_cast<uint64_t>(expected - observed)) {
            err.raiseException("SFTP-EXCHANGE-LENGTH", "stream exceeds the declared length");
            return false;
        }
        if (!EVP_DigestUpdate(digest.get(), bytes, size)) {
            err.raiseException("SFTP-EXCHANGE-DIGEST", "could not update SHA-256");
            return false;
        }
        return true;
    }

    bool verify() {
        unsigned char bytes[EVP_MAX_MD_SIZE];
        unsigned length;
        if (!EVP_DigestFinal_ex(digest.get(), bytes, &length)) {
            err.raiseException("SFTP-EXCHANGE-DIGEST", "could not finalize SHA-256");
            return false;
        }
        std::string actual = hex(bytes, length);
        set("sha256", actual);
        if (observed != expected || (digest_given && actual != expected_digest)) {
            err.raiseException("SFTP-EXCHANGE-VERIFY", "content differs from the declared length or producer SHA-256");
            return false;
        }
        set("version", actual);
        return !qore_check_cancel(&err, "SFTP exchange commit");
    }

    void publish() {
        if (!open(stage, LIBSSH2_FXF_WRITE | LIBSSH2_FXF_CREAT | LIBSSH2_FXF_EXCL)) {
            return;
        }
        // Remove only an exclusively acquired stage, on the original session and before an uncertain commit.
        bool owned = true;
        ScopeGuard cleanup = MakeGuard([&]() {
            handle.tryClose();
            if (owned && client.sftp_session) {
                ExceptionSink cleanup_err;
                QSftpHelper cleanup_handle(&client, "SFTP-EXCHANGE-CLEANUP", "remove stage", timeout, &cleanup_err);
                while (libssh2_sftp_unlink(client.sftp_session, stage.c_str()) == LIBSSH2_ERROR_EAGAIN) {
                    if (cleanup_handle.waitSocket()) {
                        break;
                    }
                }
                cleanup_err.clear();
            }
        });
        std::array<char, 32768> buf;
        while (!qore_check_cancel(&err, "SFTP upload")) {
            int64 n;
            {
                client.exchange_reader = &handle;
                ScopeGuard release = MakeGuard([&]() { client.exchange_reader = nullptr; });
                AutoUnlocker unlock(client.m);
                n = input->read(buf.data(), buf.size(), &err);
            }
            if (!handle) {
                owned = false;
                err.raiseException("SFTP-EXCHANGE-DISCONNECTED", "session closed during input stream read");
                return;
            }
            client.setBlockingUnlocked(false);
            if (err) {
                return;
            }
            if (!n) {
                break;
            }
            if (n < 0 || static_cast<uint64_t>(n) > buf.size()) {
                err.raiseException("SFTP-EXCHANGE-STREAM", "invalid input stream read length");
                return;
            }
            if (!update(buf.data(), n)) {
                return;
            }
            int64 offset = 0;
            while (offset < n) {
                if (qore_check_cancel(&err, "SFTP write")) {
                    return;
                }
                auto rc = libssh2_sftp_write(*handle, buf.data() + offset, n - offset);
                if (rc == LIBSSH2_ERROR_EAGAIN || !rc) {
                    if (handle.waitSocket()) {
                        return;
                    }
                } else if (rc < 0) {
                    protocolError("write", rc);
                    return;
                } else {
                    offset += rc;
                    observed += rc;
                }
            }
        }
        if (err || !close() || !verify()) {
            return;
        }
        owned = false;
        outcome = "indeterminate";
        int rc;
        do {
#ifdef HAVE_LIBSSH2_POSIX_RENAME
            if (policy == "replace") {
                rc = libssh2_sftp_posix_rename(client.sftp_session, stage.c_str(), remote.c_str());
            } else
#endif
            {
                rc = libssh2_sftp_rename_ex(client.sftp_session, stage.c_str(), stage.size(), remote.c_str(),
                    remote.size(), 0);
            }
            if (rc == LIBSSH2_ERROR_EAGAIN && handle.waitSocket()) {
                return;
            }
        } while (rc == LIBSSH2_ERROR_EAGAIN);
        if (!rc) {
            outcome = "confirmed";
            return;
        }
        unsigned long status = libssh2_sftp_last_error(client.sftp_session);
        if (rc == LIBSSH2_FX_OP_UNSUPPORTED || (rc == LIBSSH2_ERROR_SFTP_PROTOCOL
                && status == LIBSSH2_FX_OP_UNSUPPORTED)) {
            outcome = "unsupported";
            owned = true;
        } else if (rc == LIBSSH2_ERROR_SFTP_PROTOCOL && status != LIBSSH2_FX_OK
                && status != LIBSSH2_FX_CONNECTION_LOST && status != LIBSSH2_FX_NO_CONNECTION) {
            outcome = "failed";
            owned = true;
        }
        protocolError("commit", rc);
    }

    void acquire() {
        QoreSandboxManagerHelper smh(QoreSandboxManagerHelper::Policy);
        if (smh && (!smh->checkFilesystemAccess(local.c_str(), QSEC_CREATE | QSEC_WRITE | QSEC_DELETE, &err)
                || !smh->checkFilesystemAccess(stage.c_str(), QSEC_CREATE | QSEC_WRITE | QSEC_DELETE, &err))) {
            return;
        }
        QoreFile file;
        if (file.open2(&err, stage.c_str(), O_CREAT | O_EXCL | O_WRONLY, 0600)) {
            return;
        }
        ScopeGuard cleanup = MakeGuard([&]() {
            file.close();
            ::unlink(stage.c_str());
        });
        if (!open(remote, LIBSSH2_FXF_READ)) {
            return;
        }
        ScopeGuard close_handle = MakeGuard([&]() { handle.tryClose(); });
        std::array<char, 32768> buf;
        while (!qore_check_cancel(&err, "SFTP download")) {
            auto n = libssh2_sftp_read(*handle, buf.data(), buf.size());
            if (n == LIBSSH2_ERROR_EAGAIN) {
                if (handle.waitSocket()) {
                    return;
                }
                continue;
            }
            if (n < 0) {
                protocolError("read", n);
                return;
            }
            if (!n) {
                break;
            }
            if (!update(buf.data(), n)) {
                return;
            }
            if (file.write(buf.data(), n, &err) != n || err) {
                // A partial kernel write can precede a terminal error within QoreFile::write().
                size_t pos = file.getPos();
                if (pos != static_cast<size_t>(-1)) {
                    observed = static_cast<int64>(pos);
                }
                if (!err) {
                    err.raiseException("SFTP-EXCHANGE-LOCAL", "short write to private local stage");
                }
                return;
            }
            observed += n;
        }
        if (err || !close()) {
            return;
        }
        if (file.close(&err)) {
            if (!err) {
                err.raiseErrnoException("SFTP-EXCHANGE-LOCAL", errno, "could not close the local stage");
            }
            return;
        }
        if (!verify()) {
            return;
        }
        if (::rename(stage.c_str(), local.c_str())) {
            err.raiseErrnoException("SFTP-EXCHANGE-LOCAL", errno, "could not commit local file '%s'", local.c_str());
            return;
        }
        cleanup.Dismiss();
        outcome = "confirmed";
    }
};

QoreHashNode* SFTPClient::exchange(InputStream* input, const QoreStringNode* local, const QoreHashNode* spec,
        int64 timeout, ExceptionSink* xsink) {
    if (timeout <= 0 || timeout > INT_MAX) {
        xsink->raiseException("SFTP-EXCHANGE-ARG", "expected a positive network timeout in milliseconds");
        return nullptr;
    }
    AutoLocker lock(m);
    ReferenceHolder<QoreHashNode> receipt(new QoreHashNode(hashdeclSftpExchangeReceipt, xsink), xsink);
    ExceptionSink err;
    {
        SftpExchange op(*this, input, local, spec, static_cast<int>(timeout), **receipt, err);
        op.run();
        receipt->setKeyValue("length", op.observed, xsink);
        receipt->setKeyValue("outcome", new QoreStringNode(op.outcome), xsink);
    }
    if (err) {
        receipt->setKeyValue("error", QoreStringNodeValueHelper(err.getExceptionErr()).getReferencedValue(), xsink);
        receipt->setKeyValue("detail", QoreStringNodeValueHelper(err.getExceptionDesc()).getReferencedValue(), xsink);
        err.clear();
    }
    return receipt.release();
}
