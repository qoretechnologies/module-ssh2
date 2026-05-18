/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    SSH2Client.h

    libssh2 ssh2 client integration in Qore

    Qore Programming Language

    Copyright 2009 Wolfgang Ritzinger
    Copyright (C) 2010 - 2026 Qore Technologies, s.r.o.

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2.1 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with this library; if not, write to the Free Software
    Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

#ifndef _QORE_SSH2CLIENT_H

#define _QORE_SSH2CLIENT_H

#include "ssh2-module.h"

#include <qore/QoreSocket.h>
#ifdef _QORE_HAS_QUEUE_OBJECT
#include <qore/QoreQueue.h>
#endif

#include <time.h>
#include <stdarg.h>

#ifdef HAVE_STDINT_H
#include <stdint.h>
#endif

#include <set>
#include <string>

// for maximum SSH2 performance, a 32K buffer is needed
#define QSSH2_BUFSIZE 32768

DLLLOCAL QoreClass *initSSH2ClientClass(QoreNamespace& ns);
DLLLOCAL extern qore_classid_t CID_SSH2CLIENT;

DLLLOCAL std::string mode2str(const int mode);
DLLLOCAL int method_type_from_string(const char* method_type, ExceptionSink* xsink);

#define QAUTH_PASSWORD             (1 << 0)
#define QAUTH_KEYBOARD_INTERACTIVE (1 << 1)
#define QAUTH_PUBLICKEY            (1 << 2)
#define QAUTH_AGENT                (1 << 3)

// 60 second keepalive defaut
#define QKEEPALIVE_DEFAULT  60

DLLLOCAL extern const char *SSH2_ERROR;
DLLLOCAL extern const char *SSH2_CONNECTED;

class SSH2Channel;
class SSH2Listener;
class BlockingHelper;

class AbstractDisconnectionHelper {
public:
    DLLLOCAL ~AbstractDisconnectionHelper() {
    }

    // called before a disconnect
    DLLLOCAL virtual void preDisconnect() = 0;
};

class SSH2Client : public AbstractPrivateData {
    friend class SSH2Channel;
    friend class SSH2Listener;
    friend class BlockingHelper;

private:
    typedef std::set<SSH2Channel*> channel_set_t;

    // connection host
    std::string sshhost,
        // authentication
        sshuser,
        sshpass,
        sshkeys_pub,
        sshkeys_priv;

    // in-memory key data for authentication
    std::string sshkeys_priv_data,
        sshkeys_pub_data,
        sshkeys_passphrase;

    // known hosts verification
    std::string known_hosts_file;
    bool verify_host_key;
    int host_key_policy;
    QoreObject* host_key_store;
    QoreObject* client_identity_provider;
    int client_auth_order;
    int client_identity_fallback_policy;

    // algorithm preferences (method_type -> prefs string)
    std::map<int, std::string> method_prefs;
    std::string ssh_banner;

    // connection port
    uint32_t sshport;

    // keepalive interval in seconds (0 = disabled)
    int keepalive_interval;

    // whether to try SSH agent authentication
    bool use_agent;
    bool explicit_key_files;
    bool explicit_key_data;
    bool auto_key_files;

    // server info
    const char *sshauthenticatedwith;
    bool connect_in_progress;

    // set of connected channels
    channel_set_t channel_set;

#ifdef HAVE_LIBSSH2_FORWARD_LISTEN
    typedef std::set<SSH2Listener*> listener_set_t;
    // set of active listeners
    listener_set_t listener_set;
#endif

protected:
    // socket object for the connection
    QoreSocket socket;

    /*
        * close session/connection
        * free resources
        */
    DLLLOCAL virtual ~SSH2Client();

    DLLLOCAL void setKeysIntern();

    // sets the default known_hosts file (the local OS user's ~/.ssh/known_hosts) when filesystem
    // access is permitted; used to enable secure host key verification by default
    DLLLOCAL void setKnownHostsIntern();

    DLLLOCAL virtual void deref(ExceptionSink*);

    DLLLOCAL int startupUnlocked();
    DLLLOCAL int sshConnectedUnlocked();
    DLLLOCAL bool sshSessionActiveUnlocked() const {
        return connect_in_progress || ssh_session;
    }

