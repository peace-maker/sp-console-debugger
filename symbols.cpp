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
#include "symbols.h"
#include "debugger.h"

std::unique_ptr<SymbolWrapper>
SymbolManager::FindDebugSymbol(const std::string name, cell_t scopeaddr, SourcePawn::IDebugSymbolIterator* symbol_iterator)
{
  cell_t codestart = 0, codeend = 0;
  const SourcePawn::IDebugSymbol* matching_symbol = nullptr;

  SourcePawn::IPluginDebugInfo *debuginfo = debugger_->ctx()->GetRuntime()->GetDebugInfo();
  while (!symbol_iterator->Done()) {
    const SourcePawn::IDebugSymbol* sym = nullptr;
    while (!symbol_iterator->Done()) {
      sym = symbol_iterator->Next();
      // find (next) matching variable
      if (name == sym->name())
        break;
    }

    // check the range, keep a pointer to the symbol with the smallest range
    if (name == sym->name() &&
      ((codestart == 0 && codeend == 0) ||
      (sym->codestart() >= codestart &&
        sym->codeend() <= codeend)))
    {
      matching_symbol = sym;
      codestart = sym->codestart();
      codeend = sym->codeend();
    }
  }
  return std::make_unique<SymbolWrapper>(debugger_, matching_symbol);
}

void
SymbolWrapper::DisplayVariable(uint32_t index[], uint32_t idxlevel)
{
  assert(index != nullptr);

  // first check whether the variable is visible at all
  if (debugger_->cip() < symbol_->codestart() || debugger_->cip() > symbol_->codeend()) {
    fputs("(not in scope)", stdout);
    return;
  }

  const SourcePawn::ISymbolType* type = symbol_->type();
  if (type->isArray()) {
    // Check whether any of the indices are out of range
    uint32_t dim;
    for (dim = 0; dim < idxlevel && dim < type->dimcount(); dim++) {
      if (type->dimension(dim) > 0 && index[dim] >= type->dimension(dim))
        break;
    }
    if (dim < idxlevel) {
      fputs("(index out of range)", stdout);
      return;
    }
  }

  cell_t value;
  if (type->isEnumStruct()) {
    uint32_t idx[sDIMEN_MAX];
    memset(idx, 0, sizeof(idx));
    fputs("{", stdout);
    for (size_t i = 0; i < type->esfieldcount(); i++) {
      if (i > 0)
        fputs(", ", stdout);

      const SourcePawn::IEnumStructField* field = type->esfield(i);
      printf("%s: ", field->name());
      if (field->type()->isArray())
        fputs("(array)", stdout);
      else {
        if (GetSymbolValue(field->offset(), &value))
          PrintValue(field->type(), value);
        else
          fputs("?", stdout);
      }
    }
    fputs("}", stdout);
  }
  // Print first dimension of array
  else if (type->isArray() && idxlevel == 0)
  {
    // Print string
    if (type->isString()) {
      const char *str = GetSymbolString();
      if (str != nullptr)
        printf("\"%s\"", str); // TODO: truncate to 40 chars
      else
        fputs("NULL_STRING", stdout);
    }
    // Print one-dimensional array
    else if (type->dimcount() == 1) {
      uint32_t len = type->dimension(0);
      // Only print the first 5 elements
      if (len > 5)
        len = 5;
      else if (len == 0)
        len = 1; // unknown array length, assume at least 1 element

      fputs("{", stdout);
      uint32_t i;
      for (i = 0; i < len; i++) {
        if (i > 0)
          fputs(",", stdout);
        if (GetSymbolValue(i, &value))
          PrintValue(type, value);
        else
          fputs("?", stdout);
      }
      if (len < type->dimension(0) || type->dimension(0) == 0)
        fputs(",...", stdout);
      fputs("}", stdout);
    }
    // Not supported..
    else {
      fputs("(multi-dimensional array)", stdout);
    }
  }
  else if (!type->isArray() && idxlevel > 0) {
    // index used on a non-array
    fputs("(invalid index, not an array)", stdout);
  }
  else {
    // simple variable, or indexed array element
    assert(idxlevel > 0 || index[0] == 0); // index should be zero if non-array
    uint32_t dim = 0;
    cell_t base = 0;
    if (idxlevel > 0) {
      for (; dim < idxlevel - 1; dim++) {
        base += index[dim];
        if (!GetSymbolValue(base, &value))
          break;
        base += value / sizeof(cell_t);
      }
    }

    if (GetSymbolValue(base + index[dim], &value) &&
      type->dimcount() == idxlevel)
      PrintValue(type, value);
    else if (type->dimcount() != idxlevel)
      fputs("(invalid number of dimensions)", stdout);
    else
      fputs("?", stdout);
  }
}

