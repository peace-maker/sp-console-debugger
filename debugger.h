/**
* vim: set ts=4 :
* =============================================================================
* SourceMod Console Debugger Extension
* Copyright (C) 2018-2021 Peace-Maker  All rights reserved.
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

#ifndef _INCLUDE_DEBUGGER_H
#define _INCLUDE_DEBUGGER_H

#include <string>
#include <vector>

#include "sp_vm_api.h"
#include "amtl/am-hashmap.h"
#include "console-helpers.h"
#include "breakpoints.h"
#include "symbols.h"

enum Runmode {
  STEPPING, /* step into functions */
  STEPOVER, /* step over functions */
  STEPOUT, /* run until the function returns */
  RUNNING, /* just run */
};

class DebuggerCommand;

class Debugger {
public:
  Debugger(SourcePawn::IPluginContext *context);
  bool Initialize();
  bool active() const {
    return active_;
  }
  void Activate();
  void Deactivate();
  SourcePawn::IPluginDebugInfo* GetDebugInfo() const;

  void HandleInput(cell_t cip, cell_t frm, bool isBp);
  void ListCommands(const std::string command);

private:
  //void HandleFrameCmd(char *params);
 // void HandleDisplayFormatChangeCmd(char *params);

public:
  Runmode runmode() const {
    return runmode_;
  }
  void SetRunmode(Runmode runmode) {
    runmode_ = runmode;
  }
  cell_t lastframe() const {
    return lastfrm_;
  }
  void SetLastFrame(cell_t lastfrm) {
    lastfrm_ = lastfrm;
  }
  uint32_t lastline() const {
    return lastline_;
  }
  void SetLastLine(uint32_t line) {
    lastline_ = line;
  }
  uint32_t breakcount() const {
    return breakcount_;
  }
  void SetBreakCount(uint32_t breakcount) {
    breakcount_ = breakcount;
  }
  const char *currentfile() const {
    return currentfile_;
  }
  void SetCurrentFile(const char *file) {
    currentfile_ = file;
  }
  const char *currentfunction() const {
    return currentfunction_;
  }
  void SetCurrentFunction(const char *function) {
    currentfunction_ = function;
  }
  BreakpointManager& breakpoints() {
    return breakpoints_;
  }
  SymbolManager& symbols() {
    return symbols_;
  }
  cell_t cip() const {
    return cip_;
  }
  cell_t frm() const {
    return frm_;
  }
  SourcePawn::IPluginContext* ctx() const {
    return selected_context_;
  }

  void DumpStack();
  void PrintCurrentPosition();
  std::string FindFileByPartialName(const std::string partialname);

private:
  std::shared_ptr<DebuggerCommand> ResolveCommandString(const std::string command);

private:
  SourcePawn::IPluginContext * context_;
  Runmode runmode_;
  cell_t lastfrm_;
  uint32_t lastline_;
  uint32_t breakcount_;
  const char *currentfile_;
  const char *currentfunction_;
  bool is_breakpoint_;
  bool active_;
  std::vector<std::shared_ptr<DebuggerCommand>> commands_;
  BreakpointManager breakpoints_;
  SymbolManager symbols_;

  // Temporary variables to use inside command loop
  cell_t cip_;
  cell_t frm_;
  uint32_t frame_count_;
  uint32_t selected_frame_;
  SourcePawn::IPluginContext *selected_context_;
};

#endif // _INCLUDE_DEBUGGER_H