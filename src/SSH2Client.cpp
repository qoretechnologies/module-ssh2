/* -*- indent-tabs-mode: nil -*- */
/*
    SSH2Client.cpp

    libssh2 ssh2 client integration into qore

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

#include "SSH2Client.h"
#include "SSH2Channel.h"
#include "SSH2Listener.h"

#include <memory>
#include <string>
#include <map>
#include <utility>
#include <sys/types.h>
#ifdef HAVE_PWD_H
#include <pwd.h>
#endif
#include <errno.h>
#include <strings.h>
#include <sys/stat.h>

#include <assert.h>
#include <unistd.h>

static const char *SSH2CLIENT_TIMEOUT = "SSH2CLIENT-TIMEOUT";
static const char *SSH2CLIENT_NOT_CONNECTED = "SSH2CLIENT-NOT-CONNECTED";
const char *SSH2_ERROR = "SSH2-ERROR";
const char *SSH2_CONNECTED = "SSH2-CONNECTED";

std::string mode2str(const int mode) {
    std::string ret=std::string("----------");
    int tmode=mode;
    for(int i=2; i>=0; i--) {
        if (tmode & 001) {
            ret[1+2+i*3]='x';
        }
        if (tmode & 002) {
            ret[1+1+i*3]='w';
        }
        if (tmode & 004) {
            ret[1+0+i*3]='r';
        }
        tmode>>=3;
    }
#ifdef S_ISDIR
    if (S_ISDIR(mode)) {
        ret[0]='d';
    }
#endif
#ifdef S_ISBLK
    if (S_ISBLK(mode)) {
        ret[0]='b';
    }
#endif
#ifdef S_ISCHR
    if (S_ISCHR(mode)) {
        ret[0]='c';
    }
#endif
#ifdef S_ISFIFO
    if (S_ISFIFO(mode)) {
        ret[0]='p';
    }
#endif
#ifdef S_ISLNK
    if (S_ISLNK(mode)) {
        ret[0]='l';
    }
#endif
#ifdef S_ISSOCK
    if (S_ISSOCK(mode)) {
        ret[0]='s';
    }
#endif

    return ret;
}

static void map_ssh2_sbuf_to_hash(QoreHashNode *h, struct stat *sbuf, ExceptionSink* xsink) {
    // note that dev_t on Linux is an unsigned 64-bit integer, so we could lose precision here
    h->setKeyValue("mode",        sbuf->st_mode, xsink);
    h->setKeyValue("permissions", new QoreStringNode(mode2str(sbuf->st_mode)), xsink);
    h->setKeyValue("size",        sbuf->st_size, xsink);

    h->setKeyValue("uid",         sbuf->st_uid, xsink);
    h->setKeyValue("gid",         sbuf->st_gid, xsink);

    h->setKeyValue("atime",       DateTimeNode::makeAbsolute(currentTZ(), (int64)sbuf->st_atime), xsink);
    h->setKeyValue("mtime",       DateTimeNode::makeAbsolute(currentTZ(), (int64)sbuf->st_mtime), xsink);
}

/**
 * SSH2Client constructor
 *
 * this just prefills the values for connection with hostname and port
 */
SSH2Client::SSH2Client(const char *hostname, const uint32_t port) : sshhost(hostname), sshport(port), keepalive_interval(QKEEPALIVE_DEFAULT), use_agent(true), verify_host_key(false), host_key_policy(SSH2_HOSTKEY_REJECT), sshauthenticatedwith(0), ssh_session(0) {
    setKeysIntern();
}

SSH2Client::SSH2Client(QoreURL &url, const uint32_t port) :
    sshhost(url.getHost() ? url.getHost()->getBuffer() : ""),
    sshuser(url.getUserName() ? url.getUserName()->getBuffer() : ""),
    sshpass(url.getPassword() ? url.getPassword()->getBuffer() : ""),
    sshport(port ? port : (uint32_t)url.getPort()),
    keepalive_interval(QKEEPALIVE_DEFAULT),
    use_agent(true),
    verify_host_key(false),
    host_key_policy(SSH2_HOSTKEY_REJECT),
    sshauthenticatedwith(0),
    ssh_session(0) {
    if (!sshport)
        sshport = DEFAULT_SSH_PORT;

    setKeysIntern();
}

/*
 * close session/connection
 * free resources
 */
SSH2Client::~SSH2Client() {
    QORE_TRACE("SSH2Client::~SSH2Client()");
    printd(5, "SSH2Client::~SSH2Client() this: %p\n", this);

    // disconnect
    disconnectUnlocked(true);
}

void SSH2Client::setKeysIntern() {
#ifdef HAVE_PWD_H
    // prefill the user and 'estimate' the key files for rsa
    struct passwd* usrpwd = getpwuid(getuid());
    //printd(5, "SSH2Client::setKeysIntern() usrpwd: %p (sshuser: '%s')\n", usrpwd, sshuser.c_str());

    // set the keys automatically from the current user's information if there is no explicit user (in which case the
    // current user will be set automatically) or if the explicit user is the same as the current user
    if (usrpwd && (sshuser.empty() || sshuser == usrpwd->pw_name)) {
        // only set keys if the current user has access to the filesystem
        if (!(getProgram()->getParseOptions() & PO_NO_FILESYSTEM)) {
            sshkeys_priv = usrpwd->pw_dir;
            sshkeys_priv += "/.ssh/id_rsa";
            if (!q_path_is_readable(sshkeys_priv.c_str())) {
                printd(5, "SSH2Client::setKeysIntern() skipping automatic setting of keys because '%s' is not readable\n", sshkeys_priv.c_str());
                sshkeys_priv.clear();
                return;
            }
            printd(5, "SSH2Client::setKeysIntern() set priv: '%s'\n", sshkeys_priv.c_str());
            sshkeys_pub = usrpwd->pw_dir;
            sshkeys_pub += "/.ssh/id_rsa.pub";
            if (!q_path_is_readable(sshkeys_pub.c_str())) {
                printd(5, "SSH2Client::setKeysIntern() skipping automatic setting of keys because '%s' is not readable\n", sshkeys_pub.c_str());
                sshkeys_priv.clear();
                sshkeys_pub.clear();
                return;
            }
            printd(5, "SSH2Client::setKeysIntern() set pub: '%s'\n", sshkeys_pub.c_str());
        }
        if (sshuser.empty()) {
            sshuser = usrpwd->pw_name;
        }
    }
#endif
}

/**
 * disconnect from the server if connected
 *
 * if force is not 0 then there will be no exception written.
 *
 * return 0 on ok.
 * sets errno
 */
int SSH2Client::disconnectUnlocked(bool force, int timeout_ms, AbstractDisconnectionHelper* adh, ExceptionSink *xsink) {
#ifdef HAVE_LIBSSH2_FORWARD_LISTEN
    // cancel all active listeners
    for (auto it = listener_set.begin(); it != listener_set.end(); ) {
        SSH2Listener* sl = *it;
        ++it;
        sl->cancelUnlocked();
    }
    listener_set.clear();
#endif

    // close all open channels
    for (channel_set_t::iterator i = channel_set.begin(), e = channel_set.end(); i != e; ++i) {
        (*i)->closeUnlocked();
    }

    if (!ssh_session) {
        if (!force) {
            errno = EINVAL;
            xsink && xsink->raiseException(SSH2CLIENT_NOT_CONNECTED, "disconnect(): %s", strerror(errno));
        }
    } else {
        if (adh)
            adh->preDisconnect();

        setBlockingUnlocked(false);

        // close ssh session if not null
        int rc;
        while ((rc = libssh2_session_disconnect(ssh_session, (char*)"qore program disconnect")) == LIBSSH2_ERROR_EAGAIN) {
            if (waitSocketUnlocked(xsink, SSH2CLIENT_TIMEOUT, "SSSHCLIENT-DISCONNECT", "SSHClient::disconnect", timeout_ms, true))
                break;
        }

        while ((rc = libssh2_session_free(ssh_session)) == LIBSSH2_ERROR_EAGAIN) {
            // there can be a memory leak here, but there is no other way to free memory without waiting for the remote socket
            if (waitSocketUnlocked(xsink, SSH2CLIENT_TIMEOUT, "SSSHCLIENT-DISCONNECT", "SSHClient::disconnect", timeout_ms, true))
                break;
        }

        ssh_session = 0;
    }

    if (sshauthenticatedwith)
        sshauthenticatedwith = 0;

    socket.close();
    return 0;
}

