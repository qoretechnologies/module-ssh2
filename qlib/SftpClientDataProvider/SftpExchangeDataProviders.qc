# Copyright 2026 Qore Technologies, s.r.o.
# SPDX-License-Identifier: MIT

public namespace SftpClientDataProvider {
#! Publish a complete file over SFTP
public class SftpClientPublishDataProvider inherits AbstractDataProvider {
    private {
        SFTPClient sftp;
    }
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "publish-verified", "type": "SftpClientPublishDataProvider", "desc": "Publish a complete file over SFTP",
            "constructor_options": SftpClientDataProvider::ConstructorOptions, "supports_request": True,
        };
        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys
        });
    }
    constructor(SFTPClient sftp) {
        self.sftp = sftp;
    }
    string getName() {
        return ProviderInfo.name;
    }
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        string local_path = remove req.local_path;
        hash<SftpExchangeSpec> spec = cast<hash<SftpExchangeSpec>>(req);
        return sftp.publishVerified(new FileInputStream(local_path), spec);
    }
    private *AbstractDataProviderType getRequestTypeImpl() {
        return SftpPublishRequestDataType;
    }
    private *AbstractDataProviderType getResponseTypeImpl() {
        return SftpExchangeReceiptDataType;
    }
    hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

#! Acquire a committed file over SFTP
public class SftpClientAcquireDataProvider inherits AbstractDataProvider {
    private {
        SFTPClient sftp;
    }
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "acquire-verified", "type": "SftpClientAcquireDataProvider", "desc": "Acquire a committed file over SFTP",
            "constructor_options": SftpClientDataProvider::ConstructorOptions, "supports_request": True,
        };
        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys
        });
    }
    constructor(SFTPClient sftp) {
        self.sftp = sftp;
    }
    string getName() {
        return ProviderInfo.name;
    }
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        string local_path = remove req.local_path;
        hash<SftpExchangeSpec> spec = cast<hash<SftpExchangeSpec>>(req);
        return sftp.acquireVerified(spec, local_path);
    }
    private *AbstractDataProviderType getRequestTypeImpl() {
        return SftpAcquireRequestDataType;
    }
    private *AbstractDataProviderType getResponseTypeImpl() {
        return SftpExchangeReceiptDataType;
    }
    hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}
}
