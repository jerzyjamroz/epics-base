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

static void atInitHook(const initHookState State)
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
    const size_t cmd_len = strlen(pCmd) + 1;

    struct cmditem *item = mallocMustSucceed(sizeof(struct cmditem) + cmd_len,
                                             ERL_ERROR " iocshInitHooks: "
                                                       "failed to allocate memory for cmditem\n");
    item->hook = Hook;
    item->cmd = (char *)(item + 1);
    strncpy(item->cmd, pCmd, cmd_len);

    ellAdd(&s_cmdlist, &item->node);

    return item;
}

static void atInitHookFunc(const iocshArgBuf *pArgs)
{
    const char *const hook = pArgs[0].sval;
    const char *const cmd = pArgs[1].sval;

    if (s_initendflag) {
        printf(ERL_WARNING " iocshInitHooks: "
                           "can only be used before iocInit\n");
        return;
    }

    if (!hook || !hook[0]) {
        printf(ERL_WARNING " iocshInitHooks: "
                           "received an empty 'hook' argument\n");
        return;
    }

    if (!cmd || !cmd[0]) {
        printf(ERL_WARNING " iocshInitHooks: "
                           "received an empty 'command' argument\n");
        return;
    }

    if (strcmp(hook, "beginning") == 0)
        cmdItemAdd(initHookAtBeginning, cmd);
    else if (strcmp(hook, "running") == 0)
        cmdItemAdd(initHookAfterIocRunning, cmd);
    else if (strcmp(hook, "shutdown") == 0)
        cmdItemAdd(initHookAtShutdown, cmd);
    else
        printf(ERL_ERROR " iocshInitHooks: "
                         "hook '%s' is not supported\n",
               hook);
}

static const iocshFuncDef atInitHookDef = {
    "atHook",
    2,
    (const iocshArg *[]){
        &(iocshArg){"<hook:{beginning|running|shutdown}>", iocshArgString},
        &(iocshArg){"<command>", iocshArgString}},
    "Allows you to define commands to be run at the specific hook\n"
    "hook 'beginning' is triggered at initHookAtBeginning\n"
    "hook 'running' is triggered at initHookAfterIocRunning\n"
    "hook 'shutdown' is triggered at initHookAtShutdown\n"
    "Example commands:\n"
    "  atHook running \"dbpf <PV> <VAL>\"\n"
    "  atHook shutdown \"date\"\n"};

// Initialiaze
void iocshInitHooksRegister(void)
{
    static int first_time = 1;
    if (first_time) {
        first_time = 0;
        iocshRegister(&atInitHookDef, atInitHookFunc);
        initHookRegister(atInitHook);
    }
}
