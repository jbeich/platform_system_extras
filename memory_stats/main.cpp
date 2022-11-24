/*
 * Copyright (C) 2022 The Android Open Source Project
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

#include "MemoryStats.h"

#include "OpGenStats.h"
#include "OpOverlap.h"

#include <string_view>

#include <iostream>

namespace {
class OptionBase;

class OptionRegistry {
  public:
    static OptionRegistry& get() {
        static OptionRegistry registry;
        return registry;
    }

    void RegisterOption(OptionBase* opt) { options_.push_back(opt); }

    auto begin() { return options_.begin(); }
    auto end() { return options_.end(); }

    void Dump(std::ostream& os, std::string_view indent = "\t");

  private:
    OptionRegistry() {}

    std::vector<OptionBase*> options_;
};

class OptionBase {
  public:
    template <typename... Analyses>
    OptionBase(std::string_view name, std::string_view desc) : name_(name), desc_(desc) {
        OptionRegistry::get().RegisterOption(this);
    }

    std::string_view getName() const { return name_; }
    std::string_view getDesc() const { return desc_; }

    void GetAnalyses(std::vector<Analysis*>& analyses) {
        for (auto& analysis : analyses_) analyses.push_back(analysis.get());
    }

  protected:
    template <typename Analysis, typename... Analyses,
              std::enable_if_t<(sizeof...(Analyses) > 0), bool> = true>
    void AppendAnalyses() {
        AppendAnalyses<Analysis>();
        AppendAnalyses<Analyses...>();
    }
    template <typename Analysis>
    void AppendAnalyses() {
        analyses_.push_back(std::move(std::make_unique<Analysis>()));
    }

    std::string_view name_;
    std::string_view desc_;
    std::vector<std::unique_ptr<Analysis>> analyses_;
};

template <typename... Analyses>
class Option : public OptionBase {
  public:
    Option(std::string_view name, std::string_view desc) : OptionBase(name, desc) {
        AppendAnalyses<Analyses...>();
    }
};

}  // namespace

void OptionRegistry::Dump(std::ostream& os, std::string_view indent) {
    for (OptionBase* opt : options_) {
        os << indent << "-" << opt->getName() << ": " << opt->getDesc() << std::endl;
    }
}

static std::vector<Analysis*> ParseAnalyses(int argc, char** argv) {
    std::vector<Analysis*> analyses;
    auto& registry = OptionRegistry::get();
    for (int i = 0; i < argc; ++i) {
        if (argv[i][0] != '-') continue;

        std::string_view arg(argv[i]);
        for (auto* option : registry) {
            if (arg.find(option->getName()) != std::string_view::npos)
                option->GetAnalyses(analyses);
        }
    }
    return analyses;
}

int main(int argc, char** argv) {
    static Option<OpMinMax, OpAverage> gen_stats(
            "op-gen-stats", "get the min/max/avg of each kind of alloc operation");

    static Option<OpOverlap> overlap(
            "op-overlap", "get the amount of overlap in between each kind of alloc operation");

    if (argc < 2) {
        std::cerr << "Usage: memory_stats $RECORD_FILE [Analyses...]\n";
        std::cerr << "Analyses:\n";
        OptionRegistry::get().Dump(std::cerr);
        return 1;
    }

    MemoryStats stats;
    stats.InitFromFile(argv[1]);
    auto analyses = ParseAnalyses(argc - 2, argv + 2);

    for (Analysis* a : analyses) {
        stats.runAnalysis(a);
        a->GetResult();
    }
}
