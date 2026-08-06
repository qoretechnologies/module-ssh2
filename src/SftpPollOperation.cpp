/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    SftpPollOperation.cpp

    non-blocking SFTP file retrieval for the ssh2 module

    Qore Programming Language

    Copyright (C) 2026 Qore Technologies, s.r.o.

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

#include "SftpPollOperation.h"

#include <stdarg.h>
#include <string.h>

// the number of read calls made between cooperative cancellation checks
#define SFTP_POLL_CANCEL_CHECK_INTERVAL 16

// the operation name used in cancellation and error messages
static const char* SFTP_POLL_METHOD = "SFTPClient::startPollGetFile";

// the error code used when a remote file handle cannot be closed in the timeout period
static const char* SFTP_POLL_TIMEOUT = "SFTPCLIENT-TIMEOUT";

// the return value of stepAgentAuth() when the agent has no (more) usable identities
#define SFTP_AGENT_NO_MORE 1
// the return value of stepAgentAuth() when an exception was raised
#define SFTP_AGENT_ERROR -1

SftpPollOperationPriv::SftpPollOperationPriv(QoreObject* self, SFTPClient* client, const char* path,
        int timeout_ms) : SocketPollOperationBase(self), client(client), path(path), timeout_ms(timeout_ms),
        data(new BinaryNode) {
    assert(client);
    // the client's private data is referenced for the life of this object; the DGC-visible
    // reference to the client object itself is held by the "client" member of the object
    // wrapping this private data
    client->ref();
}

SftpPollOperationPriv::~SftpPollOperationPriv() {
    // all resources must be released by deref() before destruction
    assert(!provider_candidates);
    assert(!agent);
    assert(!sftp_handle);
    assert(!client);
}

void SftpPollOperationPriv::deref(ExceptionSink* xsink) {
    if (ROdereference()) {
        {
            AutoLocker al(client->m);
            // release any remote resources still held; the session is left connected so that the
            // client can be reused, exactly as it would be after a blocking retrieval
            closeIntern(xsink, false);
        }
        client->deref(xsink);
        client = nullptr;
        delete this;
    }
}

void SftpPollOperationPriv::abort(ExceptionSink* xsink) {
    AutoLocker al(client->m);
    if (state == SftpPollState::DONE || state == SftpPollState::CLOSED) {
        return;
    }
    // an operation abandoned mid-flight leaves the session in an indeterminate state (a request
    // may be in flight, or authentication may be incomplete), so the connection is closed
    closeIntern(xsink, true);
}

QoreValue SftpPollOperationPriv::getOutput() const {
    if (state != SftpPollState::DONE || !*data) {
        return QoreValue();
    }
    // an empty file is returned as an empty binary value, not as NOTHING
    return const_cast<BinaryNode*>(*data)->refSelf();
}

QoreStringNode* SftpPollOperationPriv::getGoal() const {
    return new QoreStringNodeMaker("sftp-get-file %s", path.c_str());
}

const char* SftpPollOperationPriv::getStateImpl() const {
    switch (state) {
        case SftpPollState::START: return "start";
        case SftpPollState::CONNECTING: return "connecting";
        case SftpPollState::HANDSHAKE: return "ssh-handshake";
        case SftpPollState::AUTHLIST: return "ssh-auth-list";
        case SftpPollState::AUTH: return "ssh-auth";
        case SftpPollState::SFTP_INIT: return "sftp-init";
        case SftpPollState::REALPATH: return "sftp-realpath";
        case SftpPollState::STAT: return "sftp-stat";
        case SftpPollState::OPEN: return "sftp-open";
        case SftpPollState::READ: return "sftp-read";
        case SftpPollState::CLOSE: return "sftp-close";
        case SftpPollState::DONE: return "done";
        case SftpPollState::CLOSED: return "closed";
    }
    return "unknown";
}