/**
 * return 1 if we think we are connected
 */
int SSH2Client::sshConnectedUnlocked() {
    return (ssh_session? 1: 0);
}

int SSH2Client::sshConnected() {
    AutoLocker al(m);

    return sshConnectedUnlocked();
}

QoreObject* SSH2Client::registerChannelUnlocked(LIBSSH2_CHANNEL *channel) {
    return new QoreObject(QC_SSH2CHANNEL, getProgram(), registerChannelUnlockedRaw(channel));
}

SSH2Channel* SSH2Client::registerChannelUnlockedRaw(LIBSSH2_CHANNEL *channel) {
    SSH2Channel* chan = new SSH2Channel(channel, this);
    channel_set.insert(chan);
    return chan;
}

const char *SSH2Client::getHost() {
    return sshhost.c_str();
}

const uint32_t SSH2Client::getPort() {
    return sshport;
}

const char *SSH2Client::getAuthenticatedWith() {
    return sshauthenticatedwith;
}

void SSH2Client::deref(ExceptionSink *xsink) {
    if (ROdereference()) {
#ifdef _QORE_HAS_SOCKET_PERF_API
        // this function is only exported in versions of qore with the socket performance API
        // and must be called before the QoreSocket object is destroyed
        socket.cleanup(xsink);
#endif
        delete this;
    }
}

int SSH2Client::setUser(const char *user) {
    AutoLocker al(m);

    if (sshConnectedUnlocked())
        return -1;

    sshuser = user;
    return 0;
}

const char *SSH2Client::getUser() {
    return sshuser.c_str();
}

int SSH2Client::setPassword(const char *pwd) {
    AutoLocker al(m);

    if (sshConnectedUnlocked())
        return -1;

    sshpass = pwd;
    return 0;
}

const char* SSH2Client::getPassword() {
    return sshpass.c_str();
}

int SSH2Client::setKeys(const char *priv, const char *pub, ExceptionSink* xsink) {
    AutoLocker al(m);

    if (sshConnectedUnlocked()) {
        xsink->raiseException(SSH2_CONNECTED, "usage of SSH2Base::setKeys() is not allowed when connected");
        return -1;
    }

    sshkeys_priv.clear();
    sshkeys_pub.clear();

    // if the strings are null then ignore
    if (priv && strlen(priv)) {
        sshkeys_priv = priv;
#ifdef _QORE_HAS_PATH_IS_READABLE
        if (!q_path_is_readable(sshkeys_priv.c_str())) {
            xsink->raiseException("SSH2-SETKEYS-ERROR", "private key '%s' is not readable", sshkeys_priv.c_str());
            sshkeys_priv.clear();
            return -1;
        }
#endif

        // Check filesystem sandbox access for private key
        QoreSandboxManagerHelper smh;
        if (smh && !smh->checkFilesystemAccess(sshkeys_priv.c_str(), QSEC_READ, xsink)) {
            sshkeys_priv.clear();
            return -1;
        }

        if (pub)
            sshkeys_pub = pub;
        else {
            sshkeys_pub = priv;
            sshkeys_pub += ".pub";
        }

#ifdef _QORE_HAS_PATH_IS_READABLE
        if (!q_path_is_readable(sshkeys_pub.c_str())) {
            xsink->raiseException("SSH2-SETKEYS-ERROR", "public key '%s' is not readable", sshkeys_pub.c_str());
            sshkeys_priv.clear();
            sshkeys_pub.clear();
            return -1;
        }
#endif

        // Check filesystem sandbox access for public key
        if (smh && !smh->checkFilesystemAccess(sshkeys_pub.c_str(), QSEC_READ, xsink)) {
            sshkeys_priv.clear();
            sshkeys_pub.clear();
            return -1;
        }

    }
    return 0;
}

const char *SSH2Client::getKeyPriv() {
    return sshkeys_priv.c_str();
}

const char *SSH2Client::getKeyPub() {
    return sshkeys_pub.c_str();
}

QoreStringNode *SSH2Client::fingerprintUnlocked() {
    if (!sshConnectedUnlocked())
        return 0;

    const char *fingerprint = libssh2_hostkey_hash(ssh_session, LIBSSH2_HOSTKEY_HASH_MD5);

    if (!fingerprint)
        return 0;

    QoreStringNode *fpstr = new QoreStringNode;
    fpstr->sprintf("%02X", (unsigned char)fingerprint[0]);
    for (int i = 1; i < 16; i++)
        fpstr->sprintf(":%02X", (unsigned char)fingerprint[i]);
    return fpstr;
}

/**
 * return the fingerprint given from the server as md5 string
 */
QoreStringNode *SSH2Client::fingerprint() {
    AutoLocker al(m);

    return fingerprintUnlocked();
}

static void kbd_callback(const char *name, int name_len,
        const char *instruction, int instruction_len, int num_prompts,
        const LIBSSH2_USERAUTH_KBDINT_PROMPT *prompts,
        LIBSSH2_USERAUTH_KBDINT_RESPONSE *responses,
        void **abstract) {
    const char *password = keyboardPassword.get();
    //printd(5, "kdb_callback() num_prompts=%d pass=%s\n", num_prompts, password);
    if (num_prompts == 1) {
        responses[0].text = strdup(password);
        responses[0].length = strlen(password);
    }
} /* kbd_callback */

int SSH2Client::startupUnlocked() {
#ifdef HAVE_LIBSSH2_SESSION_HANDSHAKE
    return libssh2_session_handshake(ssh_session, socket.getSocket());
#else
    return libssh2_session_startup(ssh_session, socket.getSocket());
#endif
}

/**
 * connect()
 * returns:
 * 0    ok
 * 1    host not found
 * 2    port not identified
 * 3    socket not created
 * 4    session init failure
 */
