/**
 * vim: set ts=4 :
 * =============================================================================
 * SourceMod Console Debugger Extension
 * Copyright (C) 2016-2018 AlliedModders LLC.  All rights reserved.
 * =============================================================================
 *
 * This program is free software; you can redistribute it and/or modify it under
 * the terms of the GNU General Public License, version 3.0, as published by the
 * Free Software Foundation.
 * 
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * As a special exception, AlliedModders LLC gives you permission to link the
 * code of this program (as well as its derivative works) to "Half-Life 2," the
 * "Source Engine," the "SourcePawn JIT," and any Game MODs that run on software
 * by the Valve Corporation.  You must obey the GNU General Public License in
 * all respects for all other code used.  Additionally, AlliedModders LLC grants
 * this exception to all derivative works.  AlliedModders LLC defines further
 * exceptions, found in LICENSE.txt (as of this writing, version JULY-31-2007),
 * or <http://www.sourcemod.net/license.php>.
 *
 * Version: $Id$
 */

#include "extension.h"
#include "debugger.h"
#include "console-helpers.h"
#include <amtl/am-platform.h>
#include <amtl/am-autoptr.h>
#include <amtl/am-string.h>
#ifdef KE_POSIX
#include <ctype.h>
#endif

using namespace ke;

#define DEBUGGER_CONTEXT_KEY 4

/**
 * @file extension.cpp
 * @brief Implement extension code here.
 */

ConsoleDebugger g_Debugger;		/**< Global singleton for extension's main interface */

SMEXT_LINK(&g_Debugger);

void OnDebugBreak(IPluginContext *ctx, sp_debug_break_info_t& dbginfo, const SourcePawn::IErrorReport *report);

bool
ConsoleDebugger::SDK_OnLoad(char *error, size_t maxlength, bool late)
{
  plsys->AddPluginsListener(this);

  rootconsole->AddRootConsoleCommand3("debug", "Debug Plugins", this);

  if (!late)
    return true;

  IPluginIterator *pliter = plsys->GetPluginIterator();
  while (pliter->MorePlugins())
  {
    IPlugin *pl = pliter->GetPlugin();
    OnPluginLoaded(pl);
    pliter->NextPlugin();
  }
  delete pliter;

  return true;
}

void
ConsoleDebugger::SDK_OnUnload()
{
  plsys->RemovePluginsListener(this);
  rootconsole->RemoveRootConsoleCommand("debug", this);

  IPluginIterator *pliter = plsys->GetPluginIterator();
  while (pliter->MorePlugins())
  {
    IPlugin *pl = pliter->GetPlugin();
    OnPluginUnloaded(pl);
    pliter->NextPlugin();
  }
  delete pliter;
}

void
ConsoleDebugger::OnPluginLoaded(IPlugin *plugin)
{
  SPVM_DEBUGBREAK oldDebugCallback;
  int err = plugin->GetBaseContext()->SetDebugBreak(OnDebugBreak, &oldDebugCallback);
  if (err != SP_ERROR_NONE) {
    smutils->LogError(myself, "Failed to install debug break callback %d. Newer VM required?", err);
    return;
  }

  // Create a debugger object for this plugin.
  Debugger *debugger = new Debugger(plugin->GetBaseContext(), oldDebugCallback);
  if (!debugger->Initialize()) {
    smutils->LogError(myself, "Failed to initialize debugger instance for plugin %s.", plugin->GetFilename());
    delete debugger;
    return;
  }
  SetPluginDebugger(plugin->GetBaseContext(), debugger);

  // Start debugging this plugin right away.
  if (debug_next_plugin_) {
    debug_next_plugin_ = false;

    debugger->Activate();
    debugger->SetRunmode(STEPPING);
  }
}

void
ConsoleDebugger::OnPluginUnloaded(IPlugin *plugin)
{
  // Cleanup after the plugin.
  Debugger *debugger = GetPluginDebugger(plugin->GetBaseContext());
  if (!debugger)
    return;

  // Restore old callback function.
  SPVM_DEBUGBREAK oldDebugCallback;
  plugin->GetBaseContext()->SetDebugBreak(debugger->olddebughandler(), &oldDebugCallback);
  assert(oldDebugCallback == OnDebugBreak);

  delete debugger;
  SetPluginDebugger(plugin->GetBaseContext(), nullptr);
}

