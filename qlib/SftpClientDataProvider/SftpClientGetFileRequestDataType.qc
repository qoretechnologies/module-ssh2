# -*- mode: qore; indent-tabs-mode: nil -*-

/** SftpClientGetFileRequestDataType.qc Copyright 2023 - 2024 Qore Technologies, s.r.o.

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
#! Data type for SFTP client get file request calls
public class SftpClientGetFileRequestDataType inherits HashDataType {
    public {
        #! Field descriptions
        const Fields = {
            "path": {
                "display_name": "Path",
                "short_desc": "The path on the SFTP server",
                "desc": "The remote path on the SFTP server to the file to retrieve",
                "type": AbstractDataProviderTypeMap."string",
                "required": True,
                "example_value": "/home/user/file.txt",
            },
            "text": {
                "display_name": "Text Mode",
                "short_desc": "Return file as text string",
                "desc": "If `True` the file will be returned as a string, `False` (the default) as binary data",
                "type": AbstractDataProviderTypeMap."bool",
                "default_value": False,
            },
            "encoding": {
                "display_name": "Encoding",
                "short_desc": "Text encoding for text mode",
                "desc": "The string encoding to use if `text` = `True` (default: `UTF-8`)",
                "type": AbstractDataProviderTypeMap."string",
                "default_value": "UTF-8",
            },
        };
    }

    #! Creates the object
    constructor() : HashDataType("SftpClientGetFileRequest") {
        addQoreFields(Fields);
    }
}
}
