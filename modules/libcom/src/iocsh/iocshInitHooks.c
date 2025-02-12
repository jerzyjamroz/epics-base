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
#include <epicsTypes.h>

#include "iocshInitHooks.h"

// The registered/supported iocsh hooks
static const struct {
    const char *name;
    initHookState state;
} iocshInitHooksRegistry[] = {
    {"init", initHookAfterInitDatabase},
    {"running", initHookAfterIocRunning},
    {"shutdown", initHookAtShutdown}};

static const iocshArg iocshInitHookArg0 = {"<hook:{init|running|shutdown}>", iocshArgString};
static const iocshArg iocshInitHookArg1 = {"<command>", iocshArgString};
static const iocshArg *const iocshInitHookArgs[] = {&iocshInitHookArg0, &iocshInitHookArg1};
static const iocshFuncDef iocshInitHookDef = {
    "atHook",
    2,
    iocshInitHookArgs,
    "Allows you to define commands to be run at the specific hook:\n"
    "  hook 'init' is triggered after iocInit and before autosave\n"
    "  hook 'running' is triggered after iocInit and after autosave\n"
    "  hook 'shutdown' is triggered at the IOC shutdown\n"
    "Example commands:\n"
    "  atHook running \"dbpf <PV> <VAL>\"\n"
    "  atHook shutdown \"date\"\n"};

struct cmditem {
    ELLNODE node;
    initHookState hook;
    char *cmd;
};

static ELLLIST cmdList = ELLLIST_INIT;
static int initEndFlag = 0; // Defines the end of the initialization

static void iocshInitHook(const initHookState HookState)
{
    const char *valid_hook_name = NULL;
    struct cmditem *item = NULL, *item_next = NULL;
    size_t i = 0;

    if (HookState == initHookAfterIocRunning)
        initEndFlag = 1;

    // Validate the defined hooks only
    for (i = 0; i < sizeof(iocshInitHooksRegistry) / sizeof(iocshInitHooksRegistry[0]); i++) {
        if (iocshInitHooksRegistry[i].state == HookState) {
            valid_hook_name = iocshInitHooksRegistry[i].name;
            break;
        }
    }
    if (valid_hook_name == NULL) return;

    item = (struct cmditem *)ellFirst(&cmdList);
    while (item) {
        item_next = (struct cmditem *)ellNext(&item->node);
        if (HookState == item->hook) {
            printf(ANSI_GREEN("iocshInitHooks %s: ") "%s\n",
                   valid_hook_name, item->cmd);

            if (iocshCmd(item->cmd))
                printf(ERL_ERROR " iocshInitHooks %s: "
                                 "command '%s' failed to run\n",
                       valid_hook_name, item->cmd);

            ellDelete(&cmdList, &item->node);
            free(item);
        }
        item = item_next;
    }
}

static struct cmditem *cmdItemAdd(const initHookState HookState, const char *pCmd)
{
    const size_t cmd_len = strnlen(pCmd, MAX_STRING_SIZE - 1) + 1;

    struct cmditem *item = mallocMustSucceed(sizeof(struct cmditem) + cmd_len, "iocshInitHooks");
    item->hook = HookState;
    item->cmd = (char *)(item + 1);
    memcpy(item->cmd, pCmd, cmd_len);

    ellAdd(&cmdList, &item->node);

    return item;
}

static void iocshInitHookFunc(const iocshArgBuf *pArgs)
{
    const char *const hook = pArgs[0].sval;
    const char *const cmd = pArgs[1].sval;
    size_t i = 0;

    if (initEndFlag) {
        printf(ERL_WARNING " iocshInitHooks: "
                           "can only be used before iocInit\n");
        return;
    }

    if (!hook || !hook[0]) {
        printf(ERL_ERROR " iocshInitHooks: "
                         "received an empty 'hook' argument\n");
        return;
    }

    if (!cmd || !cmd[0]) {
        printf(ERL_ERROR " iocshInitHooks: "
                         "received an empty 'command' argument\n");
        return;
    }

    for (i = 0; i < sizeof(iocshInitHooksRegistry) / sizeof(iocshInitHooksRegistry[0]); i++) {
        if (strcmp(hook, iocshInitHooksRegistry[i].name) == 0) {
            cmdItemAdd(iocshInitHooksRegistry[i].state, cmd);
            return;
        }
    }

    printf(ERL_ERROR " iocshInitHooks: "
                     " hook '%s' is not supported\n",
           hook);
}

// Initialiaze
void iocshInitHooksRegister(void)
{
    static int first_time = 1;
    if (first_time) {
        first_time = 0;
        iocshRegister(&iocshInitHookDef, iocshInitHookFunc);
        initHookRegister(iocshInitHook);
    }
}