int SSH2Client::sshConnectUnlocked(int timeout_ms, ExceptionSink *xsink = 0) {
    // check for host connectivity
    // getaddrinfo(3)
    // see Socket class
    // create socket
    // init session
    // set to blocking
    // startup session with socket

    static const char *SSH2CLIENT_CONNECT_ERROR = "SSH2CLIENT-CONNECT-ERROR";

    QORE_TRACE("SSH2Client::connect()");

    printd(1, "SSH2Client::connect(%s:%d, %dms)\n", sshhost.c_str(), sshport, timeout_ms);

    // Check for interrupt before connect
    if (qore_check_cancel(xsink)) {
        return -1;
    }

    // sanity check of data
    if (sshuser.empty()) {
        xsink && xsink->raiseException(SSH2CLIENT_CONNECT_ERROR, "ssh user must not be NOTHING");
        return -1;
    }

    int auth_pw = 0;
    char *userauthlist;
    int rc;

    bool loggedin = false; // tells us if we are logged in (or at least think so)

    // force disconnect session if already connected
    if (ssh_session)
        disconnectUnlocked(true);

    if (socket.connectINET(sshhost.c_str(), sshport, timeout_ms, xsink))
        return -1;

    // Create a session instance
    ssh_session = libssh2_session_init();
    if (!ssh_session) {
        disconnectUnlocked(true); // clean up connection
        xsink && xsink->raiseException(SSH2_ERROR, "error in libssh2_session_init(): ", strerror(errno));
        return -1;
    }

    // apply algorithm preferences before handshake
    for (auto& p : method_prefs) {
        int prc = libssh2_session_method_pref(ssh_session, p.first, p.second.c_str());
        if (prc) {
            printd(5, "SSH2Client::connect(): libssh2_session_method_pref(%d, '%s') returned %d\n", p.first, p.second.c_str(), prc);
        }
    }

    // apply banner before handshake
    if (!ssh_banner.empty()) {
        libssh2_session_banner_set(ssh_session, ssh_banner.c_str());
    }

    // make sure the connection is made with non-blocking I/O
    setBlockingUnlocked(false);

    // ... start it up. This will trade welcome banners, exchange keys,
    // and setup crypto, compression, and MAC layers
    while ((rc = startupUnlocked()) == LIBSSH2_ERROR_EAGAIN) {
        if (waitSocketUnlocked(xsink, SSH2CLIENT_TIMEOUT, SSH2_ERROR, "SSH2Client::connect", timeout_ms)) {
            disconnectUnlocked(true); // clean up connection
            return -1;
        }
    }

    if (rc) {
        disconnectUnlocked(true); // clean up connection
        xsink && xsink->raiseException(SSH2_ERROR, "failure establishing SSH session: %d", rc);
        return -1;
    }

#ifdef HAVE_LIBSSH2_KNOWNHOST_API
    // verify host key if enabled
    if (verify_host_key) {
        size_t hostkey_len;
        int hostkey_type;
        const char* hostkey = libssh2_session_hostkey(ssh_session, &hostkey_len, &hostkey_type);
        if (!hostkey) {
            disconnectUnlocked(true);
            xsink && xsink->raiseException("SSH2-HOSTKEY-ERROR", "could not retrieve server host key");
            return -1;
        }

        LIBSSH2_KNOWNHOSTS* nh = libssh2_knownhost_init(ssh_session);
        if (!nh) {
            disconnectUnlocked(true);
            xsink && xsink->raiseException("SSH2-HOSTKEY-ERROR", "could not initialize known hosts context");
            return -1;
        }

        // load known hosts file if specified
        if (!known_hosts_file.empty()) {
            libssh2_knownhost_readfile(nh, known_hosts_file.c_str(), LIBSSH2_KNOWNHOST_FILE_OPENSSH);
        }

        // determine key type for check
        int kh_type = LIBSSH2_KNOWNHOST_TYPE_PLAIN | LIBSSH2_KNOWNHOST_KEYENC_RAW;
        switch (hostkey_type) {
            case LIBSSH2_HOSTKEY_TYPE_RSA:
                kh_type |= LIBSSH2_KNOWNHOST_KEY_SSHRSA;
                break;
            case LIBSSH2_HOSTKEY_TYPE_DSS:
                kh_type |= LIBSSH2_KNOWNHOST_KEY_SSHDSS;
                break;
#ifdef LIBSSH2_HOSTKEY_TYPE_ECDSA_256
            case LIBSSH2_HOSTKEY_TYPE_ECDSA_256:
                kh_type |= LIBSSH2_KNOWNHOST_KEY_ECDSA_256;
                break;
            case LIBSSH2_HOSTKEY_TYPE_ECDSA_384:
                kh_type |= LIBSSH2_KNOWNHOST_KEY_ECDSA_384;
                break;
            case LIBSSH2_HOSTKEY_TYPE_ECDSA_521:
                kh_type |= LIBSSH2_KNOWNHOST_KEY_ECDSA_521;
                break;
#endif
#ifdef LIBSSH2_HOSTKEY_TYPE_ED25519
            case LIBSSH2_HOSTKEY_TYPE_ED25519:
                kh_type |= LIBSSH2_KNOWNHOST_KEY_ED25519;
                break;
#endif
            default:
                kh_type |= LIBSSH2_KNOWNHOST_KEY_UNKNOWN;
                break;
        }

        int check = libssh2_knownhost_checkp(nh, sshhost.c_str(), sshport, hostkey, hostkey_len, kh_type, nullptr);

        if (check == LIBSSH2_KNOWNHOST_CHECK_MISMATCH) {
            libssh2_knownhost_free(nh);
            disconnectUnlocked(true);
            xsink && xsink->raiseException("SSH2-HOSTKEY-MISMATCH",
                "host key for '%s:%d' does not match the key in the known hosts file '%s'; "
                "this could indicate a man-in-the-middle attack",
                sshhost.c_str(), sshport, known_hosts_file.c_str());
            return -1;
        } else if (check == LIBSSH2_KNOWNHOST_CHECK_NOTFOUND) {
            if (host_key_policy == SSH2_HOSTKEY_TOFU) {
                // Trust On First Use: add the key and continue
                libssh2_knownhost_addc(nh, sshhost.c_str(), nullptr, hostkey, hostkey_len, nullptr, 0, kh_type, nullptr);
                if (!known_hosts_file.empty()) {
                    libssh2_knownhost_writefile(nh, known_hosts_file.c_str(), LIBSSH2_KNOWNHOST_FILE_OPENSSH);
                }
                printd(5, "SSH2Client::connect(): host key for '%s:%d' added to known hosts (TOFU)\n", sshhost.c_str(), sshport);
            } else {
                // REJECT: raise exception
                libssh2_knownhost_free(nh);
                disconnectUnlocked(true);
                xsink && xsink->raiseException("SSH2-HOSTKEY-UNKNOWN",
                    "host key for '%s:%d' is not in the known hosts file '%s'; "
                    "use addKnownHost() or setHostKeyPolicy(SSH2_HOSTKEY_TOFU) to accept it",
                    sshhost.c_str(), sshport, known_hosts_file.c_str());
                return -1;
            }
        } else if (check == LIBSSH2_KNOWNHOST_CHECK_FAILURE) {
            libssh2_knownhost_free(nh);
            disconnectUnlocked(true);
            xsink && xsink->raiseException("SSH2-HOSTKEY-ERROR", "internal error checking known hosts for '%s:%d'", sshhost.c_str(), sshport);
            return -1;
        }
        // LIBSSH2_KNOWNHOST_CHECK_MATCH: host key matches, continue

        libssh2_knownhost_free(nh);
    }
#endif

    // check what types are available for authentifcation
    while (true) {
        userauthlist = libssh2_userauth_list(ssh_session, sshuser.c_str(), sshuser.size());
        if (!userauthlist && libssh2_session_last_errno(ssh_session) == LIBSSH2_ERROR_EAGAIN) {
            if (waitSocketUnlocked(xsink, SSH2CLIENT_TIMEOUT, SSH2_ERROR, "SSH2Client::connect", timeout_ms)) {
                disconnectUnlocked(true); // clean up connection
                return -1;
            }
            continue;
        }
        break;
    }

    assert(!sshauthenticatedwith);

    printd(5, "userauthlist: %s\n", userauthlist ? userauthlist : "n/a");

    // set flags for use with authentification if we have the required information
    if (userauthlist) {
        // only try pubkey authentication if we have the required keys
        if (strstr(userauthlist, "publickey") && !sshkeys_priv.empty() && !sshkeys_pub.empty()) {
            auth_pw |= QAUTH_PUBLICKEY;
        }
        // only try password authentication if we have a password
        if (!sshpass.empty()) {
            if (strstr(userauthlist, "password")) {
                auth_pw |= QAUTH_PASSWORD;
            }
            if (strstr(userauthlist, "keyboard-interactive")) {
                auth_pw |= QAUTH_KEYBOARD_INTERACTIVE;
            }
        }
    }

    // try auth
#ifdef HAVE_LIBSSH2_AGENT_API
    // try SSH agent authentication first (most secure and convenient when available)
    if (!loggedin && use_agent && userauthlist && strstr(userauthlist, "publickey")) {
        printd(5, "SSH2Client::connect(): trying SSH agent authentication\n");
        LIBSSH2_AGENT* agent = libssh2_agent_init(ssh_session);
        if (agent) {
            if (!libssh2_agent_connect(agent)) {
                if (!libssh2_agent_list_identities(agent)) {
                    struct libssh2_agent_publickey* identity = nullptr;
                    struct libssh2_agent_publickey* prev_identity = nullptr;
                    while (!libssh2_agent_get_identity(agent, &identity, prev_identity)) {
                        while ((rc = libssh2_agent_userauth(agent, sshuser.c_str(), identity)) == LIBSSH2_ERROR_EAGAIN) {
                            if (waitSocketUnlocked(xsink, SSH2CLIENT_TIMEOUT, SSH2_ERROR, "SSH2Client::connect", timeout_ms)) {
                                libssh2_agent_disconnect(agent);
                                libssh2_agent_free(agent);
                                disconnectUnlocked(true);
                                return -1;
                            }
                        }
                        if (!rc) {
                            loggedin = true;
                            sshauthenticatedwith = "agent";
                            printd(5, "SSH agent authentication succeeded\n");
                            break;
                        }
                        prev_identity = identity;
                    }
                }
                libssh2_agent_disconnect(agent);
            }
            libssh2_agent_free(agent);
        }
#ifdef DEBUG
        if (!loggedin) {
            printd(5, "SSH agent authentication failed or agent not available\n");
        }
#endif
    }
#endif

#ifdef HAVE_LIBSSH2_PUBLICKEY_FROMMEMORY
    // try publickey from memory if key data was provided
    if (!loggedin && !sshkeys_priv_data.empty() && userauthlist && strstr(userauthlist, "publickey")) {
        printd(5, "SSH2Client::connect(): try publickey auth from memory\n");
        const char* passphrase = sshkeys_passphrase.empty() ? (sshpass.empty() ? "" : sshpass.c_str()) : sshkeys_passphrase.c_str();
        while ((rc = libssh2_userauth_publickey_frommemory(ssh_session, sshuser.c_str(), sshuser.size(),
                sshkeys_pub_data.empty() ? nullptr : sshkeys_pub_data.c_str(),
                sshkeys_pub_data.size(),
                sshkeys_priv_data.c_str(), sshkeys_priv_data.size(),
                passphrase)) == LIBSSH2_ERROR_EAGAIN) {
            if (waitSocketUnlocked(xsink, SSH2CLIENT_TIMEOUT, SSH2_ERROR, "SSH2Client::connect", timeout_ms)) {
                disconnectUnlocked(true);
                return -1;
            }
        }
        if (!rc) {
            loggedin = true;
            sshauthenticatedwith = "publickey";
            printd(5, "publickey (from memory) authentication succeeded\n");
        }
#ifdef DEBUG
        else {
            printd(5, "publickey (from memory) authentication failed\n");
        }
#endif
    }
#endif

    // try publickey from file if available
    if (!loggedin && (auth_pw & QAUTH_PUBLICKEY)) {
        // Verify filesystem sandbox access for key files before reading
        QoreSandboxManagerHelper smh;
        if (smh) {
            if (!smh->checkFilesystemAccess(sshkeys_priv.c_str(), QSEC_READ, xsink)) {
                disconnectUnlocked(true);
                return -1;
            }
            if (!smh->checkFilesystemAccess(sshkeys_pub.c_str(), QSEC_READ, xsink)) {
                disconnectUnlocked(true);
                return -1;
            }
        }

        printd(5, "SSH2Client::connect(): try pubkey auth: %s %s\n", sshkeys_priv.c_str(), sshkeys_pub.c_str());
        while ((rc = libssh2_userauth_publickey_fromfile(ssh_session, sshuser.c_str(), sshkeys_pub.c_str(), sshkeys_priv.c_str(), sshpass.c_str())) == LIBSSH2_ERROR_EAGAIN) {
            if (waitSocketUnlocked(xsink, SSH2CLIENT_TIMEOUT, SSH2_ERROR, "SSH2Client::connect", timeout_ms)) {
                disconnectUnlocked(true); // clean up connection
                return -1;
            }
        }
        if (!rc) {
            loggedin = true;
            sshauthenticatedwith = "publickey";
            printd(5, "publickey authentication succeeded\n");
        }
#ifdef DEBUG
        else
            printd(5, "publickey authentication failed\n");
#endif
    } else {
            printd(5, "no publickey authentication attempted: priv: '%s' pub: '%s'\n", sshkeys_priv.empty() ? "n/a" : sshkeys_priv.c_str(), sshkeys_pub.empty() ? "n/a" : sshkeys_pub.c_str());
    }

    // try password and keyboard-interactive first if a password was given
    if (!loggedin && (auth_pw & QAUTH_PASSWORD)) {
        printd(5, "SSH2Client::connect(): try user/pass auth: %s/%s\n", sshuser.c_str(), sshpass.c_str());
        while ((rc = libssh2_userauth_password(ssh_session, sshuser.c_str(), sshpass.c_str())) == LIBSSH2_ERROR_EAGAIN) {
            if (waitSocketUnlocked(xsink, SSH2CLIENT_TIMEOUT, SSH2_ERROR, "SSH2Client::connect", timeout_ms)) {
                disconnectUnlocked(true); // clean up connection
                return -1;
            }
        }
        if (!rc) {
            loggedin = true;
            sshauthenticatedwith = "password";
            printd(5, "password authentication succeeded\n");
        }
#ifdef DEBUG
        else
            printd(5, "password authentication failed\n");
#endif
    }

    if (!loggedin && (auth_pw & QAUTH_KEYBOARD_INTERACTIVE)) {
        printd(5, "SSH2Client::connect(): try user/pass with keyboard-interactive auth: %s/%s\n", sshuser.c_str(), sshpass.c_str());
        // thread thread-local storage for password for fake keyboard-interactive authentication
        keyboardPassword.set(sshpass.c_str());
        while ((rc = libssh2_userauth_keyboard_interactive(ssh_session, sshuser.c_str(), &kbd_callback)) == LIBSSH2_ERROR_EAGAIN) {
            if (waitSocketUnlocked(xsink, SSH2CLIENT_TIMEOUT, SSH2_ERROR, "SSH2Client::connect", timeout_ms)) {
                disconnectUnlocked(true); // clean up connection
                return -1;
            }
        }
        if (!rc) {
            loggedin = true;
            sshauthenticatedwith = "keyboard-interactive";
            printd(5, "keyboard-interactive authentication succeeded\n");
        }
#ifdef DEBUG
        else
            printd(5, "keyboard-interactive authentication failed\n");
#endif
    }

    // could we auth?
    if (!loggedin) {
        disconnectUnlocked(true); // clean up connection
        xsink && xsink->raiseException("SSH2CLIENT-AUTH-ERROR", "No proper authentication method found");
        return -1;
    }

    setBlockingUnlocked(true);

#ifdef HAVE_LIBSSH2_KEEPALIVE_CONFIG
    // set keepalive
    if (keepalive_interval > 0) {
        libssh2_keepalive_config(ssh_session, 1, keepalive_interval);
    }
#endif

    return 0;
}

