/*
 * Copyright © 018 VMware, Inc.  All Rights Reserved.
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

DWORD
VmDirGetRegKeyValue(
    PCSTR   pszConfigParamKeyPath,
    PCSTR   pszKey,
    PSTR    pszValue,
    size_t  valueLen
    )
{
    DWORD   dwError = 0;
    PSTR    pszLocalValue = NULL;
    DWORD   dwLocalValueLen = 0;

    if (pszValue == NULL)
    {
        dwError = ERROR_INVALID_PARAMETER;
        BAIL_ON_VMDIR_ERROR(dwError);
    }
    VMDIR_LOG_VERBOSE( VMDIR_LOG_MASK_ALL, "Reading Reg: %s", pszKey );

    dwError = RegUtilGetValue(
                NULL,
                HKEY_THIS_MACHINE,
                NULL,
                pszConfigParamKeyPath,
                pszKey,
                NULL,
                (PVOID*)&pszLocalValue,
                &dwLocalValueLen);

    BAIL_ON_VMDIR_ERROR(dwError);

    if (dwLocalValueLen > valueLen) // in case of string values, dwLocalValueLen includes '\0' and therefore valueLen
                                    // should also include space for '\0'
    {
        dwError = ERROR_INVALID_PARAMETER; // TBD: Better error code??
        BAIL_ON_VMDIR_ERROR(dwError);
    }

    dwError = VmDirCopyMemory( pszValue, valueLen, pszLocalValue, dwLocalValueLen );
    BAIL_ON_VMDIR_ERROR(dwError);

cleanup:
    if (pszLocalValue)
    {
        RegFreeMemory(pszLocalValue);
    }
    return dwError;

error:
    VMDIR_LOG_VERBOSE( VMDIR_LOG_MASK_ALL, "VmDirGetRegKeyValue failed with error (%u)(%s)\n", dwError, pszKey);

    goto cleanup;
}

DWORD
VmDirGetRegKeyValueDword(
    PCSTR   pszConfigParamKeyPath,
    PCSTR   pszKey,
    PDWORD  pdwValue,
    DWORD   dwDefaultValue
    )
{
    DWORD dwError = 0;
    DWORD dwValue = 0;
    REG_DATA_TYPE RegType = 0;

    if (pszConfigParamKeyPath == NULL || pszKey == NULL || pdwValue == NULL)
    {
        dwError = ERROR_INVALID_PARAMETER;
        BAIL_ON_VMDIR_ERROR(dwError);
    }

    VMDIR_LOG_VERBOSE(VMDIR_LOG_MASK_ALL, "Reading Reg: %s", pszKey);

    dwError = RegUtilGetValue(
                NULL,
                HKEY_THIS_MACHINE,
                NULL,
                pszConfigParamKeyPath,
                pszKey,
                &RegType,
                (PVOID*)&dwValue,
                NULL);

    BAIL_ON_VMDIR_ERROR(dwError);

    if (RegType != REG_DWORD)
    {
        dwError = ERROR_INVALID_CONFIGURATION;
        BAIL_ON_VMDIR_ERROR(dwError);
    }

cleanup:
    *pdwValue = dwValue;
    return dwError;

error:
    dwValue = dwDefaultValue;

    VMDIR_LOG_VERBOSE(
        VMDIR_LOG_MASK_ALL,
        "VmDirGetRegKeyValueDword failed with error (%u)(%s)",
        dwError,
        pszKey);

    goto cleanup;
}

DWORD
VmDirSetRegKeyValueString(
    PCSTR pszConfigParamKeyPath,
    PCSTR pszKey,
    PCSTR pszValue,
    DWORD dwLength /* Should not include +1 for terminating null */
    )
{
    DWORD dwError;

    dwError = RegUtilSetValue(NULL,
                              HKEY_THIS_MACHINE,
                              pszConfigParamKeyPath,
                              NULL,
                              pszKey,
                              REG_SZ,
                              (BYTE*)pszValue,
                              dwLength + 1);
    BAIL_ON_VMDIR_ERROR(dwError);

cleanup:
    return dwError;

error:
    goto cleanup;
}

