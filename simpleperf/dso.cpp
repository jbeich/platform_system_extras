/*
 * Copyright (C) 2015 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "dso.h"

#include "environment.h"
#include "read_elf.h"

bool SymbolComparator::operator()(const SymbolEntry& symbol1, const SymbolEntry& symbol2) {
  return symbol1.addr < symbol2.addr;
}

const SymbolEntry* DsoEntry::FindSymbol(uint64_t offset_in_dso) {
  SymbolEntry symbol = {
      .name = "", .addr = offset_in_dso, .len = 0,
  };

  auto it = symbols.upper_bound(symbol);
  if (it != symbols.begin()) {
    --it;
    if (it->addr <= offset_in_dso && it->addr + it->len > offset_in_dso) {
      return &*it;
    }
  }
  return nullptr;
}

static bool IsKernelFunctionSymbol(const KernelSymbol& symbol) {
  return (symbol.type == 'T' || symbol.type == 't' || symbol.type == 'W' || symbol.type == 'w');
}

static bool KernelSymbolCallback(const KernelSymbol& symbol, DsoEntry* dso) {
  if (IsKernelFunctionSymbol(symbol)) {
    SymbolEntry entry;
    entry.name = symbol.name;
    entry.addr = symbol.addr;
    entry.len = 0;
    dso->symbols.insert(entry);
  }
  return false;
}

DsoEntry DsoFactory::LoadKernel() {
  DsoEntry dso;
  dso.path = "[kernel.kallsyms]";

  ProcessKernelSymbols("/proc/kallsyms",
                       std::bind(&KernelSymbolCallback, std::placeholders::_1, &dso));
  // Fix symbol.len.
  auto prev_it = dso.symbols.end();
  for (auto it = dso.symbols.begin(); it != dso.symbols.end(); ++it) {
    if (prev_it != dso.symbols.end()) {
      const_cast<SymbolEntry*>(&*prev_it)->len = it->addr - prev_it->addr;
    }
    prev_it = it;
  }
  if (prev_it != dso.symbols.end()) {
    const_cast<SymbolEntry*>(&*prev_it)->len = ULLONG_MAX - prev_it->addr;
  }
  return dso;
}

static void ParseSymbolCallback(const ElfFileSymbol& elf_symbol, DsoEntry* dso,
                                bool (*filter)(const ElfFileSymbol&)) {
  if (filter(elf_symbol)) {
    SymbolEntry symbol;
    symbol.name = elf_symbol.name;
    symbol.addr = elf_symbol.start_in_file;
    symbol.len = elf_symbol.len;
    dso->symbols.insert(symbol);
  }
}

static bool SymbolFilterForKernelModule(const ElfFileSymbol& elf_symbol) {
  // TODO: Parse symbol outside of .text section.
  return (elf_symbol.is_func && elf_symbol.is_in_text_section);
}

DsoEntry DsoFactory::LoadKernelModule(const std::string& dso_path) {
  DsoEntry dso;
  dso.path = dso_path;
  ParseSymbolsFromElfFile(dso_path, std::bind(ParseSymbolCallback, std::placeholders::_1, &dso,
                                              SymbolFilterForKernelModule));
  return dso;
}

static bool SymbolFilterForDso(const ElfFileSymbol& elf_symbol) {
  return elf_symbol.is_func;
}

DsoEntry DsoFactory::LoadDso(const std::string& dso_path) {
  DsoEntry dso;
  dso.path = dso_path;
  ParseSymbolsFromElfFile(
      dso_path, std::bind(ParseSymbolCallback, std::placeholders::_1, &dso, SymbolFilterForDso));
  return dso;
}