int method_type_from_string(const char* method_type, ExceptionSink* xsink) {
    if (!strcasecmp(method_type, "KEX")) {
        return LIBSSH2_METHOD_KEX;
    } else if (!strcasecmp(method_type, "HOSTKEY")) {
        return LIBSSH2_METHOD_HOSTKEY;
    } else if (!strcasecmp(method_type, "CRYPT_CS")) {
        return LIBSSH2_METHOD_CRYPT_CS;
    } else if (!strcasecmp(method_type, "CRYPT_SC")) {
        return LIBSSH2_METHOD_CRYPT_SC;
    } else if (!strcasecmp(method_type, "MAC_CS")) {
        return LIBSSH2_METHOD_MAC_CS;
    } else if (!strcasecmp(method_type, "MAC_SC")) {
        return LIBSSH2_METHOD_MAC_SC;
    } else if (!strcasecmp(method_type, "COMP_CS")) {
        return LIBSSH2_METHOD_COMP_CS;
    } else if (!strcasecmp(method_type, "COMP_SC")) {
        return LIBSSH2_METHOD_COMP_SC;
    }
    xsink->raiseException("SSH2-METHOD-ERROR", "invalid method type '%s'; expected one of: KEX, HOSTKEY, CRYPT_CS, CRYPT_SC, MAC_CS, MAC_SC, COMP_CS, COMP_SC", method_type);
    return -1;
}