DWORD
VmDirSetRegKeyValueDword(
    PCSTR pszConfigParamKeyPath,
    PCSTR pszKey,
    DWORD dwValue
    )
{
    DWORD dwError;

    dwError = RegUtilSetValue(NULL,
                              HKEY_THIS_MACHINE,
                              pszConfigParamKeyPath,
                              NULL,
                              pszKey,
                              REG_DWORD,
                              &dwValue,
                              sizeof(dwValue));
    BAIL_ON_VMDIR_ERROR(dwError);

cleanup:
    return dwError;

error:
    goto cleanup;
}

DWORD
VmDirConfigSetSZKey(
    PCSTR pszKeyPath,
    PCSTR pszKeyName,
    PCSTR pszKeyValue
    )
{
    DWORD   dwError = 0;

    if (IsNullOrEmptyString(pszKeyValue))
    {
        dwError = VMDIR_ERROR_INVALID_PARAMETER;
        BAIL_ON_VMDIR_ERROR(dwError);
    }

    dwError = RegUtilSetValue(
                NULL,
                HKEY_THIS_MACHINE,
                pszKeyPath,
                NULL,
                pszKeyName,
                REG_SZ,
                (PVOID)pszKeyValue,
                VmDirStringLenA(pszKeyValue)+1);
    BAIL_ON_VMDIR_ERROR(dwError);

cleanup:

    return dwError;
error:
    VmDirLog(LDAP_DEBUG_ANY, "%s failed (%s)(%s)(%s) with error (%u)", __FUNCTION__,
             VDIR_SAFE_STRING(pszKeyPath), VDIR_SAFE_STRING(pszKeyName), VDIR_SAFE_STRING(pszKeyValue), dwError);
    goto cleanup;
}

DWORD
VmDirGetRegGuid(
    PCSTR pszKey,
    PSTR  pszGuid
    )
{
    return VmDirGetRegKeyValue( VMDIR_CONFIG_PARAMETER_KEY_PATH, pszKey, pszGuid, VMDIR_GUID_STR_LEN );
}

DWORD
VmDirGetRegKeyTabFile(
    PSTR  pszKeyTabFile
    )
{
    return VmDirGetRegKeyValue( VMAFD_CONFIG_PARAMETER_KEY_PATH, VMDIR_REG_KEY_KEYTAB_FILE, pszKeyTabFile,
                                VMDIR_MAX_FILE_NAME_LEN );
}

DWORD
VmDirGetLocalLduGuid(
    PSTR pszLduGuid
    )
{
    return VmDirGetRegGuid(VMDIR_REG_KEY_LDU_GUID, pszLduGuid);
}

DWORD
VmDirGetLocalSiteGuid(
    PSTR pszSiteGuid
    )
{
    return VmDirGetRegGuid(VMDIR_REG_KEY_SITE_GUID, pszSiteGuid);
}

DWORD
VmDirWriteDCAccountOldPassword(
    PCSTR pszOldPassword,
    DWORD dwLength /* Length of the string, not including null */
    )
{
    DWORD dwError = 0;

    dwError = VmDirSetRegKeyValueString(
                VMDIR_CONFIG_PARAMETER_KEY_PATH,
                VMDIR_REG_KEY_DC_ACCOUNT_OLD_PWD,
                pszOldPassword,
                dwLength);
    BAIL_ON_VMDIR_ERROR(dwError);

cleanup:
    return dwError;

error:
    VMDIR_LOG_VERBOSE( VMDIR_LOG_MASK_ALL, "VmDirWriteDCAccountOldPassword failed with error code: %d", dwError );
    goto cleanup;
}

DWORD
VmDirWriteDCAccountPassword(
    PCSTR pszPassword,
    DWORD dwLength /* Length of the string, not including null */
    )
{
    DWORD dwError = 0;

    dwError = VmDirSetRegKeyValueString(
                VMDIR_CONFIG_PARAMETER_KEY_PATH,
                VMDIR_REG_KEY_DC_ACCOUNT_PWD,
                pszPassword,
                dwLength);
    BAIL_ON_VMDIR_ERROR(dwError);

cleanup:
    return dwError;

error:
    VMDIR_LOG_VERBOSE( VMDIR_LOG_MASK_ALL, "VmDirWriteDCAccountPassword failed with error code: %d", dwError );
    goto cleanup;
}

