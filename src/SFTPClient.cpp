/* -*- indent-tabs-mode: nil -*- */
/*
    SFTPClient.cpp

    libssh2 SFTP client integration into qore

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

#include "SFTPClient.h"

#include <memory>
#include <string>
#include <map>
#include <utility>
#include <sys/types.h>
#include <sys/stat.h>
#ifdef HAVE_PWD_H
#include <pwd.h>
#endif
#include <assert.h>
#include <unistd.h>

#include <memory>

static const char* SFTPCLIENT_CONNECT_ERROR = "SFTPCLIENT-CONNECT-ERROR";
static const char* SFTPCLIENT_TIMEOUT = "SFTPCLIENT-TIMEOUT";

static const char* ssh2_mode_to_perm(mode_t mode, QoreString& perm) {
    const char* type;
    if (S_ISBLK(mode)) {
        type = "BLOCK-DEVICE";
        perm.concat('b');
    } else if (S_ISDIR(mode)) {
        type = "DIRECTORY";
        perm.concat('d');
    } else if (S_ISCHR(mode)) {
        type = "CHARACTER-DEVICE";
        perm.concat('c');
    } else if (S_ISFIFO(mode)) {
        type = "FIFO";
        perm.concat('p');
    }
#ifdef S_ISLNK
    else if (S_ISLNK(mode)) {
        type = "SYMBOLIC-LINK";
        perm.concat('l');
    }
#endif
#ifdef S_ISSOCK
    else if (S_ISSOCK(mode)) {
        type = "SOCKET";
        perm.concat('s');
    }
#endif
    else if (S_ISREG(mode)) {
        type = "REGULAR";
        perm.concat('-');
    } else {
        type = "UNKNOWN";
        perm.concat('?');
    }

    // add user permission flags
    perm.concat(mode & S_IRUSR ? 'r' : '-');
    perm.concat(mode & S_IWUSR ? 'w' : '-');
#ifdef S_ISUID
    if (mode & S_ISUID)
        perm.concat(mode & S_IXUSR ? 's' : 'S');
    else
        perm.concat(mode & S_IXUSR ? 'x' : '-');
#else
    // Windows
    perm.concat('-');
#endif

   // add group permission flags
#ifdef S_IRGRP
    perm.concat(mode & S_IRGRP ? 'r' : '-');
    perm.concat(mode & S_IWGRP ? 'w' : '-');
#else
    // Windows
    perm.concat("--");
#endif
#ifdef S_ISGID
    if (mode & S_ISGID)
        perm.concat(mode & S_IXGRP ? 's' : 'S');
    else
        perm.concat(mode & S_IXGRP ? 'x' : '-');
#else
    // Windows
    perm.concat('-');
#endif

#ifdef S_IROTH
    // add other permission flags
    perm.concat(mode & S_IROTH ? 'r' : '-');
    perm.concat(mode & S_IWOTH ? 'w' : '-');
#ifdef S_ISVTX
    if (mode & S_ISVTX)
        perm.concat(mode & S_IXOTH ? 't' : 'T');
    else
#endif
        perm.concat(mode & S_IXOTH ? 'x' : '-');
#else
    // Windows
    perm.concat("---");
#endif

    return type;
}

/**
 * SFTPClient constructor
 *
 * this is for creating the connection to the host/port given.
 * this raises errors if the host/port cannot be resolved
 */
SFTPClient::SFTPClient(const char* hostname, const uint32_t port) : SSH2Client(hostname, port), sftp_session(0) {
   printd(5, "SFTPClient::SFTPClient() this: %p\n", this);
}

SFTPClient::SFTPClient(QoreURL &url, const uint32_t port) : SSH2Client(url, port), sftp_session(0) {
   printd(5, "SFTPClient::SFTPClient() this: %p\n", this);
}

/*
 * close session/connection
 * free resources
 */
SFTPClient::~SFTPClient() {
   QORE_TRACE("SFTPClient::~SFTPClient()");
   printd(5, "SFTPClient::~SFTPClient() this: %p\n", this);

   doShutdown();
}

void SFTPClient::doShutdown(int timeout_ms, ExceptionSink* xsink) {
    if (sftp_session) {
        BlockingHelper bh(this);

        int rc;
        while ((rc = libssh2_sftp_shutdown(sftp_session)) == LIBSSH2_ERROR_EAGAIN) {
            if (waitSocketUnlocked(xsink, SFTPCLIENT_TIMEOUT, "SFTPCLIENT-DISCONNECT", "SFTPClient::disconnect", timeout_ms, true))
                break;
        }

        // NOTE: there is a known memory leak here if libssh2_sftp_shutdown times out;
        // this is a libssh2 limitation - there is no other way to free the memory
        // without waiting for the remote end
        sftp_session = nullptr;
    }
}

/*
 * cleanup
 */
void SFTPClient::deref(ExceptionSink* xsink) {
    if (ROdereference()) {
        // this function must be called before the QoreSocket object is destroyed
        socket.cleanup(xsink);
        delete this;
    }
}

bool SFTPClient::sftpConnectedUnlocked(ExceptionSink* xsink) {
    return (sftp_session ? sftpIsAliveUnlocked(100, xsink) : false);
}

bool SFTPClient::sftpConnected(ExceptionSink* xsink) {
    AutoLocker al(m);
    return sftpConnectedUnlocked();
}

int SFTPClient::disconnectUnlocked(bool force, int timeout_ms, AbstractDisconnectionHelper* adh, ExceptionSink* xsink) {
    //printd(5, "SFTPClient::disconnectUnlocked() force: %d timeout_ms: %d adh: %p xsink: %p\n", force, timeout_ms, adh, xsink);
    int rc;

    // disconnect dependent objects first
    if (adh)
        adh->preDisconnect();

    // close sftp session if not null
    doShutdown(timeout_ms, xsink);

    // close ssh session if not null
    rc = SSH2Client::disconnectUnlocked(force, timeout_ms, nullptr, xsink);

    return rc;
}