int SSH2Client::setMethodPreference(int method_type, const char* prefs, ExceptionSink* xsink) {
    AutoLocker al(m);
    if (sshConnectedUnlocked()) {
        xsink->raiseException(SSH2_CONNECTED, "usage of SSH2Base::setMethodPreference() is not allowed when connected");
        return -1;
    }
    method_prefs[method_type] = prefs;
    return 0;
}

QoreListNode* SSH2Client::getSupportedAlgorithms(int method_type, ExceptionSink* xsink) {
#ifdef HAVE_LIBSSH2_SUPPORTED_ALGS
    AutoLocker al(m);

    // we need a session to query supported algs; create a temporary one if not connected
    LIBSSH2_SESSION* session = ssh_session;
    bool temp_session = false;
    if (!session) {
        session = libssh2_session_init();
        if (!session) {
            xsink->raiseException(SSH2_ERROR, "could not create temporary session for algorithm query");
            return nullptr;
        }
        temp_session = true;
    }

    const char** algs = nullptr;
    int rc = libssh2_session_supported_algs(session, method_type, &algs);

    if (rc <= 0) {
        if (temp_session) {
            libssh2_session_free(session);
        }
        xsink->raiseException(SSH2_ERROR, "could not get supported algorithms for method type %d", method_type);
        return nullptr;
    }

    // copy the algorithm names before freeing the session/algs
    ReferenceHolder<QoreListNode> ret(new QoreListNode(stringTypeInfo), xsink);
    for (int i = 0; i < rc; i++) {
        ret->push(new QoreStringNode(algs[i]), xsink);
    }

    // free algs before freeing the session that allocated them
    libssh2_free(session, algs);

    if (temp_session) {
        libssh2_session_free(session);
    }

    return ret.release();
#else
    xsink->raiseException("MISSING-FEATURE-ERROR", "the getSupportedAlgorithms() method is not available; the ssh2 module was compiled with a version of libssh2 that does not support this operation");
    return nullptr;
#endif
}

int SSH2Client::setBanner(const char* banner, ExceptionSink* xsink) {
    AutoLocker al(m);
    if (sshConnectedUnlocked()) {
        xsink->raiseException(SSH2_CONNECTED, "usage of SSH2Base::setBanner() is not allowed when connected");
        return -1;
    }
    ssh_banner = banner ? banner : "";
    return 0;
}

QoreStringNode* SSH2Client::getBannerLocked(ExceptionSink* xsink) {
    AutoLocker al(m);
    if (!sshConnectedUnlocked()) {
        // return the stored banner if not connected
        return ssh_banner.empty() ? nullptr : new QoreStringNode(ssh_banner);
    }
    // when connected, get the server's banner
    const char* banner = libssh2_session_banner_get(ssh_session);
    return banner ? new QoreStringNode(banner) : nullptr;
}

int SSH2Client::setTraceLevelLocked(int bitmask, ExceptionSink* xsink) {
#ifdef HAVE_LIBSSH2_TRACE
    AutoLocker al(m);
    if (!sshConnectedUnlocked()) {
        xsink->raiseException(SSH2CLIENT_NOT_CONNECTED, "cannot call SSH2Base::setTraceLevel() while client is not connected");
        return -1;
    }
    libssh2_trace(ssh_session, bitmask);
    return 0;
#else
    xsink->raiseException("MISSING-FEATURE-ERROR", "the setTraceLevel() method is not available; the ssh2 module was compiled with a version of libssh2 that does not support this operation");
    return -1;
#endif
}

QoreHashNode* SSH2Client::getHostKeyLocked(ExceptionSink* xsink) {
    AutoLocker al(m);

    if (!sshConnectedUnlocked()) {
        xsink->raiseException(SSH2CLIENT_NOT_CONNECTED, "cannot call SSH2Base::getHostKey() while client is not connected");
        return nullptr;
    }

    size_t len;
    int type;
    const char* hostkey = libssh2_session_hostkey(ssh_session, &len, &type);
    if (!hostkey) {
        xsink->raiseException("SSH2-HOSTKEY-ERROR", "could not retrieve server host key");
        return nullptr;
    }

    ReferenceHolder<QoreHashNode> ret(new QoreHashNode(hashdeclSsh2HostKeyInfo, xsink), xsink);

    // MD5 fingerprint
    const char* md5 = libssh2_hostkey_hash(ssh_session, LIBSSH2_HOSTKEY_HASH_MD5);
    if (md5) {
        QoreStringNode* fpstr = new QoreStringNode;
        fpstr->sprintf("%02X", (unsigned char)md5[0]);
        for (int i = 1; i < 16; i++) {
            fpstr->sprintf(":%02X", (unsigned char)md5[i]);
        }
        ret->setKeyValue("hash_md5", fpstr, xsink);
    }

    // SHA1 fingerprint
    const char* sha1 = libssh2_hostkey_hash(ssh_session, LIBSSH2_HOSTKEY_HASH_SHA1);
    if (sha1) {
        QoreStringNode* fpstr = new QoreStringNode;
        fpstr->sprintf("%02X", (unsigned char)sha1[0]);
        for (int i = 1; i < 20; i++) {
            fpstr->sprintf(":%02X", (unsigned char)sha1[i]);
        }
        ret->setKeyValue("hash_sha1", fpstr, xsink);
    }

    // SHA256 fingerprint
#ifdef LIBSSH2_HOSTKEY_HASH_SHA256
    const char* sha256 = libssh2_hostkey_hash(ssh_session, LIBSSH2_HOSTKEY_HASH_SHA256);
    if (sha256) {
        QoreStringNode* fpstr = new QoreStringNode;
        fpstr->sprintf("%02X", (unsigned char)sha256[0]);
        for (int i = 1; i < 32; i++) {
            fpstr->sprintf(":%02X", (unsigned char)sha256[i]);
        }
        ret->setKeyValue("hash_sha256", fpstr, xsink);
    }
#endif

    // key type
    const char* type_str;
    switch (type) {
        case LIBSSH2_HOSTKEY_TYPE_RSA: type_str = "ssh-rsa"; break;
        case LIBSSH2_HOSTKEY_TYPE_DSS: type_str = "ssh-dss"; break;
#ifdef LIBSSH2_HOSTKEY_TYPE_ECDSA_256
        case LIBSSH2_HOSTKEY_TYPE_ECDSA_256: type_str = "ecdsa-sha2-nistp256"; break;
        case LIBSSH2_HOSTKEY_TYPE_ECDSA_384: type_str = "ecdsa-sha2-nistp384"; break;
        case LIBSSH2_HOSTKEY_TYPE_ECDSA_521: type_str = "ecdsa-sha2-nistp521"; break;
#endif
#ifdef LIBSSH2_HOSTKEY_TYPE_ED25519
        case LIBSSH2_HOSTKEY_TYPE_ED25519: type_str = "ssh-ed25519"; break;
#endif
        default: type_str = "unknown"; break;
    }
    ret->setKeyValue("key_type", new QoreStringNode(type_str), xsink);

    // raw key data
    SimpleRefHolder<BinaryNode> key_data(new BinaryNode);
    key_data->append(hostkey, len);
    ret->setKeyValue("key_data", key_data.release(), xsink);

    return ret.release();
}