DWORD
VmDirReadDCAccountPassword(
    PSTR* ppszPassword)
{
    int     dwError = 0;
    PSTR    pLocalPassword = NULL;

    dwError = VmDirAllocateMemory( VMDIR_KDC_RANDOM_PWD_LEN + 1, (PVOID *)&pLocalPassword );
    BAIL_ON_VMDIR_ERROR(dwError);

    dwError = VmDirGetRegKeyValue( VMDIR_CONFIG_PARAMETER_KEY_PATH, VMDIR_REG_KEY_DC_ACCOUNT_PWD, pLocalPassword,
                                   VMDIR_KDC_RANDOM_PWD_LEN + 1 );
    BAIL_ON_VMDIR_ERROR(dwError);

    *ppszPassword = pLocalPassword;

cleanup:
    return dwError;

error:
    VMDIR_SECURE_FREE_STRINGA(pLocalPassword);
    VMDIR_LOG_ERROR( VMDIR_LOG_MASK_ALL, "VmDirReadDCAccountPassword failed with error code: %d", dwError );
    goto cleanup;
}

DWORD
VmDirReadDCAccountOldPassword(
    PSTR* ppszPassword)
{
    int     dwError = 0;
    PSTR    pLocalPassword = NULL;

    dwError = VmDirAllocateMemory( VMDIR_KDC_RANDOM_PWD_LEN + 1, (PVOID *)&pLocalPassword );
    BAIL_ON_VMDIR_ERROR(dwError);

    dwError = VmDirGetRegKeyValue( VMDIR_CONFIG_PARAMETER_KEY_PATH, VMDIR_REG_KEY_DC_ACCOUNT_OLD_PWD, pLocalPassword,
                                   VMDIR_KDC_RANDOM_PWD_LEN + 1 );
    BAIL_ON_VMDIR_ERROR(dwError);

    *ppszPassword = pLocalPassword;

cleanup:
    return dwError;

error:
    VMDIR_SECURE_FREE_STRINGA(pLocalPassword);
    VMDIR_LOG_VERBOSE( VMDIR_LOG_MASK_ALL, "VmDirReadDCAccountOldPassword failed with error code: %d", dwError );
    goto cleanup;
}

DWORD
VmDirRegReadDCAccount(
    PSTR* ppszDCAccount
    )
{
    int     dwError = 0;
    PSTR    pszLocal = NULL;
    char    pszBuf[512] = {0};

    dwError = VmDirGetRegKeyValue( VMDIR_CONFIG_PARAMETER_KEY_PATH,
                                   VMDIR_REG_KEY_DC_ACCOUNT,
                                   pszBuf,
                                   sizeof(pszBuf)-1 );
    BAIL_ON_VMDIR_ERROR(dwError);

    dwError = VmDirAllocateStringA( pszBuf, &pszLocal );
    BAIL_ON_VMDIR_ERROR(dwError);

    *ppszDCAccount = pszLocal;

cleanup:

    return dwError;

error:
    VMDIR_SAFE_FREE_MEMORY(pszLocal);
    VMDIR_LOG_ERROR( VMDIR_LOG_MASK_ALL, "VmDirRegReadDCAccount failed (%d)", dwError );

    goto cleanup;
}

DWORD
VmDirRegReadDCAccountDn(
    PSTR* ppszDCAccount
    )
{
    int     dwError = 0;
    PSTR    pszLocal = NULL;
    char    pszBuf[512] = {0};

    dwError = VmDirGetRegKeyValue( VMDIR_CONFIG_PARAMETER_KEY_PATH,
                                   VMDIR_REG_KEY_DC_ACCOUNT_DN,
                                   pszBuf,
                                   sizeof(pszBuf)-1 );
    BAIL_ON_VMDIR_ERROR(dwError);

    dwError = VmDirAllocateStringA( pszBuf, &pszLocal );
    BAIL_ON_VMDIR_ERROR(dwError);

    *ppszDCAccount = pszLocal;

cleanup:

    return dwError;

error:
    VMDIR_SAFE_FREE_MEMORY(pszLocal);
    VMDIR_LOG_ERROR( VMDIR_LOG_MASK_ALL, "VmDirRegReadDCAccountDn failed (%d)", dwError );

    goto cleanup;
}

