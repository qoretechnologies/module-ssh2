# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore SftpClientDataProvider module definition

/** SftpClientStatvfsDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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
#! The SFTP client statvfs data provider class
public class SftpClientStatvfsDataProvider inherits AbstractDataProvider {
    public {
        #! SFTP connection
        SFTPClient sftp;

        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "statvfs",
            "desc": "SFTP statvfs data provider; returns filesystem statistics for a remote path",
            "type": "SftpClientStatvfsDataProvider",
            "constructor_options": SftpClientDataProvider::ConstructorOptions,
            "supports_request": True,
        };

        #! Provider summary info
        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys
        });

        #! Request type
        const RequestType = SftpClientPathDataType;

        #! Response type
        const ResponseType = new SftpClientStatvfsResponseDataType();
    }

    #! Creates the object from constructor options
    constructor(*hash<auto> options) {
        checkOptions("CONSTRUCTOR-ERROR", SftpClientDataProvider::ConstructorOptions, options);
        sftp = SftpClientDataProvider::getSftpClientConnection(options);
    }

    #! Creates the object from an SFTP connection
    constructor(SFTPClient sftp) {
        self.sftp = sftp;
    }

    #! Returns the data provider name
    string getName() {
        return "statvfs";
    }

    #! Makes a request and returns the response
    /** @param req the request info
        @param request_options the request options; will be processed by validateRequestOptions()

        @return the response to the request
    */
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        string path = SftpClientDataProvider::getPath(sftp, req.path);
        return sftp.statvfs(path);
    }

    #! Returns the description of a successful request message, if any
    /** @return the request type for this provider
    */
    private *AbstractDataProviderType getRequestTypeImpl() {
        return RequestType;
    }

    #! Returns the description of a response message, if this object represents a response message
    /** @return the response type for this response message
    */
    private *AbstractDataProviderType getResponseTypeImpl() {
        return ResponseType;
    }

    #! Returns data provider static info
    hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

#! Data type for SFTP client statvfs response
public class SftpClientStatvfsResponseDataType inherits HashDataType {
    public {
        #! Field descriptions
        const Fields = {
            "f_bsize": {
                "display_name": "Block Size",
                "short_desc": "Filesystem block size",
                "desc": "The filesystem block size in bytes",
                "type": AbstractDataProviderTypeMap."int",
            },
            "f_frsize": {
                "display_name": "Fragment Size",
                "short_desc": "Fragment size",
                "desc": "The fragment size in bytes",
                "type": AbstractDataProviderTypeMap."int",
            },
            "f_blocks": {
                "display_name": "Total Blocks",
                "short_desc": "Total data blocks in filesystem",
                "desc": "The total number of data blocks in the filesystem",
                "type": AbstractDataProviderTypeMap."int",
            },
            "f_bfree": {
                "display_name": "Free Blocks",
                "short_desc": "Free blocks in filesystem",
                "desc": "The number of free blocks in the filesystem",
                "type": AbstractDataProviderTypeMap."int",
            },
            "f_bavail": {
                "display_name": "Available Blocks",
                "short_desc": "Free blocks available to non-root",
                "desc": "The number of free blocks available to non-privileged users",
                "type": AbstractDataProviderTypeMap."int",
            },
            "f_files": {
                "display_name": "Total Inodes",
                "short_desc": "Total file nodes in filesystem",
                "desc": "The total number of file nodes (inodes) in the filesystem",
                "type": AbstractDataProviderTypeMap."int",
            },
            "f_ffree": {
                "display_name": "Free Inodes",
                "short_desc": "Free file nodes in filesystem",
                "desc": "The number of free file nodes in the filesystem",
                "type": AbstractDataProviderTypeMap."int",
            },
            "f_favail": {
                "display_name": "Available Inodes",
                "short_desc": "Free file nodes available to non-root",
                "desc": "The number of free file nodes available to non-privileged users",
                "type": AbstractDataProviderTypeMap."int",
            },
            "f_fsid": {
                "display_name": "Filesystem ID",
                "short_desc": "Filesystem ID",
                "desc": "The filesystem ID",
                "type": AbstractDataProviderTypeMap."int",
            },
            "f_flag": {
                "display_name": "Mount Flags",
                "short_desc": "Mount flags",
                "desc": "The mount flags for the filesystem",
                "type": AbstractDataProviderTypeMap."int",
            },
            "f_namemax": {
                "display_name": "Max Filename Length",
                "short_desc": "Maximum filename length",
                "desc": "The maximum filename length supported by the filesystem",
                "type": AbstractDataProviderTypeMap."int",
            },
        };
    }

    #! Creates the object
    constructor() : HashDataType("SftpClientStatvfsResponse") {
        addQoreFields(Fields);
    }
}
}