int SftpPollOperationPriv::setFdNonBlocking(int fd, bool non_blocking) {
    if (fd < 0) {
        return -1;
    }
#ifdef _Q_WINDOWS
    u_long mode = non_blocking ? 1 : 0;
    return ioctlsocket(fd, FIONBIO, &mode) ? -1 : 0;
#else
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags < 0) {
        return -1;
    }
    int new_flags = non_blocking ? (flags | O_NONBLOCK) : (flags & ~O_NONBLOCK);
    if (new_flags == flags) {
        // no change necessary
        return 1;
    }
    return fcntl(fd, F_SETFL, new_flags) < 0 ? -1 : 0;
#endif
}

void SftpPollOperationPriv::restoreBlockingIntern() {
    if (restore_session_blocking) {
        client->setBlockingUnlocked(true);
        restore_session_blocking = false;
    }
    // NOTE: the socket descriptor is deliberately left in non-blocking mode; that is the state a
    // connection established with the blocking API is in as well, and libssh2 implements its own
    // blocking mode on top of a non-blocking descriptor - reverting the descriptor to blocking
    // mode deadlocks the next blocking call on the client
}

void SftpPollOperationPriv::closeIntern(ExceptionSink* xsink, bool disconnect) {
    // the SSH agent connection must be released before the session it was created from
    agent.reset();

    if (provider_candidates) {
        provider_candidates->deref(xsink);
        provider_candidates = nullptr;
    }

    connect_state.reset();

    if (sftp_handle) {
        // close the remote file handle; the session is still in non-blocking mode here, so the
        // close is retried until it completes or the session reports a hard error
        int rc;
        while ((rc = libssh2_sftp_close_handle(sftp_handle)) == LIBSSH2_ERROR_EAGAIN) {
            if (client->waitSocketUnlocked(xsink, SFTP_POLL_TIMEOUT, "SFTPCLIENT-CLOSE-ERROR",
                    SFTP_POLL_METHOD, timeout_ms, true)) {
                // the handle cannot be closed; the session must be dropped to release it
                printd(0, "SftpPollOperationPriv::closeIntern() session %p: cannot close remote file "
                    "descriptor, forcing session disconnect\n", client->ssh_session);
                disconnect = true;
                break;
            }
        }
        sftp_handle = nullptr;
    }

    if (disconnect && client->ssh_session) {
        client->disconnectUnlocked(true, timeout_ms, nullptr, xsink);
    }

    restoreBlockingIntern();

    if (state != SftpPollState::DONE) {
        state = SftpPollState::CLOSED;
    }

    if (client_reserved) {
        client->poll_op_in_progress = false;
        client_reserved = false;
    }
}

void SftpPollOperationPriv::sessionError(ExceptionSink* xsink, const char* fmt, ...) {
    va_list args;
    QoreStringNode* desc = new QoreStringNode;

    while (true) {
        va_start(args, fmt);
        int rc = desc->vsprintf(fmt, args);
        va_end(args);
        if (!rc) {
            break;
        }
    }

    // this raises the same exception the blocking API raises for the same failure
    client->doSessionErrUnlocked(xsink, desc);
}

QoreHashNode* SftpPollOperationPriv::getPollInfo(ExceptionSink* xsink, int fd, int events,
        const std::vector<ExtraWaitFd>* extra_fds) {
    std::vector<std::pair<int, int>> extra;
    if (extra_fds) {
        for (auto& i : *extra_fds) {
            int ev = 0;
            if (i.want_read) {
                ev |= SOCK_POLLIN;
            }
            if (i.want_write) {
                ev |= SOCK_POLLOUT;
            }
            if (!ev) {
                ev = SOCK_POLLIN;
            }
            if (fd < 0) {
                // there is no primary descriptor yet (asynchronous name resolution); wait on the
                // first extra descriptor instead
                fd = i.fd;
                events = ev;
                continue;
            }
            extra.push_back(std::make_pair(i.fd, ev));
        }
    }

    if (fd < 0) {
        xsink->raiseException("SSH2-POLL-ERROR", "there is no descriptor to wait on in state %y in %s()",
            getStateImpl(), SFTP_POLL_METHOD);
        closeIntern(xsink, true);
        return nullptr;
    }

    poll_fd = fd;

    ReferenceHolder<QoreHashNode> info(new QoreHashNode(hashdeclSocketPollInfo, xsink), xsink);
    info->setKeyValue("events", events, xsink);
    // this object is the pollable object for the operation; the reference is released with the
    // hash returned to the caller
    info->setKeyValue("socket", (*self)->objectRefSelf(), xsink);
    if (!extra.empty()) {
        ReferenceHolder<QoreListNode> list(new QoreListNode(hashdeclExtraPollFdInfo->getTypeInfo()), xsink);
        for (auto& i : extra) {
            ReferenceHolder<QoreHashNode> h(new QoreHashNode(hashdeclExtraPollFdInfo, xsink), xsink);
            h->setKeyValue("fd", i.first, xsink);
            h->setKeyValue("events", i.second, xsink);
            list->push(h.release(), xsink);
        }
        info->setKeyValue("extra_fds", list.release(), xsink);
    }
    if (*xsink) {
        closeIntern(xsink, true);
        return nullptr;
    }
    return info.release();
}