void
SymbolWrapper::PrintValue(const SourcePawn::ISymbolType* type, long value)
{
  if (type->isFloat()) {
    printf("%f", sp_ctof(value));
  }
  else if (type->isBoolean()) {
    switch (value)
    {
    case 0:
      fputs("false", stdout);
      break;
    case 1:
      fputs("true", stdout);
      break;
    default:
      printf("%ld (false)", value);
      break;
    }
  }
  else {
    printf("%ld", value);
  }
  /*case DISP_HEX:
    printf("%lx", value);
    break;*/
}

const char *
SymbolWrapper::ScopeToString()
{
  SourcePawn::SymbolScope scope = symbol_->scope();
  static const char *scope_names[] = { "glb", "loc", "sta", "arg" };
  if (scope < 0 || scope >= sizeof(scope_names) / sizeof(scope_names[0]))
    return "unk";
  return scope_names[scope];
}

bool
SymbolWrapper::GetSymbolValue(uint32_t index, cell_t* value)
{
  // Tried to index a non-array.
  if (index > 0 && !symbol_->type()->isArray())
    return false;

  // Index out of bounds.
  if (symbol_->type()->dimcount() > 0 && index >= symbol_->type()->dimension(0))
    return false;

  cell_t addr;
  if (!GetEffectiveSymbolAddress(&addr))
    return false;

  // Resolve index into array.
  cell_t *vptr;
  if (debugger_->ctx()->LocalToPhysAddr(addr + index * sizeof(cell_t), &vptr) != SP_ERROR_NONE)
    return false;

  if (vptr != nullptr)
    *value = *vptr;

  return vptr != nullptr;
}

bool
SymbolWrapper::SetSymbolValue(uint32_t index, cell_t value)
{
  // Tried to index a non-array.
  if (index > 0 && !symbol_->type()->isArray())
    return false;

  // Index out of bounds.
  if (symbol_->type()->dimcount() > 0 && index >= symbol_->type()->dimension(0))
    return false;

  cell_t addr;
  if (!GetEffectiveSymbolAddress(&addr))
    return false;

  // Resolve index into array.
  cell_t *vptr;
  if (debugger_->ctx()->LocalToPhysAddr(addr + index * sizeof(cell_t), &vptr) != SP_ERROR_NONE)
    return false;

  if (vptr != nullptr)
    *vptr = value;

  return vptr != nullptr;
}

const char*
SymbolWrapper::GetSymbolString()
{
  // Make sure we're dealing with a one-dimensional array.
  if (!symbol_->type()->isArray() || symbol_->type()->dimcount() != 1)
    return nullptr;

  cell_t addr;
  if (!GetEffectiveSymbolAddress(&addr))
    return false;

  char *str;
  if (debugger_->ctx()->LocalToStringNULL(addr, &str) != SP_ERROR_NONE)
    return nullptr;
  return str;
}

bool
SymbolWrapper::SetSymbolString(const char* value)
{
  // Make sure we're dealing with a one-dimensional array.
  if (!symbol_->type()->isArray() || symbol_->type()->dimcount() != 1)
    return nullptr;

  cell_t addr;
  if (!GetEffectiveSymbolAddress(&addr))
    return false;

  return debugger_->ctx()->StringToLocalUTF8(addr, symbol_->type()->dimension(0), value, NULL) == SP_ERROR_NONE;
}

bool
SymbolWrapper::GetEffectiveSymbolAddress(cell_t *address)
{
  cell_t base = symbol_->address();
  // addresses of local vars are relative to the frame
  if (symbol_->scope() == SourcePawn::Local || symbol_->scope() == SourcePawn::Argument)
    base += debugger_->frm();

  // a reference
  cell_t *addr;
  if (symbol_->type()->isReference()) {
    if (debugger_->ctx()->LocalToPhysAddr(base, &addr) != SP_ERROR_NONE)
      return false;

    assert(addr != nullptr);
    base = *addr;
  }

  *address = base;
  return true;
}