QoreHashNode* SFTPClient::sftpList(const char* path, int timeout_ms, ExceptionSink* xsink) {
    AutoLocker al(m);

    // try to make an implicit connection
    if (!sftpConnectedUnlocked() && sftpConnectUnlocked(timeout_ms, xsink))
        return nullptr;

    std::string pstr;
    if (!path) // there is no path given so we use the sftpPath
        pstr = sftppath;
    else if (path[0] == '/') // absolute path, take it
        pstr = path;
    else // relative path
        pstr = sftppath + "/" + path;

    BlockingHelper bh(this);

    QSftpHelper qh(this, "SFTPCLIENT-LIST-ERROR", "SFTPClient::list", timeout_ms, xsink);

    {
        QoreSocketTimeoutHelper th(socket, "list");

        do {
            qh.assign(libssh2_sftp_opendir(sftp_session, pstr.c_str()));
            if (!qh) {
                if (libssh2_session_last_errno(ssh_session) == LIBSSH2_ERROR_EAGAIN) {
                    if (qh.waitSocket())
                        return nullptr;
                } else {
                    qh.err("error reading directory '%s'", pstr.c_str());
                    return nullptr;
                }
            }
        } while (!qh);
    }

    // create objects after only possible error
    ReferenceHolder<QoreListNode> files(new QoreListNode(stringTypeInfo), xsink);
    ReferenceHolder<QoreListNode> dirs(new QoreListNode(stringTypeInfo), xsink);
    ReferenceHolder<QoreListNode> links(new QoreListNode(stringTypeInfo), xsink);

    char buff[PATH_MAX];
    LIBSSH2_SFTP_ATTRIBUTES attrs;

    while (true) {
        int rc;
        while ((rc = libssh2_sftp_readdir(*qh, buff, sizeof(buff), &attrs)) == LIBSSH2_ERROR_EAGAIN) {
            if (qh.waitSocket())
                return nullptr;
        }
        if (!rc)
            break;
        if (rc < 0) {
            qh.err("error reading directory '%s'", pstr.c_str());
            return nullptr;
        }
        if (attrs.flags & LIBSSH2_SFTP_ATTR_PERMISSIONS) {
            // contains st_mode() from sys/stat.h
            if (S_ISDIR(attrs.permissions))
                dirs->push(new QoreStringNode(buff), xsink);
#ifdef S_ISLNK
            else if (S_ISLNK(attrs.permissions))
                links->push(new QoreStringNode(buff), xsink);
#endif
            else // everything else is a file
                files->push(new QoreStringNode(buff), xsink);
        } else
            // no info for filetype. we take it as file
            files->push(new QoreStringNode(buff), xsink);
    }

    ReferenceHolder<QoreHashNode> ret(new QoreHashNode(hashdeclSftpDirInfo, xsink), xsink);

    ret->setKeyValue("path", new QoreStringNode(pstr.c_str()), xsink);
    // QoreListNode::sort() returns a new QoreListNode object
    ret->setKeyValue("directories", dirs->sort(xsink), xsink);
    ret->setKeyValue("files", files->sort(xsink), xsink);
    ret->setKeyValue("links", links->sort(xsink), xsink);

    return ret.release();
}

QoreListNode* SFTPClient::sftpListFull(const char* path, int timeout_ms, ExceptionSink* xsink) {
    AutoLocker al(m);

    // try to make an implicit connection
    if (!sftpConnectedUnlocked() && sftpConnectUnlocked(timeout_ms, xsink))
        return nullptr;

    std::string pstr;
    if (!path) // there is no path given so we use the sftpPath
        pstr = sftppath;
    else if (path[0] == '/') // absolute path, take it
        pstr = path;
    else // relative path
        pstr = sftppath + "/" + path;

    BlockingHelper bh(this);

    QSftpHelper qh(this, "SFTPCLIENT-LISTFULL-ERROR", "SFTPClient::listFull", timeout_ms, xsink);

    {
        QoreSocketTimeoutHelper th(socket, "list");

        do {
            qh.assign(libssh2_sftp_opendir(sftp_session, pstr.c_str()));
            if (!qh) {
                if (libssh2_session_last_errno(ssh_session) == LIBSSH2_ERROR_EAGAIN) {
                    if (qh.waitSocket())
                        return nullptr;
                } else {
                    qh.err("error reading directory '%s'", pstr.c_str());
                    return nullptr;
                }
            }
        } while (!qh);
    }

    // create objects after only possible error
    ReferenceHolder<QoreListNode> rv(new QoreListNode(hashdeclSftpFileInfo->getTypeInfo()), xsink);

    char buff[PATH_MAX];
    LIBSSH2_SFTP_ATTRIBUTES attrs;

    while (true) {
        int rc;
        while ((rc = libssh2_sftp_readdir(*qh, buff, sizeof(buff), &attrs)) == LIBSSH2_ERROR_EAGAIN) {
            if (qh.waitSocket())
                return nullptr;
        }
        if (!rc)
            break;
        if (rc < 0) {
            qh.err("error reading directory '%s'", pstr.c_str());
            return nullptr;
        }

        ReferenceHolder<QoreHashNode> h(new QoreHashNode(hashdeclSftpFileInfo, xsink), xsink);
        h->setKeyValue("name", new QoreStringNode(buff), xsink);

        if (attrs.flags & LIBSSH2_SFTP_ATTR_PERMISSIONS) {
            SimpleRefHolder<QoreStringNode> perm(new QoreStringNode);
            const char* type = ssh2_mode_to_perm(attrs.permissions, **perm);

            h->setKeyValue("size", attrs.filesize, xsink);
            h->setKeyValue("atime", DateTimeNode::makeAbsolute(currentTZ(), (int64)attrs.atime), xsink);
            h->setKeyValue("mtime", DateTimeNode::makeAbsolute(currentTZ(), (int64)attrs.mtime), xsink);
            h->setKeyValue("uid", attrs.uid, xsink);
            h->setKeyValue("gid", attrs.gid, xsink);
            h->setKeyValue("mode", attrs.permissions, xsink);
            h->setKeyValue("type", new QoreStringNode(type), xsink);
            h->setKeyValue("perm", perm.release(), xsink);
        }

        rv->push(h.release(), xsink);
    }

    return rv.release();
}

// return 0 if ok, -1 otherwise
int SFTPClient::sftpChmod(const char* file, const int mode, int timeout_ms, ExceptionSink* xsink) {
    static const char* SFTPCLIENT_CHMOD_ERROR = "SFTPCLIENT-CHMOD-ERROR";

    assert(file);

    AutoLocker al(m);

    QSftpHelper qh(this, SFTPCLIENT_CHMOD_ERROR, "SFTPClient::chmod", timeout_ms, xsink);

    if (!file || !file[0]) {
        qh.err("file argument is empty");
        return -3;
    }

    // try to make an implicit connection
    if (!sftpConnectedUnlocked() && sftpConnectUnlocked(timeout_ms, xsink))
        return -2;

    std::string pstr;
    if (file[0] == '/')
        pstr = std::string(file);
    else
        pstr = sftppath + "/" + std::string(file);

    BlockingHelper bh(this);

    // try to get stats for this file
    LIBSSH2_SFTP_ATTRIBUTES attrs;

    int rc;
    {
        QoreSocketTimeoutHelper th(socket, "chmod");

        while ((rc = libssh2_sftp_stat(sftp_session, pstr.c_str(), &attrs)) == LIBSSH2_ERROR_EAGAIN) {
            if (qh.waitSocket())
                return -3;
        }
    }

    if (rc < 0) {
        qh.err("libssh2_sftp_stat(%s) returned an error", pstr.c_str());
        return rc;
    }

    // overwrite permissions
    if (!(attrs.flags & LIBSSH2_SFTP_ATTR_PERMISSIONS)) {
        qh.err("permissions not supported by sftp server");
        return -3;
    }

    // set the permissions for file only (ugo)
    unsigned long newmode = (attrs.permissions & (-1^SFTP_UGOMASK)) | (mode & SFTP_UGOMASK);
    attrs.permissions = newmode;

    {
        QoreSocketTimeoutHelper th(socket, "chmod");

        // set the permissions (stat). it happens that we get a 'SFTP Protocol Error' so we check manually
        while ((rc = libssh2_sftp_setstat(sftp_session, pstr.c_str(), &attrs)) == LIBSSH2_ERROR_EAGAIN) {
            if (qh.waitSocket())
                return -3;
        }
    }

    if (rc < 0) {
        {
            QoreSocketTimeoutHelper th(socket, "chmod");

            // re-read the attributes
            while ((rc = libssh2_sftp_stat(sftp_session, pstr.c_str(), &attrs)) == LIBSSH2_ERROR_EAGAIN) {
                if (qh.waitSocket())
                return -3;
            }
        }

        // they are how they should be, so we are done
        if (rc >= 0 && attrs.permissions == newmode)
            return 0;

        // ok, there was a error
        qh.err("libssh2_sftp_setstat(%s) returned an error", pstr.c_str());
    }

    return rc;
}