bool SftpPollOperationPriv::connectionLost() const {
    if (!client->ssh_session) {
        return true;
    }

    int err = libssh2_session_last_errno(client->ssh_session);
    switch (err) {
        case LIBSSH2_ERROR_SOCKET_SEND:
        case LIBSSH2_ERROR_SOCKET_RECV:
        case LIBSSH2_ERROR_SOCKET_DISCONNECT:
        case LIBSSH2_ERROR_SOCKET_TIMEOUT:
            return true;

        case LIBSSH2_ERROR_SFTP_PROTOCOL: {
            if (!client->sftp_session) {
                return false;
            }
            unsigned long serr = libssh2_sftp_last_error(client->sftp_session);
            return serr == LIBSSH2_FX_NO_CONNECTION || serr == LIBSSH2_FX_CONNECTION_LOST;
        }

        default:
            break;
    }
    return false;
}

bool SftpPollOperationPriv::retryAfterConnectionLoss() {
    // only a session that this operation adopted can be stale; a session established by the
    // operation itself has just been authenticated, so a failure on it is a real error
    if (!reused_session || reconnect_attempted || !connectionLost()) {
        return false;
    }

    printd(5, "SftpPollOperationPriv: the reused session is no longer usable; reconnecting\n");
    reconnect_attempted = true;

    // drop the dead session; retrieving a file is idempotent, so starting over is safe
    client->disconnectUnlocked(true, timeout_ms, nullptr, nullptr);
    // the session is gone, so its blocking mode no longer has to be restored
    restore_session_blocking = false;

    // discard anything read from the dead session
    abs_path.clear();
    file_size = 0;
    file_permissions = 0;
    bytes_read = 0;
    read_calls = 0;
    data = new BinaryNode;

    state = SftpPollState::START;
    return true;
}

QoreHashNode* SftpPollOperationPriv::getSessionPollInfo(ExceptionSink* xsink) {
    assert(client->ssh_session);

    int dir = libssh2_session_block_directions(client->ssh_session);
    int events = 0;
    if (dir & LIBSSH2_SESSION_BLOCK_INBOUND) {
        events |= SOCK_POLLIN;
    }
    if (dir & LIBSSH2_SESSION_BLOCK_OUTBOUND) {
        events |= SOCK_POLLOUT;
    }
    if (!events) {
        // libssh2 did not report a direction; wait for data from the server
        events = SOCK_POLLIN;
    }

    return getPollInfo(xsink, client->socket.getSocket(), events);
}

QoreHashNode* SftpPollOperationPriv::continuePoll(ExceptionSink* xsink) {
    AutoLocker al(client->m);

    if (state == SftpPollState::DONE || state == SftpPollState::CLOSED) {
        return nullptr;
    }

    if (qore_check_cancel(xsink, SFTP_POLL_METHOD)) {
        closeIntern(xsink, true);
        return nullptr;
    }

    return continuePollIntern(xsink);
}