    class ConnectInProgressHelper {
    private:
        SSH2Client& client;

    public:
        DLLLOCAL ConnectInProgressHelper(SSH2Client& n_client) : client(n_client) {
            assert(!client.connect_in_progress);
            client.connect_in_progress = true;
        }

        DLLLOCAL ~ConnectInProgressHelper() {
            client.connect_in_progress = false;
        }
    };

    DLLLOCAL int sshConnectUnlocked(int timeout_ms, ExceptionSink *xsink);
    DLLLOCAL QoreHashNode* makeProviderContext(const char* purpose, int timeout_ms, ExceptionSink* xsink) const;
    DLLLOCAL QoreHashNode* makeObservedHostKeyInfo(const char* hostkey, size_t hostkey_len, int hostkey_type,
        ExceptionSink* xsink) const;
    DLLLOCAL int verifyHostKeyWithStore(QoreObject* store, const char* hostkey, size_t hostkey_len, int hostkey_type,
        int timeout_ms, ExceptionSink* xsink);
    DLLLOCAL int tryAgentAuth(const char* userauthlist, int timeout_ms, bool& loggedin, ExceptionSink* xsink);
    DLLLOCAL int tryPublicKeyDataAuth(const char* userauthlist, int timeout_ms, bool& loggedin, ExceptionSink* xsink);
    DLLLOCAL int tryPublicKeyMemoryAuth(const char* public_key, size_t public_key_len, const char* private_key,
        size_t private_key_len, const char* passphrase, int timeout_ms, bool& loggedin, ExceptionSink* xsink);
    DLLLOCAL int tryPublicKeyFileAuth(const char* public_key_path, const char* private_key_path,
        const char* passphrase, int timeout_ms, bool& loggedin, ExceptionSink* xsink);
    DLLLOCAL int tryProviderAuth(const char* userauthlist, int timeout_ms, bool& loggedin, ExceptionSink* xsink);
    DLLLOCAL void channelDeletedUnlocked(SSH2Channel *channel) {
#ifdef DEBUG
        int rc =
#endif
        channel_set.erase(channel);
        assert(rc);
    }

    // the following functions are unlocked so are protected
    DLLLOCAL const char *getHost();
    DLLLOCAL const uint32_t getPort();
    DLLLOCAL const char *getUser();
    DLLLOCAL const char *getPassword();
    DLLLOCAL const char *getKeyPriv();
    DLLLOCAL const char *getKeyPub();
    DLLLOCAL const char *getAuthenticatedWith();

    DLLLOCAL QoreStringNode *fingerprintUnlocked();

    DLLLOCAL const char *getSessionErrUnlocked() {
        assert(ssh_session);
        char* msg = 0;
        libssh2_session_last_error(ssh_session, &msg, 0, 0);
        assert(msg);
        return msg;
    }

    DLLLOCAL void doSessionErrUnlocked(ExceptionSink* xsink) {
        xsink->raiseException(SSH2_ERROR, "libssh2 returned error %d: %s", libssh2_session_last_errno(ssh_session), getSessionErrUnlocked());
    }

    DLLLOCAL void doSessionErrUnlocked(ExceptionSink* xsink, const char *fmt, ...) {
        va_list args;
        QoreStringNode *desc = new QoreStringNode;

        while (true) {
            va_start(args, fmt);
            int rc = desc->vsprintf(fmt, args);
            va_end(args);
            if (!rc)
                break;
        }

        desc->sprintf(": libssh2 returned error %d: %s", libssh2_session_last_errno(ssh_session), getSessionErrUnlocked());

        xsink->raiseException(SSH2_ERROR, desc);
    }
    DLLLOCAL void setBlockingUnlocked(bool block) {
        if (ssh_session)
            libssh2_session_set_blocking(ssh_session, (int)block);
    }