void
ConsoleDebugger::OnRootConsoleCommand(const char *cmdname, const ICommandArgs *args)
{
  int argcount = args->ArgC();
  if (argcount < 3) {
    // Draw the main menu
    rootconsole->ConsolePrint("SourceMod Debug Menu:");
    rootconsole->DrawGenericOption("start", "Start debugging a plugin");
    rootconsole->DrawGenericOption("next", "Start debugging the plugin which is loaded next");
    rootconsole->DrawGenericOption("bp", "Handle breakpoints in a plugin");
    return;
  }
  
  const char *cmd = args->Arg(2);
  if (!strcmp(cmd, "start")) {
    if (argcount < 4) {
      rootconsole->ConsolePrint("[SM] Usage: sm debug start <#|file>");
      return;
    }

    const char *plugin = args->Arg(3);
    IPlugin *pl = FindPluginByConsoleArg(plugin);
    if (!pl) {
      rootconsole->ConsolePrint("[SM] Plugin %s is not loaded.", plugin);
      return;
    }

    char name[PLATFORM_MAX_PATH];
    const sm_plugininfo_t *info = pl->GetPublicInfo();
    if (pl->GetStatus() <= Plugin_Paused)
      SafeStrcpy(name, sizeof(name), (info->name[0] != '\0') ? info->name : pl->GetFilename());
    else
      SafeStrcpy(name, sizeof(name), pl->GetFilename());

    // Break on the next instruction.
    if (StartPluginDebugging(pl->GetBaseContext()))
      rootconsole->ConsolePrint("[SM] Pausing plugin %s for debugging. Will halt on next instruction.", name);
    else
      rootconsole->ConsolePrint("[SM] Failed to pause plugin %s for debugging.", name);
  }
  else if (!strcmp(cmd, "next")) {
    debug_next_plugin_ = true;
    rootconsole->ConsolePrint("[SM] Will halt on the first instruction of the next loaded plugin.");
  }
  else if (!strcmp(cmd, "bp")) {
    if (argcount < 5) {
      // Draw the sub menu
      rootconsole->ConsolePrint("[SM] Usage: sm debug bp <#|file> <option>");
      rootconsole->DrawGenericOption("list", "List breakpoints");
      rootconsole->DrawGenericOption("add", "Add a breakpoint");
      rootconsole->DrawGenericOption("clear", "Remove a breakpoint");
      return;
    }

    const char *plugin = args->Arg(3);
    IPlugin *pl = FindPluginByConsoleArg(plugin);
    if (!pl) {
      rootconsole->ConsolePrint("[SM] Plugin %s is not loaded.", plugin);
      return;
    }

    // Get a nice printable name.
    char name[PLATFORM_MAX_PATH];
    const sm_plugininfo_t *info = pl->GetPublicInfo();
    if (pl->GetStatus() <= Plugin_Paused)
      SafeStrcpy(name, sizeof(name), (info->name[0] != '\0') ? info->name : pl->GetFilename());
    else
      SafeStrcpy(name, sizeof(name), pl->GetFilename());

    // Get the debugger instance.
    Debugger *debugger = GetPluginDebugger(pl->GetBaseContext());
    if (!debugger || !debugger->active()) {
      rootconsole->ConsolePrint("[SM] Debugger is not active on plugin %s.", name);
      return;
    }

    // Check the sub command.
    const char *arg = args->Arg(4);
    if (!strcmp(arg, "list")) {
      rootconsole->ConsolePrint("[SM] Listing %zu breakpoint(s) for plugin %s:", debugger->GetBreakpointCount(), name);

      debugger->ListBreakpoints();
    }
    else if (!strcmp(arg, "add")) {
      if (argcount < 6) {
        rootconsole->ConsolePrint("[SM] Usage: sm debug bp <#|file> add <file:line | file:function>");
        return;
      }

      const char *bpline = args->Arg(5);

      // check if a filename precedes the breakpoint location
      const char *filename = nullptr;
      bpline = debugger->ParseBreakpointLine((char *)bpline, &filename);

      // User didn't specify a filename. 
      // Use the main source file by default (last one in the list).
      if (!filename) {
        IPluginDebugInfo *debuginfo = pl->GetRuntime()->GetDebugInfo();
        if (debuginfo->NumFiles() <= 0)
          return;
        filename = debuginfo->GetFileName(debuginfo->NumFiles() - 1);
      }

      Breakpoint *bp = nullptr;
      // User specified a line number
      if (isdigit(*bpline))
        bp = debugger->AddBreakpoint(filename, strtol(bpline, NULL, 10) - 1, false);
      // User specified a function name
      else
        bp = debugger->AddBreakpoint(filename, bpline, false);

      if (!bp)
        rootconsole->ConsolePrint("[SM] Invalid breakpoint address specification.");
      else
        rootconsole->ConsolePrint("[SM] Added breakpoint in file %s on line %d", bp->filename(), bp->line());
    }
    // Remove a breakpoint for a plugin.
    else if (!strcmp(arg, "clear")) {
      if (argcount < 6) {
        rootconsole->ConsolePrint("[SM] Usage: sm debug bp <#|file> clear <#>");
        return;
      }

      const char *bpstr = args->Arg(5);
      int bpnum = strtoul(bpstr, NULL, 10);
      if (debugger->ClearBreakpoint(bpnum))
        rootconsole->ConsolePrint("[SM] Breakpoint cleared.");
      else
        rootconsole->ConsolePrint("[SM] Failed to clear breakpoint.");
    }
  }
}

