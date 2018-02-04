/*
 * Copyright © 2012-2015 VMware, Inc.  All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the “License”); you may not
 * use this file except in compliance with the License.  You may obtain a copy
 * of the License at http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an “AS IS” BASIS, without
 * warranties or conditions of any kind, EITHER EXPRESS OR IMPLIED.  See the
 * License for the specific language governing permissions and limitations
 * under the License.
 */



#include "includes.h"

/*
 *  Could have get this via replication agreement cache.
 *  But current access replication agreement cache is not thr safe.
 */
static
DWORD
_VmDirGetLocalProcessedUSN(
    PSTR*   ppszLocalProcessedUSN
    )
{
    DWORD               dwError = ERROR_SUCCESS;
    VDIR_ENTRY_ARRAY    entryArray = {0};
    PVDIR_ATTRIBUTE     pAttrLocalUSN= NULL;
    PVDIR_ATTRIBUTE     pAttrURI= NULL;
    size_t              iCnt = 0;
    PSTR                pszLocalProcessedUSN = NULL;
    PSTR                pszURIHost = NULL;
    size_t              dwLength = 0;
    size_t              dwCurrent = 0;

    dwError = VmDirSimpleEqualFilterInternalSearch(
                        gVmdirServerGlobals.serverObjDN.lberbv.bv_val,
                        LDAP_SCOPE_SUBTREE,
                        ATTR_OBJECT_CLASS,
                        OC_REPLICATION_AGREEMENT,
                        &entryArray);
    BAIL_ON_VMDIR_ERROR(dwError);

    if (entryArray.iSize > 0)
    {
        // calculate length
        for ( iCnt = 0; iCnt < entryArray.iSize; iCnt ++ )
        {
            pAttrLocalUSN = VmDirFindAttrByName(&(entryArray.pEntry[iCnt]), ATTR_LAST_LOCAL_USN_PROCESSED);

            pAttrURI = VmDirFindAttrByName(&(entryArray.pEntry[iCnt]), ATTR_LABELED_URI);
            if (!pAttrURI)
            {
                dwError = VMDIR_ERROR_NO_SUCH_ATTRIBUTE;
                BAIL_ON_VMDIR_ERROR(dwError);
            }
            dwLength += ( (pAttrLocalUSN ? pAttrLocalUSN->vals[0].lberbv_len:1) +
                          pAttrURI->vals[0].lberbv_len + 2 );   // add '|' and ',' separator
        }

        dwLength++; // for NULL terminator
        dwError = VmDirAllocateMemory( sizeof(CHAR)*(dwLength), (PVOID)&pszLocalProcessedUSN );
        BAIL_ON_VMDIR_ERROR(dwError);

        for ( iCnt = 0, dwCurrent = 0; iCnt < entryArray.iSize; iCnt ++ )
        {
            pAttrLocalUSN = VmDirFindAttrByName(&(entryArray.pEntry[iCnt]), ATTR_LAST_LOCAL_USN_PROCESSED);

            pAttrURI = VmDirFindAttrByName(&(entryArray.pEntry[iCnt]), ATTR_LABELED_URI);

            VMDIR_SAFE_FREE_MEMORY(pszURIHost);
            dwError=VmDirReplURIToHostname( pAttrURI->vals[0].lberbv_val, &pszURIHost);
            BAIL_ON_VMDIR_ERROR(dwError);

            dwError = VmDirStringPrintFA( pszLocalProcessedUSN + dwCurrent,
                                          dwLength-dwCurrent,
                                          "%s|%s,",
                                          pszURIHost, // strlen(pszURIHost) <= pAttrURI->vals[0].lberbv_len
                                          pAttrLocalUSN ? pAttrLocalUSN->vals[0].lberbv_val:"0");
            BAIL_ON_VMDIR_ERROR(dwError);

            dwCurrent += ( (pAttrLocalUSN ? pAttrLocalUSN->vals[0].lberbv_len:1) + strlen(pszURIHost) + 2 );
        }

        *ppszLocalProcessedUSN = pszLocalProcessedUSN;
        pszLocalProcessedUSN = NULL;
    }

cleanup:
    VMDIR_SAFE_FREE_MEMORY(pszLocalProcessedUSN);
    VMDIR_SAFE_FREE_MEMORY(pszURIHost);
    VmDirFreeEntryArrayContent(&entryArray);

    return dwError;

error:

    goto cleanup;
}

