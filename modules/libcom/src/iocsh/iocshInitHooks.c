/*************************************************************************\
* Copyright (C) 2020 Dirk Zimoch
* Copyright (C) 2020-2025 European Spallation Source, ERIC
* SPDX-License-Identifier: EPICS
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

#include <cantProceed.h>
#include <ellLib.h>
#include <errlog.h>
#include <initHooks.h>
#include <iocsh.h>
#include <string.h>

#include "iocshInitHooks.h"

struct cmditem {
    ELLNODE node;
    initHookState hook;
    char *cmd;
};

static ELLLIST s_cmdlist = ELLLIST_INIT;
static int s_initendflag = 0; // Defines the end of the initialization

static void univHook(const initHookState State)
{
    // Handle only specific hooks, ignore not relevant
    if (State != initHookAtBeginning &&
        State != initHookAfterIocRunning &&
        State != initHookAtShutdown) {
        return;
    }

    if (State == initHookAfterIocRunning)
        s_initendflag = 1;

    struct cmditem *item = (struct cmditem *)ellFirst(&s_cmdlist);

    while (item) {
        struct cmditem *item_next = (struct cmditem *)ellNext(&item->node);
        if (State == item->hook) {
            printf("%s\n", item->cmd);

            if (iocshCmd(item->cmd))
                printf(ERL_ERROR " iocshInitHooks: "
                                 "command '%s' failed to run for the init hook %d\n",
                       item->cmd, (int)item->hook);

            ellDelete(&s_cmdlist, &item->node);
            free(item);
        }
        item = item_next;
    }
}

static struct cmditem *cmdItemAdd(const initHookState Hook, const char *pCmd)
{
    size_t cmd_len = strlen(pCmd) + 1;

    struct cmditem *item = mallocMustSucceed(sizeof(struct cmditem) + cmd_len,
                                             ERL_ERROR " iocshInitHooks: "
                                                       "failed to allocate memory for cmditem\n");
    item->hook = Hook;
    item->cmd = (char *)(item + 1);
    strncpy(item->cmd, pCmd, cmd_len);

    ellAdd(&s_cmdlist, &item->node);

    return item;
}

typedef void (*IfaceWrapper)(const char *);

static void univIface(const iocshArgBuf *pArgs, const IfaceWrapper fIfaceWrapper)
{
    char *cmd = pArgs[0].sval;

    if (s_initendflag) {
        printf(ERL_WARNING " iocshInitHooks: "
                           "can only be used before iocInit\n");
        return;
    }

    if (!cmd || !cmd[0]) {
        printf(ERL_WARNING " iocshInitHooks: "
                           "received an empty argument\n");
        return;
    }

    fIfaceWrapper(cmd);
}

// initHookAfterIocRunning (atInit) declaration

static const iocshFuncDef atInitDef = {
    "atInit",
    1,
    (const iocshArg *[]){&(iocshArg){"command", iocshArgString}},
    "Triggered at initHookAfterIocRunning"};

static void atInitWrapper(const char *pCmd)
{
    cmdItemAdd(initHookAfterIocRunning, pCmd);
}

static void atInitFunc(const iocshArgBuf *pArgs)
{
    univIface(pArgs, atInitWrapper);
}

// initHookAtShutdown (atShut) declaration

static const iocshFuncDef atShutDef = {
    "atShut",
    1,
    (const iocshArg *[]){&(iocshArg){"command", iocshArgString}},
    "Triggered at initHookAtShutdown"};

static void atShutWrapper(const char *pCmd)
{
    cmdItemAdd(initHookAtShutdown, pCmd);
}

static void atShutFunc(const iocshArgBuf *pArgs)
{
    univIface(pArgs, atShutWrapper);
}

// initHookAtBeginning (atBegin) declaration

static const iocshFuncDef atBeginDef = {
    "atBegin",
    1,
    (const iocshArg *[]){&(iocshArg){"command", iocshArgString}},
    "Triggered at initHookAtBeginning"};

static void atBeginWrapper(const char *pCmd)
{
    cmdItemAdd(initHookAtBeginning, pCmd);
}

static void atBeginFunc(const iocshArgBuf *pArgs)
{
    univIface(pArgs, atBeginWrapper);
}

// Initialiaze all iocshInitHooks
void iocshInitHooksRegister(void)
{
    static int first_time = 1;
    if (first_time) {
        first_time = 0;
        iocshRegister(&atBeginDef, atBeginFunc);
        iocshRegister(&atInitDef, atInitFunc);
        iocshRegister(&atShutDef, atShutFunc);
        initHookRegister(univHook);
    }
}