// return 0 if ok, -1 otherwise
int SFTPClient::sftpMkdir(const char* dir, const int mode, int timeout_ms, ExceptionSink* xsink) {
    assert(dir);

    AutoLocker al(m);

    QSftpHelper qh(this, "SFTPCLIENT-MKDIR-ERROR", "SFTPClient::mkdir", timeout_ms, xsink);

    if (!dir || !dir[0]) {
        qh.err("directory name is empty");
        return -3;
    }

    // try to make an implicit connection
    if (!sftpConnectedUnlocked() && sftpConnectUnlocked(timeout_ms, xsink))
        return -2;

    std::string pstr;
    if (dir[0] == '/')
        pstr = std::string(dir);
    else
        pstr = sftppath + "/" + std::string(dir);

    BlockingHelper bh(this);

    int rc;

    {
        QoreSocketTimeoutHelper th(socket, "mkdir");

        while ((rc = libssh2_sftp_mkdir(sftp_session, pstr.c_str(), mode)) == LIBSSH2_ERROR_EAGAIN) {
            if (qh.waitSocket())
                return -3;
        }
    }

    if (rc < 0)
        qh.err("libssh2_sftp_mkdir(%s) returned an error", pstr.c_str());

    return rc;
}

int SFTPClient::sftpRmdir(const char* dir, int timeout_ms, ExceptionSink* xsink) {
    assert(dir);

    AutoLocker al(m);

    QSftpHelper qh(this, "SFTPCLIENT-RMDIR-ERROR", "SFTPClient::rmdir", timeout_ms, xsink);

    if (!dir || !dir[0]) {
        qh.err("directory name is empty");
        return -3;
    }

    // try to make an implicit connection
    if (!sftpConnectedUnlocked() && sftpConnectUnlocked(timeout_ms, xsink))
        return -2;

    std::string pstr;
    if (dir[0] == '/')
        pstr = std::string(dir);
    else
        pstr = sftppath + "/" + std::string(dir);

    BlockingHelper bh(this);

    int rc;
    {
        QoreSocketTimeoutHelper th(socket, "rmdir");

        while ((rc = libssh2_sftp_rmdir(sftp_session, pstr.c_str())) == LIBSSH2_ERROR_EAGAIN) {
            if (qh.waitSocket())
                return -3;
        }
    }

    if (rc < 0)
        qh.err("libssh2_sftp_rmdir(%s) returned an error", pstr.c_str());

    return rc;
}

int SFTPClient::sftpRename(const char* from, const char* to, int timeout_ms, ExceptionSink* xsink) {
    assert(from && to);

    AutoLocker al(m);

    QSftpHelper qh(this, "SFTPCLIENT-RENAME-ERROR", "SFTPClient::rename", timeout_ms, xsink);

    // try to make an implicit connection
    if (!sftpConnectedUnlocked() && sftpConnectUnlocked(timeout_ms, xsink))
        return -2;

    std::string fstr, tstr;
    fstr = absolute_filename(this, from);
    tstr = absolute_filename(this, to);

    BlockingHelper bh(this);

    int rc;
    {
        QoreSocketTimeoutHelper th(socket, "rename");

        while ((rc = libssh2_sftp_rename(sftp_session, fstr.c_str(), tstr.c_str())) == LIBSSH2_ERROR_EAGAIN) {
            if (qh.waitSocket())
                return -3;
        }
    }

    if (rc < 0)
        qh.err("libssh2_sftp_rename(%s, %s) returned an error", fstr.c_str(), tstr.c_str());

    return rc;
}

int SFTPClient::sftpUnlink(const char* file, int timeout_ms, ExceptionSink* xsink) {
    assert(file);

    AutoLocker al(m);

    QSftpHelper qh(this, "SFTPCLIENT-REMOVEFILE-ERROR", "SFTPClient::removeFile", timeout_ms, xsink);

    // try to make an implicit connection
    if (!sftpConnectedUnlocked() && sftpConnectUnlocked(timeout_ms, xsink))
        return -2;

    std::string fstr;
    if (file[0] == '/')
        fstr = std::string(file);
    else
        fstr = sftppath + "/" + std::string(file);

    BlockingHelper bh(this);

    int rc;
    {
        QoreSocketTimeoutHelper th(socket, "unlink");

        while ((rc = libssh2_sftp_unlink(sftp_session, fstr.c_str())) == LIBSSH2_ERROR_EAGAIN) {
            if (qh.waitSocket())
                return -3;
        }
    }

    if (rc < 0)
        qh.err("libssh2_sftp_unlink(%s) returned an error", fstr.c_str());

    return rc;
}

QoreStringNode* SFTPClient::sftpChdir(const char* nwd, int timeout_ms, ExceptionSink* xsink) {
    char buff[PATH_MAX] = { '\0' };

    AutoLocker al(m);

    QSftpHelper qh(this, "SFTPCLIENT-CHDIR-ERROR", "SFTPClient::chdir", timeout_ms, xsink);

    // try to make an implicit connection
    if (!sftpConnectedUnlocked() && sftpConnectUnlocked(timeout_ms, xsink))
        return nullptr;

    // calc the path. if it starts with '/', replace with nwd
    std::string npath;
    if (!nwd)
        npath = sftppath;
    else if (nwd[0] == '/')
        npath = std::string(nwd);
    else
        npath = sftppath + "/" + std::string(nwd);

    BlockingHelper bh(this);

    // returns the amount of chars
    int rc;
    {
        QoreSocketTimeoutHelper th(socket, "chdir");

        while ((rc = libssh2_sftp_symlink_ex(sftp_session, npath.c_str(), npath.size(), buff, sizeof(buff) - 1, LIBSSH2_SFTP_REALPATH)) == LIBSSH2_ERROR_EAGAIN) {
            if (qh.waitSocket())
                return nullptr;
        }
    }
    if (rc <= 0) {
        qh.err("failed to retrieve the remote path for: '%s'", npath.c_str());
        return nullptr;
    }

    // check if it is a directory
    {
        QoreSocketTimeoutHelper th(socket, "chdir");

        do {
            qh.assign(libssh2_sftp_opendir(sftp_session, buff));
            if (!qh) {
                if (libssh2_session_last_errno(ssh_session) == LIBSSH2_ERROR_EAGAIN) {
                    if (qh.waitSocket())
                        return nullptr;
                } else {
                    qh.err("'%s' is not a directory", buff);
                    return nullptr;
                }
            }
        } while (!qh);
    }

    // save new path
    sftppath = buff;

    return new QoreStringNode(sftppath);
}

QoreStringNode* SFTPClient::sftpPathUnlocked() {
   return sftppath.empty() ? nullptr : new QoreStringNode(sftppath);
}

QoreStringNode* SFTPClient::sftpPath() {
   AutoLocker al(m);
   return sftpPathUnlocked();
}

/**
 * SFTPClient::sftpIsAliveUnlocked returns true if connection is alive, false otherwise
 */
bool SFTPClient::sftpIsAliveUnlocked(int timeout_ms, ExceptionSink* xsink) {
    std::string path = sftppath.empty() ? std::string("/") : sftppath;

    QSftpHelper qh(this, "SFTPCLIENT-ERROR", xsink ? "SFTPClient::isAliveEx" : "SFTPClient::isAlive", timeout_ms, xsink);
    do {
        qh.assign(libssh2_sftp_opendir(sftp_session, path.c_str()));
        if (!qh) {
            if (!ssh_session) {
                return false;
            }
            if (libssh2_session_last_errno(ssh_session) == LIBSSH2_ERROR_EAGAIN) {
                if (qh.waitSocket())
                    return false;
            } else {
                // We consider error 43 as disconnected
                if (libssh2_session_last_errno(ssh_session) == -43) {
                    return false;
                }
                qh.err("SFTP connection test ended with error: %i", libssh2_session_last_errno(ssh_session));
                return false;
            }
        }
        return true;
    } while (!qh);

    return false;
}