/*
 *  Could have get this via gVmdirServerGlobals.utdVector.
 *  But current access gVmdirServerGlobals.utdVector is not thr safe.
 */
static
DWORD
_VmDirGetLocalUTDVector(
    PSTR*               ppszUtdVector
    )
{
    DWORD               dwError = ERROR_SUCCESS;
    PSTR                pszUtdVector = NULL;
    VDIR_ENTRY_ARRAY    entryArray = {0};
    PVDIR_ATTRIBUTE     pAttrUTDVector= NULL;

    dwError = VmDirSimpleEqualFilterInternalSearch(
                        gVmdirServerGlobals.serverObjDN.lberbv.bv_val,
                        LDAP_SCOPE_BASE,
                        ATTR_OBJECT_CLASS,
                        OC_DIR_SERVER,
                        &entryArray);
    BAIL_ON_VMDIR_ERROR(dwError);

    if (entryArray.iSize == 1)
    {
        pAttrUTDVector = VmDirFindAttrByName(&(entryArray.pEntry[0]), ATTR_UP_TO_DATE_VECTOR);
        if (!pAttrUTDVector)
        {
            dwError = VMDIR_ERROR_NO_SUCH_ATTRIBUTE;
            BAIL_ON_VMDIR_ERROR(dwError);
        }

        dwError = VmDirAllocateAndCopyMemory(   pAttrUTDVector->vals[0].lberbv_val,
                                                pAttrUTDVector->vals[0].lberbv_len,
                                                (PVOID*)&pszUtdVector);
        BAIL_ON_VMDIR_ERROR(dwError);
    }

    *ppszUtdVector = pszUtdVector;
    pszUtdVector = NULL;

cleanup:
    VmDirFreeEntryArrayContent(&entryArray);
    VMDIR_SAFE_FREE_MEMORY(pszUtdVector);

    return dwError;

error:

    goto cleanup;
}

VOID
VmDirdStateSet(
    VDIR_SERVER_STATE   state)
{
    BOOLEAN             bInLock = FALSE;
    VDIR_BACKEND_CTX    beCtx = {0};
    VDIR_SERVER_STATE   currentState = VMDIRD_STATE_UNDEFINED;
    DWORD ulError = 0;

    VMDIR_LOCK_MUTEX(bInLock, gVmdirdStateGlobals.pMutex);
    currentState = gVmdirdStateGlobals.vmdirdState;

    // Do not transition to normal running states from FAILURE.
    if (state == VMDIRD_STATE_UNDEFINED ||
        (currentState == VMDIRD_STATE_FAILURE && state != VMDIRD_STATE_SHUTDOWN))
    {

        VMDIR_LOG_INFO(VMDIR_LOG_MASK_ALL,
                       "%s: State (%d) cannot transition to state (%d)",
                       __FUNCTION__,
                       currentState,
                       state);
        goto error;
    }

    if (state == VMDIRD_STATE_READ_ONLY_DEMOTE)
    {
        /*
         * Force RPC server to stop listening during demote operation.
         * SRP operations can't work properly while in this state.
         */
        state = VMDIRD_STATE_READ_ONLY;
        rpc_mgmt_stop_server_listening(NULL, (unsigned32*)&ulError);
    }

    gVmdirdStateGlobals.vmdirdState = state;

    if (state == VMDIRD_STATE_READ_ONLY) // Wait for the pending write transactions to be over before returning
    {
        beCtx.pBE = VmDirBackendSelect(NULL);
        assert(beCtx.pBE);

        while (beCtx.pBE->pfnBEGetLeastOutstandingUSN(&beCtx, TRUE) != 0)
        {
            VMDIR_LOG_VERBOSE( VMDIR_LOG_MASK_ALL, "VmDirdStateSet: Waiting for the pending write transactions to be over" );
            VmDirSleep(2*1000); // sleep for 2 seconds
        }
    }

    VMDIR_LOG_INFO( VMDIR_LOG_MASK_ALL, "VmDir State (%u)", state);

cleanup:
    VMDIR_UNLOCK_MUTEX(bInLock, gVmdirdStateGlobals.pMutex);
    VmDirBackendCtxContentFree(&beCtx);

    return;

error:
    goto cleanup;
}

