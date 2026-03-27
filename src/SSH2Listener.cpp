/* -*- indent-tabs-mode: nil -*- */
/*
    SSH2Listener.cpp

    libssh2 ssh2 reverse port forwarding listener integration in Qore

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

#include "SSH2Listener.h"

#ifdef HAVE_LIBSSH2_FORWARD_LISTEN

#include "SSH2Client.h"
#include "SSH2Channel.h"

static const char* SSH2LISTENER_TIMEOUT = "SSH2LISTENER-TIMEOUT";
static const char* SSH2LISTENER_ERROR = "SSH2LISTENER-ERROR";

SSH2Listener::~SSH2Listener() {
    if (listener) {
        destructor();
    }
    assert(!listener);
}

void SSH2Listener::destructor() {
    AutoLocker al(parent->m);
    if (listener) {
        cancelUnlocked();
    }
}

void SSH2Listener::cancelUnlocked() {
    if (listener) {
        parent->listenerDeletedUnlocked(this);
        libssh2_channel_forward_cancel(listener);
        listener = nullptr;
    }
}

QoreObject* SSH2Listener::accept(ExceptionSink* xsink, int timeout_ms) {
    AutoLocker al(parent->m);

    if (!listener) {
        xsink->raiseException(SSH2LISTENER_ERROR, "the SSH2 listener has been cancelled");
        return nullptr;
    }

    if (!parent->sshConnectedUnlocked()) {
        xsink->raiseException(SSH2LISTENER_ERROR, "cannot accept on listener while client is not connected");
        return nullptr;
    }

    BlockingHelper bh(parent);

    LIBSSH2_CHANNEL* channel;
    while (true) {
        channel = libssh2_channel_forward_accept(listener);
        if (!channel) {
            if (libssh2_session_last_errno(parent->ssh_session) == LIBSSH2_ERROR_EAGAIN) {
                if (parent->waitSocketUnlocked(xsink, SSH2LISTENER_TIMEOUT, SSH2LISTENER_ERROR, "SSH2Listener::accept", timeout_ms)) {
                    return nullptr;
                }
                continue;
            }
            parent->doSessionErrUnlocked(xsink);
            return nullptr;
        }
        break;
    }

    return parent->registerChannelUnlocked(channel);
}

void SSH2Listener::cancel(ExceptionSink* xsink) {
    AutoLocker al(parent->m);

    if (!listener) {
        xsink->raiseException(SSH2LISTENER_ERROR, "the SSH2 listener has already been cancelled");
        return;
    }

    cancelUnlocked();
}

#endif // HAVE_LIBSSH2_FORWARD_LISTEN