QoreHashNode* SftpPollOperationPriv::continuePollIntern(ExceptionSink* xsink) {
    // each handler either returns a poll info hash (the operation must wait), advances the state
    // (the loop continues immediately), or terminates the operation
    while (state != SftpPollState::DONE && state != SftpPollState::CLOSED) {
        QoreHashNode* rv = nullptr;
        switch (state) {
            case SftpPollState::START: {
                if (client->ssh_session) {
                    // reuse the established session
                    reused_session = true;
                    state = client->sftp_session ? SftpPollState::STAT : SftpPollState::SFTP_INIT;
                    // the socket must be non-blocking for the session to report EAGAIN
                    if (setFdNonBlocking(client->socket.getSocket(), true) < 0) {
                        xsink->raiseErrnoException("SSH2-POLL-ERROR", errno,
                            "cannot set the socket to non-blocking mode in %s()", SFTP_POLL_METHOD);
                        closeIntern(xsink, false);
                        return nullptr;
                    }
                    client->setBlockingUnlocked(false);
                    restore_session_blocking = true;
                    continue;
                }

                if (client->checkConnectPreconditionsUnlocked(xsink)) {
                    closeIntern(xsink, false);
                    return nullptr;
                }

                // IPv6 addresses must be given in bracket notation
                QoreString target;
                if (strchr(client->sshhost.c_str(), ':')) {
                    target.sprintf("[%s]:%d", client->sshhost.c_str(), (int)client->sshport);
                } else {
                    target.sprintf("%s:%d", client->sshhost.c_str(), (int)client->sshport);
                }
                connect_state.reset(client->socket.startConnect(xsink, target.c_str()));
                if (*xsink || !connect_state) {
                    if (!*xsink) {
                        xsink->raiseException("SSH2CLIENT-CONNECT-ERROR",
                            "cannot start a connection to '%s' in %s()", target.c_str(), SFTP_POLL_METHOD);
                    }
                    closeIntern(xsink, true);
                    return nullptr;
                }
                state = SftpPollState::CONNECTING;
                continue;
            }

            case SftpPollState::CONNECTING: rv = handleConnecting(xsink); break;
            case SftpPollState::HANDSHAKE: rv = handleHandshake(xsink); break;
            case SftpPollState::AUTHLIST: rv = handleAuthList(xsink); break;
            case SftpPollState::AUTH: rv = handleAuth(xsink); break;
            case SftpPollState::SFTP_INIT: rv = handleSftpInit(xsink); break;
            case SftpPollState::REALPATH: rv = handleRealPath(xsink); break;
            case SftpPollState::STAT: rv = handleStat(xsink); break;
            case SftpPollState::OPEN: rv = handleOpen(xsink); break;
            case SftpPollState::READ: rv = handleRead(xsink); break;
            case SftpPollState::CLOSE: rv = handleClose(xsink); break;

            default:
                assert(false);
                return nullptr;
        }

        if (rv) {
            return rv;
        }
    }

    return nullptr;
}

QoreHashNode* SftpPollOperationPriv::handleConnecting(ExceptionSink* xsink) {
    assert(connect_state);

    int rc = connect_state->continuePoll(xsink);
    if (*xsink) {
        closeIntern(xsink, true);
        return nullptr;
    }
    if (rc) {
        // the connection is still in progress; wait for the reported events.  While the host name
        // is being resolved asynchronously there is no socket descriptor yet, and the resolver's
        // descriptors are reported as extra descriptors to wait on
        std::vector<ExtraWaitFd> extra = connect_state->getExtraWaitFds();
        return getPollInfo(xsink, client->socket.getSocket(), rc, &extra);
    }

    connect_state.reset();

    // the socket is connected; the poll-based connect leaves the descriptor in non-blocking mode,
    // which is what the session requires, but this is enforced explicitly so that the state does
    // not depend on the connect implementation
    if (setFdNonBlocking(client->socket.getSocket(), true) < 0) {
        xsink->raiseErrnoException("SSH2-POLL-ERROR", errno,
            "cannot set the socket to non-blocking mode in %s()", SFTP_POLL_METHOD);
        closeIntern(xsink, true);
        return nullptr;
    }

    if (client->prepareSessionUnlocked(xsink)) {
        closeIntern(xsink, false);
        return nullptr;
    }
    restore_session_blocking = true;

    state = SftpPollState::HANDSHAKE;
    return nullptr;
}

