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

#ifndef _INCLUDE_DEBUGGER_BREAKPOINT_H
#define _INCLUDE_DEBUGGER_BREAKPOINT_H

#include <sp_vm_api.h>
#include "amtl/am-hashmap.h"
#include <string>
#include "console-helpers.h"

class Breakpoint;
class Debugger;

class BreakpointManager {
public:
  BreakpointManager(Debugger* debugger) : debugger_(debugger) {}
  bool Initialize();
  Breakpoint *AddBreakpoint(const std::string& file, cell_t addr, bool temporary);
  Breakpoint *AddBreakpoint(const std::string& file, const std::string& function, bool temporary);
  bool ClearBreakpoint(int number);
  bool ClearBreakpoint(Breakpoint *);
  void ClearAllBreakpoints();
  bool CheckBreakpoint(cell_t cip);
  int FindBreakpoint(const std::string& breakpoint);
  void ListBreakpoints();
  const std::string ParseBreakpointLine(const std::string& input, std::string* filename);
  size_t GetBreakpointCount() const;

public:
  struct BreakpointMapPolicy {

    static inline uint32_t hash(ucell_t value) {
      return ke::HashInteger<4>(value);
    }

    static inline bool matches(ucell_t a, ucell_t b) {
      return a == b;
    }
  };
  typedef ke::HashMap<ucell_t, Breakpoint *, BreakpointMapPolicy> BreakpointMap;
  BreakpointMap breakpoint_map_;
  Debugger* debugger_;
};

class Breakpoint {
public:
  Breakpoint(SourcePawn::IPluginDebugInfo *debuginfo, ucell_t addr, const char *name, bool temporary = false)
    : debuginfo_(debuginfo),
    addr_(addr),
    name_(name),
    temporary_(temporary)
  {}

  ucell_t addr() {
    return addr_;
  }
  const char *name() {
    return name_;
  }
  bool temporary() {
    return temporary_;
  }
  const char *filename() {
    const char *filename;
    if (debuginfo_->LookupFile(addr_, &filename) == SP_ERROR_NONE)
      return SkipPath(filename);
    return "";
  }
  uint32_t line() {
    uint32_t line;
    if (debuginfo_->LookupLine(addr_, &line) == SP_ERROR_NONE)
      return line;
    return 0;
  }
private:
  SourcePawn::IPluginDebugInfo * debuginfo_; /* debug info of plugin the address is in */
  ucell_t addr_; /* address (in code or data segment) */
  const char *name_; /* name of the symbol (function) */
  bool temporary_; /* delete breakpoint when hit? */
};

#endif // _INCLUDE_DEBUGGER_BREAKPOINT_H
