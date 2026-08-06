/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    SftpPollOperation.h

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

#ifndef _QORE_SFTPPOLLOPERATION_H

#define _QORE_SFTPPOLLOPERATION_H

#include "ssh2-module.h"
#include "SFTPClient.h"

#include <qore/Qore.h>
#include <qore/AbstractPollState.h>
#include <qore/SocketPollOperationBase.h>

#include <deque>
#include <memory>
#include <string>
#include <vector>

DLLLOCAL QoreClass* initSftpPollOperationClass(QoreNamespace& ns);
DLLLOCAL extern qore_classid_t CID_SFTPPOLLOPERATION;
DLLLOCAL extern QoreClass* QC_SFTPPOLLOPERATION;

// the socket poll event values used in SocketPollInfo hashes; these are the values of the
// @ref Qore::SOCK_POLLIN and @ref Qore::SOCK_POLLOUT constants, which are not exported in a
// public %Qore header
#ifndef SOCK_POLLIN
#define SOCK_POLLIN  (1 << 0)
#endif
#ifndef SOCK_POLLOUT
#define SOCK_POLLOUT (1 << 1)
#endif

//! the stages of a non-blocking SFTP file retrieval
enum class SftpPollState {
    START,          //!< nothing has been started yet
    CONNECTING,     //!< establishing the TCP connection
    HANDSHAKE,      //!< SSH transport layer handshake
    AUTHLIST,       //!< retrieving the authentication methods supported by the server
    AUTH,           //!< executing the authentication plan
    SFTP_INIT,      //!< establishing the SFTP session
    REALPATH,       //!< resolving the remote current working directory
    STAT,           //!< retrieving the remote file's attributes
    OPEN,           //!< opening the remote file
    READ,           //!< reading the remote file's data
    CLOSE,          //!< closing the remote file
    DONE,           //!< the data has been retrieved
    CLOSED,         //!< the operation was aborted or failed
};

//! Retrieves a file over SFTP without blocking the calling thread
/** Every stage of the conversation - the TCP connection, the SSH handshake, authentication, the
    SFTP session, and the file transfer itself - can report that it would block; instead of
    waiting for the socket, this class reports the direction libssh2 is blocked on to the caller
    as a @ref SocketPollInfo hash, so that the caller can wait for that event along with any
    other I/O it is managing.

    All authentication policy and host key verification is shared with the blocking connect path
    (see SSH2Client::buildAuthPlanUnlocked() and SSH2Client::verifyHostKeyUnlocked()), so the two
    paths cannot disagree about security decisions.

    The client's session is switched to non-blocking mode for the life of the operation and
    restored when the operation completes, is aborted, or is destroyed.  The socket's file
    descriptor is set to non-blocking I/O, which is the state the blocking API leaves it in as
    well; libssh2 implements its own blocking mode on top of a non-blocking descriptor.
*/
class SftpPollOperationPriv : public SocketPollOperationBase {
public:
    //! Creates the operation; the client must not be in use by any other operation
    /** @param self the object wrapping this private data
        @param client the client to use; a reference is managed by this object
        @param path the remote file path to retrieve
        @param timeout_ms the timeout used for cleanup operations and callback contexts
    */
    DLLLOCAL SftpPollOperationPriv(QoreObject* self, SFTPClient* client, const char* path, int timeout_ms);

    DLLLOCAL virtual ~SftpPollOperationPriv();

    DLLLOCAL virtual void deref(ExceptionSink* xsink) override;

    //! Returns True when the file's data is available
    DLLLOCAL virtual bool goalReached() const override {
        return state == SftpPollState::DONE;
    }

    //! Aborts the operation and releases all remote and local resources
    DLLLOCAL virtual void abort(ExceptionSink* xsink) override;

    //! Advances the operation; returns a SocketPollInfo hash to wait on or nullptr when complete
    DLLLOCAL virtual QoreHashNode* continuePoll(ExceptionSink* xsink) override;

    //! Returns the file's data as a binary value
    DLLLOCAL virtual QoreValue getOutput() const override;

    //! Returns a description of the current stage
    DLLLOCAL virtual const char* getStateImpl() const override;

    //! continuePoll() can call %Qore code and read key files, so it must not run on the I/O thread
    DLLLOCAL virtual bool needsWorkerDispatch() const override {
        return true;
    }

    //! Returns the descriptor to wait on for the current stage
    DLLLOCAL int getPollableDescriptor() const {
        return poll_fd;
    }

    //! Returns a description of the operation's goal
    DLLLOCAL QoreStringNode* getGoal() const;

private:
    //! the client; a reference to the private data is held for the life of this object
    SFTPClient* client;

    //! the remote path to retrieve as given by the caller
    std::string path;

    //! the absolute remote path resolved when the SFTP session is established
    std::string abs_path;

    //! the timeout used for cleanup operations and for %Qore callback contexts
    int timeout_ms;

    //! the current stage
    SftpPollState state = SftpPollState::START;

    //! the descriptor to wait on for the current stage
    int poll_fd = -1;

