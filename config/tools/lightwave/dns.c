/*
 * Copyright © 2018 VMware, Inc.  All Rights Reserved.
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
VOID
ShowUsage(
    VOID
    );

int
LightwaveDns(
    int argc,
    char* argv[])
{
    int iRetCode = 0;

    if (!strcmp(argv[0], "delete-dc"))
    {
        iRetCode = LightwaveDCDnsDelete(argc-1, &argv[1]);
    }
    else
    {
        ShowUsage();
    }

    return iRetCode;
}

static
VOID
ShowUsage(
    VOID
    )
{
    char pszUsageText[] =
        "Usage: lightwave dns COMMAND {arguments}\n"
        "\n"
        "Commands:\n"
        "    delete-dc   delete DNS records of a DC\n"
        "\n"
        "Run 'lightwave dns COMMAND --help' for more information on a particular command'\n";

    printf("%s", pszUsageText);
}