# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore SftpClientDataProvider module definition

/** SftpClientCreateFileLocationDataProvider.qc Copyright 2023 - 2024 Qore Technologies, s.r.o.

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
#! The SFTP client create file data provider class
public class SftpClientCreateFileLocationDataProvider inherits AbstractDataProvider {
    public {
        #! SFTP connection
        SFTPClient sftp;

        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "create-file-location",
            "desc": "SFTP create file data provider; creates a file in the given location",
            "type": "SftpClientCreateFileLocationDataProvider",
            "constructor_options": SftpClientDataProvider::ConstructorOptions,
            "supports_request": True,
        };

        #! Provider summary info
        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys
        });

        #! Request type
        const RequestType = new SftpClientCreateFileLocationRequestDataType();

        #! Response type
        const ResponseType = SftpClientPathDataType;
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
        return "create-file-location";
    }

    #! Makes a request and returns the response
    /** @param req the request info
        @param request_options the request options after processing by validateRequestOptions()

        @return the response to the request
    */
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        string path = SftpClientDataProvider::getPath(sftp, req.path);
        InputStream stream = FileLocationHandler::getBinaryStreamFromLocation(req.location);
        sftp.put(stream, path);
        return {
            "path": path,
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

public class SftpClientCreateFileLocationRequestDataType inherits HashDataType {
    public {
        #! Field descriptions
        const Fields = SftpClientCreateFileRequestDataType::Fields - "data" + {
            "location": {
                "display_name": "Location",
                "short_desc": "Source location for the file data",
                "desc": "Source location for the file data",
                "type": AbstractDataProviderTypeMap."string",
                "example_value": "file:///tmp/my-file-location.txt",
            },
        };
    }

    constructor() : HashDataType("SftpClientCreateFileLocationRequest") {
        addQoreFields(Fields);
    }
}
}