    //! the TCP connection poll state; only set while connecting
    std::unique_ptr<AbstractPollState> connect_state;

    //! the remaining authentication attempts to make
    std::deque<Ssh2AuthAttempt> auth_queue;

    //! set once sandboxing checks have been made for the attempt at the front of the queue
    bool auth_attempt_checked = false;

    //! set when the authentication attempt at the front of the queue succeeded
    bool loggedin = false;

    //! the client identity provider's candidate list; only set while resolving provider candidates
    QoreListNode* provider_candidates = nullptr;

    //! the index of the next client identity provider candidate to resolve
    size_t provider_index = 0;

    //! the SSH agent connection; only set while trying agent authentication
    std::unique_ptr<Ssh2AgentHelper> agent;

    //! set when the next agent identity must be selected before authenticating
    bool agent_need_identity = true;

    //! set when the operation adopted a session established before it started
    bool reused_session = false;

    //! set once the operation has reestablished a reused session that turned out to be dead
    bool reconnect_attempted = false;

    //! the open remote file handle
    LIBSSH2_SFTP_HANDLE* sftp_handle = nullptr;

    //! the size of the remote file as reported by the server
    size_t file_size = 0;

    //! the permissions of the remote file as reported by the server
    unsigned long file_permissions = 0;

    //! the number of bytes read so far
    size_t bytes_read = 0;

    //! the number of read calls made; used for cooperative cancellation checks
    size_t read_calls = 0;

    //! the file's data
    SimpleRefHolder<BinaryNode> data;

    //! set when the session's blocking mode was changed and must be restored
    bool restore_session_blocking = false;

    //! set while this operation holds the client's poll reservation
    bool client_reserved = true;

    //! Advances the state machine; returns the poll info hash or nullptr when there is nothing to wait for
    DLLLOCAL QoreHashNode* continuePollIntern(ExceptionSink* xsink);

    // stage handlers; each returns the poll info hash to wait on, or nullptr to continue or stop
    DLLLOCAL QoreHashNode* handleConnecting(ExceptionSink* xsink);
    DLLLOCAL QoreHashNode* handleHandshake(ExceptionSink* xsink);
    DLLLOCAL QoreHashNode* handleAuthList(ExceptionSink* xsink);
    DLLLOCAL QoreHashNode* handleAuth(ExceptionSink* xsink);
    DLLLOCAL QoreHashNode* handleSftpInit(ExceptionSink* xsink);
    DLLLOCAL QoreHashNode* handleRealPath(ExceptionSink* xsink);
    DLLLOCAL QoreHashNode* handleStat(ExceptionSink* xsink);
    DLLLOCAL QoreHashNode* handleOpen(ExceptionSink* xsink);
    DLLLOCAL QoreHashNode* handleRead(ExceptionSink* xsink);
    DLLLOCAL QoreHashNode* handleClose(ExceptionSink* xsink);

    //! Executes a single step of SSH agent authentication
    /** @return \c LIBSSH2_ERROR_EAGAIN if the caller must wait, 0 if the agent authenticated,
        1 if the agent has no (more) usable identities, -1 if an exception was raised
    */
    DLLLOCAL int stepAgentAuth(ExceptionSink* xsink);

    //! Resolves the next client identity provider candidate into concrete attempts
    /** @return 0 for OK, -1 if an exception was raised
    */
    DLLLOCAL int expandProviderAttempt(ExceptionSink* xsink);

    //! Returns True if the last libssh2 error means that the connection is gone
    DLLLOCAL bool connectionLost() const;

    //! Restarts the operation on a new connection if a reused session turned out to be dead
    /** This gives the poll operation the same implicit reconnection behavior that the blocking
        API gets from its SFTPClient::isAlive() check, without the extra round trip: the stage
        that first uses the session is itself the liveness probe.

        @return True if the operation was restarted, False if the failure must be reported
    */
    DLLLOCAL bool retryAfterConnectionLoss();

    //! Returns a poll info hash for the direction libssh2 is currently blocked on
    DLLLOCAL QoreHashNode* getSessionPollInfo(ExceptionSink* xsink);

    //! Returns a poll info hash for the given events on the given descriptor
    DLLLOCAL QoreHashNode* getPollInfo(ExceptionSink* xsink, int fd, int events,
        const std::vector<ExtraWaitFd>* extra_fds = nullptr);

    //! Raises an exception describing the last libssh2 session error
    DLLLOCAL void sessionError(ExceptionSink* xsink, const char* fmt, ...);

    //! Releases all resources held by the operation and marks it closed
    DLLLOCAL void closeIntern(ExceptionSink* xsink, bool disconnect);

    //! Restores the session's and the socket's blocking modes
    DLLLOCAL void restoreBlockingIntern();

    //! Sets the descriptor's blocking mode; returns 0 for OK, -1 for error
    DLLLOCAL static int setFdNonBlocking(int fd, bool non_blocking);
};

#endif // _QORE_SFTPPOLLOPERATION_H