    DLLLOCAL int waitSocketUnlocked(ExceptionSink* xsink, const char *toerr, const char *err, const char* m, int timeout_ms = DEFAULT_TIMEOUT_MS, bool in_disconnect = false, AbstractDisconnectionHelper* adh = 0) {
        // check for I/O interrupt before waiting (skip during disconnect to ensure cleanup)
        if (!in_disconnect && xsink && qore_check_cancel(xsink, m)) {
            disconnectUnlocked(true, timeout_ms > DEFAULT_TIMEOUT_MS ? timeout_ms : DEFAULT_TIMEOUT_MS, adh, xsink);
            return -1;
        }

        int rc = waitSocketUnlocked(timeout_ms);
        if (!rc) {
            if (xsink)
                xsink->raiseException(toerr, "network timeout after %dms in %s(); closing connection", timeout_ms, m);
            if (!in_disconnect)
                disconnectUnlocked(true, timeout_ms > DEFAULT_TIMEOUT_MS ? timeout_ms : DEFAULT_TIMEOUT_MS, adh, xsink);
            return -1;
        }
        if (rc < 0) {
            if (xsink)
                xsink->raiseErrnoException(err, errno, "error waiting for network (timeout: %dms) in %s(); closing connection", timeout_ms, m);
            if (!in_disconnect)
                disconnectUnlocked(true, timeout_ms > DEFAULT_TIMEOUT_MS ? timeout_ms : DEFAULT_TIMEOUT_MS, adh, xsink);
            return -1;
        }
        return 0;
    }

    DLLLOCAL int waitSocketUnlocked(int timeout_ms) const {
        return waitSocketUnlocked(libssh2_session_block_directions(ssh_session), timeout_ms);
    }

    DLLLOCAL int waitSocketUnlocked(int dir, int timeout_ms) const {
        return socket.asyncIoWait(timeout_ms, dir & LIBSSH2_SESSION_BLOCK_INBOUND, dir & LIBSSH2_SESSION_BLOCK_OUTBOUND);
    }

    DLLLOCAL QoreObject *registerChannelUnlocked(LIBSSH2_CHANNEL *channel);
    DLLLOCAL SSH2Channel *registerChannelUnlockedRaw(LIBSSH2_CHANNEL *channel);

    DLLLOCAL virtual int disconnectUnlocked(bool force, int timeout_ms = DEFAULT_TIMEOUT_MS, AbstractDisconnectionHelper* adh = 0, ExceptionSink* xsink = 0);

    DLLLOCAL LIBSSH2_CHANNEL *scpGetRaw(ExceptionSink *xsink, const char *path, int timeout_ms = -1, QoreHashNode *statinfo = 0);
    DLLLOCAL LIBSSH2_CHANNEL *scpPutRaw(ExceptionSink *xsink, const char *path, size_t size, int mode = 0644, long mtime = 0, long atime = 0, int timeout_ms = -1);

    // to ensure thread-safe operations
    mutable QoreThreadLock m;
    LIBSSH2_SESSION* ssh_session;

public:
    DLLLOCAL SSH2Client(const char*, const uint32_t);
    DLLLOCAL SSH2Client(QoreURL &url, const uint32_t = 0);
    DLLLOCAL int setUser(const char *);
    DLLLOCAL int setPassword(const char *);
    DLLLOCAL int setKeys(const char *, const char *, ExceptionSink* xsink);
    DLLLOCAL QoreStringNode *fingerprint();

    DLLLOCAL void getHostLocked(QoreString& str) {
        AutoLocker al(m);
        str.concat(sshhost);
    }

    DLLLOCAL const uint32_t getPortLocked() {
        AutoLocker al(m);
        return getPort();
    }

    DLLLOCAL void getUserLocked(QoreString& str) {
        AutoLocker al(m);
        str.concat(sshuser);
    }

    DLLLOCAL void getPasswordLocked(QoreString& str) {
        AutoLocker al(m);
        str.concat(sshpass);
    }

    DLLLOCAL void getKeyPrivLocked(QoreString& str) {
        AutoLocker al(m);
        str.concat(sshkeys_priv);
    }

    DLLLOCAL void getKeyPubLocked(QoreString& str) {
        AutoLocker al(m);
        str.concat(sshkeys_pub);
    }

    DLLLOCAL void getAuthenticatedWithLocked(QoreString& str) {
        AutoLocker al(m);
        if (sshauthenticatedwith) {
            str.concat(sshauthenticatedwith);
        }
    }

    DLLLOCAL virtual int connect(int timeout_ms, ExceptionSink *xsink) {
        return sshConnect(timeout_ms, xsink);
    }