/**
 * SFTPClient::sftpIsAlive returns 1 if connection is alive, 0 otherwise
 */
bool SFTPClient::sftpIsAliveEx(int timeout_ms, ExceptionSink* xsink) {
    AutoLocker al(m);
    return sftpIsAliveUnlocked(timeout_ms, xsink);
}
/**
 * connect()
 * returns:
 * 0	ok
 * 1	host not found
 * 2	port not identified
 * 3	socket not created
 * 4	session init failure
 */
int SFTPClient::sftpConnectUnlocked(int timeout_ms, ExceptionSink* xsink) {
    if (sftp_session)
        disconnectUnlocked(true);

    int rc;

    rc = sshConnectUnlocked(timeout_ms, xsink);
    if (rc)
        return rc;

    QORE_TRACE("SFTPClient::connect()");

    BlockingHelper bh(this);

    QSftpHelper qh(this, SFTPCLIENT_CONNECT_ERROR, "SFTPClient::connect", timeout_ms, xsink);

    {
        QoreSocketTimeoutHelper th(socket, "connect (SFTP session)");

        do {
            // init sftp session
            sftp_session = libssh2_sftp_init(ssh_session);

            if (!sftp_session) {
                if (libssh2_session_last_errno(ssh_session) == LIBSSH2_ERROR_EAGAIN) {
                    if (qh.waitSocket())
                        return -1;
                } else {
                    if (xsink)
                        qh.err("Unable to initialize SFTP session");
                    disconnectUnlocked(true); // force shutdown
                    return -1;
                }
            }
        } while (!sftp_session);
    }

    if (sftppath.empty()) {
        // get the cwd for the path
        char buff[PATH_MAX];
        {
            QoreSocketTimeoutHelper th(socket, "connect (SFTP realpath)");

            // returns the amount of chars
            while ((rc = libssh2_sftp_symlink_ex(sftp_session, ".", 1, buff, sizeof(buff) - 1, LIBSSH2_SFTP_REALPATH)) == LIBSSH2_ERROR_EAGAIN) {
                if (qh.waitSocket())
                    return -1;
            }
        }
        if (rc <= 0) {
            if (xsink)
                qh.err("libssh2_sftp_realpath() returned an error");
            disconnectUnlocked(true); // force shutdown
            return -1;
        }
        // for safety: do end string
        buff[rc] = '\0';
        sftppath = buff;
    }

    return 0;
}

int SFTPClient::sftpConnect(int timeout_ms, ExceptionSink* xsink) {
    AutoLocker al(m);

    return sftpConnectUnlocked(timeout_ms, xsink);
}

BinaryNode* SFTPClient::sftpGetFile(const char* file, int timeout_ms, ExceptionSink* xsink) {
    AutoLocker al(m);

    // try to make an implicit connection
    if (!sftpConnectedUnlocked() && sftpConnectUnlocked(timeout_ms, xsink))
        return nullptr;

    std::string fname = absolute_filename(this, file);

    BlockingHelper bh(this);

    QSftpHelper qh(this, "SFTPCLIENT-GETFILE-ERROR", "SFTPClient::getFile", timeout_ms, xsink);

    LIBSSH2_SFTP_ATTRIBUTES attrs;
    int rc;
    {
        QoreSocketTimeoutHelper th(socket, "getFile (stat)");

        while ((rc = libssh2_sftp_stat(sftp_session, fname.c_str(), &attrs)) == LIBSSH2_ERROR_EAGAIN) {
            if (qh.waitSocket())
                return nullptr;
        }
    }
    if (rc < 0) {
        qh.err("libssh2_sftp_stat(%s) returned an error", fname.c_str());
        return nullptr;
    }
    //printd(5, "SFTPClient::sftpGetFile() permissions: %lo\n", attrs.permissions);
    size_t fsize = attrs.filesize;

    {
        QoreSocketTimeoutHelper th(socket, "getFile (open)");

        // open handle
        do {
            qh.assign(libssh2_sftp_open(sftp_session, fname.c_str(), LIBSSH2_FXF_READ, attrs.permissions));
            if (!qh) {
                if (libssh2_session_last_errno(ssh_session) == LIBSSH2_ERROR_EAGAIN) {
                    if (qh.waitSocket())
                        return nullptr;
                }
                else {
                    qh.err("libssh2_sftp_open(%s) returned an error", fname.c_str());
                    return nullptr;
                }
            }
        } while (!qh);
    }

    // close file
    // errors can be ignored, because by the time we close, we should have already what we want

    // create binary node for return with the size the server gave us on stat
    SimpleRefHolder<BinaryNode> bn(new BinaryNode);
    bn->preallocate(fsize);

    QoreSocketThroughputHelper th(socket, false);

    size_t tot = 0;
    while (fsize - tot) {
        while ((rc = libssh2_sftp_read(*qh, (char*)bn->getPtr() + tot, fsize - tot)) == LIBSSH2_ERROR_EAGAIN) {
            if (qh.waitSocket())
                return nullptr;
        }
        if (rc < 0) {
            qh.err("libssh2_sftp_read(" QLLD ") failed: total read: " QLLD " while reading '%s' size " QLLD, fsize - tot, tot, fname.c_str(), fsize);
            return nullptr;
        }
        if (rc)
            tot += rc;
        if (tot >= fsize)
            break;
    }
    bn->setSize(tot);

    th.finalize(tot);

    return bn.release();
}

QoreStringNode* SFTPClient::sftpGetTextFile(const char* file, int timeout_ms, const QoreEncoding *encoding, ExceptionSink* xsink) {
    AutoLocker al(m);

    // try to make an implicit connection
    if (!sftpConnectedUnlocked() && sftpConnectUnlocked(timeout_ms, xsink))
        return nullptr;

    std::string fname = absolute_filename(this, file);

    BlockingHelper bh(this);

    QSftpHelper qh(this, "SFTPCLIENT-GETTEXTFILE-ERROR", "SFTPClient::getTextFile", timeout_ms, xsink);

    LIBSSH2_SFTP_ATTRIBUTES attrs;
    int rc;
    {
        QoreSocketTimeoutHelper th(socket, "getTextFile (stat)");

        while ((rc = libssh2_sftp_stat(sftp_session, fname.c_str(), &attrs)) == LIBSSH2_ERROR_EAGAIN) {
            if (qh.waitSocket())
                return nullptr;
        }
    }
    if (rc < 0) {
        qh.err("libssh2_sftp_stat(%s) returned an error", fname.c_str());
        return nullptr;
    }
    size_t fsize = attrs.filesize;

    {
        QoreSocketTimeoutHelper th(socket, "getTextFile (open)");

        // open handle
        do {
            qh.assign(libssh2_sftp_open(sftp_session, fname.c_str(), LIBSSH2_FXF_READ, attrs.permissions));
            if (!qh) {
                if (libssh2_session_last_errno(ssh_session) == LIBSSH2_ERROR_EAGAIN) {
                    if (qh.waitSocket())
                        return nullptr;
                } else {
                    qh.err("libssh2_sftp_open(%s) returned an error", fname.c_str());
                    return nullptr;
                }
            }
        } while (!qh);
    }

    // close file
    // errors can be ignored, because by the time we close, we should already have what we want

    // create buffer for return with the size the server gave us on stat + 1 byte for termination char
    SimpleRefHolder<QoreStringNode> str(new QoreStringNode(encoding));
    str->allocate(fsize + 1);

    QoreSocketThroughputHelper th(socket, false);

    size_t tot = 0;
    while (fsize - tot) {
        while ((rc = libssh2_sftp_read(*qh, (char*)str->getBuffer() + tot, fsize - tot)) == LIBSSH2_ERROR_EAGAIN) {
            if (qh.waitSocket())
                return nullptr;
        }
        if (rc < 0) {
            qh.err("libssh2_sftp_read(" QLLD ") failed: total read: " QLLD " while reading '%s' size " QLLD, fsize - tot, tot, fname.c_str(), fsize);
            return nullptr;
        }
        if (rc)
            tot += rc;
        if (tot >= fsize)
            break;
    }
    str->terminate(tot);

    th.finalize(tot);

    return str.release();
}

