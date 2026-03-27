/*
    modules/ssh2/ssh2-module.cpp

    SSH2/SFTP integration to QORE

    Qore Programming Language

    Copyright (C) 2009 Wolfgang Ritzinger
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

#include <qore/Qore.h>

#include "ssh2-module.h"
#include "QC_SSH2Base.h"
#include "SSH2Client.h"
#include "SFTPClient.h"
#include "SSH2Channel.h"
#include "SSH2Listener.h"

#include <string.h>

// thread-local storage for password for faked keyboard-interactive authentication
TLKeyboardPassword keyboardPassword;

static QoreNamespace ssh2ns("Qore::SSH2"); // namespace

// for verifying the minimum required version of the library
static const char *qore_libssh2_version = 0;

static void ssh2_module_init(QoreModuleInitContext& ctx, ExceptionSink& xsink);
static void ssh2_module_ns_init(QoreNamespace* rns, QoreNamespace* qns, ExceptionSink& xsink);
static void ssh2_module_delete();

extern "C" DLLEXPORT void ssh2_qore_module_desc(QoreModuleInfo& mod_info) {
    mod_info.name = "ssh2";
    mod_info.version = PACKAGE_VERSION;
    mod_info.desc = "SSH2/SFTP client module";
    mod_info.author = "Wolfgang Ritzinger";
    mod_info.url = "http://qore.org";
    mod_info.api_major = QORE_MODULE_API_MAJOR;
    mod_info.api_minor = QORE_MODULE_API_MINOR;
    mod_info.init = ssh2_module_init;
    mod_info.ns_init = ssh2_module_ns_init;
    mod_info.del = ssh2_module_delete;
    mod_info.license = QL_MIT;
    mod_info.license_str = "MIT";
}

emap_t ssh2_emap;
edmap_t sftp_emap;

DLLLOCAL const TypedHashDecl* hashdeclSftpFileInfo;
DLLLOCAL const TypedHashDecl* hashdeclSftpDirInfo;
DLLLOCAL const TypedHashDecl* hashdeclSftpConnectionInfo;
DLLLOCAL const TypedHashDecl* hashdeclSsh2ConnectionInfo;
DLLLOCAL const TypedHashDecl* hashdeclSsh2StatInfo;
DLLLOCAL const TypedHashDecl* hashdeclSftpStatVfsInfo;
DLLLOCAL const TypedHashDecl* hashdeclSsh2ExitSignalInfo;
DLLLOCAL const TypedHashDecl* hashdeclSsh2HostKeyInfo;

static void ssh2_module_init(QoreModuleInitContext& ctx, ExceptionSink& xsink) {
    qore_libssh2_version = libssh2_version(LIBSSH2_VERSION_NUM);
    if (!qore_libssh2_version) {
        xsink.raiseException("MODULE-INIT-ERROR", "the runtime version of the library is too old; got '%s', expecting minimum version '%s'", libssh2_version(0), LIBSSH2_VERSION);
        return;
    }

    // setup ssh2 error map
    ssh2_emap.insert(emap_t::value_type(LIBSSH2_ERROR_SOCKET_NONE, "LIBSSH2_ERROR_SOCKET_NONE"));
#ifdef LIBSSH2_ERROR_BANNER_RECV
    ssh2_emap.insert(emap_t::value_type(LIBSSH2_ERROR_BANNER_RECV, "LIBSSH2_ERROR_BANNER_RECV"));
#endif
    ssh2_emap.insert(emap_t::value_type(LIBSSH2_ERROR_BANNER_SEND, "LIBSSH2_ERROR_BANNER_SEND"));
    ssh2_emap.insert(emap_t::value_type(LIBSSH2_ERROR_INVALID_MAC, "LIBSSH2_ERROR_INVALID_MAC"));
    ssh2_emap.insert(emap_t::value_type(LIBSSH2_ERROR_KEX_FAILURE, "LIBSSH2_ERROR_KEX_FAILURE"));
    ssh2_emap.insert(emap_t::value_type(LIBSSH2_ERROR_ALLOC, "LIBSSH2_ERROR_ALLOC"));
    ssh2_emap.insert(emap_t::value_type(LIBSSH2_ERROR_SOCKET_SEND, "LIBSSH2_ERROR_SOCKET_SEND"));
    ssh2_emap.insert(emap_t::value_type(LIBSSH2_ERROR_KEY_EXCHANGE_FAILURE, "LIBSSH2_ERROR_KEY_EXCHANGE_FAILURE"));
    ssh2_emap.insert(emap_t::value_type(LIBSSH2_ERROR_TIMEOUT, "LIBSSH2_ERROR_TIMEOUT"));
    ssh2_emap.insert(emap_t::value_type(LIBSSH2_ERROR_HOSTKEY_INIT, "LIBSSH2_ERROR_HOSTKEY_INIT"));
    ssh2_emap.insert(emap_t::value_type(LIBSSH2_ERROR_HOSTKEY_SIGN, "LIBSSH2_ERROR_HOSTKEY_SIGN"));
    ssh2_emap.insert(emap_t::value_type(LIBSSH2_ERROR_DECRYPT, "LIBSSH2_ERROR_DECRYPT"));
    ssh2_emap.insert(emap_t::value_type(LIBSSH2_ERROR_SOCKET_DISCONNECT, "LIBSSH2_ERROR_SOCKET_DISCONNECT"));
    ssh2_emap.insert(emap_t::value_type(LIBSSH2_ERROR_PROTO, "LIBSSH2_ERROR_PROTO"));
    ssh2_emap.insert(emap_t::value_type(LIBSSH2_ERROR_PASSWORD_EXPIRED, "LIBSSH2_ERROR_PASSWORD_EXPIRED"));
    ssh2_emap.insert(emap_t::value_type(LIBSSH2_ERROR_FILE, "LIBSSH2_ERROR_FILE"));
    ssh2_emap.insert(emap_t::value_type(LIBSSH2_ERROR_METHOD_NONE, "LIBSSH2_ERROR_METHOD_NONE"));
#ifdef LIBSSH2_ERROR_AUTHENTICATION_FAILED
    ssh2_emap.insert(emap_t::value_type(LIBSSH2_ERROR_AUTHENTICATION_FAILED, "LIBSSH2_ERROR_AUTHENTICATION_FAILED"));
#endif
    ssh2_emap.insert(emap_t::value_type(LIBSSH2_ERROR_PUBLICKEY_UNRECOGNIZED, "LIBSSH2_ERROR_PUBLICKEY_UNRECOGNIZED"));
    ssh2_emap.insert(emap_t::value_type(LIBSSH2_ERROR_PUBLICKEY_UNVERIFIED, "LIBSSH2_ERROR_PUBLICKEY_UNVERIFIED"));
    ssh2_emap.insert(emap_t::value_type(LIBSSH2_ERROR_CHANNEL_OUTOFORDER, "LIBSSH2_ERROR_CHANNEL_OUTOFORDER"));
    ssh2_emap.insert(emap_t::value_type(LIBSSH2_ERROR_CHANNEL_FAILURE, "LIBSSH2_ERROR_CHANNEL_FAILURE"));
    ssh2_emap.insert(emap_t::value_type(LIBSSH2_ERROR_CHANNEL_REQUEST_DENIED, "LIBSSH2_ERROR_CHANNEL_REQUEST_DENIED"));
    ssh2_emap.insert(emap_t::value_type(LIBSSH2_ERROR_CHANNEL_UNKNOWN, "LIBSSH2_ERROR_CHANNEL_UNKNOWN"));
    ssh2_emap.insert(emap_t::value_type(LIBSSH2_ERROR_CHANNEL_WINDOW_EXCEEDED, "LIBSSH2_ERROR_CHANNEL_WINDOW_EXCEEDED"));
    ssh2_emap.insert(emap_t::value_type(LIBSSH2_ERROR_CHANNEL_PACKET_EXCEEDED, "LIBSSH2_ERROR_CHANNEL_PACKET_EXCEEDED"));
    ssh2_emap.insert(emap_t::value_type(LIBSSH2_ERROR_CHANNEL_CLOSED, "LIBSSH2_ERROR_CHANNEL_CLOSED"));
    ssh2_emap.insert(emap_t::value_type(LIBSSH2_ERROR_CHANNEL_EOF_SENT, "LIBSSH2_ERROR_CHANNEL_EOF_SENT"));
    ssh2_emap.insert(emap_t::value_type(LIBSSH2_ERROR_SCP_PROTOCOL, "LIBSSH2_ERROR_SCP_PROTOCOL"));
    ssh2_emap.insert(emap_t::value_type(LIBSSH2_ERROR_ZLIB, "LIBSSH2_ERROR_ZLIB"));
    ssh2_emap.insert(emap_t::value_type(LIBSSH2_ERROR_SOCKET_TIMEOUT, "LIBSSH2_ERROR_SOCKET_TIMEOUT"));
    ssh2_emap.insert(emap_t::value_type(LIBSSH2_ERROR_SFTP_PROTOCOL, "LIBSSH2_ERROR_SFTP_PROTOCOL"));
    ssh2_emap.insert(emap_t::value_type(LIBSSH2_ERROR_REQUEST_DENIED, "LIBSSH2_ERROR_REQUEST_DENIED"));
    ssh2_emap.insert(emap_t::value_type(LIBSSH2_ERROR_METHOD_NOT_SUPPORTED, "LIBSSH2_ERROR_METHOD_NOT_SUPPORTED"));
    ssh2_emap.insert(emap_t::value_type(LIBSSH2_ERROR_INVAL, "LIBSSH2_ERROR_INVAL"));
    ssh2_emap.insert(emap_t::value_type(LIBSSH2_ERROR_INVALID_POLL_TYPE, "LIBSSH2_ERROR_INVALID_POLL_TYPE"));
    ssh2_emap.insert(emap_t::value_type(LIBSSH2_ERROR_PUBLICKEY_PROTOCOL, "LIBSSH2_ERROR_PUBLICKEY_PROTOCOL"));
    ssh2_emap.insert(emap_t::value_type(LIBSSH2_ERROR_EAGAIN, "LIBSSH2_ERROR_EAGAIN"));
    ssh2_emap.insert(emap_t::value_type(LIBSSH2_ERROR_BUFFER_TOO_SMALL, "LIBSSH2_ERROR_BUFFER_TOO_SMALL"));
    ssh2_emap.insert(emap_t::value_type(LIBSSH2_ERROR_BAD_USE, "LIBSSH2_ERROR_BAD_USE"));
    ssh2_emap.insert(emap_t::value_type(LIBSSH2_ERROR_COMPRESS, "LIBSSH2_ERROR_COMPRESS"));
    ssh2_emap.insert(emap_t::value_type(LIBSSH2_ERROR_OUT_OF_BOUNDARY, "LIBSSH2_ERROR_OUT_OF_BOUNDARY"));
#ifdef LIBSSH2_ERROR_AGENT_PROTOCOL
    ssh2_emap.insert(emap_t::value_type(LIBSSH2_ERROR_AGENT_PROTOCOL, "LIBSSH2_ERROR_AGENT_PROTOCOL"));
#endif
#ifdef LIBSSH2_ERROR_SOCKET_RECV
    ssh2_emap.insert(emap_t::value_type(LIBSSH2_ERROR_SOCKET_RECV, "LIBSSH2_ERROR_SOCKET_RECV"));
#endif
#ifdef LIBSSH2_ERROR_ENCRYPT
    ssh2_emap.insert(emap_t::value_type(LIBSSH2_ERROR_ENCRYPT, "LIBSSH2_ERROR_ENCRYPT"));
#endif
#ifdef LIBSSH2_ERROR_BAD_SOCKET
    ssh2_emap.insert(emap_t::value_type(LIBSSH2_ERROR_BAD_SOCKET, "LIBSSH2_ERROR_BAD_SOCKET"));
#endif
#ifdef LIBSSH2_ERROR_KNOWN_HOSTS
    ssh2_emap.insert(emap_t::value_type(LIBSSH2_ERROR_KNOWN_HOSTS, "LIBSSH2_ERROR_KNOWN_HOSTS"));
#endif
    ssh2_emap.insert(emap_t::value_type(LIBSSH2_ERROR_BANNER_NONE, "LIBSSH2_ERROR_BANNER_NONE"));

    // setup sftp error map
    sftp_emap.insert(edmap_t::value_type(LIBSSH2_FX_OK, ErrDesc("LIBSSH2_FX_OK", "success")));
    sftp_emap.insert(edmap_t::value_type(LIBSSH2_FX_EOF, ErrDesc("LIBSSH2_FX_EOF", "EOF: end of file")));
    sftp_emap.insert(edmap_t::value_type(LIBSSH2_FX_NO_SUCH_FILE, ErrDesc("LIBSSH2_FX_NO_SUCH_FILE", "file does not exist")));
    sftp_emap.insert(edmap_t::value_type(LIBSSH2_FX_PERMISSION_DENIED, ErrDesc("LIBSSH2_FX_PERMISSION_DENIED", "permission denied")));
    sftp_emap.insert(edmap_t::value_type(LIBSSH2_FX_FAILURE, ErrDesc("LIBSSH2_FX_FAILURE", "command failed")));
    sftp_emap.insert(edmap_t::value_type(LIBSSH2_FX_BAD_MESSAGE, ErrDesc("LIBSSH2_FX_BAD_MESSAGE", "bad message")));
    sftp_emap.insert(edmap_t::value_type(LIBSSH2_FX_NO_CONNECTION, ErrDesc("LIBSSH2_FX_NO_CONNECTION", "no connection")));
    sftp_emap.insert(edmap_t::value_type(LIBSSH2_FX_CONNECTION_LOST, ErrDesc("LIBSSH2_FX_CONNECTION_LOST", "connection lost")));
    sftp_emap.insert(edmap_t::value_type(LIBSSH2_FX_OP_UNSUPPORTED, ErrDesc("LIBSSH2_FX_OP_UNSUPPORTED", "sshd sftp server does not support this operation")));
    sftp_emap.insert(edmap_t::value_type(LIBSSH2_FX_INVALID_HANDLE, ErrDesc("LIBSSH2_FX_INVALID_HANDLE", "invalid handle")));
    sftp_emap.insert(edmap_t::value_type(LIBSSH2_FX_NO_SUCH_PATH, ErrDesc("LIBSSH2_FX_NO_SUCH_PATH", "path does not exist")));
    sftp_emap.insert(edmap_t::value_type(LIBSSH2_FX_FILE_ALREADY_EXISTS, ErrDesc("LIBSSH2_FX_FILE_ALREADY_EXISTS", "file already exists")));
    sftp_emap.insert(edmap_t::value_type(LIBSSH2_FX_WRITE_PROTECT, ErrDesc("LIBSSH2_FX_WRITE_PROTECT", "write protected")));
    sftp_emap.insert(edmap_t::value_type(LIBSSH2_FX_NO_MEDIA, ErrDesc("LIBSSH2_FX_NO_MEDIA", "no media")));
    sftp_emap.insert(edmap_t::value_type(LIBSSH2_FX_NO_SPACE_ON_FILESYSTEM, ErrDesc("LIBSSH2_FX_NO_SPACE_ON_FILESYSTEM", "filesystem full")));
    sftp_emap.insert(edmap_t::value_type(LIBSSH2_FX_QUOTA_EXCEEDED, ErrDesc("LIBSSH2_FX_QUOTA_EXCEEDED", "quota exceeded")));
#ifdef LIBSSH2_FX_UNKNOWN_PRINCIPAL
    sftp_emap.insert(edmap_t::value_type(LIBSSH2_FX_UNKNOWN_PRINCIPAL, ErrDesc("LIBSSH2_FX_UNKNOWN_PRINCIPAL", "unknown principal")));
#else
    sftp_emap.insert(edmap_t::value_type(LIBSSH2_FX_UNKNOWN_PRINCIPLE, ErrDesc("LIBSSH2_FX_UNKNOWN_PRINCIPAL", "unknown principal")));
#endif
#ifdef LIBSSH2_FX_LOCK_CONFLICT
    sftp_emap.insert(edmap_t::value_type(LIBSSH2_FX_LOCK_CONFLICT, ErrDesc("LIBSSH2_FX_LOCK_CONFLICT", "lock conflict")));
#else
    sftp_emap.insert(edmap_t::value_type(LIBSSH2_FX_LOCK_CONFlICT, ErrDesc("LIBSSH2_FX_LOCK_CONFLICT", "lock conflict")));
#endif
    sftp_emap.insert(edmap_t::value_type(LIBSSH2_FX_DIR_NOT_EMPTY, ErrDesc("LIBSSH2_FX_DIR_NOT_EMPTY", "directory not empty")));
    sftp_emap.insert(edmap_t::value_type(LIBSSH2_FX_NOT_A_DIRECTORY, ErrDesc("LIBSSH2_FX_NOT_A_DIRECTORY", "not a directory")));
    sftp_emap.insert(edmap_t::value_type(LIBSSH2_FX_INVALID_FILENAME, ErrDesc("LIBSSH2_FX_INVALID_FILENAME", "invalid filename")));
    sftp_emap.insert(edmap_t::value_type(LIBSSH2_FX_LINK_LOOP, ErrDesc("LIBSSH2_FX_LINK_LOOP", "link loop")));

    // add all hashdecls first
    hashdeclSftpFileInfo = init_hashdecl_SftpFileInfo(ssh2ns);
    hashdeclSftpDirInfo = init_hashdecl_SftpDirInfo(ssh2ns);
    hashdeclSftpConnectionInfo = init_hashdecl_SftpConnectionInfo(ssh2ns);
    hashdeclSsh2ConnectionInfo = init_hashdecl_Ssh2ConnectionInfo(ssh2ns);
    hashdeclSsh2StatInfo = init_hashdecl_Ssh2StatInfo(ssh2ns);
    hashdeclSftpStatVfsInfo = init_hashdecl_SftpStatVfsInfo(ssh2ns);
    hashdeclSsh2ExitSignalInfo = init_hashdecl_Ssh2ExitSignalInfo(ssh2ns);
    hashdeclSsh2HostKeyInfo = init_hashdecl_Ssh2HostKeyInfo(ssh2ns);

    // all classes belonging to here
    // NOTE: SSH2Listener must be initialized before SSH2Client because SSH2Client
    // references SSH2Listener as a return type for forwardListen()
    ssh2ns.addSystemClass(initSSH2BaseClass(ssh2ns));
    ssh2ns.addSystemClass(initSSH2ChannelClass(ssh2ns));
#ifdef HAVE_LIBSSH2_FORWARD_LISTEN
    ssh2ns.addSystemClass(initSSH2ListenerClass(ssh2ns));
#endif
    ssh2ns.addSystemClass(initSSH2ClientClass(ssh2ns));
    ssh2ns.addSystemClass(initSFTPClientClass(ssh2ns));

    // constants
    ssh2ns.addConstant("Version", new QoreStringNode(qore_libssh2_version));

    // host key policy constants
    ssh2ns.addConstant("SSH2_HOSTKEY_REJECT", SSH2_HOSTKEY_REJECT);
    ssh2ns.addConstant("SSH2_HOSTKEY_TOFU", SSH2_HOSTKEY_TOFU);

#ifdef HAVE_LIBSSH2_TRACE
    // trace bitmask constants
    ssh2ns.addConstant("SSH2_TRACE_TRANS", (int64)LIBSSH2_TRACE_TRANS);
    ssh2ns.addConstant("SSH2_TRACE_KEX", (int64)LIBSSH2_TRACE_KEX);
    ssh2ns.addConstant("SSH2_TRACE_AUTH", (int64)LIBSSH2_TRACE_AUTH);
    ssh2ns.addConstant("SSH2_TRACE_CONN", (int64)LIBSSH2_TRACE_CONN);
    ssh2ns.addConstant("SSH2_TRACE_SCP", (int64)LIBSSH2_TRACE_SCP);
    ssh2ns.addConstant("SSH2_TRACE_SFTP", (int64)LIBSSH2_TRACE_SFTP);
    ssh2ns.addConstant("SSH2_TRACE_ERROR", (int64)LIBSSH2_TRACE_ERROR);
    ssh2ns.addConstant("SSH2_TRACE_PUBLICKEY", (int64)LIBSSH2_TRACE_PUBLICKEY);
    ssh2ns.addConstant("SSH2_TRACE_SOCKET", (int64)LIBSSH2_TRACE_SOCKET);
#endif

}

static void ssh2_module_ns_init(QoreNamespace* rns, QoreNamespace* qns, ExceptionSink& xsink) {
   QORE_TRACE("ssh2_module_ns_init()");

#ifdef LIBSSH2_INIT_NO_CRYPTO
   libssh2_init(LIBSSH2_INIT_NO_CRYPTO);
#endif

   qns->addInitialNamespace(ssh2ns.copy());
}

static void ssh2_module_delete() {
   QORE_TRACE("ssh2_module_delete()");
}