    DLLLOCAL int disconnect(bool force = false, int timeout_ms = DEFAULT_TIMEOUT_MS, ExceptionSink *xsink = 0) {
        AutoLocker al(m);
        if (connect_in_progress) {
            xsink && xsink->raiseException("SSH2-CONNECT-IN-PROGRESS",
                "cannot disconnect while SSH2Base::connect() is in progress");
            return -1;
        }

        return disconnectUnlocked(force, timeout_ms, 0, xsink);
    }

    DLLLOCAL int sshConnect(int timeout_ms, ExceptionSink *xsink);

    DLLLOCAL int sshConnected();

    DLLLOCAL QoreHashNode *sshInfo(const TypedHashDecl* hashdecl, ExceptionSink* xsink);
    DLLLOCAL QoreHashNode *sshInfoIntern(const TypedHashDecl* hashdecl, ExceptionSink* xsink);

    DLLLOCAL QoreObject *openSessionChannel(ExceptionSink *xsink, int timeout_ms = -1);
    DLLLOCAL QoreObject *openDirectTcpipChannel(ExceptionSink *xsink, const char *host, int port, const char *shost = "127.0.0.1", int sport = 22, int timeout_ms = -1);
#ifdef HAVE_LIBSSH2_CHANNEL_DIRECT_STREAMLOCAL
    DLLLOCAL QoreObject *openDirectStreamLocalChannel(ExceptionSink *xsink, const char *socket_path, const char *shost = "127.0.0.1", int sport = 22, int timeout_ms = -1);
#endif
    DLLLOCAL QoreObject *scpGet(ExceptionSink *xsink, const char *path, int timeout_ms = -1, QoreHashNode *statinfo = 0);
    DLLLOCAL void scpGet(ExceptionSink *xsink, const char *path, OutputStream *os, int timeout_ms = -1);
    DLLLOCAL QoreObject *scpPut(ExceptionSink *xsink, const char *path, size_t size, int mode = 0644, long mtime = 0, long atime = 0, int timeout_ms = -1);
    DLLLOCAL void scpPut(ExceptionSink *xsink, const char *path, InputStream *is, size_t size, int mode = 0644, long mtime = 0, long atime = 0, int timeout_ms = -1);

    DLLLOCAL int setKnownHostsFile(const char* path, ExceptionSink* xsink) {
        AutoLocker al(m);
        if (sshSessionActiveUnlocked()) {
            xsink->raiseException(SSH2_CONNECTED,
                "usage of SSH2Base::setKnownHostsFile() is not allowed when connected or connecting");
            return -1;
        }
        known_hosts_file = path ? path : "";
        return 0;
    }

    DLLLOCAL void getKnownHostsFileLocked(QoreString& str) {
        AutoLocker al(m);
        str.concat(known_hosts_file);
    }

    DLLLOCAL int setVerifyHostKey(bool verify, ExceptionSink* xsink) {
        AutoLocker al(m);
        if (sshSessionActiveUnlocked()) {
            xsink->raiseException(SSH2_CONNECTED,
                "usage of SSH2Base::setVerifyHostKey() is not allowed when connected or connecting");
            return -1;
        }
        verify_host_key = verify;
        return 0;
    }

    DLLLOCAL bool getVerifyHostKey() const {
        AutoLocker al(m);
        return verify_host_key;
    }

    DLLLOCAL int setHostKeyPolicy(int policy, ExceptionSink* xsink) {
        AutoLocker al(m);
        if (connect_in_progress) {
            xsink->raiseException(SSH2_CONNECTED,
                "usage of SSH2Base::setHostKeyPolicy() is not allowed while connecting");
            return -1;
        }
        if (policy != SSH2_HOSTKEY_REJECT && policy != SSH2_HOSTKEY_TOFU
            && policy != SSH2_HOSTKEY_TOFU_SESSION) {
            xsink->raiseException("SSH2-HOSTKEY-POLICY-ERROR",
                "invalid host key policy %d; expected SSH2_HOSTKEY_REJECT (%d), SSH2_HOSTKEY_TOFU "
                "(%d), or SSH2_HOSTKEY_TOFU_SESSION (%d)", policy, SSH2_HOSTKEY_REJECT,
                SSH2_HOSTKEY_TOFU, SSH2_HOSTKEY_TOFU_SESSION);
            return -1;
        }
        host_key_policy = policy;
        return 0;
    }