#ifdef HAVE_LIBSSH2_KNOWNHOST_API
int SSH2Client::addKnownHostLocked(const char* host, int port, ExceptionSink* xsink) {
    AutoLocker al(m);

    if (!sshConnectedUnlocked()) {
        xsink->raiseException(SSH2CLIENT_NOT_CONNECTED, "cannot call SSH2Base::addKnownHost() while client is not connected");
        return -1;
    }

    if (known_hosts_file.empty()) {
        xsink->raiseException("SSH2-HOSTKEY-ERROR", "no known hosts file configured; call setKnownHostsFile() first");
        return -1;
    }

    size_t hostkey_len;
    int hostkey_type;
    const char* hostkey = libssh2_session_hostkey(ssh_session, &hostkey_len, &hostkey_type);
    if (!hostkey) {
        xsink->raiseException("SSH2-HOSTKEY-ERROR", "could not retrieve server host key");
        return -1;
    }

    LIBSSH2_KNOWNHOSTS* nh = libssh2_knownhost_init(ssh_session);
    if (!nh) {
        xsink->raiseException("SSH2-HOSTKEY-ERROR", "could not initialize known hosts context");
        return -1;
    }

    // load existing file
    libssh2_knownhost_readfile(nh, known_hosts_file.c_str(), LIBSSH2_KNOWNHOST_FILE_OPENSSH);

    // determine key type
    int kh_type = LIBSSH2_KNOWNHOST_TYPE_PLAIN | LIBSSH2_KNOWNHOST_KEYENC_RAW;
    switch (hostkey_type) {
        case LIBSSH2_HOSTKEY_TYPE_RSA:
            kh_type |= LIBSSH2_KNOWNHOST_KEY_SSHRSA;
            break;
        case LIBSSH2_HOSTKEY_TYPE_DSS:
            kh_type |= LIBSSH2_KNOWNHOST_KEY_SSHDSS;
            break;
#ifdef LIBSSH2_HOSTKEY_TYPE_ECDSA_256
        case LIBSSH2_HOSTKEY_TYPE_ECDSA_256:
            kh_type |= LIBSSH2_KNOWNHOST_KEY_ECDSA_256;
            break;
        case LIBSSH2_HOSTKEY_TYPE_ECDSA_384:
            kh_type |= LIBSSH2_KNOWNHOST_KEY_ECDSA_384;
            break;
        case LIBSSH2_HOSTKEY_TYPE_ECDSA_521:
            kh_type |= LIBSSH2_KNOWNHOST_KEY_ECDSA_521;
            break;
#endif
#ifdef LIBSSH2_HOSTKEY_TYPE_ED25519
        case LIBSSH2_HOSTKEY_TYPE_ED25519:
            kh_type |= LIBSSH2_KNOWNHOST_KEY_ED25519;
            break;
#endif
        default:
            kh_type |= LIBSSH2_KNOWNHOST_KEY_UNKNOWN;
            break;
    }

    const char* use_host = host ? host : sshhost.c_str();
    int use_port = port > 0 ? port : (int)sshport;

    int rc = libssh2_knownhost_addc(nh, use_host, nullptr, hostkey, hostkey_len, nullptr, 0, kh_type, nullptr);
    if (rc) {
        libssh2_knownhost_free(nh);
        xsink->raiseException("SSH2-HOSTKEY-ERROR", "could not add host key for '%s:%d'", use_host, use_port);
        return -1;
    }

    rc = libssh2_knownhost_writefile(nh, known_hosts_file.c_str(), LIBSSH2_KNOWNHOST_FILE_OPENSSH);
    libssh2_knownhost_free(nh);

    if (rc) {
        xsink->raiseException("SSH2-HOSTKEY-ERROR", "could not write known hosts file '%s'", known_hosts_file.c_str());
        return -1;
    }

    return 0;
}
#endif

int SSH2Client::sshConnect(int timeout_ms, ExceptionSink *xsink = 0) {
    AutoLocker al(m);

    return sshConnectUnlocked(timeout_ms, xsink);
}

QoreHashNode *SSH2Client::sshInfo(const TypedHashDecl* hashdecl, ExceptionSink* xsink) {
    AutoLocker al(m);

    return sshInfoIntern(hashdecl, xsink);
}

QoreHashNode *SSH2Client::sshInfoIntern(const TypedHashDecl* hashdecl, ExceptionSink* xsink) {
    ReferenceHolder<QoreHashNode> ret(new QoreHashNode(hashdecl, xsink), xsink);
    ret->setKeyValue("ssh2host", new QoreStringNode(getHost()), xsink);
    ret->setKeyValue("ssh2port", getPort(), xsink);
    ret->setKeyValue("ssh2user", new QoreStringNode(getUser()), xsink);
    ret->setKeyValue("keyfile_priv", new QoreStringNode(getKeyPriv()), xsink);
    ret->setKeyValue("keyfile_pub", new QoreStringNode(getKeyPub()), xsink);
    ret->setKeyValue("fingerprint", fingerprintUnlocked(), xsink);
    const char* str = getAuthenticatedWith();
    ret->setKeyValue("authenticated", str ? QoreValue(new QoreStringNode(str)) : QoreValue(), xsink);
    ret->setKeyValue("connected", (bool)sshConnectedUnlocked(), xsink);

    if (sshConnectedUnlocked()) {
        const char* meth;
        ReferenceHolder<QoreHashNode> methods(new QoreHashNode(stringTypeInfo), xsink);
#ifdef LIBSSH2_METHOD_KEX
        meth = libssh2_session_methods(ssh_session, LIBSSH2_METHOD_KEX);
        if (meth)
            methods->setKeyValue("KEX", new QoreStringNode(meth), xsink);
#endif
#ifdef LIBSSH2_METHOD_HOSTKEY
        meth = libssh2_session_methods(ssh_session, LIBSSH2_METHOD_HOSTKEY);
        if (meth)
            methods->setKeyValue("HOSTKEY", new QoreStringNode(meth), xsink);
#endif
#ifdef LIBSSH2_METHOD_CRYPT_CS
        meth = libssh2_session_methods(ssh_session, LIBSSH2_METHOD_CRYPT_CS);
        if (meth)
            methods->setKeyValue("CRYPT_CS", new QoreStringNode(meth), xsink);
#endif
#ifdef LIBSSH2_METHOD_CRYPT_SC
        meth = libssh2_session_methods(ssh_session, LIBSSH2_METHOD_CRYPT_SC);
        if (meth)
            methods->setKeyValue("CRYPT_SC", new QoreStringNode(meth), xsink);
#endif
#ifdef LIBSSH2_METHOD_MAC_CS
        meth = libssh2_session_methods(ssh_session, LIBSSH2_METHOD_MAC_CS);
        if (meth)
            methods->setKeyValue("MAC_CS", new QoreStringNode(meth), xsink);
#endif
#ifdef LIBSSH2_METHOD_MAC_SC
        meth = libssh2_session_methods(ssh_session, LIBSSH2_METHOD_MAC_SC);
        if (meth)
            methods->setKeyValue("MAC_SC", new QoreStringNode(meth), xsink);
#endif
#ifdef LIBSSH2_METHOD_COMP_CS
        meth = libssh2_session_methods(ssh_session, LIBSSH2_METHOD_COMP_CS);
        if (meth)
            methods->setKeyValue("COMP_CS", new QoreStringNode(meth), xsink);
#endif
#ifdef LIBSSH2_METHOD_COMP_SC
        meth = libssh2_session_methods(ssh_session, LIBSSH2_METHOD_COMP_SC);
        if (meth)
            methods->setKeyValue("COMP_SC", new QoreStringNode(meth), xsink);
#endif
#ifdef LIBSSH2_METHOD_LANG_CS
        meth = libssh2_session_methods(ssh_session, LIBSSH2_METHOD_LANG_CS);
        if (meth)
            methods->setKeyValue("LANG_CS", new QoreStringNode(meth), xsink);
#endif
#ifdef LIBSSH2_METHOD_LANG_SC
        meth = libssh2_session_methods(ssh_session, LIBSSH2_METHOD_LANG_SC);
        if (meth)
            methods->setKeyValue("LANG_SC", new QoreStringNode(meth), xsink);
#endif
        ret->setKeyValue("methods", methods.release(), xsink);
    }

    return ret.release();
}

