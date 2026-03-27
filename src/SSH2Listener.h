/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    SSH2Listener.h

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

#ifndef _QORE_SSH2LISTENER_H

#define _QORE_SSH2LISTENER_H

#include "ssh2-module.h"

#ifdef HAVE_LIBSSH2_FORWARD_LISTEN

DLLLOCAL extern qore_classid_t CID_SSH2LISTENER;
DLLLOCAL extern QoreClass* QC_SSH2LISTENER;

DLLLOCAL QoreClass* initSSH2ListenerClass(QoreNamespace& ns);

class SSH2Client;

class SSH2Listener : public AbstractPrivateData {
    friend class SSH2Client;

protected:
    LIBSSH2_LISTENER* listener;
    SSH2Client* parent;
    int bound_port;

    DLLLOCAL void cancelUnlocked();

public:
    DLLLOCAL SSH2Listener(LIBSSH2_LISTENER* n_listener, SSH2Client* n_parent, int n_bound_port)
        : listener(n_listener), parent(n_parent), bound_port(n_bound_port) {
    }

    DLLLOCAL ~SSH2Listener();

    DLLLOCAL void destructor();

    DLLLOCAL QoreObject* accept(ExceptionSink* xsink, int timeout_ms = -1);
    DLLLOCAL void cancel(ExceptionSink* xsink);
    DLLLOCAL int getBoundPort() const { return bound_port; }
};

#endif // HAVE_LIBSSH2_FORWARD_LISTEN

#endif // _QORE_SSH2LISTENER_H