int64 SFTPClient::sftpRetrieveFile(const char* remote_file, const char* local_file, int timeout_ms, int mode, ExceptionSink* xsink) {
    AutoLocker al(m);

    // try to make an implicit connection
    if (!sftpConnectedUnlocked() && sftpConnectUnlocked(timeout_ms, xsink))
        return -1;

    std::string fname = absolute_filename(this, remote_file);

    BlockingHelper bh(this);

    QSftpHelper qh(this, "SFTPCLIENT-RETRIEVEFILE-ERROR", "SFTPClient::getFile", timeout_ms, xsink);

    LIBSSH2_SFTP_ATTRIBUTES attrs;
    int rc;
    {
        QoreSocketTimeoutHelper th(socket, "getFile (stat)");

        while ((rc = libssh2_sftp_stat(sftp_session, fname.c_str(), &attrs)) == LIBSSH2_ERROR_EAGAIN) {
            if (qh.waitSocket())
                return -1;
        }
    }
    if (rc < 0) {
        qh.err("libssh2_sftp_stat(%s) returned an error", fname.c_str());
        return -1;
    }
    //printd(5, "SFTPClient::sftpGetFile() permissions: %lo\n", attrs.permissions);
    size_t fsize = attrs.filesize;

    {
        QoreSocketTimeoutHelper th(socket, "getFile (open)");

        // open handle
        do {
            qh.assign(libssh2_sftp_open(sftp_session, fname.c_str(), LIBSSH2_FXF_READ, attrs.permissions));
            if (!qh) {
                if (libssh2_session_last_errno(ssh_session) == LIBSSH2_ERROR_EAGAIN) {
                    if (qh.waitSocket())
                        return -1;
                }
                else {
                    qh.err("libssh2_sftp_open(%s) returned an error", fname.c_str());
                    return -1;
                }
            }
        } while (!qh);
    }

    // check filesystem sandbox access for writing the local file
    {
        QoreSandboxManagerHelper smh;
        if (smh && !smh->checkFilesystemAccess(local_file, QSEC_WRITE, xsink)) {
            return -1;
        }
    }

    // open output file
    QoreFile f;
    if (f.open2(xsink, local_file, O_CREAT|O_WRONLY|O_TRUNC, mode))
        return -1;

    // allocate a buffer for reading
    std::unique_ptr<char[]> buf(new char[QSSH2_BUFSIZE]);

    QoreSocketThroughputHelper th(socket, false);

    size_t tot = 0;

    while (true) {
        size_t bs = fsize - tot;
        if (!bs)
            break;
        if (bs > QSSH2_BUFSIZE)
            bs = QSSH2_BUFSIZE;

        while ((rc = libssh2_sftp_read(*qh, buf.get(), bs)) == LIBSSH2_ERROR_EAGAIN) {
            if (qh.waitSocket()) {
                assert(*xsink);
                return -1;
            }
        }
        if (rc < 0) {
            qh.err("libssh2_sftp_read(" QLLD ") failed: total read: " QLLD " while reading '%s' size " QLLD, fsize - tot, tot, fname.c_str(), fsize);
            assert(*xsink);
            return -1;
        }
        if (rc) {
            tot += rc;
            if (f.write(buf.get(), rc, xsink) < 0) {
                assert(*xsink);
                return -1;
            }
        }
        if (tot >= fsize)
            break;
    }

    th.finalize(tot);

    return tot;
}

int64 SFTPClient::sftpGet(const char* remote_file, OutputStream *os, int timeout_ms, ExceptionSink* xsink) {
    AutoLocker al(m);

    // try to make an implicit connection
    if (!sftpConnectedUnlocked() && sftpConnectUnlocked(timeout_ms, xsink))
        return -1;

    std::string fname = absolute_filename(this, remote_file);

    BlockingHelper bh(this);

    QSftpHelper qh(this, "SFTPCLIENT-GET-ERROR", "SFTPClient::get", timeout_ms, xsink);

    LIBSSH2_SFTP_ATTRIBUTES attrs;
    int rc;
    {
        QoreSocketTimeoutHelper th(socket, "getFile (stat)");

        while ((rc = libssh2_sftp_stat(sftp_session, fname.c_str(), &attrs)) == LIBSSH2_ERROR_EAGAIN) {
            if (qh.waitSocket())
                return -1;
        }
    }
    if (rc < 0) {
        qh.err("libssh2_sftp_stat(%s) returned an error", fname.c_str());
        return -1;
    }
    //printd(5, "SFTPClient::sftpGetFile() permissions: %lo\n", attrs.permissions);
    size_t fsize = attrs.filesize;

    {
        QoreSocketTimeoutHelper th(socket, "getFile (open)");

        // open handle
        do {
            qh.assign(libssh2_sftp_open(sftp_session, fname.c_str(), LIBSSH2_FXF_READ, attrs.permissions));
            if (!qh) {
                if (libssh2_session_last_errno(ssh_session) == LIBSSH2_ERROR_EAGAIN) {
                    if (qh.waitSocket())
                        return -1;
                }
                else {
                    qh.err("libssh2_sftp_open(%s) returned an error", fname.c_str());
                    return -1;
                }
            }
        } while (!qh);
    }

    // allocate a buffer for reading
    std::unique_ptr<char[]> buf(new char[QSSH2_BUFSIZE]);

    QoreSocketThroughputHelper th(socket, false);

    size_t tot = 0;

    while (true) {
        size_t bs = fsize - tot;
        if (!bs)
            break;
        if (bs > QSSH2_BUFSIZE)
            bs = QSSH2_BUFSIZE;

        while ((rc = libssh2_sftp_read(*qh, buf.get(), bs)) == LIBSSH2_ERROR_EAGAIN) {
            if (qh.waitSocket()) {
                assert(*xsink);
                return -1;
            }
        }
        if (rc < 0) {
            qh.err("libssh2_sftp_read(" QLLD ") failed: total read: " QLLD " while reading '%s' size " QLLD, fsize - tot, tot, fname.c_str(), fsize);
            assert(*xsink);
            return -1;
        }
        if (rc) {
            tot += rc;
            {
                AutoUnlocker unlock(m);
                os->write(buf.get(), rc, xsink);
                if (*xsink) {
                return 0;
                }
            }
        }
        if (tot >= fsize)
            break;
    }

    th.finalize(tot);

    return tot;
}