QoreObject *SSH2Client::openSessionChannel(ExceptionSink *xsink, int timeout_ms) {
    static const char *SSH2CLIENT_OPENSESSIONCHANNEL_ERROR = "SSH2CLIENT-OPENSESSIONCHANNEL-ERROR";

    AutoLocker al(m);

    if (!sshConnectedUnlocked()) {
        xsink->raiseException(SSH2CLIENT_NOT_CONNECTED, "cannot call SSH2Client::openSessionChannel() while client is not connected");
        return nullptr;
    }

    BlockingHelper bh(this);

    LIBSSH2_CHANNEL *channel;
    while (true) {
        channel = libssh2_channel_open_session(ssh_session);
        //printd(5, "SSH2Client::openSessionChannel(timeout_ms = %d) channel=%p rc=%d\n", timeout_ms, channel, libssh2_session_last_errno(ssh_session));
        if (!channel) {
            if (libssh2_session_last_error(ssh_session, 0, 0, 0) == LIBSSH2_ERROR_EAGAIN) {
                if (waitSocketUnlocked(xsink, SSH2CLIENT_TIMEOUT, SSH2CLIENT_OPENSESSIONCHANNEL_ERROR, "SSH2Client::openSessionChannel", timeout_ms))
                    return nullptr;
                continue;
            }
            doSessionErrUnlocked(xsink);
            return nullptr;
        }
        break;
    }

    return registerChannelUnlocked(channel);
}

QoreObject *SSH2Client::openDirectTcpipChannel(ExceptionSink *xsink, const char *host, int port, const char *shost, int sport, int timeout_ms) {
    static const char *SSH2CLIENT_OPENDIRECTTCPIPCHANNEL_ERROR = "SSH2CLIENT-OPENDIRECTTCPIPCHANNEL-ERROR";

    AutoLocker al(m);

    if (!sshConnectedUnlocked()) {
        xsink->raiseException(SSH2CLIENT_NOT_CONNECTED, "cannot call SSH2Client::openDirectTcpipChannel() while client is not connected");
        return nullptr;
    }

    BlockingHelper bh(this);

    LIBSSH2_CHANNEL *channel;
    while (true) {
        channel = libssh2_channel_direct_tcpip_ex(ssh_session, host, port, shost, sport);
        if (!channel) {
            if (libssh2_session_last_error(ssh_session, 0, 0, 0) == LIBSSH2_ERROR_EAGAIN) {
                if (waitSocketUnlocked(xsink, SSH2CLIENT_TIMEOUT, SSH2CLIENT_OPENDIRECTTCPIPCHANNEL_ERROR, "SSH2Client::openDirectTcpipChannel", timeout_ms))
                    return nullptr;
                continue;
            }
            doSessionErrUnlocked(xsink);
            return nullptr;
        }
        break;
    }

    return registerChannelUnlocked(channel);
}

#ifdef HAVE_LIBSSH2_FORWARD_LISTEN
QoreObject* SSH2Client::forwardListen(ExceptionSink* xsink, const char* host, int port, int queue_maxsize, int timeout_ms) {
    static const char* SSH2CLIENT_FORWARDLISTEN_ERROR = "SSH2CLIENT-FORWARDLISTEN-ERROR";

    AutoLocker al(m);

    if (!sshConnectedUnlocked()) {
        xsink->raiseException(SSH2CLIENT_NOT_CONNECTED, "cannot call SSH2Client::forwardListen() while client is not connected");
        return nullptr;
    }

    BlockingHelper bh(this);

    int bound_port = 0;
    LIBSSH2_LISTENER* listener;
    while (true) {
        listener = libssh2_channel_forward_listen_ex(ssh_session, host, port, &bound_port, queue_maxsize);
        if (!listener) {
            if (libssh2_session_last_error(ssh_session, 0, 0, 0) == LIBSSH2_ERROR_EAGAIN) {
                if (waitSocketUnlocked(xsink, SSH2CLIENT_TIMEOUT, SSH2CLIENT_FORWARDLISTEN_ERROR, "SSH2Client::forwardListen", timeout_ms)) {
                    return nullptr;
                }
                continue;
            }
            doSessionErrUnlocked(xsink);
            return nullptr;
        }
        break;
    }

    SSH2Listener* sl = new SSH2Listener(listener, this, bound_port);
    listener_set.insert(sl);
    return new QoreObject(QC_SSH2LISTENER, getProgram(), sl);
}
#endif

#ifdef HAVE_LIBSSH2_CHANNEL_DIRECT_STREAMLOCAL
QoreObject *SSH2Client::openDirectStreamLocalChannel(ExceptionSink *xsink, const char *socket_path, const char *shost, int sport, int timeout_ms) {
    static const char *SSH2CLIENT_OPENDIRECTSTREAMLOCALCHANNEL_ERROR = "SSH2CLIENT-OPENDIRECTSTREAMLOCALCHANNEL-ERROR";

    AutoLocker al(m);

    if (!sshConnectedUnlocked()) {
        xsink->raiseException(SSH2CLIENT_NOT_CONNECTED, "cannot call SSH2Client::openDirectStreamLocalChannel() while client is not connected");
        return nullptr;
    }

    BlockingHelper bh(this);

    LIBSSH2_CHANNEL *channel;
    while (true) {
        channel = libssh2_channel_direct_streamlocal_ex(ssh_session, socket_path, shost, sport);
        if (!channel) {
            if (libssh2_session_last_error(ssh_session, 0, 0, 0) == LIBSSH2_ERROR_EAGAIN) {
                if (waitSocketUnlocked(xsink, SSH2CLIENT_TIMEOUT, SSH2CLIENT_OPENDIRECTSTREAMLOCALCHANNEL_ERROR, "SSH2Client::openDirectStreamLocalChannel", timeout_ms)) {
                    return nullptr;
                }
                continue;
            }
            doSessionErrUnlocked(xsink);
            return nullptr;
        }
        break;
    }

    return registerChannelUnlocked(channel);
}
#endif

