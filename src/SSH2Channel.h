/*
    SSH2Channel.h

    libssh2 ssh2 channel integration in Qore

    Qore Programming Language

    Copyright 2010 Wolfgang Ritzinger
    Copyright 2010 - 2026 Qore Technologies, s.r.o.

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

#ifndef _QORE_SSH2CHANNEL_H

#define _QORE_SSH2CHANNEL_H

#include "ssh2.h"

#include <qore/Qore.h>

DLLLOCAL extern qore_classid_t CID_SSH2CHANNEL;
DLLLOCAL extern QoreClass* QC_SSH2CHANNEL;

DLLLOCAL QoreClass* initSSH2ChannelClass(QoreNamespace& ns);

class SSH2Client;

class SSH2Channel : public AbstractPrivateData {
    friend class SSH2Client;

protected:
    LIBSSH2_CHANNEL* channel;
    SSH2Client* parent;
    const QoreEncoding* enc;

    void closeUnlocked() {
        libssh2_channel_free(channel);
        channel = nullptr;
    }

    int check_open(ExceptionSink* xsink) {
        if (channel)
            return 0;

        xsink->raiseException("SSH2-CHANNEL-ERROR", "The SSH2 channel has already been closed");
        return -1;
    }

public:
    // channel is already registered with parent when it's created
    DLLLOCAL SSH2Channel(LIBSSH2_CHANNEL *n_channel, SSH2Client *n_parent) : channel(n_channel), parent(n_parent), enc(QCS_DEFAULT) {
    }

    DLLLOCAL ~SSH2Channel() {
        if (channel) {
            destructor();
        }

        // channel must be closed before object is destroyed
        assert(!channel);
    }

    DLLLOCAL void destructor();

    DLLLOCAL void setEncoding(const QoreEncoding* n_enc) {
        enc = n_enc;
    }

    DLLLOCAL const QoreEncoding* getEncoding() const {
        return enc;
    }

    DLLLOCAL int setenv(const char* name, const char* value, int timeout_ms, ExceptionSink* xsink);
    DLLLOCAL int requestPty(ExceptionSink* xsink, const QoreString& term, const QoreString& modes, int width = LIBSSH2_TERM_WIDTH,
                int height = LIBSSH2_TERM_HEIGHT, int width_px = LIBSSH2_TERM_WIDTH_PX,
                int height_px = LIBSSH2_TERM_HEIGHT_PX, int timeout_ms = -1);
    DLLLOCAL int shell(ExceptionSink* xsink, int timeout_ms = -1);
    DLLLOCAL bool eof(ExceptionSink* xsink);
    DLLLOCAL int sendEof(ExceptionSink* xsink, int timeout_ms = -1);
    DLLLOCAL int waitEof(ExceptionSink* xsink, int timeout_ms = -1);
    DLLLOCAL int exec(const char *command, int timeout_ms, ExceptionSink* xsink);
    DLLLOCAL int subsystem(const char *command, int timeout_ms, ExceptionSink* xsink);
    DLLLOCAL QoreStringNode *read(ExceptionSink* xsink, int stream_id, int timeout_ms = DEFAULT_TIMEOUT_MS);
    // read a block of a particular size, timeout_ms mandatory
    DLLLOCAL QoreStringNode *read(qore_size_t size, int stream_id, int timeout_ms, ExceptionSink* xsink);
    DLLLOCAL BinaryNode *readBinary(ExceptionSink* xsink, int stream_id, int timeout_ms = DEFAULT_TIMEOUT_MS);
    // read a block of a particular size, timeout_ms mandatory
    DLLLOCAL BinaryNode *readBinary(qore_size_t size, int stream_id, int timeout_ms, ExceptionSink* xsink);
    DLLLOCAL qore_size_t read(ExceptionSink* xsink, void *buf, qore_size_t size, int stream_id = 0, int timeout_ms = -1);
    DLLLOCAL qore_size_t write(ExceptionSink* xsink, const void *buf, qore_size_t buflen, int stream_id = 0, int timeout_ms = -1);
    DLLLOCAL int close(ExceptionSink* xsink, int timeout_ms = -1);
    DLLLOCAL int waitClosed(ExceptionSink* xsink, int timeout_ms = -1);
    DLLLOCAL int getExitStatus(ExceptionSink* xsink);
    DLLLOCAL int requestX11Forwarding(ExceptionSink* xsink, int screen_number, bool single_connection = false, const char *auth_proto = 0, const char *auth_cookie = 0, int timeout_ms = -1);
    DLLLOCAL int extendedDataNormal(ExceptionSink* xsink, int timeout_ms = -1);
    DLLLOCAL int extendedDataMerge(ExceptionSink* xsink, int timeout_ms = -1);
    DLLLOCAL int extendedDataIgnore(ExceptionSink* xsink, int timeout_ms = -1);
    DLLLOCAL int requestPtySize(ExceptionSink* xsink, int width, int height, int width_px = 0, int height_px = 0, int timeout_ms = -1);
#ifdef HAVE_LIBSSH2_GET_EXIT_SIGNAL
    DLLLOCAL QoreHashNode* getExitSignal(ExceptionSink* xsink);
#endif
#ifdef HAVE_LIBSSH2_CHANNEL_SIGNAL_EX
    DLLLOCAL int sendSignal(const char* signal_name, ExceptionSink* xsink, int timeout_ms = -1);
#endif
#ifdef HAVE_LIBSSH2_CHANNEL_REQUEST_AUTH_AGENT
    DLLLOCAL int requestAuthAgent(ExceptionSink* xsink, int timeout_ms = -1);
#endif
};

#endif //_QORE_SSH2CHANNEL_H
