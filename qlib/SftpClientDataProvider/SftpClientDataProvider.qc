# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore SftpClientDataProvider class definition

/** SftpClientDataProvider.qc Copyright 2023 - 2026 Qore Technologies, s.r.o.

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

#! Readable host-key policy constructor option values
public enum SftpClientHostKeyPolicyOption : string {
    #! Reject unknown host keys
    Reject = "reject",
    #! Trust unknown host keys on first use and persist them
    TrustOnFirstUse = "tofu",
    #! Trust unknown host keys for the current connection only when needed
    TrustOnFirstUseSession = "tofu-session",
}

const HostKeyPolicyOptionValues = (
    SftpClientHostKeyPolicyOption::Reject,
    SftpClientHostKeyPolicyOption::TrustOnFirstUse,
    SftpClientHostKeyPolicyOption::TrustOnFirstUseSession,
);

const HostKeyPolicyAllowedValues = (
    <AllowedValueInfo>{
        "display_name": "Reject",
        "short_desc": "Reject unknown host keys",
        "desc": "Reject unknown host keys",
        "value": SftpClientHostKeyPolicyOption::Reject,
    },
    <AllowedValueInfo>{
        "display_name": "Trust On First Use",
        "short_desc": "Trust unknown host keys on first use and persist them",
        "desc": "Trust unknown host keys on first use and persist them to the configured known_hosts file",
        "value": SftpClientHostKeyPolicyOption::TrustOnFirstUse,
    },
    <AllowedValueInfo>{
        "display_name": "Session Trust On First Use",
        "short_desc": "Trust unknown host keys for the current connection only when needed",
        "desc": "Trust unknown host keys for the current connection only if no known_hosts file is configured",
        "value": SftpClientHostKeyPolicyOption::TrustOnFirstUseSession,
    },
);

enum<Ssh2HostKeyPolicy> sub getHostKeyPolicyFromOption(string policy) {
    if (policy == SftpClientHostKeyPolicyOption::Reject) {
        return Ssh2HostKeyPolicy::Reject;
    }
    if (policy == SftpClientHostKeyPolicyOption::TrustOnFirstUse) {
        return Ssh2HostKeyPolicy::TrustOnFirstUse;
    }
    if (policy == SftpClientHostKeyPolicyOption::TrustOnFirstUseSession) {
        return Ssh2HostKeyPolicy::TrustOnFirstUseSession;
    }

    throw "OPTION-VALUE-ERROR", sprintf("invalid host_key_policy option value %y; expected one of %y",
        policy, HostKeyPolicyOptionValues);
}

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
            "known_hosts": <DataProviderOptionInfo>{
                "display_name": "Known Hosts File",
                "short_desc": "A path to an OpenSSH known_hosts file for host key verification",
                "type": AbstractDataProviderTypeMap."string",
                "desc": "A path to an OpenSSH known_hosts file for host key verification; environment variables "
                    "can be prefixed with '$' in the path; if set to an empty string, no known_hosts file is "
                    "configured; if not set, the ssh2 module default applies",
            },
            "host_key_policy": <DataProviderOptionInfo>{
                "display_name": "Host Key Policy",
                "short_desc": "Policy for unknown host keys",
                "type": AbstractDataProviderTypeMap."string",
                "desc": "Policy for unknown host keys when host-key verification is enabled: `reject` rejects "
                    "unknown keys, `tofu` persists first-use keys to the configured known_hosts file, and "
                    "`tofu-session` trusts unknown keys for the current connection only if no known_hosts file is "
                    "configured",
                "allowed_values": HostKeyPolicyAllowedValues,
                "default_value": SftpClientHostKeyPolicyOption::TrustOnFirstUse,
            },
            "verify_host_key": <DataProviderOptionInfo>{
                "display_name": "Verify Host Key",
                "short_desc": "Enable host key verification against the known_hosts file",
                "type": AbstractDataProviderTypeMap."bool",
                "desc": "Enable host key verification against the known_hosts file",
                "default_value": True,
            },
            "use_agent": <DataProviderOptionInfo>{
                "display_name": "Use SSH Agent",
                "short_desc": "Enable SSH agent authentication",
                "type": AbstractDataProviderTypeMap."bool",
                "desc": "Enable SSH agent authentication; when enabled, the client will attempt to authenticate "
                    "using keys from the running SSH agent before other methods",
                "default_value": True,
            },
            "keepalive_interval": <DataProviderOptionInfo>{
                "display_name": "Keepalive Interval",
                "short_desc": "Keepalive interval in seconds",
                "type": AbstractDataProviderTypeMap."int",
                "desc": "Keepalive interval in seconds; 0 disables keepalive; default: 60",
                "default_value": 60,
            },
        };

        SFTPClient sftp;
    }

    private {
        const ChildMap = {
            "chmod": Class::forName("SftpClientDataProvider::SftpClientChmodDataProvider"),
            "chown": Class::forName("SftpClientDataProvider::SftpClientChownDataProvider"),
            "create-file": Class::forName("SftpClientDataProvider::SftpClientCreateFileDataProvider"),
            "create-file-location": Class::forName("SftpClientDataProvider::SftpClientCreateFileLocationDataProvider"),
            "delete": Class::forName("SftpClientDataProvider::SftpClientDeleteDataProvider"),
            "exists": Class::forName("SftpClientDataProvider::SftpClientExistsDataProvider"),
            "get-file": Class::forName("SftpClientDataProvider::SftpClientGetFileDataProvider"),
            "get-file-location": Class::forName("SftpClientDataProvider::SftpClientGetFileLocationDataProvider"),
            "list": Class::forName("SftpClientDataProvider::SftpClientListDataProvider"),
            "lstat": Class::forName("SftpClientDataProvider::SftpClientLstatDataProvider"),
            "mkdir": Class::forName("SftpClientDataProvider::SftpClientMkdirDataProvider"),
            "move": Class::forName("SftpClientDataProvider::SftpClientMoveDataProvider"),
            "readlink": Class::forName("SftpClientDataProvider::SftpClientReadlinkDataProvider"),
            "rmdir": Class::forName("SftpClientDataProvider::SftpClientRmdirDataProvider"),
            "stat": Class::forName("SftpClientDataProvider::SftpClientStatDataProvider"),
            "statvfs": Class::forName("SftpClientDataProvider::SftpClientStatvfsDataProvider"),
            "symlink": Class::forName("SftpClientDataProvider::SftpClientSymlinkDataProvider"),
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
        if (options.keyfile.val()) {
            sftp.setKeys(substitute_env_vars(options.keyfile));
        }
        if (exists options.known_hosts) {
            sftp.setKnownHostsFile(substitute_env_vars(options.known_hosts));
        }
        if (exists options.host_key_policy) {
            sftp.setHostKeyPolicy(getHostKeyPolicyFromOption(options.host_key_policy));
        }
        if (exists options.verify_host_key) {
            sftp.setVerifyHostKey(options.verify_host_key);
        }
        if (exists options.use_agent) {
            sftp.setUseAgent(options.use_agent);
        }
        if (exists options.keepalive_interval) {
            sftp.setKeepalive(options.keepalive_interval);
        }
        return sftp;
    }

    #! Returns the path based on the SftpClient's base path
    static string getPath(SFTPClient sftp, string path1) {
        # If path1 starts with /, treat it as absolute and return as-is
        if (path1 =~ /^\//) {
            return path1;
        }
        # Get base path from info
        *string base_path = sftp.info().path;
        if (!base_path.val()) {
            # No base path set - use path relative to home directory
            return path1;
        }
        # Combine base path with relative path
        return base_path + "/" + path1;
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