LIBSSH2_CHANNEL* SSH2Client::scpGetRaw(ExceptionSink *xsink, const char *path, int timeout_ms, QoreHashNode *statinfo) {
    static const char *SSH2CLIENT_SCPGET_ERROR = "SSH2CLIENT-SCPGET-ERROR";

    AutoLocker al(m);

    if (!sshConnectedUnlocked()) {
        xsink->raiseException(SSH2CLIENT_NOT_CONNECTED, "cannot call SSH2Client::scpGet() while client is not connected");
        return nullptr;
    }

    BlockingHelper bh(this);

#ifdef HAVE_LIBSSH2_SCP_RECV2
    libssh2_struct_stat sb;
    LIBSSH2_CHANNEL *channel;
    while (true) {
        channel = libssh2_scp_recv2(ssh_session, path, &sb);
        if (!channel) {
            if (libssh2_session_last_error(ssh_session, 0, 0, 0) == LIBSSH2_ERROR_EAGAIN) {
                if (waitSocketUnlocked(xsink, SSH2CLIENT_TIMEOUT, SSH2CLIENT_SCPGET_ERROR, "SSH2Client::scpGet", timeout_ms)) {
                    return nullptr;
                }
                continue;
            }
            doSessionErrUnlocked(xsink);
            return nullptr;
        }
        break;
    }

    // write file status info to statinfo if available
    if (statinfo) {
        statinfo->setKeyValue("mode", sb.st_mode, xsink);
        statinfo->setKeyValue("permissions", new QoreStringNode(mode2str(sb.st_mode)), xsink);
        statinfo->setKeyValue("size", (int64)sb.st_size, xsink);
        statinfo->setKeyValue("uid", (int64)sb.st_uid, xsink);
        statinfo->setKeyValue("gid", (int64)sb.st_gid, xsink);
        statinfo->setKeyValue("atime", DateTimeNode::makeAbsolute(currentTZ(), (int64)sb.st_atime), xsink);
        statinfo->setKeyValue("mtime", DateTimeNode::makeAbsolute(currentTZ(), (int64)sb.st_mtime), xsink);
    }
#else
    struct stat sb;
    LIBSSH2_CHANNEL *channel;
    while (true) {
        channel = libssh2_scp_recv(ssh_session, path, &sb);
        if (!channel) {
            if (libssh2_session_last_error(ssh_session, 0, 0, 0) == LIBSSH2_ERROR_EAGAIN) {
                if (waitSocketUnlocked(xsink, SSH2CLIENT_TIMEOUT, SSH2CLIENT_SCPGET_ERROR, "SSH2Client::scpGet", timeout_ms)) {
                    return nullptr;
                }
                continue;
            }
            doSessionErrUnlocked(xsink);
            return nullptr;
        }
        break;
    }

    // write file status info to statinfo if available
    if (statinfo) {
        map_ssh2_sbuf_to_hash(statinfo, &sb, xsink);
    }
#endif

    return channel;
}

QoreObject *SSH2Client::scpGet(ExceptionSink *xsink, const char *path, int timeout_ms, QoreHashNode *statinfo) {
    return registerChannelUnlocked(scpGetRaw(xsink, path, timeout_ms, statinfo));
}

void SSH2Client::scpGet(ExceptionSink *xsink, const char *path, OutputStream *os, int timeout_ms) {
    std::unique_ptr<SSH2Channel> c(registerChannelUnlockedRaw(scpGetRaw(xsink, path, timeout_ms, 0)));
    if (!c->sendEof(xsink, timeout_ms)) {
        qore_offset_t rc;
        char buffer[4096];
        while (!c->eof(xsink)) {
            rc = c->read(xsink, buffer, sizeof(buffer), 0, timeout_ms);
            if (rc > 0) {
                os->write(buffer, rc, xsink);
            }
            if (*xsink) {
                break;
            }
        }
    }
    c->waitClosed(xsink, timeout_ms);
}

LIBSSH2_CHANNEL *SSH2Client::scpPutRaw(ExceptionSink *xsink, const char *path, size_t size, int mode, long mtime, long atime, int timeout_ms) {
    static const char *SSH2CLIENT_SCPPUT_ERROR = "SSH2CLIENT-SCPPUT-ERROR";

    AutoLocker al(m);

    if (!sshConnectedUnlocked()) {
        xsink->raiseException(SSH2CLIENT_NOT_CONNECTED, "cannot call SSH2Client::scpPut() while client is not connected");
        return 0;
    }

    BlockingHelper bh(this);

    LIBSSH2_CHANNEL *channel;
    while (true) {
#ifdef HAVE_LIBSSH2_SCP_SEND64
        channel = libssh2_scp_send64(ssh_session, path, mode, (libssh2_int64_t)size, (time_t)mtime, (time_t)atime);
#else
        channel = libssh2_scp_send_ex(ssh_session, path, mode, size, mtime, atime);
#endif
        if (!channel) {
            if (libssh2_session_last_error(ssh_session, 0, 0, 0) == LIBSSH2_ERROR_EAGAIN) {
                if (waitSocketUnlocked(xsink, SSH2CLIENT_TIMEOUT, SSH2CLIENT_SCPPUT_ERROR, "SSH2Client::scpPut", timeout_ms)) {
                    return nullptr;
                }
                continue;
            }
            doSessionErrUnlocked(xsink);
            return nullptr;
        }
        break;
    }
    return channel;
}

QoreObject *SSH2Client::scpPut(ExceptionSink *xsink, const char *path, size_t size, int mode, long mtime, long atime, int timeout_ms) {
   return registerChannelUnlocked(scpPutRaw(xsink, path, size, mode, mtime, atime, timeout_ms));
}

void SSH2Client::scpPut(ExceptionSink *xsink, const char *path, InputStream *is, size_t size, int mode, long mtime, long atime, int timeout_ms) {
    static const char *SSH2CLIENT_SCPPUT_ERROR = "SSH2CLIENT-SCPPUT-ERROR";

    std::unique_ptr<SSH2Channel> c(registerChannelUnlockedRaw(scpPutRaw(xsink, path, size, mode, mtime, atime, timeout_ms)));

    char buffer[4096];
    while (size > 0) {
        int64 r = is->read(buffer, QORE_MIN(sizeof(buffer), size), xsink);
        if (*xsink) {
            break;
        }
        if (r == 0) {
            xsink->raiseException(SSH2CLIENT_SCPPUT_ERROR, "Unexpected end of stream");
            break;
        }
        c->write(xsink, buffer, r, 0, timeout_ms);
        if (*xsink) {
            break;
        }
        size -= r;
    }

    if (!c->sendEof(xsink, timeout_ms)) {
        if (!c->waitEof(xsink, timeout_ms)) {
            c->waitClosed(xsink, timeout_ms);
        }
    }
}

#ifdef _QORE_HAS_SOCKET_PERF_API
void SSH2Client::clearWarningQueue(ExceptionSink* xsink) {
   AutoLocker al(m);
   socket.clearWarningQueue(xsink);
}

void SSH2Client::setWarningQueue(ExceptionSink* xsink, int64 warning_ms, int64 warning_bs, Queue* wq, QoreValue arg, int64 min_ms) {
    AutoLocker al(m);
    socket.setWarningQueue(xsink, warning_ms, warning_bs, wq, arg, min_ms);
}

QoreHashNode* SSH2Client::getUsageInfo() const {
   AutoLocker al(m);
   return socket.getUsageInfo();
}

void SSH2Client::clearStats() {
   AutoLocker al(m);
   socket.clearStats();
}
#endif
