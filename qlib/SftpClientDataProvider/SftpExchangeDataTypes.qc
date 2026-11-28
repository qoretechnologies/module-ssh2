# Copyright 2026 Qore Technologies, s.r.o.
# SPDX-License-Identifier: MIT

public namespace SftpClientDataProvider {
class ExchangeFields {
    public {
        const Fields = {
            "profile": {
                "display_name": "Endpoint profile",
                "short_desc": "Publication agreement for this endpoint",
                "desc": "Publication agreement for this endpoint",
                "type": AbstractDataProviderTypeMap."string",
                "required": True,
                "allowed_values": (
                    <AllowedValueInfo>{"display_name": "OpenSSH POSIX", "value": "openssh-posix"},
                    <AllowedValueInfo>{"display_name": "Hosted immutable", "value": "hosted-immutable"},
                ),
            },
            "collision_policy": {
                "display_name": "Collision policy",
                "short_desc": "Select how the endpoint handles an existing file",
                "desc": "Select how the endpoint handles an existing file",
                "type": AbstractDataProviderTypeMap."string",
                "required": True,
                "allowed_values": (
                    <AllowedValueInfo>{"display_name": "Replace", "value": "replace"},
                    <AllowedValueInfo>{"display_name": "No replace", "value": "no-replace"},
                ),
            },
            "remote_path": {
                "display_name": "Remote path",
                "short_desc": "Absolute path of the committed file",
                "desc": "Absolute path of the committed file",
                "type": AbstractDataProviderTypeMap."string",
                "required": True,
                "example_value": "/inbound/orders-0042.csv",
            },
            "length": {
                "display_name": "Expected length",
                "short_desc": "Declared file length in bytes",
                "desc": "Declared file length in bytes",
                "type": AbstractDataProviderTypeMap."int",
                "required": True,
                "example_value": 4096,
            },
            "sha256": {
                "display_name": "Producer SHA-256",
                "short_desc": "Expected lowercase SHA-256, required when acquiring a replaceable file",
                "desc": "Expected lowercase SHA-256, required when acquiring a replaceable file",
                "type": AbstractDataProviderTypeMap."*string",
                "example_value": "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
            },
            "host_key_sha256": {
                "display_name": "Host key SHA-256",
                "short_desc": "Pinned SSH host key in lowercase hex, required for the hosted profile",
                "desc": "Pinned SSH host key in lowercase hex, required for the hosted profile",
                "type": AbstractDataProviderTypeMap."*string",
                "example_value": "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb",
            },
        };
    }
}

#! Field definitions for SftpPublishRequest
public class SftpPublishRequestType inherits HashDataType {
    public {
        const Fields = ExchangeFields::Fields + {
            "local_path": {
                "display_name": "Source file",
                "short_desc": "Local file to publish",
                "desc": "Local file to publish",
                "type": AbstractDataProviderTypeMap."string",
                "required": True,
                "example_value": "/out/orders.csv",
            },
            "staging_dir": {
                "display_name": "Staging directory",
                "short_desc": "Private staging directory on the remote filesystem",
                "desc": "Private staging directory on the remote filesystem",
                "type": AbstractDataProviderTypeMap."string",
                "required": True,
                "example_value": "/staging",
            },
        };
    }
    constructor() : HashDataType("SftpPublishRequest") {
        addQoreFields(Fields);
    }
}
public const SftpPublishRequestDataType = new SftpPublishRequestType();

#! Field definitions for SftpAcquireRequest
public class SftpAcquireRequestType inherits HashDataType {
    public {
        const Fields = ExchangeFields::Fields + {
            "local_path": {
                "display_name": "Destination file",
                "short_desc": "Local file to replace after verification",
                "desc": "Local file to replace after verification",
                "type": AbstractDataProviderTypeMap."string",
                "required": True,
                "example_value": "/received/orders.csv",
            },
        };
    }
    constructor() : HashDataType("SftpAcquireRequest") {
        addQoreFields(Fields);
    }
}
public const SftpAcquireRequestDataType = new SftpAcquireRequestType();

#! Field definitions for SftpExchangeReceipt
public class SftpExchangeReceiptType inherits HashDataType {
    private {
        const Fields = {
            "outcome": {
                "display_name": "Outcome",
                "short_desc": "Result of the commit",
                "desc": "Result of the commit",
                "type": AbstractDataProviderTypeMap."string",
                "allowed_values": (
                    <AllowedValueInfo>{"display_name": "Confirmed", "value": "confirmed"},
                    <AllowedValueInfo>{"display_name": "Failed", "value": "failed"},
                    <AllowedValueInfo>{"display_name": "Unsupported", "value": "unsupported"},
                    <AllowedValueInfo>{"display_name": "Indeterminate", "value": "indeterminate"},
                ),
            },
            "host": {
                "display_name": "Host",
                "short_desc": "SSH server hostname",
                "desc": "SSH server hostname",
                "type": AbstractDataProviderTypeMap."string",
            },
            "port": {
                "display_name": "Port",
                "short_desc": "SSH server port",
                "desc": "SSH server port",
                "type": AbstractDataProviderTypeMap."int",
            },
            "principal": {
                "display_name": "Principal",
                "short_desc": "Authenticated SSH account",
                "desc": "Authenticated SSH account",
                "type": AbstractDataProviderTypeMap."string",
            },
            "host_key_sha256": {
                "display_name": "Host key SHA-256",
                "short_desc": "Negotiated host key fingerprint",
                "desc": "Negotiated host key fingerprint",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "remote_path": {
                "display_name": "Remote path",
                "short_desc": "Absolute path of the committed file",
                "desc": "Absolute path of the committed file",
                "type": AbstractDataProviderTypeMap."string",
            },
            "expected_sha256": {
                "display_name": "Producer SHA-256",
                "short_desc": "Digest declared by the producer",
                "desc": "Digest declared by the producer",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "expected_length": {
                "display_name": "Expected length",
                "short_desc": "Declared file length in bytes",
                "desc": "Declared file length in bytes",
                "type": AbstractDataProviderTypeMap."int",
            },
            "length": {
                "display_name": "Observed length",
                "short_desc": "Bytes transferred",
                "desc": "Bytes transferred",
                "type": AbstractDataProviderTypeMap."int",
            },
            "sha256": {
                "display_name": "SHA-256",
                "short_desc": "Digest of the complete stream",
                "desc": "Digest of the complete stream",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "version": {
                "display_name": "Version",
                "short_desc": "Content version within the endpoint, account and path",
                "desc": "Content version within the endpoint, account and path",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "staging_path": {
                "display_name": "Staging path",
                "short_desc": "Private remote staging path",
                "desc": "Private remote staging path",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "local_path": {
                "display_name": "Local path",
                "short_desc": "Local destination path",
                "desc": "Local destination path",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "error": {
                "display_name": "Error",
                "short_desc": "Error category",
                "desc": "Error category",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "detail": {
                "display_name": "Detail",
                "short_desc": "Diagnostic message",
                "desc": "Diagnostic message",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "profile": {
                "display_name": "Endpoint profile",
                "short_desc": "Publication agreement for this endpoint",
                "desc": "Publication agreement for this endpoint",
                "type": AbstractDataProviderTypeMap."string",
                "allowed_values": (
                    <AllowedValueInfo>{"display_name": "OpenSSH POSIX", "value": "openssh-posix"},
                    <AllowedValueInfo>{"display_name": "Hosted immutable", "value": "hosted-immutable"},
                ),
            },
            "collision_policy": {
                "display_name": "Collision policy",
                "short_desc": "Select how the endpoint handles an existing file",
                "desc": "Select how the endpoint handles an existing file",
                "type": AbstractDataProviderTypeMap."string",
                "allowed_values": (
                    <AllowedValueInfo>{"display_name": "Replace", "value": "replace"},
                    <AllowedValueInfo>{"display_name": "No replace", "value": "no-replace"},
                ),
            },
        };
    }
    constructor() : HashDataType("SftpExchangeReceipt") {
        addQoreFields(Fields);
    }
}
public const SftpExchangeReceiptDataType = new SftpExchangeReceiptType();

}