DWORD
VmDirRegReadKrb5Conf(
    PSTR* ppszKrb5Conf
    )
{
    int     dwError = 0;
    PSTR    pszLocal = NULL;
    char    pszBuf[512] = {0};

    dwError = VmDirGetRegKeyValue( VMAFD_CONFIG_PARAMETER_KEY_PATH,
                                   VMAFD_REG_KEY_KRB5_CONF,
                                   pszBuf,
                                   sizeof(pszBuf)-1 );
    BAIL_ON_VMDIR_ERROR(dwError);

    dwError = VmDirAllocateStringA( pszBuf, &pszLocal );
    BAIL_ON_VMDIR_ERROR(dwError);

    *ppszKrb5Conf = pszLocal;

cleanup:

    return dwError;

error:
    VMDIR_SAFE_FREE_MEMORY(pszLocal);
    VMDIR_LOG_ERROR( VMDIR_LOG_MASK_ALL, "VmDirRegReadKrb5Conf failed (%d)", dwError );

    goto cleanup;
}

BOOLEAN
VmDirRegReadJoinWithPreCopiedDB(
    VOID
    )
{
    DWORD   dwPreCopiedDB = 0;

    VmDirGetRegKeyValueDword(
        VMDIR_CONFIG_PARAMETER_V1_KEY_PATH,
        VMDIR_REG_KEY_JOIN_WITH_PRE_COPIED_DB,
        &dwPreCopiedDB,
        0);

    return dwPreCopiedDB == 1;
}

DWORD
VmDirConfigSetDefaultSiteandLduGuid(
    PCSTR pszDefaultSiteGuid,
    PCSTR pszDefaultLduGuid
    )
{
    DWORD   dwError = 0;

    if (IsNullOrEmptyString(pszDefaultSiteGuid) || IsNullOrEmptyString(pszDefaultLduGuid))
    {
        dwError = ERROR_INVALID_PARAMETER;
        BAIL_ON_VMDIR_ERROR(dwError);
    }

    dwError = RegUtilSetValue(
                NULL,
                HKEY_THIS_MACHINE,
                VMDIR_CONFIG_PARAMETER_KEY_PATH,
                NULL,
                VMDIR_REG_KEY_SITE_GUID,
                REG_SZ,
                (PVOID)pszDefaultSiteGuid,
                VmDirStringLenA(pszDefaultSiteGuid)+1);
    BAIL_ON_VMDIR_ERROR(dwError);

    dwError = RegUtilSetValue(
                NULL,
                HKEY_THIS_MACHINE,
                VMDIR_CONFIG_PARAMETER_KEY_PATH,
                NULL,
                VMDIR_REG_KEY_LDU_GUID,
                REG_SZ,
                (PVOID)pszDefaultLduGuid,
                VmDirStringLenA(pszDefaultLduGuid)+1);
    BAIL_ON_VMDIR_ERROR(dwError);

cleanup:
    return dwError;
error:
    VmDirLog(LDAP_DEBUG_ANY, "VmDirConfigSetDefaultSiteandLduGuid failed with error (%u)", dwError);
    goto cleanup;
}