QoreHashNode* SftpPollOperationPriv::handleHandshake(ExceptionSink* xsink) {
    int rc = client->startupUnlocked();
    if (rc == LIBSSH2_ERROR_EAGAIN) {
        return getSessionPollInfo(xsink);
    }
    if (rc) {
        xsink->raiseException(SSH2_ERROR, "failure establishing SSH session: %d", rc);
        closeIntern(xsink, true);
        return nullptr;
    }

    // verify the server's host key according to the configured policy; this performs no I/O on
    // the SSH socket and closes the connection itself if verification fails
    if (client->verifyHostKeyUnlocked(timeout_ms, xsink)) {
        closeIntern(xsink, false);
        return nullptr;
    }

    state = SftpPollState::AUTHLIST;
    return nullptr;
}

QoreHashNode* SftpPollOperationPriv::handleAuthList(ExceptionSink* xsink) {
    char* userauthlist = libssh2_userauth_list(client->ssh_session, client->sshuser.c_str(),
        client->sshuser.size());
    if (!userauthlist && libssh2_session_last_errno(client->ssh_session) == LIBSSH2_ERROR_EAGAIN) {
        return getSessionPollInfo(xsink);
    }

    assert(!client->sshauthenticatedwith);

    printd(5, "userauthlist: %s\n", userauthlist ? userauthlist : "n/a");

    // the authentication plan is built with the same policy the blocking connect path uses
    ssh2_auth_plan_t plan;
    client->buildAuthPlanUnlocked(userauthlist, plan);
    auth_queue.assign(plan.begin(), plan.end());

    state = SftpPollState::AUTH;
    return nullptr;
}

QoreHashNode* SftpPollOperationPriv::handleAuth(ExceptionSink* xsink) {
    while (!loggedin && !auth_queue.empty()) {
        const Ssh2AuthAttempt& attempt = auth_queue.front();

        if (attempt.kind == Ssh2AuthAttempt::PROVIDER) {
            // resolve the next client identity provider candidate into concrete attempts
            if (expandProviderAttempt(xsink)) {
                closeIntern(xsink, true);
                return nullptr;
            }
            continue;
        }

        if (attempt.kind == Ssh2AuthAttempt::AGENT) {
            int rc = stepAgentAuth(xsink);
            if (rc == LIBSSH2_ERROR_EAGAIN) {
                return getSessionPollInfo(xsink);
            }
            if (rc == SFTP_AGENT_ERROR) {
                closeIntern(xsink, true);
                return nullptr;
            }
            if (!rc) {
                client->setAuthenticatedWithUnlocked(attempt);
                loggedin = true;
                agent.reset();
                break;
            }
            // the agent has no (more) usable identities
            assert(rc == SFTP_AGENT_NO_MORE);
            auth_queue.pop_front();
            continue;
        }

        if (!auth_attempt_checked) {
            // enforce sandboxing restrictions before reading any key file
            if (client->checkAuthAttemptAccessUnlocked(attempt, xsink)) {
                closeIntern(xsink, false);
                return nullptr;
            }
            auth_attempt_checked = true;
        }

        int rc = client->authCallUnlocked(attempt);
        if (rc == LIBSSH2_ERROR_EAGAIN) {
            return getSessionPollInfo(xsink);
        }
        if (!rc) {
            client->setAuthenticatedWithUnlocked(attempt);
            loggedin = true;
            printd(5, "%s authentication succeeded\n", attempt.getAuthenticatedWith());
            break;
        }
        printd(5, "%s authentication failed\n", attempt.getAuthenticatedWith());
        auth_queue.pop_front();
        auth_attempt_checked = false;
    }

    if (!loggedin) {
        xsink->raiseException("SSH2CLIENT-AUTH-ERROR", "No proper authentication method found");
        closeIntern(xsink, true);
        return nullptr;
    }

    // release any resources held by the authentication plan
    auth_queue.clear();
    agent.reset();
    if (provider_candidates) {
        provider_candidates->deref(xsink);
        provider_candidates = nullptr;
    }

    client->finishConnectUnlocked();

    state = SftpPollState::SFTP_INIT;
    return nullptr;
}

