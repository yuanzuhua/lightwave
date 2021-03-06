/*
 * Copyright © 2017 VMware, Inc.  All Rights Reserved.
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

static
DWORD
VmDnsRcodeErrorMetricsInit(
    VOID
    );

static
DWORD
VmDnsMetricsCounterInit(
    VOID
    );

static
DWORD
VmDnsMetricsHistogramInit(
    VOID
    );

static
DWORD
VmDnsMetricsGaugeInit(
    VOID
    );

static VM_METRICS_LABEL labelDurationOps[2][1] = {{{"operation","query"}},
                                                  {{"operation","update"}}};

static VM_METRICS_LABEL labelCacheOps[2][1] = {{{"operation","lookup"}},
                                               {{"operation","miss"}}};

static VM_METRICS_LABEL labelCachePurgeType[3][1] = {{{"operation","modify_purge"}},
                                                     {{"operation","lru_purge"}},
                                                     {{"operation","replication_purge"}}};

DWORD
VmDnsMetricsInit(
    VOID
    )
{
    DWORD dwError = 0;

    dwError = VmMetricsInit(&gVmDnsMetricsContext);
    BAIL_ON_VMDNS_ERROR(dwError);

    dwError = VmDnsRcodeErrorMetricsInit();
    BAIL_ON_VMDNS_ERROR(dwError);

    dwError = VmDnsMetricsCounterInit();
    BAIL_ON_VMDNS_ERROR(dwError);

    dwError = VmDnsMetricsHistogramInit();
    BAIL_ON_VMDNS_ERROR(dwError);

    dwError = VmDnsMetricsGaugeInit();
    BAIL_ON_VMDNS_ERROR(dwError);

cleanup:
    return dwError;

error:
    VmDnsLog(VMDNS_LOG_LEVEL_ERROR, "%s failed, error (%d)", __FUNCTION__, dwError);
    goto cleanup;
}

DWORD
VmDnsRcodeErrorMetricsInit(
    VOID
    )
{
    DWORD dwError = 0;
    DWORD i = 0, j = 0, k = 0;

    // use this template to construct labels
    VM_METRICS_LABEL labels[3] =
    {
            {"domain",      NULL},
            {"operation",   NULL},
            {"rcode",       NULL}
    };

    for (i = 0; i < METRICS_VDNS_RCODE_DOMAIN_COUNT; i++)
    {
        for (j = 0; j < METRICS_VDNS_RCODE_OP_COUNT; j++)
        {
            for (k = 0; k < METRICS_VDNS_RCODE_ERROR_COUNT; k++)
            {
                labels[0].pszValue = VmDnsMetricsRcodeDomainString(i);
                labels[1].pszValue = VmDnsMetricsRcodeOperationString(j);
                labels[2].pszValue = VmDnsMetricsRcodeErrorString(k);

                dwError = VmMetricsCounterNew(
                            gVmDnsMetricsContext,
                            "vmdns_dns_error_count",
                            labels, 3,
                            "DNS Error Counts",
                            &gVmDnsRcodeErrorMetrics[i][j][k]
                            );
                BAIL_ON_VMDNS_ERROR(dwError);
            }
        }
    }

cleanup:
    return dwError;

error:
    VmDnsLog(VMDNS_LOG_LEVEL_ERROR, "%s failed, error (%d)", __FUNCTION__, dwError);
    goto cleanup;
}

static
DWORD
VmDnsMetricsCounterInit(
    VOID
    )
{
    DWORD dwError = 0;
    int i;

    for (i=CACHE_ZONE_LOOKUP; i<=CACHE_ZONE_MISS; i++ )
    {
        dwError = VmMetricsCounterNew(
                    gVmDnsMetricsContext,
                    "vmdns_cache_zone",
                    labelCacheOps[i - CACHE_ZONE_LOOKUP],
                    1,
                    "DNS Cache Zone",
                    &gVmDnsCounterMetrics[i]
                    );
        BAIL_ON_VMDNS_ERROR(dwError);
    }
    for (i=CACHE_CACHE_LOOKUP; i<=CACHE_CACHE_MISS; i++ )
    {
        dwError = VmMetricsCounterNew(
                    gVmDnsMetricsContext,
                    "vmdns_cache_cache",
                    labelCacheOps[i - CACHE_CACHE_LOOKUP],
                    1,
                    "DNS Cache Object",
                    &gVmDnsCounterMetrics[i]
                    );
        BAIL_ON_VMDNS_ERROR(dwError);
    }
    for (i=CACHE_MODIFY_PURGE_COUNT; i<=CACHE_NOTIFY_PURGE_COUNT; i++ )
    {
        dwError = VmMetricsCounterNew(
                    gVmDnsMetricsContext,
                    "vmdns_cache_purge",
                    labelCachePurgeType[i - CACHE_MODIFY_PURGE_COUNT],
                    1,
                    "DNS Error Counts",
                    &gVmDnsCounterMetrics[i]
                    );
        BAIL_ON_VMDNS_ERROR(dwError);
    }

cleanup:
    return dwError;

error:
    VmDnsLog(VMDNS_LOG_LEVEL_ERROR, "%s failed, error (%d)", __FUNCTION__, dwError);
    goto cleanup;
}

static
DWORD
VmDnsMetricsHistogramInit(
    VOID
    )
{
    DWORD dwError = 0;
    UINT64 buckets[8] = {50, 100, 250, 500, 1000, 2500, 3000, 4000};
    int i;

    for (i=DNS_QUERY_DURATION; i<=DNS_UPDATE_DURATION; i++)
    {
        dwError = VmMetricsHistogramNew(
                    gVmDnsMetricsContext,
                    "vmdns_dns_request_duration",
                    labelDurationOps[i - DNS_QUERY_DURATION],
                    1,
                    "DNS Protocol Process Request Duration",
                    buckets,
                    8,
                    &gVmDnsHistogramMetrics[i]
                    );
        BAIL_ON_VMDNS_ERROR(dwError);
    }

    for (i=STORE_QUERY_DURATION; i<=STORE_UPDATE_DURATION; i++)
    {
        dwError = VmMetricsHistogramNew(
                    gVmDnsMetricsContext,
                    "vmdns_store_request_duration",
                    labelDurationOps[i - STORE_QUERY_DURATION],
                    1,
                    "Store Process Request Duration",
                    buckets,
                    8,
                    &gVmDnsHistogramMetrics[i]
                    );
        BAIL_ON_VMDNS_ERROR(dwError);
    }

    for (i=RPC_QUERY_DURATION; i<=RPC_UPDATE_DURATION; i++)
    {
        dwError = VmMetricsHistogramNew(
                    gVmDnsMetricsContext,
                    "vmdns_rpc_request_duration",
                    labelDurationOps[i - RPC_QUERY_DURATION],
                    1,
                    "Rpc Process Request Duration",
                    buckets,
                    8,
                    &gVmDnsHistogramMetrics[i]
                    );
        BAIL_ON_VMDNS_ERROR(dwError);
    }

cleanup:
    return dwError;

error:
    VmDnsLog(VMDNS_LOG_LEVEL_ERROR, "%s failed, error (%d)", __FUNCTION__, dwError);
    goto cleanup;
}

static
DWORD
VmDnsMetricsGaugeInit(
    VOID
    )
{
    DWORD dwError = 0;

    dwError = VmMetricsGaugeNew(
                gVmDnsMetricsContext,
                "vmdns_dns_outstanding_request_count",
                NULL,
                0,
                "Number of outstanding io requests in the queue",
                &gVmDnsGaugeMetrics[DNS_OUTSTANDING_REQUEST_COUNT]
                );
    BAIL_ON_VMDNS_ERROR(dwError);

    dwError = VmMetricsGaugeNew(
                gVmDnsMetricsContext,
                "vmdns_dns_active_service_threads",
                NULL,
                0,
                "Numbers of threads active for servicing the requests",
                &gVmDnsGaugeMetrics[DNS_ACTIVE_SERVICE_THREADS]
                );
    BAIL_ON_VMDNS_ERROR(dwError);

    dwError = VmMetricsGaugeNew(
                gVmDnsMetricsContext,
                "vmdns_cache_object_count",
                NULL,
                0,
                "Number of active objects in the cache",
                &gVmDnsGaugeMetrics[CACHE_OBJECT_COUNT]
                );
    BAIL_ON_VMDNS_ERROR(dwError);

cleanup:
    return dwError;

error:
    VmDnsLog(VMDNS_LOG_LEVEL_ERROR, "%s failed, error (%d)", __FUNCTION__, dwError);
    goto cleanup;
}