DWORD
VmDirConfigSetDCAccountInfo(
    PCSTR pszDCAccount,
    PCSTR pszDCAccountDN,
    PCSTR pszDCAccountPassword,
    DWORD dwPasswordSize,
    PCSTR pszMachineGUID
    )
{
    DWORD   dwError = 0;
    PSTR    pszPasswordBuf = NULL;

    if (IsNullOrEmptyString(pszDCAccountDN) ||
        IsNullOrEmptyString(pszDCAccount) ||
        IsNullOrEmptyString(pszDCAccountPassword) ||
        IsNullOrEmptyString(pszMachineGUID) ||
        dwPasswordSize == 0)
    {
        dwError = VMDIR_ERROR_INVALID_PARAMETER;
        BAIL_ON_VMDIR_ERROR(dwError);
    }

    dwError = VmDirAllocateMemory(
                dwPasswordSize+1,
                (PVOID*)&pszPasswordBuf);
    BAIL_ON_VMDIR_ERROR(dwError);

    dwError = VmDirStringNCpyA(
                pszPasswordBuf,
                dwPasswordSize+1,
                pszDCAccountPassword,
                dwPasswordSize);
    BAIL_ON_VMDIR_ERROR(dwError);

    dwError = RegUtilSetValue(
                NULL,
                HKEY_THIS_MACHINE,
                VMDIR_CONFIG_PARAMETER_KEY_PATH,
                NULL,
                VMDIR_REG_KEY_DC_ACCOUNT_DN,
                REG_SZ,
                (PVOID)pszDCAccountDN,
                VmDirStringLenA(pszDCAccountDN)+1);
    BAIL_ON_VMDIR_ERROR(dwError);

    dwError = RegUtilSetValue(
                NULL,
                HKEY_THIS_MACHINE,
                VMDIR_CONFIG_PARAMETER_KEY_PATH,
                NULL,
                VMDIR_REG_KEY_DC_ACCOUNT,
                REG_SZ,
                (PVOID)pszDCAccount,
                VmDirStringLenA(pszDCAccount)+1);
    BAIL_ON_VMDIR_ERROR(dwError);

    dwError = RegUtilSetValue(
                NULL,
                HKEY_THIS_MACHINE,
                VMDIR_CONFIG_PARAMETER_KEY_PATH,
                NULL,
                VMDIR_REG_KEY_DC_ACCOUNT_PWD,
                REG_SZ,
                (PVOID)pszPasswordBuf,
                dwPasswordSize+1);
    BAIL_ON_VMDIR_ERROR(dwError);

    // Ignore error
    RegUtilDeleteValue(
        NULL,
        HKEY_THIS_MACHINE,
        VMDIR_CONFIG_PARAMETER_KEY_PATH,
        NULL,
        VMDIR_REG_KEY_DC_ACCOUNT_OLD_PWD);

    dwError = RegUtilSetValue(
                NULL,
                HKEY_THIS_MACHINE,
                VMDIR_CONFIG_PARAMETER_KEY_PATH,
                NULL,
                VMDIR_REG_KEY_MACHINE_GUID,
                REG_SZ,
                (PVOID)pszMachineGUID,
                VmDirStringLenA(pszMachineGUID)+1);
    BAIL_ON_VMDIR_ERROR(dwError);

cleanup:
    VMDIR_SAFE_FREE_MEMORY(pszPasswordBuf);
    return dwError;
error:
    VmDirLog(LDAP_DEBUG_ANY, "VmDirConfigSetDCAccountInfo failed with error (%u)", dwError);
    goto cleanup;
}

DWORD
VmDirConfigSetDCAccountPassword(
    PCSTR pszDCAccountPassword,
    DWORD dwPasswordSize
    )
{
    DWORD   dwError = 0;
    PSTR    pszOldPassword = NULL;

    if (IsNullOrEmptyString(pszDCAccountPassword) || dwPasswordSize == 0)
    {
        dwError = ERROR_INVALID_PARAMETER;
        BAIL_ON_VMDIR_ERROR(dwError);
    }

    /*
     * There might not be a current password so ignore any errors.
     */
    (VOID)VmDirReadDCAccountPassword(&pszOldPassword);
    BAIL_ON_VMDIR_ERROR(dwError);

    dwError = VmDirWriteDCAccountPassword(
                pszDCAccountPassword,
                dwPasswordSize);
    BAIL_ON_VMDIR_ERROR(dwError);

    if (pszOldPassword != NULL)
    {
        dwError = VmDirWriteDCAccountOldPassword(
                    pszOldPassword,
                    (DWORD)strlen(pszOldPassword));
        BAIL_ON_VMDIR_ERROR(dwError);
    }

cleanup:
    VMDIR_SAFE_FREE_MEMORY(pszOldPassword);
    return dwError;
error:
    VmDirLog(LDAP_DEBUG_ANY, "VmDirConfigSetDCAccountPassword failed with error (%u)", dwError);
    goto cleanup;
}
