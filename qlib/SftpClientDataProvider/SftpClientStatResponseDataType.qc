# -*- mode: qore; indent-tabs-mode: nil -*-

/** SftpClientStatResponseDataType.qc Copyright 2023 - 2024 Qore Technologies, s.r.o.

    Permission is hereby granted, free of charge, to any person obtaining a
    copy of this software and associated documentation files (the "Software"),
    to deal in the Software without restriction, including without limitation
    the rights to use, copy, modify, merge, publish, distribute, sublicense,
    and/or sell copies of the Software, and to permit persons to whom the
    Software is furnished to do so, subject to the following conditions:

    The above copyright notice and this permission notice shall be included in
    all copies or substantial portions of the Software.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
    IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
    FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
    AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
    LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
    FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
    DEALINGS IN THE SOFTWARE.
*/

#! contains all public definitions in the SftpClientDataProvider module
public namespace SftpClientDataProvider {
#! Data type for SFTP client stat response calls
public class SftpClientStatResponseDataType inherits HashDataType {
    public {
        #! Markdown descriptions for hashdecl members
        const Fields = {
            "mode": {
                "display_name": "Mode",
                "short_desc": "The file protection mode",
                "desc": "The file protection mode; a bitfield of file permissions",
                "example_value": 33261,
            },
            "permissions": {
                "display_name": "Permissions",
                "short_desc": "A string giving UNIX-style permissions for the file",
                "desc": "A string giving UNIX-style permissions for the file (ex: `-rwxr-xr-x`)",
                "example_value": "-rwxr-xr-x",
            },
            "size": {
                "display_name": "Size",
                "short_desc": "The size of the file in bytes",
                "desc": "The size of the file in bytes",
                "example_value": 903,
            },
            "atime": {
                "display_name": "Access Timestamp",
                "short_desc": "The last accessed date/time of the file",
                "desc": "The last accessed date/time of the file",
                "example_value": now_us(),
            },
            "mtime": {
                "display_name": "Modified Timestamp",
                "short_desc": "The last modified date/time of the file",
                "desc": "The last modified date/time of the file",
                "example_value": now_us(),
            },
            "uid": {
                "display_name": "User ID",
                "short_desc": "The UID of the owner of the file",
                "desc": "The UID of the owner of the file",
                "example_value": 1000,
            },
            "gid": {
                "display_name": "Group ID",
                "short_desc": "The GID of the owner of the file",
                "desc": "The GID of the owner of the file",
                "example_value": 20,
            },
        };
    }

    #! Creates the type
    constructor() {
        addQoreFields(map {$1.getName(): Fields{$1.getName()} + {
            "type": $1.getType(),
        }}, TypedHash::forName("SSH2::Ssh2StatInfo").getMembers());
    }
}
}
