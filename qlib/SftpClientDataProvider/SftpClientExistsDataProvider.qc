# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore SftpClientDataProvider module definition

/** SftpClientExistsDataProvider.qc Copyright 2023 - 2024 Qore Technologies, s.r.o.

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
#! The SFTP client exists data provider class
public class SftpClientExistsDataProvider inherits AbstractDataProvider {
    public {
        #! SFTP connection
        SFTPClient sftp;

        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "exists",
            "desc": "SFTP exists data provider; checks if a file or directory exists on the server",
            "type": "SftpClientExistsDataProvider",
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
        const ResponseType = new SftpClientExistsResponseDataType();
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
        return "exists";
    }

    #! Makes a request and returns the response
    /** @param req the request info
        @param request_options the request options; will be processed by validateRequestOptions()

        @return the response to the request with exists=True/False and type information
    */
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        string path = SftpClientDataProvider::getPath(sftp, req.path);
        *hash<auto> info = sftp.stat(path);
        if (!info) {
            return {
                "path": path,
                "exists": False,
            };
        }
        # Determine type from mode
        string type = "unknown";
        int mode = info.mode;
        if ((mode & 0170000) == 0040000) {
            type = "directory";
        } else if ((mode & 0170000) == 0100000) {
            type = "file";
        } else if ((mode & 0170000) == 0120000) {
            type = "symlink";
        }
        return {
            "path": path,
            "exists": True,
            "type": type,
            "size": info.size,
        };
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

#! Data type for SFTP client exists response
public class SftpClientExistsResponseDataType inherits HashDataType {
    public {
        #! Field descriptions
        const Fields = {
            "path": {
                "display_name": "Path",
                "short_desc": "The path that was checked",
                "desc": "The remote path on the SFTP server that was checked",
                "type": AbstractDataProviderTypeMap."string",
                "example_value": "/home/user/file.txt",
            },
            "exists": {
                "display_name": "Exists",
                "short_desc": "Whether the path exists",
                "desc": "`True` if the file or directory exists, `False` otherwise",
                "type": AbstractDataProviderTypeMap."bool",
                "example_value": True,
            },
            "type": {
                "display_name": "Type",
                "short_desc": "The type of the path",
                "desc": "The type of the path: `file`, `directory`, `symlink`, or `unknown`. "
                    "Only present if `exists=True`",
                "type": AbstractDataProviderTypeMap."*string",
                "example_value": "file",
            },
            "size": {
                "display_name": "Size",
                "short_desc": "The size in bytes",
                "desc": "The size of the file in bytes. Only present if `exists=True`",
                "type": AbstractDataProviderTypeMap."*int",
                "example_value": 1024,
            },
        };
    }

    #! Creates the object
    constructor() : HashDataType("SftpClientExistsResponse") {
        addQoreFields(Fields);
    }
}
}