// putFile(binary to put, filename on server, mode of the created file)
size_t SFTPClient::sftpPutFile(const char* outb, size_t towrite, const char* fname, int mode, int timeout_ms, ExceptionSink* xsink) {
    AutoLocker al(m);

    // try to make an implicit connection
    if (!sftpConnectedUnlocked() && sftpConnectUnlocked(timeout_ms, xsink))
        return -1;

    std::string file = absolute_filename(this, fname);

    BlockingHelper bh(this);

    QSftpHelper qh(this, "SFTPCLIENT-PUTFILE-ERROR", "SFTPClient::putFile", timeout_ms, xsink);

    {
        QoreSocketTimeoutHelper th(socket, "putFile (open)");

        // if this works we try to open an sftp handle on the other side
        do {
            qh.assign(libssh2_sftp_open_ex(sftp_session, file.c_str(), file.size(), LIBSSH2_FXF_WRITE|LIBSSH2_FXF_CREAT|LIBSSH2_FXF_TRUNC, mode, LIBSSH2_SFTP_OPENFILE));
            if (!qh) {
                if (libssh2_session_last_errno(ssh_session) == LIBSSH2_ERROR_EAGAIN) {
                    if (qh.waitSocket())
                        return -1;
                }
                else {
                    qh.err("libssh2_sftp_open_ex(%s) returned an error", file.c_str());
                    return -1;
                }
            }
        } while (!qh);
    }

    QoreSocketThroughputHelper th(socket, true);

    size_t size = 0;
    while (size < towrite) {
        ssize_t rc;
        while ((rc = libssh2_sftp_write(*qh, outb + size, towrite - size)) == LIBSSH2_ERROR_EAGAIN) {
            if (qh.waitSocket()) {
                // note: memory leak here! we cannot close the handle due to the timeout
                return -1;
            }
        }
        if (rc < 0) {
            qh.err("libssh2_sftp_write(" QLLD " failed while writing '%s', total written: " QLLD ", total to write: " QLLD, towrite - size, file.c_str(), size, towrite);
            return -1;
        }
        size += rc;
    }
    assert(size == towrite);

    th.finalize(size);

    int rc = qh.close();
    if (rc && rc != LIBSSH2_ERROR_EAGAIN) {
        qh.err("libssh2_sftp_close_handle() returned an error while closing '%s'", file.c_str());
        return -1;
    }

    return (int64)size; // the bytes actually written
}

// transferFile(local path, filename on server, mode of the created file)
int64 SFTPClient::sftpTransferFile(const char* local_path, const char* remote_path, int mode, int timeout_ms, ExceptionSink* xsink) {
    // check filesystem sandbox access for reading the local file
    {
        QoreSandboxManagerHelper smh;
        if (smh && !smh->checkFilesystemAccess(local_path, QSEC_READ, xsink)) {
            return -1;
        }
    }

    // open local file
    QoreFile f;
    if (f.open2(xsink, local_path))
        return -1;

    struct stat sbuf;
    if (fstat(f.getFD(), &sbuf)) {
        xsink->raiseErrnoException("FILE-STAT-ERROR", errno, "%s: fstat() call failed", local_path);
        return -1;
    }

    if (!mode)
        mode = sbuf.st_mode;

    size_t towrite = sbuf.st_size;

    AutoLocker al(m);

    // try to make an implicit connection
    if (!sftpConnectedUnlocked() && sftpConnectUnlocked(timeout_ms, xsink))
        return -1;

    std::string file = absolute_filename(this, remote_path);

    BlockingHelper bh(this);

    QSftpHelper qh(this, "SFTPCLIENT-TRANSFERFILE-ERROR", "SFTPClient::transferFile", timeout_ms, xsink);

    {
        QoreSocketTimeoutHelper th(socket, "transferFile (open)");

        // if this works we try to open an sftp handle on the other side
        do {
            qh.assign(libssh2_sftp_open_ex(sftp_session, file.c_str(), file.size(), LIBSSH2_FXF_WRITE|LIBSSH2_FXF_CREAT|LIBSSH2_FXF_TRUNC, mode, LIBSSH2_SFTP_OPENFILE));
            if (!qh) {
                if (libssh2_session_last_errno(ssh_session) == LIBSSH2_ERROR_EAGAIN) {
                    if (qh.waitSocket())
                        return -1;
                } else {
                    qh.err("libssh2_sftp_open_ex(%s) returned an error", file.c_str());
                    return -1;
                }
            }
        } while (!qh);
    }

    SimpleRefHolder<BinaryNode> buf(new BinaryNode);
    // issue #2982: do not preallocate the binary buffer here; it will be managed in the QoreFile::readbinary() call below

    QoreSocketThroughputHelper th(socket, true);

    size_t size = 0;
    while (size < towrite) {
        size_t bs = towrite - size;
        if (bs > QSSH2_BUFSIZE)
            bs = QSSH2_BUFSIZE;

        if (f.readBinary(**buf, bs, xsink)) {
            return -1;
        }

        assert(buf->size());

        // issue #2633: if libssh2_sftp_write() returns less than the buffer size, then we have to keep sending that data
        // and cannot change the transfer buffer: https://www.libssh2.org/libssh2_sftp_write.html
        ssize_t total = 0;
        ssize_t rc;
        while (true) {
            while ((rc = libssh2_sftp_write(*qh, (const char*)buf->getPtr() + total, buf->size() - total)) == LIBSSH2_ERROR_EAGAIN) {
                if (qh.waitSocket()) {
                    // note: memory leak here! we cannot close the handle due to the timeout
                    return -1;
                }
            }
            if (rc < 0) {
                qh.err("libssh2_sftp_write(" QLLD ") failed while writing '%s', total written: " QLLD ", total to write: " QLLD, towrite - size, file.c_str(), size, towrite);
                return -1;
            }
            assert((size_t)rc <= (buf->size() - total));
            total += rc;
            assert((size_t)total <= buf->size());
            if ((size_t)total == buf->size())
                break;
        }
        size += total;
        buf->setSize(0);
    }

    th.finalize(size);

    int rc = qh.close();
    if (rc && rc != LIBSSH2_ERROR_EAGAIN) {
        qh.err("libssh2_sftp_close_handle() returned an error while closing '%s'", file.c_str());
        return -1;
    }

    return size; // the bytes actually written
}