    DLLLOCAL int getHostKeyPolicy() const {
        AutoLocker al(m);
        return host_key_policy;
    }

    DLLLOCAL QoreHashNode* getHostKeyLocked(ExceptionSink* xsink);
    DLLLOCAL int addKnownHostLocked(const char* host, int port, ExceptionSink* xsink);

    DLLLOCAL int setClientIdentityProvider(QoreObject* provider, ExceptionSink* xsink) {
        AutoLocker al(m);
        if (sshSessionActiveUnlocked()) {
            xsink->raiseException(SSH2_CONNECTED,
                "usage of SSH2Base::setClientIdentityProvider() is not allowed when connected or connecting");
            return -1;
        }
        QoreObject* old = client_identity_provider;
        client_identity_provider = provider ? provider->objectRefSelf() : nullptr;
        if (old) {
            old->deref(xsink);
        }
        return 0;
    }

    DLLLOCAL QoreObject* getClientIdentityProvider() const {
        AutoLocker al(m);
        return client_identity_provider ? client_identity_provider->objectRefSelf() : nullptr;
    }

    DLLLOCAL int setHostKeyStore(QoreObject* store, ExceptionSink* xsink) {
        AutoLocker al(m);
        if (sshSessionActiveUnlocked()) {
            xsink->raiseException(SSH2_CONNECTED,
                "usage of SSH2Base::setHostKeyStore() is not allowed when connected or connecting");
            return -1;
        }
        QoreObject* old = host_key_store;
        host_key_store = store ? store->objectRefSelf() : nullptr;
        if (old) {
            old->deref(xsink);
        }
        return 0;
    }

    DLLLOCAL QoreObject* getHostKeyStore() const {
        AutoLocker al(m);
        return host_key_store ? host_key_store->objectRefSelf() : nullptr;
    }

    DLLLOCAL int setClientAuthOrder(int order, ExceptionSink* xsink) {
        AutoLocker al(m);
        if (connect_in_progress) {
            xsink->raiseException(SSH2_CONNECTED,
                "usage of SSH2Base::setClientAuthOrder() is not allowed while connecting");
            return -1;
        }
        if (order != SSH2_CLIENT_AUTH_EXPLICIT_FIRST && order != SSH2_CLIENT_AUTH_PROVIDER_FIRST) {
            xsink->raiseException("SSH2-CLIENT-AUTH-ORDER-ERROR",
                "invalid client auth order %d; expected SSH2_CLIENT_AUTH_EXPLICIT_FIRST (%d) "
                "or SSH2_CLIENT_AUTH_PROVIDER_FIRST (%d)", order, SSH2_CLIENT_AUTH_EXPLICIT_FIRST,
                SSH2_CLIENT_AUTH_PROVIDER_FIRST);
            return -1;
        }
        client_auth_order = order;
        return 0;
    }

    DLLLOCAL int getClientAuthOrder() const {
        AutoLocker al(m);
        return client_auth_order;
    }

    DLLLOCAL int setClientIdentityFallbackPolicy(int policy, ExceptionSink* xsink) {
        AutoLocker al(m);
        if (connect_in_progress) {
            xsink->raiseException(SSH2_CONNECTED,
                "usage of SSH2Base::setClientIdentityFallbackPolicy() is not allowed while connecting");
            return -1;
        }
        if (policy != SSH2_CLIENT_ID_FALLBACK_DISABLED && policy != SSH2_CLIENT_ID_FALLBACK_AGENT
            && policy != SSH2_CLIENT_ID_FALLBACK_DEFAULT_KEYS
            && policy != SSH2_CLIENT_ID_FALLBACK_AGENT_AND_DEFAULT_KEYS) {
            xsink->raiseException("SSH2-CLIENT-IDENTITY-FALLBACK-POLICY-ERROR",
                "invalid client identity fallback policy %d; expected "
                "SSH2_CLIENT_ID_FALLBACK_DISABLED (%d), SSH2_CLIENT_ID_FALLBACK_AGENT (%d), "
                "SSH2_CLIENT_ID_FALLBACK_DEFAULT_KEYS (%d), or "
                "SSH2_CLIENT_ID_FALLBACK_AGENT_AND_DEFAULT_KEYS (%d)", policy,
                SSH2_CLIENT_ID_FALLBACK_DISABLED, SSH2_CLIENT_ID_FALLBACK_AGENT,
                SSH2_CLIENT_ID_FALLBACK_DEFAULT_KEYS, SSH2_CLIENT_ID_FALLBACK_AGENT_AND_DEFAULT_KEYS);
            return -1;
        }
        client_identity_fallback_policy = policy;
        return 0;
    }

