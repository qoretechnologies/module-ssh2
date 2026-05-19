/*
    modules/ssh2/ssh2-module.h

    SSH2/SFTP integration to QORE

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

#ifndef _QORE_SSH2_MODULE_H

#define _QORE_SSH2_MODULE_H

// include configure defines first
#include <config.h>

// include Qore API
#include <qore/Qore.h>
#include <qore/QoreSandboxManager.h>

// include libssh2 API
#include "ssh2.h"

#include <map>

typedef std::map<int, const char*> emap_t;
DLLLOCAL extern emap_t ssh2_emap;

struct ErrDesc {
    const char* err;
    const char* desc;

    DLLLOCAL ErrDesc(const char* e, const char* d) : err(e), desc(d) {
    }
};

typedef std::map<int, ErrDesc> edmap_t;
DLLLOCAL extern edmap_t sftp_emap;

DLLLOCAL TypedHashDecl* init_hashdecl_SftpFileInfo(QoreNamespace& ns);
DLLLOCAL TypedHashDecl* init_hashdecl_SftpDirInfo(QoreNamespace& ns);
DLLLOCAL TypedHashDecl* init_hashdecl_SftpConnectionInfo(QoreNamespace& ns);
DLLLOCAL TypedHashDecl* init_hashdecl_Ssh2ConnectionInfo(QoreNamespace& ns);
DLLLOCAL TypedHashDecl* init_hashdecl_Ssh2StatInfo(QoreNamespace& ns);
DLLLOCAL TypedHashDecl* init_hashdecl_SftpStatVfsInfo(QoreNamespace& ns);
DLLLOCAL TypedHashDecl* init_hashdecl_Ssh2ExitSignalInfo(QoreNamespace& ns);
DLLLOCAL TypedHashDecl* init_hashdecl_Ssh2HostKeyInfo(QoreNamespace& ns);
DLLLOCAL QoreEnumDecl* init_enum_Ssh2HostKeyPolicy(QoreNamespace& ns);
DLLLOCAL QoreEnumDecl* init_enum_Ssh2ClientAuthOrder(QoreNamespace& ns);
DLLLOCAL QoreEnumDecl* init_enum_Ssh2ClientIdentityFallbackPolicy(QoreNamespace& ns);

DLLLOCAL extern const TypedHashDecl* hashdeclSftpFileInfo;
DLLLOCAL extern const TypedHashDecl* hashdeclSftpDirInfo;
DLLLOCAL extern const TypedHashDecl* hashdeclSftpConnectionInfo;
DLLLOCAL extern const TypedHashDecl* hashdeclSsh2ConnectionInfo;
DLLLOCAL extern const TypedHashDecl* hashdeclSsh2StatInfo;
DLLLOCAL extern const TypedHashDecl* hashdeclSftpStatVfsInfo;
DLLLOCAL extern const TypedHashDecl* hashdeclSsh2ExitSignalInfo;
DLLLOCAL extern const TypedHashDecl* hashdeclSsh2HostKeyInfo;
DLLLOCAL extern QoreEnumDecl* enumSsh2ClientAuthOrder;
DLLLOCAL extern QoreEnumDecl* enumSsh2ClientIdentityFallbackPolicy;

// module-internal cache of the sshutil abstract provider classes; populated
// in ssh2_module_init() by calling the public sshutil accessor functions
// declared in <qore/sshutil.h> (the sshutil class-pointer data symbols must
// NOT be referenced directly: Qore dlopen()s this module before loading its
// sshutil dependency, so an undefined data symbol would fail to bind)
DLLLOCAL extern QoreClass* QC_ABSTRACTSSHCLIENTIDENTITYPROVIDER;
DLLLOCAL extern QoreClass* QC_ABSTRACTSSHHOSTKEYSTORE;
#include <qore/sshutil.h>

// host key policy constants
#define SSH2_HOSTKEY_REJECT       0
#define SSH2_HOSTKEY_TOFU         1
#define SSH2_HOSTKEY_TOFU_SESSION 2

// client identity provider ordering constants
#define SSH2_CLIENT_AUTH_EXPLICIT_FIRST 0
#define SSH2_CLIENT_AUTH_PROVIDER_FIRST 1

// client identity process-user fallback policy constants
#define SSH2_CLIENT_ID_FALLBACK_DISABLED               0
#define SSH2_CLIENT_ID_FALLBACK_AGENT                  1
#define SSH2_CLIENT_ID_FALLBACK_DEFAULT_KEYS           2
#define SSH2_CLIENT_ID_FALLBACK_AGENT_AND_DEFAULT_KEYS 3

// process-global default known_hosts handling modes (see ssh2-module.cpp)
#define SSH2_KH_DEFAULT_AUTO     0   //!< built-in: the local OS user's ~/.ssh/known_hosts (filesystem-gated)
#define SSH2_KH_DEFAULT_DISABLED 1   //!< no implicit known_hosts file is used
#define SSH2_KH_DEFAULT_EXPLICIT 2   //!< use the explicitly-configured default path

#include <string>

//! parses the QORE_SSH2_DEFAULT_* environment variables into the process-global defaults; called once from ssh2_module_init()
DLLLOCAL void ssh2_init_host_key_defaults();

//! returns the process-global default for host key verification
DLLLOCAL bool ssh2_get_default_verify_host_key();
//! sets the process-global default for host key verification
DLLLOCAL void ssh2_set_default_verify_host_key(bool verify);

//! returns the process-global default host key policy
DLLLOCAL int ssh2_get_default_host_key_policy();
//! sets the process-global default host key policy; returns 0 on success, -1 if the policy is invalid
DLLLOCAL int ssh2_set_default_host_key_policy(int policy);

//! returns the process-global default known_hosts mode; if SSH2_KH_DEFAULT_EXPLICIT, \a path is set to the configured path
DLLLOCAL int ssh2_get_default_known_hosts(std::string& path);
//! sets the process-global default known_hosts mode; \a path is only used when \a mode is SSH2_KH_DEFAULT_EXPLICIT
DLLLOCAL void ssh2_set_default_known_hosts(int mode, const char* path);

#endif