int64 SFTPClient::sftpPut(InputStream *is, const char* remote_path, int mode, int timeout_ms, ExceptionSink* xsink) {
    AutoLocker al(m);

    // try to make an implicit connection
    if (!sftpConnectedUnlocked() && sftpConnectUnlocked(timeout_ms, xsink))
        return -1;

    std::string file = absolute_filename(this, remote_path);

    BlockingHelper bh(this);

    QSftpHelper qh(this, "SFTPCLIENT-PUT-ERROR", "SFTPClient::put", timeout_ms, xsink);

    {
        QoreSocketTimeoutHelper th(socket, "putFile (open)");

        // if this works we try to open an sftp handle on the other side
        do {
            qh.assign(libssh2_sftp_open_ex(sftp_session, file.c_str(), file.size(), LIBSSH2_FXF_WRITE|LIBSSH2_FXF_CREAT|LIBSSH2_FXF_TRUNC, mode, LIBSSH2_SFTP_OPENFILE));
            if (!qh) {
                if (libssh2_session_last_errno(ssh_session) == LIBSSH2_ERROR_EAGAIN) {
                    if (qh.waitSocket())
                        return -1;
                }
                else {
                    qh.err("libssh2_sftp_open_ex(%s) returned an error", file.c_str());
                    return -1;
                }
            }
        } while (!qh);
    }

    QoreSocketThroughputHelper th(socket, true);

    size_t size = 0;

    // allocate a buffer for reading
    std::unique_ptr<char[]> buf(new char[QSSH2_BUFSIZE]);

    while (true) {
        int64 r;
        {
            AutoUnlocker unlocker(m);
            r = is->read(buf.get(), QSSH2_BUFSIZE, xsink);
            if (*xsink) {
                return -1;
            }
        }
        if (!r) {
            break;
        }

        // issue #2633: if libssh2_sftp_write() returns less than the buffer size, then we have to keep sending that data
        // and cannot change the transfer buffer: https://www.libssh2.org/libssh2_sftp_write.html
        ssize_t total = 0;
        while (true) {
            ssize_t rc;
            while ((rc = libssh2_sftp_write(*qh, buf.get() + total, r - total)) == LIBSSH2_ERROR_EAGAIN) {
                if (qh.waitSocket()) {
                    // note: memory leak here! we cannot close the handle due to the timeout
                    return -1;
                }
            }
            if (rc < 0) {
                qh.err("libssh2_sftp_write(" QLLD ") failed while writing '%s', total written: " QLLD, r, file.c_str(), size);
                return -1;
            }
            total += rc;
            size += rc;
            if (total == r)
                break;
        }
    }

    th.finalize(size);

    int rc = qh.close();
    if (rc && rc != LIBSSH2_ERROR_EAGAIN) {
        qh.err("libssh2_sftp_close_handle() returned an error while closing '%s'", file.c_str());
        return -1;
    }

    return size; // the bytes actually written
}

//LIBSSH2_SFTP_ATTRIBUTES
// return:
//  0   ok
// -1   generic error on libssh2 call
// -2   no such file
// -3   not connected to server
// gives the attrs in attrs argument back
int SFTPClient::sftpGetAttributes(const char* fname, LIBSSH2_SFTP_ATTRIBUTES *attrs, int timeout_ms, ExceptionSink* xsink) {
    assert(fname);

    AutoLocker al(m);

    // try to make an implicit connection
    if (!sftpConnectedUnlocked() && sftpConnectUnlocked(timeout_ms, xsink))
        return -3;

    QSftpHelper qh(this, "SFTPCLIENT-STAT-ERROR", "SFTPClient::stat", timeout_ms, xsink);

    if (!fname || !fname[0]) {
        qh.err("no file given");
        return -2;
    }

    std::string file = absolute_filename(this, fname);

    BlockingHelper bh(this);

    // stat the file
    int rc;
    {
        QoreSocketTimeoutHelper th(socket, "getAttributes (stat)");

        while ((rc = libssh2_sftp_stat(sftp_session, file.c_str(), attrs)) == LIBSSH2_ERROR_EAGAIN) {
            if (qh.waitSocket())
                return -3;
        }
    }

    if (rc < 0) {
        // check if the file does not exist
        if (libssh2_session_last_errno(ssh_session) == LIBSSH2_ERROR_SFTP_PROTOCOL
            && libssh2_sftp_last_error(sftp_session) == LIBSSH2_FX_NO_SUCH_FILE)
            return -2;

        qh.err("libssh2_sftp_stat(%s) returned an error", file.c_str());
        return -1;
    }

    return 0;
}

int SFTPClient::sftpLGetAttributes(const char* fname, LIBSSH2_SFTP_ATTRIBUTES* attrs, int timeout_ms, ExceptionSink* xsink) {
    assert(fname);

    AutoLocker al(m);

    // try to make an implicit connection
    if (!sftpConnectedUnlocked() && sftpConnectUnlocked(timeout_ms, xsink)) {
        return -3;
    }

    QSftpHelper qh(this, "SFTPCLIENT-LSTAT-ERROR", "SFTPClient::lstat", timeout_ms, xsink);

    if (!fname || !fname[0]) {
        qh.err("no file given");
        return -2;
    }

    std::string file = absolute_filename(this, fname);

    BlockingHelper bh(this);

    // lstat the file (do not follow symlinks)
    int rc;
    {
        QoreSocketTimeoutHelper th(socket, "lstat");

        while ((rc = libssh2_sftp_stat_ex(sftp_session, file.c_str(), file.size(), LIBSSH2_SFTP_LSTAT, attrs)) == LIBSSH2_ERROR_EAGAIN) {
            if (qh.waitSocket()) {
                return -3;
            }
        }
    }

    if (rc < 0) {
        // check if the file does not exist
        if (libssh2_session_last_errno(ssh_session) == LIBSSH2_ERROR_SFTP_PROTOCOL
            && libssh2_sftp_last_error(sftp_session) == LIBSSH2_FX_NO_SUCH_FILE) {
            return -2;
        }

        qh.err("libssh2_sftp_lstat(%s) returned an error", file.c_str());
        return -1;
    }

    return 0;
}

int SFTPClient::sftpSymlink(const char* target, const char* link_path, int timeout_ms, ExceptionSink* xsink) {
    assert(target && link_path);

    AutoLocker al(m);

    QSftpHelper qh(this, "SFTPCLIENT-SYMLINK-ERROR", "SFTPClient::symlink", timeout_ms, xsink);

    if (!target || !target[0]) {
        qh.err("target argument is empty");
        return -3;
    }
    if (!link_path || !link_path[0]) {
        qh.err("link_path argument is empty");
        return -3;
    }

    // try to make an implicit connection
    if (!sftpConnectedUnlocked() && sftpConnectUnlocked(timeout_ms, xsink)) {
        return -2;
    }

    std::string tstr = absolute_filename(this, target);
    std::string lstr = absolute_filename(this, link_path);

    BlockingHelper bh(this);

    int rc;
    {
        QoreSocketTimeoutHelper th(socket, "symlink");

        while ((rc = libssh2_sftp_symlink_ex(sftp_session, tstr.c_str(), tstr.size(),
                const_cast<char*>(lstr.c_str()), lstr.size(), LIBSSH2_SFTP_SYMLINK)) == LIBSSH2_ERROR_EAGAIN) {
            if (qh.waitSocket()) {
                return -3;
            }
        }
    }

    if (rc < 0) {
        qh.err("libssh2_sftp_symlink(%s, %s) returned an error", tstr.c_str(), lstr.c_str());
    }

    return rc;
}

QoreStringNode* SFTPClient::sftpReadlink(const char* path, int timeout_ms, ExceptionSink* xsink) {
    assert(path);

    AutoLocker al(m);

    QSftpHelper qh(this, "SFTPCLIENT-READLINK-ERROR", "SFTPClient::readlink", timeout_ms, xsink);

    if (!path || !path[0]) {
        qh.err("path argument is empty");
        return nullptr;
    }

    // try to make an implicit connection
    if (!sftpConnectedUnlocked() && sftpConnectUnlocked(timeout_ms, xsink)) {
        return nullptr;
    }

    std::string pstr = absolute_filename(this, path);

    BlockingHelper bh(this);

    char buff[PATH_MAX];
    int rc;
    {
        QoreSocketTimeoutHelper th(socket, "readlink");

        while ((rc = libssh2_sftp_symlink_ex(sftp_session, pstr.c_str(), pstr.size(),
                buff, sizeof(buff) - 1, LIBSSH2_SFTP_READLINK)) == LIBSSH2_ERROR_EAGAIN) {
            if (qh.waitSocket()) {
                return nullptr;
            }
        }
    }

    if (rc < 0) {
        qh.err("libssh2_sftp_readlink(%s) returned an error", pstr.c_str());
        return nullptr;
    }

    buff[rc] = '\0';
    return new QoreStringNode(buff);
}

