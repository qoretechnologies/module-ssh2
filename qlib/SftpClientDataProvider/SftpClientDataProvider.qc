# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore SftpClientDataProvider class definition

/** SftpClientDataProvider.qc Copyright 2023 - 2024 Qore Technologies, s.r.o.

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
#! Generic SFTP server app name
public const AppName = "SFTP";

#! The SFTP client data provider class, provides API classes as children
public class SftpClientDataProvider inherits AbstractDataProvider {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "type": "SftpClientDataProvider",
            "constructor_options": ConstructorOptions,
            "supports_children": True,
            "children_can_support_apis": True,
            "children_can_support_records": False,
            "children_can_support_observers": False,
        };

        #! Constructor arguments
        const ConstructorOptions = {
            "timeout": <DataProviderOptionInfo>{
                "display_name": "I/O Timeout",
                "short_desc": "The maximum time an individual I/O operation can take in milliseconds",
                "type": AbstractDataProviderTypeMap."int",
                "desc": "Transfer timeout to use in milliseconds",
            },
            "url": <DataProviderOptionInfo>{
                "display_name": "URL",
                "short_desc": "The URL to connect to",
                "type": AbstractDataProviderTypeMap."string",
                "desc": "A URL for an SFTP connection; must use an `sftp://` scheme",
                "required": True,
            },
            "keyfile": <DataProviderOptionInfo>{
                "display_name": "SSH Private Key",
                "short_desc": "A path to an SSH private key file in PEM format",
                "type": AbstractDataProviderTypeMap."string",
                "desc": "A path to an SSH private key file in PEM format to use for key-based authentication",
            },
        };

        SFTPClient sftp;
    }

    private {
        const ChildMap = {
            "create-file": Class::forName("SftpClientDataProvider::SftpClientCreateFileDataProvider"),
            "create-file-location": Class::forName("SftpClientDataProvider::SftpClientCreateFileLocationDataProvider"),
            "delete": Class::forName("SftpClientDataProvider::SftpClientDeleteDataProvider"),
            "get-file": Class::forName("SftpClientDataProvider::SftpClientGetFileDataProvider"),
            "get-file-location": Class::forName("SftpClientDataProvider::SftpClientGetFileLocationDataProvider"),
            "list": Class::forName("SftpClientDataProvider::SftpClientListDataProvider"),
            "mkdir": Class::forName("SftpClientDataProvider::SftpClientMkdirDataProvider"),
            "move": Class::forName("SftpClientDataProvider::SftpClientMoveDataProvider"),
            "stat": Class::forName("SftpClientDataProvider::SftpClientStatDataProvider"),
        };
    }

    #! Creates the object from constructor options
    constructor(*hash<auto> options) {
        checkOptions("CONSTRUCTOR-ERROR", ConstructorOptions, options);
        sftp = getSftpClientConnection(options);
    }

    #! Creates the object from an SFTP connection
    constructor(SFTPClient sftp) {
        self.sftp = sftp;
    }

    #! Returns the data provider name
    string getName() {
        return "sftp";
    }

    #! Returns the data provider description
    *string getDesc() {
        return "Data provider for SFTP servers";
    }

    #! Return data provider summary info
    *list<hash<DataProviderSummaryInfo>> getChildProviderSummaryInfo() {
        return map $1.getConstant("ProviderSummaryInfo").getValue(), ChildMap.iterator();
    }

    #! Returns an SFTP connection from constructor options
    static SFTPClient getSftpClientConnection(hash<auto> options) {
        SFTPClient sftp(options.url);
        if (options."timeout") {
            sftp.setTimeout(options."timeout");
        }
        if (options.keyfile.val()) {
            sftp.setKeys(substitute_env_vars(options.keyfile));
        }
        return sftp;
    }

    #! Returns the path based on the SftpClient's base path
    static string getPath(SFTPClient sftp, string path1) {
        path1 =~ s/^\/+//;
        return sftp.info().path + "/" + path1;
    }

    #! Returns a list of child data provider names, if any
    /** @return a list of child data provider names, if any
    */
    private *list<string> getChildProviderNamesImpl() {
        return keys ChildMap;
    }

    #! Returns the given child provider or @ref nothing if the given child is unknown
    /** @return the given child provider or @ref nothing if the given child is unknown

        @see getChildProviderEx()
    */
    private *AbstractDataProvider getChildProviderImpl(string name) {
        *Class cls = ChildMap{name};
        if (!cls) {
            return;
        }
        return cls.newObject(sftp);
    }

    #! Returns data provider static info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}
}