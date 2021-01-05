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
#ifndef _INCLUDE_DEBUGGER_SYMBOL_H
#define _INCLUDE_DEBUGGER_SYMBOL_H

#include <sp_vm_api.h>
#include <memory>
#include <string>

class Debugger;

class SymbolWrapper {
public:
  SymbolWrapper(Debugger* debugger, const SourcePawn::IDebugSymbol* symbol) : debugger_(debugger), symbol_(symbol) {}
  void DisplayVariable(uint32_t index[], uint32_t idxlevel);
  void PrintValue(const SourcePawn::ISymbolType* type, long value);
  const char *ScopeToString();
  bool GetSymbolValue(uint32_t index, cell_t* value);
  bool SetSymbolValue(uint32_t index, cell_t value);
  const char* GetSymbolString();
  bool SetSymbolString(const char* value);
  bool GetEffectiveSymbolAddress(cell_t *address);
  const SourcePawn::IDebugSymbol* symbol() const {
    return symbol_;
  }

private:
  Debugger* debugger_;
  const SourcePawn::IDebugSymbol* symbol_;
};

class SymbolManager {
public:
  SymbolManager(Debugger* debugger) : debugger_(debugger) {}
  std::unique_ptr<SymbolWrapper> FindDebugSymbol(const std::string name, cell_t scopeaddr, SourcePawn::IDebugSymbolIterator* symbol_iterator);

private:
  Debugger* debugger_;
};

#endif // _INCLUDE_DEBUGGER_SYMBOL_H