int SftpPollOperationPriv::stepAgentAuth(ExceptionSink* xsink) {
#ifdef HAVE_LIBSSH2_AGENT_API
    if (!agent) {
        printd(5, "SftpPollOperationPriv: trying SSH agent authentication\n");
        agent.reset(new Ssh2AgentHelper(client->ssh_session));
        if (agent->init()) {
            agent.reset();
            return SFTP_AGENT_NO_MORE;
        }
        agent_need_identity = true;
    }

    while (true) {
        if (agent_need_identity) {
            if (qore_check_cancel(xsink, SFTP_POLL_METHOD)) {
                agent.reset();
                return SFTP_AGENT_ERROR;
            }
            if (agent->nextIdentity()) {
                agent.reset();
                return SFTP_AGENT_NO_MORE;
            }
            agent_need_identity = false;
        }

        int rc = agent->userauth(client->sshuser.c_str());
        if (rc == LIBSSH2_ERROR_EAGAIN) {
            return LIBSSH2_ERROR_EAGAIN;
        }
        if (!rc) {
            printd(5, "SSH agent authentication succeeded\n");
            return 0;
        }
        // try the next identity
        agent_need_identity = true;
    }
#else
    return SFTP_AGENT_NO_MORE;
#endif
}

int SftpPollOperationPriv::expandProviderAttempt(ExceptionSink* xsink) {
    assert(!auth_queue.empty());
    assert(auth_queue.front().kind == Ssh2AuthAttempt::PROVIDER);

    if (!client->publicKeyAuthAvailableUnlocked()) {
        auth_queue.pop_front();
        return 0;
    }

    if (!provider_candidates) {
        provider_candidates = client->getProviderCandidatesUnlocked(timeout_ms, xsink);
        if (!provider_candidates) {
            return -1;
        }
        provider_index = 0;
    }

    if (provider_index >= provider_candidates->size()) {
        // all candidates have been tried
        auth_queue.pop_front();
        provider_candidates->deref(xsink);
        provider_candidates = nullptr;
        return 0;
    }

    if (qore_check_cancel(xsink, SFTP_POLL_METHOD)) {
        return -1;
    }

    // candidates are resolved one at a time so that the provider is not asked for key material
    // that will never be used, exactly as in the blocking authentication path
    ssh2_auth_plan_t sub_plan;
    if (client->resolveProviderCandidateUnlocked(provider_candidates, provider_index++, timeout_ms, sub_plan,
            xsink)) {
        return -1;
    }

    // the attempts resolved from this candidate are made before the remaining candidates are
    // resolved; the marker stays in the queue for the next candidate
    auth_queue.insert(auth_queue.begin(), sub_plan.begin(), sub_plan.end());
    return 0;
}

QoreHashNode* SftpPollOperationPriv::handleSftpInit(ExceptionSink* xsink) {
    client->sftp_session = libssh2_sftp_init(client->ssh_session);
    if (!client->sftp_session) {
        if (libssh2_session_last_errno(client->ssh_session) == LIBSSH2_ERROR_EAGAIN) {
            return getSessionPollInfo(xsink);
        }
        if (retryAfterConnectionLoss()) {
            return nullptr;
        }
        sessionError(xsink, "Unable to initialize SFTP session");
        closeIntern(xsink, true);
        return nullptr;
    }

    state = client->sftppath.empty() ? SftpPollState::REALPATH : SftpPollState::STAT;
    return nullptr;
}