    DLLLOCAL int getClientIdentityFallbackPolicy() const {
        AutoLocker al(m);
        return client_identity_fallback_policy;
    }

    DLLLOCAL int setMethodPreference(int method_type, const char* prefs, ExceptionSink* xsink);
    DLLLOCAL QoreListNode* getSupportedAlgorithms(int method_type, ExceptionSink* xsink);
    DLLLOCAL int setBanner(const char* banner, ExceptionSink* xsink);
    DLLLOCAL QoreStringNode* getBannerLocked(ExceptionSink* xsink);
    DLLLOCAL int setTraceLevelLocked(int bitmask, ExceptionSink* xsink);

#ifdef HAVE_LIBSSH2_FORWARD_LISTEN
    DLLLOCAL QoreObject* forwardListen(ExceptionSink* xsink, const char* host, int port, int queue_maxsize = 1, int timeout_ms = -1);
    DLLLOCAL void listenerDeletedUnlocked(SSH2Listener* listener) {
        listener_set.erase(listener);
    }
#endif

    DLLLOCAL int setKeysFromData(const BinaryNode* priv_key, const BinaryNode* pub_key, const char* passphrase, ExceptionSink* xsink) {
        AutoLocker al(m);
        if (sshSessionActiveUnlocked()) {
            xsink->raiseException(SSH2_CONNECTED,
                "usage of SSH2Base::setKeysFromData() is not allowed when connected or connecting");
            return -1;
        }
        sshkeys_priv_data.assign((const char*)priv_key->getPtr(), priv_key->size());
        if (pub_key) {
            sshkeys_pub_data.assign((const char*)pub_key->getPtr(), pub_key->size());
        } else {
            sshkeys_pub_data.clear();
        }
        if (passphrase) {
            sshkeys_passphrase = passphrase;
        } else {
            sshkeys_passphrase.clear();
        }
        explicit_key_data = true;
        return 0;
    }

    DLLLOCAL int setUseAgent(bool enable, ExceptionSink* xsink) {
        AutoLocker al(m);
        if (sshSessionActiveUnlocked()) {
            xsink->raiseException(SSH2_CONNECTED,
                "usage of SSH2Base::setUseAgent() is not allowed when connected or connecting");
            return -1;
        }
        use_agent = enable;
        return 0;
    }

    DLLLOCAL bool getUseAgent() const {
        AutoLocker al(m);
        return use_agent;
    }

    DLLLOCAL int setKeepalive(int interval, ExceptionSink* xsink) {
        AutoLocker al(m);
        if (sshSessionActiveUnlocked()) {
            xsink->raiseException(SSH2_CONNECTED,
                "usage of SSH2Base::setKeepalive() is not allowed when connected or connecting");
            return -1;
        }
        keepalive_interval = interval;
        return 0;
    }

    DLLLOCAL int getKeepalive() const {
        AutoLocker al(m);
        return keepalive_interval;
    }

    DLLLOCAL void clearWarningQueue(ExceptionSink* xsink);
    DLLLOCAL void setWarningQueue(ExceptionSink* xsink, int64 warning_ms, int64 warning_bs, Queue* wq, QoreValue arg, int64 min_ms = 1000);
    DLLLOCAL QoreHashNode* getUsageInfo() const;
    DLLLOCAL void clearStats();
};

class BlockingHelper {
protected:
    SSH2Client* client;

public:
    DLLLOCAL BlockingHelper(SSH2Client* n_client) : client(n_client) {
        client->setBlockingUnlocked(false);
    }
    DLLLOCAL ~BlockingHelper() {
        client->setBlockingUnlocked(true);
    }
};

#endif // _QORE_SSH2CLIENT_H
