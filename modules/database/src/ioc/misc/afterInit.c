/* Copyright (C) 2020 Dirk Zimoch */
/* Copyright (C) 2020-2024 European Spallation Source, ERIC */

#include <dbAccess.h>
#include <epicsExport.h>
#include <epicsStdio.h>
#include <epicsString.h>
#include <errlog.h>
#include <errno.h>
#include <initHooks.h>
#include <iocsh.h>
#include <stdlib.h>
#include <string.h>

static const char* helpMessage =
  "Usage: afterInit \"<command>\" (before iocInit)\n"
  "Allows you to define commands to be run after the iocInit\n"
  "Example commands:\n"
  "  afterInit \"dbpf <PV> <VAL>\"\n"
  "  afterInit \"date\"\n"
  "Options:\n"
  "  -h, --help : Show this help message and exit.\n";

struct cmditem
{
  struct cmditem* next;
  char* cmd;
};

static struct cmditem *cmdlist, **cmdlast = &cmdlist;

static void afterInitHook(initHookState state)
{
  if(state != initHookAfterIocRunning)
    return;

  struct cmditem* item = cmdlist;
  struct cmditem* next = NULL;
  while(item)
  {
    epicsStdoutPrintf("%s\n", item->cmd);

    if(iocshCmd(item->cmd))
      errlogPrintf("ERROR afterInit command '%s' failed to run\n", item->cmd);

    next = item->next;
    free(item->cmd);
    free(item);
    item = next;
  }
}

static struct cmditem* newItem(char* cmd)
{
  struct cmditem* item = malloc(sizeof(struct cmditem));

  if(item == NULL)
  {
    errno = ENOMEM;
    return NULL;
  }

  item->cmd = epicsStrDup(cmd);

  if(item->cmd == NULL)
  {
    free(item);
    errno = ENOMEM;
    return NULL;
  }

  item->next = NULL;
  *cmdlast = item;
  cmdlast = &item->next;
  return item;
}

static const iocshFuncDef afterInitDef = {
  "afterInit", 1,
  (const iocshArg*[]){
    &(iocshArg){"commandline", iocshArgString},
  }};

static void afterInitFunc(const iocshArgBuf* args)
{
  char* cmd = args[0].sval;

  // Check for help option
  if(cmd && (epicsStrCaseCmp(cmd, "--help") == 0 || epicsStrCaseCmp(cmd, "-h") == 0))
  {
    epicsStdoutPrintf("%s", helpMessage);
    return;
  }

  if(!cmd || !cmd[0])
  {
    errlogPrintf("WARNING Usage: afterInit \"command\", check '-h' flag for help\n");
    return;
  }

  if(interruptAccept)
  {
    errlogPrintf("WARNING afterInit can only be used before iocInit\n");
    return;
  }

  struct cmditem* item = newItem(cmd);

  if(!item)
    errlogPrintf("ERROR afterInit not adding command '%s' %s\n", cmd, strerror(errno));
}

static void afterInitRegister(void)
{
  static int after_init_unregistered = 1;
  if(after_init_unregistered)
  {
    after_init_unregistered = 0;
    iocshRegister(&afterInitDef, afterInitFunc);
    if(initHookRegister(afterInitHook) < 0)
    {
      errno = ENOMEM;
      errlogPrintf("ERROR initHookRegister memory allocation failure %s\n", strerror(errno));
    }
  }
}

epicsExportRegistrar(afterInitRegister);