int SFTPClient::sftpChown(const char* file, int uid, int gid, int timeout_ms, ExceptionSink* xsink) {
    assert(file);

    AutoLocker al(m);

    QSftpHelper qh(this, "SFTPCLIENT-CHOWN-ERROR", "SFTPClient::chown", timeout_ms, xsink);

    if (!file || !file[0]) {
        qh.err("file argument is empty");
        return -3;
    }

    // try to make an implicit connection
    if (!sftpConnectedUnlocked() && sftpConnectUnlocked(timeout_ms, xsink)) {
        return -2;
    }

    std::string pstr = absolute_filename(this, file);

    BlockingHelper bh(this);

    // get current attrs
    LIBSSH2_SFTP_ATTRIBUTES attrs;
    int rc;
    {
        QoreSocketTimeoutHelper th(socket, "chown (stat)");

        while ((rc = libssh2_sftp_stat(sftp_session, pstr.c_str(), &attrs)) == LIBSSH2_ERROR_EAGAIN) {
            if (qh.waitSocket()) {
                return -3;
            }
        }
    }

    if (rc < 0) {
        qh.err("libssh2_sftp_stat(%s) returned an error", pstr.c_str());
        return rc;
    }

    // set uid/gid
    attrs.flags = LIBSSH2_SFTP_ATTR_UIDGID;
    attrs.uid = uid;
    attrs.gid = gid;

    {
        QoreSocketTimeoutHelper th(socket, "chown (setstat)");

        while ((rc = libssh2_sftp_setstat(sftp_session, pstr.c_str(), &attrs)) == LIBSSH2_ERROR_EAGAIN) {
            if (qh.waitSocket()) {
                return -3;
            }
        }
    }

    if (rc < 0) {
        qh.err("libssh2_sftp_setstat(%s) returned an error while changing ownership", pstr.c_str());
    }

    return rc;
}

#ifdef HAVE_LIBSSH2_SFTP_STATVFS
QoreHashNode* SFTPClient::sftpStatvfs(const char* path, int timeout_ms, ExceptionSink* xsink) {
    assert(path);

    AutoLocker al(m);

    QSftpHelper qh(this, "SFTPCLIENT-STATVFS-ERROR", "SFTPClient::statvfs", timeout_ms, xsink);

    if (!path || !path[0]) {
        qh.err("path argument is empty");
        return nullptr;
    }

    // try to make an implicit connection
    if (!sftpConnectedUnlocked() && sftpConnectUnlocked(timeout_ms, xsink)) {
        return nullptr;
    }

    std::string pstr = absolute_filename(this, path);

    BlockingHelper bh(this);

    LIBSSH2_SFTP_STATVFS st;
    int rc;
    {
        QoreSocketTimeoutHelper th(socket, "statvfs");

        while ((rc = libssh2_sftp_statvfs(sftp_session, pstr.c_str(), pstr.size(), &st)) == LIBSSH2_ERROR_EAGAIN) {
            if (qh.waitSocket()) {
                return nullptr;
            }
        }
    }

    if (rc < 0) {
        qh.err("libssh2_sftp_statvfs(%s) returned an error", pstr.c_str());
        return nullptr;
    }

    ReferenceHolder<QoreHashNode> ret(new QoreHashNode(hashdeclSftpStatVfsInfo, xsink), xsink);
    ret->setKeyValue("f_bsize", (int64)st.f_bsize, xsink);
    ret->setKeyValue("f_frsize", (int64)st.f_frsize, xsink);
    ret->setKeyValue("f_blocks", (int64)st.f_blocks, xsink);
    ret->setKeyValue("f_bfree", (int64)st.f_bfree, xsink);
    ret->setKeyValue("f_bavail", (int64)st.f_bavail, xsink);
    ret->setKeyValue("f_files", (int64)st.f_files, xsink);
    ret->setKeyValue("f_ffree", (int64)st.f_ffree, xsink);
    ret->setKeyValue("f_favail", (int64)st.f_favail, xsink);
    ret->setKeyValue("f_fsid", (int64)st.f_fsid, xsink);
    ret->setKeyValue("f_flag", (int64)st.f_flag, xsink);
    ret->setKeyValue("f_namemax", (int64)st.f_namemax, xsink);

    return ret.release();
}
#endif

void SFTPClient::doSessionErrUnlocked(ExceptionSink* xsink, QoreStringNode* desc) {
    if (ssh_session) {
        int err = libssh2_session_last_errno(ssh_session);
        if (err == LIBSSH2_ERROR_SFTP_PROTOCOL) {
            unsigned long serr = libssh2_sftp_last_error(sftp_session);

            desc->sprintf(": sftp error code %lu", serr);

            edmap_t::const_iterator i = sftp_emap.find((int)serr);
            if (i != sftp_emap.end())
                desc->sprintf(" (%s): %s", i->second.err, i->second.desc);
            else
                desc->concat(": unknown sftp error code");
        }
        else
            desc->sprintf(": ssh2 error %d: %s", err, getSessionErrUnlocked());
    }

    xsink->raiseException(SSH2_ERROR, desc);

    // check if we're still connected: if there is data to be read, we assume it's the EOF marker and close the session
    int rc = waitSocketUnlocked(LIBSSH2_SESSION_BLOCK_INBOUND, 0);

    if (rc > 0) {
        printd(5, "doSessionErrUnlocked() session %p: detected disconnected session, marking as closed\n", ssh_session);
        disconnectUnlocked(true, 10, 0, xsink);
    }
}

QoreHashNode* SFTPClient::sftpInfo(ExceptionSink* xsink) {
    AutoLocker al(m);
    ReferenceHolder<QoreHashNode> h(sshInfoIntern(hashdeclSftpConnectionInfo, xsink), xsink);
    h->setKeyValue("path", sftppath.empty() ? QoreValue() : new QoreStringNode(sftppath), xsink);
    return h.release();
}

void QSftpHelper::err(const char* fmt, ...) {
    tryClose();

    va_list args;
    QoreStringNode* desc = new QoreStringNode;

    while (true) {
        va_start(args, fmt);
        int rc = desc->vsprintf(fmt, args);
        va_end(args);
        if (!rc)
            break;
    }

    client->doSessionErrUnlocked(xsink, desc);
}

int QSftpHelper::closeIntern() {
    assert(sftp_handle);

    QoreSocketTimeoutHelper th(client->socket, meth);

    // close the handle
    int rc;
    while ((rc = libssh2_sftp_close_handle(sftp_handle)) == LIBSSH2_ERROR_EAGAIN) {
        if (client->waitSocketUnlocked(xsink, SFTPCLIENT_TIMEOUT, errstr, meth, timeout_ms, true)) {
            // note: memory leak here! we cannot close the handle due to the timeout
            printd(0, "QSftpHelper::closeIntern() session %p: cannot close remote file descriptor, forcing session " \
                "disconnect; leaking descriptor\n", client->ssh_session);
            client->disconnectUnlocked(true, 10, 0, xsink);
            break;
        }
    }
    sftp_handle = 0;

    return rc;
}

int QSftpHelper::waitSocket() {
    if (client->waitSocketUnlocked(xsink, SFTPCLIENT_TIMEOUT, errstr, meth, timeout_ms, false, this))
        return -1;
    return 0;
}