QoreHashNode* SftpPollOperationPriv::handleRealPath(ExceptionSink* xsink) {
    // the remote current working directory is needed to resolve relative paths
    char buff[PATH_MAX];
    int rc = libssh2_sftp_symlink_ex(client->sftp_session, ".", 1, buff, sizeof(buff) - 1,
        LIBSSH2_SFTP_REALPATH);
    if (rc == LIBSSH2_ERROR_EAGAIN) {
        return getSessionPollInfo(xsink);
    }
    if (rc <= 0) {
        sessionError(xsink, "libssh2_sftp_realpath() returned an error");
        closeIntern(xsink, true);
        return nullptr;
    }
    buff[rc] = '\0';
    client->sftppath = buff;

    state = SftpPollState::STAT;
    return nullptr;
}

QoreHashNode* SftpPollOperationPriv::handleStat(ExceptionSink* xsink) {
    if (abs_path.empty()) {
        abs_path = absolute_filename(client, path.c_str());
    }

    LIBSSH2_SFTP_ATTRIBUTES attrs;
    int rc = libssh2_sftp_stat(client->sftp_session, abs_path.c_str(), &attrs);
    if (rc == LIBSSH2_ERROR_EAGAIN) {
        return getSessionPollInfo(xsink);
    }
    if (rc < 0) {
        // the stat is the first use of a reused session, so it doubles as the liveness check the
        // blocking API makes with SFTPClient::isAlive() before an implicit connection
        if (retryAfterConnectionLoss()) {
            return nullptr;
        }
        sessionError(xsink, "libssh2_sftp_stat(%s) returned an error", abs_path.c_str());
        closeIntern(xsink, false);
        return nullptr;
    }

    file_size = attrs.filesize;
    file_permissions = attrs.permissions;
    if (file_size) {
        data->preallocate(file_size);
    }

    state = SftpPollState::OPEN;
    return nullptr;
}

QoreHashNode* SftpPollOperationPriv::handleOpen(ExceptionSink* xsink) {
    sftp_handle = libssh2_sftp_open(client->sftp_session, abs_path.c_str(), LIBSSH2_FXF_READ,
        file_permissions);
    if (!sftp_handle) {
        if (libssh2_session_last_errno(client->ssh_session) == LIBSSH2_ERROR_EAGAIN) {
            return getSessionPollInfo(xsink);
        }
        sessionError(xsink, "libssh2_sftp_open(%s) returned an error", abs_path.c_str());
        closeIntern(xsink, false);
        return nullptr;
    }

    state = SftpPollState::READ;
    return nullptr;
}

QoreHashNode* SftpPollOperationPriv::handleRead(ExceptionSink* xsink) {
    while (bytes_read < file_size) {
        ssize_t rc = libssh2_sftp_read(sftp_handle, (char*)data->getPtr() + bytes_read,
            file_size - bytes_read);
        if (rc == LIBSSH2_ERROR_EAGAIN) {
            return getSessionPollInfo(xsink);
        }
        if (rc < 0) {
            sessionError(xsink, "libssh2_sftp_read(" QLLD ") failed: total read: " QLLD " while reading "
                "'%s' size " QLLD, (int64)(file_size - bytes_read), (int64)bytes_read, abs_path.c_str(),
                (int64)file_size);
            closeIntern(xsink, false);
            return nullptr;
        }
        if (!rc) {
            // the server returned end of file before the size reported by stat() was reached
            break;
        }
        bytes_read += rc;

        if (!(++read_calls % SFTP_POLL_CANCEL_CHECK_INTERVAL) && qore_check_cancel(xsink, SFTP_POLL_METHOD)) {
            closeIntern(xsink, true);
            return nullptr;
        }
    }

    data->setSize(bytes_read);

    state = SftpPollState::CLOSE;
    return nullptr;
}

QoreHashNode* SftpPollOperationPriv::handleClose(ExceptionSink* xsink) {
    int rc = libssh2_sftp_close_handle(sftp_handle);
    if (rc == LIBSSH2_ERROR_EAGAIN) {
        return getSessionPollInfo(xsink);
    }
    // errors closing the handle are ignored; the data has already been retrieved
    sftp_handle = nullptr;

    state = SftpPollState::DONE;

    // release the session for other operations and restore its blocking mode
    closeIntern(xsink, false);
    return nullptr;
}