VDIR_SERVER_STATE
VmDirdState(
    VOID
    )
{
    VDIR_SERVER_STATE rtnState;
    BOOLEAN bInLock = FALSE;

    VMDIR_LOCK_MUTEX(bInLock, gVmdirdStateGlobals.pMutex);
    rtnState = gVmdirdStateGlobals.vmdirdState;
    VMDIR_UNLOCK_MUTEX(bInLock, gVmdirdStateGlobals.pMutex);

    return rtnState;
}

VDIR_SERVER_STATE
VmDirdGetTargetState(
    VOID
    )
{
    VDIR_SERVER_STATE targetState;
    BOOLEAN bInLock = FALSE;

    VMDIR_LOCK_MUTEX(bInLock, gVmdirdStateGlobals.pMutex);
    targetState = gVmdirdStateGlobals.targetState;
    VMDIR_UNLOCK_MUTEX(bInLock, gVmdirdStateGlobals.pMutex);

    return targetState;
}

VOID
VmDirdSetTargetState(
    VDIR_SERVER_STATE targetState
    )
{
    BOOLEAN bInLock = FALSE;

    VMDIR_LOCK_MUTEX(bInLock, gVmdirdStateGlobals.pMutex);
    gVmdirdStateGlobals.targetState = targetState;
    VMDIR_UNLOCK_MUTEX(bInLock, gVmdirdStateGlobals.pMutex);
}

VOID
VmDirdSetReplNow(
    BOOLEAN bReplNow)
{
    BOOLEAN             bInLock = FALSE;

    VMDIR_LOCK_MUTEX(bInLock, gVmdirGlobals.mutex);
    gVmdirGlobals.bReplNow = bReplNow;
    VMDIR_UNLOCK_MUTEX(bInLock, gVmdirGlobals.mutex);

    return;
}

BOOLEAN
VmDirdGetReplNow(
    VOID
    )
{
    BOOLEAN     bReplNow = FALSE;
    BOOLEAN     bInLock = FALSE;

    VMDIR_LOCK_MUTEX(bInLock, gVmdirGlobals.mutex);
    bReplNow = gVmdirGlobals.bReplNow;
    VMDIR_UNLOCK_MUTEX(bInLock, gVmdirGlobals.mutex);

    return bReplNow;
}

BOOLEAN
VmDirdGetAllowInsecureAuth(
    VOID
    )
{
    return gVmdirGlobals.bAllowInsecureAuth;
}

VOID
VmDirGetLdapListenPorts(
    PDWORD* ppdwLdapListenPorts,
    PDWORD  pdwLdapListenPorts
    )
{
    *ppdwLdapListenPorts = gVmdirGlobals.pdwLdapListenPorts;
    *pdwLdapListenPorts = gVmdirGlobals.dwLdapListenPorts;
}

VOID
VmDirGetLdapsListenPorts(
    PDWORD* ppdwLdapsListenPorts,
    PDWORD  pdwLdapsListenPorts
    )
{
    *ppdwLdapsListenPorts = gVmdirGlobals.pdwLdapsListenPorts;
    *pdwLdapsListenPorts = gVmdirGlobals.dwLdapsListenPorts;
}

VOID
VmDirGetLdapConnectPorts(
    PDWORD* ppdwLdapConnectPorts,
    PDWORD  pdwLdapConnectPorts
    )
{
    *ppdwLdapConnectPorts = gVmdirGlobals.pdwLdapConnectPorts;
    *pdwLdapConnectPorts = gVmdirGlobals.dwLdapConnectPorts;
}

VOID
VmDirGetLdapsConnectPorts(
    PDWORD* ppdwLdapsConnectPorts,
    PDWORD  pdwLdapsConnectPorts
    )
{
    *ppdwLdapsConnectPorts = gVmdirGlobals.pdwLdapsConnectPorts;
    *pdwLdapsConnectPorts = gVmdirGlobals.dwLdapsConnectPorts;
}

DWORD
VmDirGetAllLdapPortsCount(
    VOID
    )
{
    return gVmdirGlobals.dwLdapConnectPorts + gVmdirGlobals.dwLdapsConnectPorts;
}