IPlugin *
ConsoleDebugger::FindPluginByConsoleArg(const char *arg)
{
  int id;
  char *end;
  IPlugin *pl;

  id = strtol(arg, &end, 10);

  AutoPtr<IPluginIterator> iter(plsys->GetPluginIterator());
  if (*end == '\0')
  {
    // Get plugin by order in the list.
    if (id < 1 || id >(int)plsys->GetPluginCount())
      return nullptr;

    for (int i = 1; iter->MorePlugins() && i < id; iter->NextPlugin(), i++) {
      // Empty loop.
    }
    pl = iter->GetPlugin();
    if (!pl)
      return nullptr;
  }
  else
  {
    // Check for a plugin with that name.
    char pluginfile[256];
    const char *ext = libsys->GetFileExtension(arg) ? "" : ".smx";
    libsys->PathFormat(pluginfile, sizeof(pluginfile), "%s%s", arg, ext);

    for (; iter->MorePlugins(); iter->NextPlugin()) {
      pl = iter->GetPlugin();

      if (!strcmp(pl->GetFilename(), pluginfile))
        return pl;
    }
  }

  return pl;
}

bool
ConsoleDebugger::StartPluginDebugging(IPluginContext *ctx)
{
  Debugger *debugger = GetPluginDebugger(ctx);
  if (!debugger)
    return false;

  if (!ctx->IsDebugging())
    return false;

  if (ctx->GetRuntime()->IsPaused())
    return false;

  debugger->Activate();
  debugger->SetRunmode(STEPPING);

  return true;
}

Debugger *
GetPluginDebugger(IPluginContext *ctx)
{
  Debugger *debugger = nullptr;
  if (!ctx->GetKey(DEBUGGER_CONTEXT_KEY, (void **)&debugger) || !debugger)
    return nullptr;
  return debugger;
}

void
SetPluginDebugger(IPluginContext *ctx, Debugger *debugger)
{
  ctx->SetKey(DEBUGGER_CONTEXT_KEY, debugger);
}