DWORD
VmDirCheckPortAvailability(
    DWORD   dwPort
    )
{
    DWORD   dwError = 0;
    BOOLEAN bIPV4Addr = FALSE;
    BOOLEAN bIPV6Addr = FALSE;
    int     ip4_fd = -1;
    int     ip6_fd = -1;
    int     level = 0;
    int     optname = 0;
    int     on = 1;
    struct sockaddr_in  serv_4addr = {0};
    struct sockaddr_in6 serv_6addr = {0};

#ifdef _WIN32
    level = IPPROTO_IPV6;
    optname = SO_EXCLUSIVEADDRUSE;
#else
    level = SOL_IPV6;
    optname = SO_REUSEADDR;
#endif

    dwError = VmDirWhichAddressPresent(&bIPV4Addr, &bIPV6Addr);
    BAIL_ON_VMDIR_ERROR(dwError);

    if (bIPV4Addr)
    {
        if ((ip4_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
        {
            dwError = LwErrnoToWin32Error(errno);
            BAIL_ON_VMDIR_ERROR(dwError);
        }

        bzero((char *) &serv_4addr, sizeof(serv_4addr));
        serv_4addr.sin_family = AF_INET;
        serv_4addr.sin_addr.s_addr = INADDR_ANY;
        serv_4addr.sin_port = htons(dwPort);

        if (setsockopt(ip4_fd, SOL_SOCKET, optname, (const char *)(&on), sizeof(on)) < 0 ||
            bind(ip4_fd, (struct sockaddr *)&serv_4addr, sizeof(serv_4addr)) < 0)
        {
            VMDIR_LOG_ERROR( VMDIR_LOG_MASK_ALL,
                    "%s failed to bind to (IPV4) port %d with errno %d",
                    __FUNCTION__, dwPort, errno );

            dwError = VMDIR_ERROR_INVALID_STATE;
            BAIL_ON_VMDIR_ERROR(dwError);
        }
    }

    if (bIPV6Addr)
    {
        if ((ip6_fd = socket(AF_INET6, SOCK_STREAM, 0)) < 0)
        {
            dwError = LwErrnoToWin32Error(errno);
            BAIL_ON_VMDIR_ERROR(dwError);
        }

        memset((char *) &serv_6addr, 0, sizeof(serv_6addr));
        serv_6addr.sin6_family = AF_INET6;
        serv_6addr.sin6_port = htons(dwPort);

        if (setsockopt(ip6_fd, SOL_SOCKET, optname, (const char *)(&on), sizeof(on)) < 0 ||
            setsockopt(ip6_fd, level, IPV6_V6ONLY, (const char *)(&on), sizeof(on)) < 0 ||
            bind(ip6_fd, (struct sockaddr *)&serv_6addr, sizeof(serv_6addr)) < 0)
        {
            VMDIR_LOG_ERROR( VMDIR_LOG_MASK_ALL,
                    "%s failed to bind to (IPV6) port %d with errno %d",
                    __FUNCTION__, dwPort, errno );

            dwError = VMDIR_ERROR_INVALID_STATE;
            BAIL_ON_VMDIR_ERROR(dwError);
        }
    }

cleanup:
    if (ip4_fd >= 0)
    {
        tcp_close(ip4_fd);
    }
    if (ip6_fd >= 0)
    {
        tcp_close(ip6_fd);
    }
    return dwError;

error:
    VMDIR_LOG_ERROR( VMDIR_LOG_MASK_ALL,
            "%s failed, error (%d)", __FUNCTION__, dwError);

    goto cleanup;
}

DWORD
VmDirServerStatusEntry(
    PVDIR_ENTRY*    ppEntry
    )
{
    DWORD           dwError = 0;
    int             iNumAttrs = 1 + 1 + SUPPORTED_STATUS_COUNT;     // cn/oc/6 ops
    PSTR            pszAry[SUPPORTED_STATUS_COUNT + 1] = {0};
    int             iCnt = 0;
    int             iTmp = 0;
    PSTR*           ppszAttrList = NULL;
    PVDIR_ENTRY     pEntry = NULL;
    PVDIR_SCHEMA_CTX    pSchemaCtx = NULL;

    assert( ppEntry );

    dwError = VmDirAllocateMemory(
            sizeof(PSTR) * (iNumAttrs * 2 + 1), // add 1 for VmDirFreeStringArrayA call later
            (PVOID)&ppszAttrList);
    BAIL_ON_VMDIR_ERROR(dwError);

    dwError = VmDirAllocateStringA( ATTR_CN, &ppszAttrList[iCnt++]);
    BAIL_ON_VMDIR_ERROR(dwError);

    dwError = VmDirAllocateStringA( "ServerStatus", &ppszAttrList[iCnt++]);
    BAIL_ON_VMDIR_ERROR(dwError);

    dwError = VmDirAllocateStringA( ATTR_OBJECT_CLASS, &ppszAttrList[iCnt++]);
    BAIL_ON_VMDIR_ERROR(dwError);

    dwError = VmDirAllocateStringA( OC_SERVER_STATUS, &ppszAttrList[iCnt++]);
    BAIL_ON_VMDIR_ERROR(dwError);

    pszAry[0] = VmDirOPStatistic(LDAP_REQ_BIND);        // pszAry owns PSTR
    pszAry[1] = VmDirOPStatistic(LDAP_REQ_SEARCH);
    pszAry[2] = VmDirOPStatistic(LDAP_REQ_ADD);
    pszAry[3] = VmDirOPStatistic(LDAP_REQ_MODIFY);
    pszAry[4] = VmDirOPStatistic(LDAP_REQ_DELETE);
    pszAry[5] = VmDirOPStatistic(LDAP_REQ_UNBIND);

    for (iTmp = 0; iTmp < SUPPORTED_STATUS_COUNT; iTmp++)
    {
        dwError = VmDirAllocateStringA( ATTR_SERVER_RUNTIME_STATUS, &ppszAttrList[iCnt++]);
        BAIL_ON_VMDIR_ERROR(dwError);

        dwError = VmDirAllocateStringA( pszAry[iTmp] ? pszAry[iTmp] : "NONE", &ppszAttrList[iCnt++]);
        BAIL_ON_VMDIR_ERROR(dwError);
    }

    dwError = VmDirSchemaCtxAcquire(&pSchemaCtx);
    BAIL_ON_VMDIR_ERROR(dwError);

    dwError = VmDirAttrListToNewEntry( pSchemaCtx,
                                       SERVER_STATUS_DN,
                                       ppszAttrList,
                                       FALSE,
                                       &pEntry);
    BAIL_ON_VMDIR_ERROR(dwError);

    *ppEntry = pEntry;

cleanup:

    VmDirFreeStringArrayA(pszAry);

    if (ppszAttrList != NULL)
    {
        VmDirFreeStringArrayA(ppszAttrList);
        VMDIR_SAFE_FREE_MEMORY(ppszAttrList);
    }

    if (pSchemaCtx != NULL)
    {
        VmDirSchemaCtxRelease(pSchemaCtx);
    }

    return dwError;

error:

    if (pEntry != NULL)
    {
        VmDirFreeEntry(pEntry);
    }

    goto cleanup;
}

DWORD
VmDirReplicationStatusEntry(
    PVDIR_ENTRY*    ppEntry
    )
{
    DWORD               dwError = 0;
    DWORD               dwNumAttrs = 1 + 1 + SUPPORTED_STATUS_COUNT;     // cn/oc/
    PSTR*               ppszAttrList = NULL;
    PVDIR_ENTRY         pEntry = NULL;
    PVDIR_SCHEMA_CTX    pSchemaCtx = NULL;
    VDIR_BACKEND_CTX    backendCtx = {0};
    USN                 maxPartnerVisibleUSN = 0;
    PSTR                pszPartnerVisibleUSN = NULL;
    PSTR                pszCycleCount = NULL;
    PSTR                pszServerName = NULL;
    PSTR                pszInvocationID = NULL;
    PSTR                pszUtdVector = NULL;
    PSTR                pszLocalProcessedUSN = NULL;
    USN                 maxOriginatingUSN = 0;
    PSTR                pszMaxOriginatingUSN = NULL;
    CHAR*               pAtSign = NULL;

    assert( ppEntry );

    pAtSign = VmDirStringRChrA( gVmdirServerGlobals.dcAccountUPN.lberbv_val, '@' );
    if ( pAtSign == NULL )
    {
        dwError = VMDIR_ERROR_BAD_ATTRIBUTE_DATA;
        BAIL_ON_VMDIR_ERROR( dwError );
    }

    dwError = VmDirAllocateStringOfLenA(
                    gVmdirServerGlobals.dcAccountUPN.lberbv_val,
                    (DWORD)(pAtSign - gVmdirServerGlobals.dcAccountUPN.lberbv_val),
                    &pszServerName);
    BAIL_ON_VMDIR_ERROR(dwError);

    backendCtx.pBE = VmDirBackendSelect("");

    maxPartnerVisibleUSN = backendCtx.pBE->pfnBEGetLeastOutstandingUSN( &backendCtx, FALSE ) - 1;

    maxOriginatingUSN = backendCtx.pBE->pfnBEGetMaxOriginatingUSN( &backendCtx );

    dwError = VmDirAllocateStringPrintf( &pszPartnerVisibleUSN,
                                             "%" PRId64,
                                             maxPartnerVisibleUSN);
    BAIL_ON_VMDIR_ERROR(dwError);

    dwError = VmDirAllocateStringPrintf( &pszCycleCount,
                                             "%u",
                                             VmDirGetReplCycleCounter());
    BAIL_ON_VMDIR_ERROR(dwError);

    dwError = VmDirAllocateStringPrintf( &pszMaxOriginatingUSN,
                                             "%" PRId64,
                                             maxOriginatingUSN);
    BAIL_ON_VMDIR_ERROR(dwError);

    pszInvocationID = gVmdirServerGlobals.invocationId.lberbv_val;  // this global never change.

    _VmDirGetLocalUTDVector( &pszUtdVector);            // ignore error
    _VmDirGetLocalProcessedUSN( &pszLocalProcessedUSN); // ignore error

    dwError = VmDirAllocateMemory( sizeof(PSTR) * ((dwNumAttrs) * 2 + 1), // add 1 for VmDirFreeStringArrayA call later
                                   (PVOID)&ppszAttrList);
    BAIL_ON_VMDIR_ERROR(dwError);

    // construct ppszAttrList from table.
    {
        PSTR    ppszArray[] = {
                ATTR_CN,                    NULL,                             REPLICATION_STATUS_CN,
                ATTR_OBJECT_CLASS,          NULL,                             OC_SERVER_STATUS,
                ATTR_SERVER_RUNTIME_STATUS, REPL_STATUS_SERVER_NAME,          pszServerName,
                ATTR_SERVER_RUNTIME_STATUS, REPL_STATUS_VISIBLE_USN,          pszPartnerVisibleUSN,
                ATTR_SERVER_RUNTIME_STATUS, REPL_STATUS_CYCLE_COUNT,          pszCycleCount,
                ATTR_SERVER_RUNTIME_STATUS, REPL_STATUS_INVOCATION_ID,        pszInvocationID,
                ATTR_SERVER_RUNTIME_STATUS, REPL_STATUS_UTDVECTOR,            pszUtdVector,
                ATTR_SERVER_RUNTIME_STATUS, REPL_STATUS_PROCESSED_USN_VECTOR, pszLocalProcessedUSN,
                ATTR_SERVER_RUNTIME_STATUS, REPL_STATUS_ORIGINATING_USN,      pszMaxOriginatingUSN,
                NULL };
        DWORD   dwCnt = 0;
        DWORD   dwIndex = 0;

        for ( dwCnt = 0, dwIndex = 0; dwCnt < dwNumAttrs; dwCnt++ )
        {
            dwError = VmDirAllocateStringA( ppszArray[dwCnt*3], &ppszAttrList[dwIndex++]);
            BAIL_ON_VMDIR_ERROR(dwError);

            dwError = VmDirAllocateStringPrintf( &(ppszAttrList[dwIndex++]),
                                                     "%s%s",
                                                     ppszArray[dwCnt*3+1] ? ppszArray[dwCnt*3+1] : "" ,
                                                     ppszArray[dwCnt*3+2] ? ppszArray[dwCnt*3+2] : "Unknown" );
            BAIL_ON_VMDIR_ERROR(dwError);
        }
    }

    dwError = VmDirSchemaCtxAcquire(&pSchemaCtx);
    BAIL_ON_VMDIR_ERROR(dwError);

    dwError = VmDirAttrListToNewEntry( pSchemaCtx,
                                       REPLICATION_STATUS_DN,
                                       ppszAttrList,
                                       TRUE,  // this allows read permission to all in SD construction
                                       &pEntry);
    BAIL_ON_VMDIR_ERROR(dwError);

    *ppEntry = pEntry;

cleanup:

    VMDIR_SAFE_FREE_MEMORY( pszServerName );
    VMDIR_SAFE_FREE_MEMORY( pszLocalProcessedUSN );
    VMDIR_SAFE_FREE_MEMORY( pszUtdVector );
    VMDIR_SAFE_FREE_MEMORY( pszPartnerVisibleUSN );
    VMDIR_SAFE_FREE_MEMORY( pszCycleCount );
    VMDIR_SAFE_FREE_MEMORY( pszMaxOriginatingUSN );
    VmDirBackendCtxContentFree( &backendCtx );

    if (ppszAttrList != NULL)
    {
        VmDirFreeStringArrayA(ppszAttrList);
        VMDIR_SAFE_FREE_MEMORY(ppszAttrList);
    }

    if (pSchemaCtx != NULL)
    {
        VmDirSchemaCtxRelease(pSchemaCtx);
    }

    return dwError;

error:

    if (pEntry != NULL)
    {
        VmDirFreeEntry(pEntry);
    }

    goto cleanup;
}

DWORD
VmDirSrvValidateUserCreateParams(
    PVMDIR_USER_CREATE_PARAMS_RPC pCreateParams
    )
{
    DWORD dwError = 0;

    if (!pCreateParams ||
        IsNullOrEmptyString(pCreateParams->pwszAccount) ||
        IsNullOrEmptyString(pCreateParams->pwszFirstname) ||
        IsNullOrEmptyString(pCreateParams->pwszLastname) ||
        IsNullOrEmptyString(pCreateParams->pwszPassword))
    {
        dwError = ERROR_INVALID_PARAMETER;
    }

    return dwError;
}

/*
 * Lookup servers topology internally first. Then one of the servers
 * will be used to query uptoupdate servers topology
 */
DWORD
VmDirGetHostsInternal(
    PSTR**  pppszServerInfo,
    size_t* pdwInfoCount
    )
{
    DWORD   dwError = 0;
    DWORD   i = 0;
    PSTR    pszSearchBaseDN = NULL;
    VDIR_ENTRY_ARRAY    entryArray = {0};
    PVDIR_ATTRIBUTE     pAttr = NULL;
    PSTR*   ppszServerInfo = NULL;

    dwError = VmDirAllocateStringPrintf(
            &pszSearchBaseDN,
            "cn=Sites,cn=Configuration,%s",
            gVmdirServerGlobals.systemDomainDN.bvnorm_val);
    BAIL_ON_VMDIR_ERROR(dwError);

    dwError = VmDirSimpleEqualFilterInternalSearch(
            pszSearchBaseDN,
            LDAP_SCOPE_SUBTREE,
            ATTR_OBJECT_CLASS,
            OC_DIR_SERVER,
            &entryArray);
    BAIL_ON_VMDIR_ERROR(dwError);

    if (entryArray.iSize == 0)
    {
        dwError = LDAP_NO_SUCH_OBJECT;
        BAIL_ON_VMDIR_ERROR(dwError);
    }

    dwError = VmDirAllocateMemory(
            sizeof(PSTR) * (entryArray.iSize+1),
            (PVOID*)&ppszServerInfo);
    BAIL_ON_VMDIR_ERROR(dwError);

    for (i=0; i<entryArray.iSize; i++)
    {
         pAttr = VmDirEntryFindAttribute(ATTR_CN, entryArray.pEntry+i);
         dwError = VmDirAllocateStringA(pAttr->vals[0].lberbv.bv_val, &ppszServerInfo[i]);
         BAIL_ON_VMDIR_ERROR(dwError);
    }

    *pppszServerInfo = ppszServerInfo;
    *pdwInfoCount = entryArray.iSize;

cleanup:
    VMDIR_SAFE_FREE_STRINGA(pszSearchBaseDN);
    VmDirFreeEntryArrayContent(&entryArray);
    return dwError;

error:
    VmDirFreeStrArray(ppszServerInfo);
    goto cleanup;
}

DWORD
VmDirAllocateBerValueAVsnprintf(
    PVDIR_BERVALUE pbvValue,
    PCSTR pszFormat,
    ...
    )
{
    DWORD dwError = 0;
    PSTR pszValue = NULL;
    va_list args;

    va_start(args, pszFormat);
    dwError = VmDirVsnprintf(&pszValue, pszFormat, args);
    va_end(args);

    BAIL_ON_VMDIR_ERROR(dwError);

    pbvValue->lberbv_val = pszValue;
    pbvValue->lberbv_len = VmDirStringLenA(pszValue);
    pbvValue->bOwnBvVal = TRUE;

cleanup:
    return dwError;

error:
    VMDIR_SAFE_FREE_MEMORY(pszValue);
    goto cleanup;
}

#ifdef _WIN32

DWORD
VmDirAppendStringToEnvVar(
    _TCHAR *pEnvName, // in
    _TCHAR *pStrIn,   // in
    _TCHAR *pStrOut   // out
)
{
    DWORD   dwError            = 0;
    _TCHAR* localBuf           = NULL;
    size_t  strInLen           = VmDirStringLenA(pStrIn);
    size_t  envValueLen        = 0 ;

    dwError =  VmDirGetProgramDataEnvVar(
                    pEnvName,
                    &localBuf
                    );
    BAIL_ON_VMDIR_ERROR(dwError);

    envValueLen = VmDirStringLenA(localBuf);

    if ( envValueLen + strInLen + 1 < MAX_PATH )
    {
        dwError = VmDirStringCatA(
                        localBuf,
                        MAX_PATH,
                        pStrIn
                        );
        BAIL_ON_VMDIR_ERROR(dwError);
    }
    else // path too long
    {
        dwError = ERROR_BUFFER_OVERFLOW;    // The file name is too long.
                                            // In WinError.h, this error message maps to
                                            // ERROR_BUFFER_OVERFLOW. Not very
                                            // straight forward, though.
        BAIL_ON_VMDIR_ERROR(dwError);
    }

    dwError = VmDirStringCpyA(
                    pStrOut,
                    VmDirStringLenA(localBuf) + 1,
                    localBuf
                    );
    BAIL_ON_VMDIR_ERROR(dwError);

cleanup:
    VMDIR_SAFE_FREE_MEMORY(localBuf);
    return dwError;

error:
    goto cleanup;

}

/*
 * Get the directory where we should store our schema config file, i.e.,
 * %PROGRAMDATA%\cfg\%COMPONENT%\vmdirschema.ldif
 * which usually expands to
 * C:\ProgramData\VMware\CIS\cfg\vmdir\vmdirschema.ldif
 */
DWORD
VmDirGetBootStrapSchemaFilePath(
    _TCHAR *pBootStrapSchemaFile
)
{
    DWORD dwError = 0;

    if ((dwError = VmDirGetRegKeyValue( VMDIR_CONFIG_SOFTWARE_KEY_PATH, VMDIR_REG_KEY_CONFIG_PATH, pBootStrapSchemaFile,
                                        MAX_PATH )) != 0)
    {
       dwError = VmDirAppendStringToEnvVar(
                        TEXT("PROGRAMDATA"),
                        TEXT("\\VMware\\CIS\\cfg\\vmdird\\vmdirschema.ldif"),
                        pBootStrapSchemaFile
                        );
    }
    else
    {
       dwError = VmDirStringCatA(pBootStrapSchemaFile, MAX_PATH, "\\vmdirschema.ldif");
    }
    return dwError;
}

DWORD
VmDirGetLogFilePath(
    _TCHAR *pServerLogFile
)
{
    DWORD dwError = 0;

    if ((dwError = VmDirGetRegKeyValue( VMDIR_CONFIG_SOFTWARE_KEY_PATH, VMDIR_REG_KEY_LOG_PATH, pServerLogFile,
                                        MAX_PATH )) != 0)
    {
       dwError = VmDirAppendStringToEnvVar(
                        TEXT("PROGRAMDATA"),
                        TEXT("\\VMware\\CIS\\logs\\vmdird\\vmdir.log"),
                        pServerLogFile
                        );
    }
    else
    {
       dwError = VmDirStringCatA(pServerLogFile, MAX_PATH, "\\vmdir.log");
    }

    return dwError;
}

void
VmDirGetLogMaximumOldLogs(
    PDWORD pdwMaximumOldLogs
    )
{
    (VOID)VmDirGetRegKeyValueDword(VMDIR_CONFIG_SOFTWARE_KEY_PATH, VMDIR_REG_KEY_MAXIMUM_OLD_LOGS, pdwMaximumOldLogs, VMDIR_LOG_MAX_OLD_FILES);
}

void
VmDirGetLogMaximumLogSize(
    PINT64 pI64MaximumLogSize
    )
{
    if (VmDirGetRegKeyValueQword(VMDIR_CONFIG_SOFTWARE_KEY_PATH,
        VMDIR_REG_KEY_MAXIMUM_LOG_SIZE, pI64MaximumLogSize) != 0 ||
        *pI64MaximumLogSize == 0)
    {
        *pI64MaximumLogSize = VMDIR_LOG_MAX_SIZE_BYTES;
    }
}

#endif