void
OnDebugBreak(IPluginContext *ctx, sp_debug_break_info_t& dbginfo, const SourcePawn::IErrorReport *report)
{
  // Make sure we know how to interpret the data.
  if (dbginfo.version > DEBUG_BREAK_INFO_VERSION) {
    smutils->LogError(myself, "VM is too new. Debug context version is %x (only support up to %x).", dbginfo.version, DEBUG_BREAK_INFO_VERSION);
    g_Debugger.OnPluginUnloaded(plsys->FindPluginByContext(ctx->GetContext()));
    return;
  }

  // Try to get the debugger instance for this plugin.
  Debugger *debugger = GetPluginDebugger(ctx);
  if (!debugger)
    return;

  // Continue normal execution, if this plugin isn't being debugged.
  if (!debugger->active())
    return;

  IPluginDebugInfo *debuginfo = ctx->GetRuntime()->GetDebugInfo();
  bool isBreakpoint = false;

  // Was there an exception instead of a dbreak instruction?
  if (report) {
    if (report->IsFatal())
      printf("STOP on FATAL exception: %s\n", report->Message());
    else
      printf("STOP on exception: %s\n", report->Message());

    uint32_t line = 0;
    debuginfo->LookupLine(dbginfo.cip, &line);

    // Remember on which line we halt for the next time.
    debugger->SetLastLine(line);
    debugger->SetBreakCount(0);
  }
  else {
    // Remember which runmode we've been in,
    // before changing to stepping below.
    Runmode orig_runmode = debugger->runmode();

    // When running until the function returns, 
    // check the current frame address against 
    // the saved one from the function.
    // If the current one is higher than the saved one,
    // we're in a caller, so start stepping again!
    if (debugger->runmode() == Runmode::STEPOUT &&
      dbginfo.frm > debugger->lastframe())
    {
      debugger->SetRunmode(Runmode::STEPPING);
    }

    // See if there is a breakpoint set at the current cip.
    // We don't need to check for breakpoints, 
    // if we're halting on each line anyways.
    if (debugger->runmode() != Runmode::STEPPING &&
      debugger->runmode() != Runmode::STEPOVER)
    {
      // Check breakpoint address
      isBreakpoint = debugger->CheckBreakpoint(dbginfo.cip);
      // Continue execution normally.
      if (!isBreakpoint)
        return;

      // There is a breakpoint! Start stepping through the plugin.
      debugger->SetRunmode(Runmode::STEPPING);
    }

    // Count how often we hit a breakpoint on this line.
    // Don't break multiple times on the same line,
    // and return to the previous runmode, if we didn't
    // break on this line more than 5 times already.
    debugger->SetBreakCount(debugger->breakcount() + 1);

    // Try to avoid halting on the same line twice.
    uint32_t line = 0;
    if (debuginfo->LookupLine(dbginfo.cip, &line) == SP_ERROR_NONE) {
      // Assume that there are no more than 5 breaks on a single line.
      // If there are, halt.
      if (line == debugger->lastline() && debugger->breakcount() < 5) {
        debugger->SetRunmode(orig_runmode);
        return;
      }
    }

    // Remember on which line we halt for the next time.
    debugger->SetLastLine(line);
    debugger->SetBreakCount(0);

    // If we want to skip calls, check whether
    // we are stepping through a sub-function.
    // The lastframe is set after changing to a STEPOVER or STEPOUT runmode.
    if (debugger->runmode() == Runmode::STEPOVER) {
      assert(debugger->lastframe() != 0);
      // If we're in a lower frame, just execute the code.
      if (dbginfo.frm < debugger->lastframe())
        return;
    }
  }

  // Remember which file we're in.
  const char *filename;
  if (debuginfo->LookupFile(dbginfo.cip, &filename) == SP_ERROR_NONE) {
    debugger->SetCurrentFile(filename);
  }

  // Echo input back and enable basic control.
  // This helps to have a shell-like typing experience.
  // Features depend on the operating system.
  unsigned int old_flags = EnableTerminalEcho();

  // Disable the game's watchdog timer while we're in the debug shell.
  unsigned int oldtimeout = DisableEngineWatchdog();

  // Start the debugger shell and wait for commands.
  debugger->HandleInput(dbginfo.cip, isBreakpoint);

  // Enable the watchdog timer again if it was enabled before.
  ResetEngineWatchdog(oldtimeout);

  // Reset the console input mode back to the normal flags.
  ResetTerminalEcho(old_flags);

  // step OVER functions (so save the stack frame)
  if (debugger->runmode() == Runmode::STEPOVER ||
    debugger->runmode() == Runmode::STEPOUT)
  {
    debugger->SetLastFrame(dbginfo.frm);
  }
}
