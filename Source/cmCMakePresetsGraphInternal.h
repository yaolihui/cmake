/* Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
   file Copyright.txt or https://cmake.org/licensing for details.  */
#pragma once

#include <cstddef>
#include <memory>
#include <string>
#include <vector>

#include <cm3p/json/value.h>

#include "cmCMakePresetsGraph.h"
#include "cmJSONHelpers.h"
#include "cmSystemTools.h"

#define CHECK_OK(expr)                                                        \
  do {                                                                        \
    auto _result = expr;                                                      \
    if (_result != true)                                                      \
      return _result;                                                         \
  } while (false)

namespace cmCMakePresetsGraphInternal {
enum class ExpandMacroResult
{
  Ok,
  Ignore,
  Error,
};

class MacroExpander
{
public:
  virtual ExpandMacroResult operator()(const std::string& macroNamespace,
                                       const std::string& macroName,
                                       std::string& macroOut,
                                       int version) const = 0;
  virtual ~MacroExpander() = default;
};
using MacroExpanderVector = std::vector<std::unique_ptr<MacroExpander>>;

ExpandMacroResult ExpandMacros(std::string& out,
                               MacroExpanderVector const& macroExpanders,
                               int version);

ExpandMacroResult ExpandMacro(std::string& out,
                              const std::string& macroNamespace,
                              const std::string& macroName,
                              MacroExpanderVector const& macroExpanders,
                              int version);
}

class cmCMakePresetsGraph::Condition
{
public:
  virtual ~Condition() = default;

  virtual bool Evaluate(
    const cmCMakePresetsGraphInternal::MacroExpanderVector& expanders,
    int version, cm::optional<bool>& out) const = 0;
  virtual bool IsNull() const { return false; }
};

namespace cmCMakePresetsGraphInternal {
class BaseMacroExpander : public MacroExpander
{
  cmCMakePresetsGraph const& Graph;
  cm::optional<std::string> File;

public:
  BaseMacroExpander(const cmCMakePresetsGraph& graph)
    : Graph(graph)
  {
  }
  BaseMacroExpander(const cmCMakePresetsGraph& graph, const std::string& file)
    : Graph(graph)
    , File(file)
  {
  }
  ExpandMacroResult operator()(const std::string& macroNamespace,
                               const std::string& macroName,
                               std::string& macroOut,
                               int version) const override;
};

template <class T>
class PresetMacroExpander : public MacroExpander
{
  cmCMakePresetsGraph const& Graph;
  T const& Preset;

public:
  PresetMacroExpander(const cmCMakePresetsGraph& graph, const T& preset)
    : Graph(graph)
    , Preset(preset)
  {
  }
  ExpandMacroResult operator()(const std::string& macroNamespace,
                               const std::string& macroName,
                               std::string& macroOut,
                               int version) const override
  {
    if (macroNamespace.empty()) {
      if (macroName == "presetName") {
        macroOut += Preset.Name;
        return ExpandMacroResult::Ok;
      }
      if (macroName == "generator") {
        // Generator only makes sense if preset is not hidden.
        if (!Preset.Hidden) {
          macroOut += Graph.GetGeneratorForPreset(Preset.Name);
        }
        return ExpandMacroResult::Ok;
      }
      if (macroName == "fileDir") {
        if (version < 4) {
          return ExpandMacroResult::Error;
        }
        macroOut +=
          cmSystemTools::GetParentDirectory(Preset.OriginFile->Filename);
        return ExpandMacroResult::Ok;
      }
    }
    return ExpandMacroResult::Ignore;
  }
};

class NullCondition : public cmCMakePresetsGraph::Condition
{
  bool Evaluate(MacroExpanderVector const& /*expanders*/, int /*version*/,
                cm::optional<bool>& out) const override
  {
    out = true;
    return true;
  }

  bool IsNull() const override { return true; }
};

class ConstCondition : public cmCMakePresetsGraph::Condition
{
public:
  bool Evaluate(MacroExpanderVector const& /*expanders*/, int /*version*/,
                cm::optional<bool>& out) const override
  {
    out = this->Value;
    return true;
  }

  bool Value;
};

class EqualsCondition : public cmCMakePresetsGraph::Condition
{
public:
  bool Evaluate(MacroExpanderVector const& expanders, int version,
                cm::optional<bool>& out) const override;

  std::string Lhs;
  std::string Rhs;
};

class InListCondition : public cmCMakePresetsGraph::Condition
{
public:
  bool Evaluate(MacroExpanderVector const& expanders, int version,
                cm::optional<bool>& out) const override;

  std::string String;
  std::vector<std::string> List;
};

class MatchesCondition : public cmCMakePresetsGraph::Condition
{
public:
  bool Evaluate(MacroExpanderVector const& expanders, int version,
                cm::optional<bool>& out) const override;

  std::string String;
  std::string Regex;
};

class AnyAllOfCondition : public cmCMakePresetsGraph::Condition
{
public:
  bool Evaluate(MacroExpanderVector const& expanders, int version,
                cm::optional<bool>& out) const override;

  std::vector<std::unique_ptr<Condition>> Conditions;
  bool StopValue;
};

class NotCondition : public cmCMakePresetsGraph::Condition
{
public:
  bool Evaluate(MacroExpanderVector const& expanders, int version,
                cm::optional<bool>& out) const override;

  std::unique_ptr<Condition> SubCondition;
};

bool PresetStringHelper(std::string& out, const Json::Value* value,
                        cmJSONState* state);

bool PresetNameHelper(std::string& out, const Json::Value* value,
                      cmJSONState* state);

bool PresetVectorStringHelper(std::vector<std::string>& out,
                              const Json::Value* value, cmJSONState* state);

bool PresetBoolHelper(bool& out, const Json::Value* value, cmJSONState* state);

bool PresetOptionalBoolHelper(cm::optional<bool>& out,
                              const Json::Value* value, cmJSONState* state);

bool PresetIntHelper(int& out, const Json::Value* value, cmJSONState* state);

bool PresetOptionalIntHelper(cm::optional<int>& out, const Json::Value* value,
                             cmJSONState* state);

bool PresetVectorIntHelper(std::vector<int>& out, const Json::Value* value,
                           cmJSONState* state);

bool ConfigurePresetsHelper(
  std::vector<cmCMakePresetsGraph::ConfigurePreset>& out,
  const Json::Value* value, cmJSONState* state);

bool BuildPresetsHelper(std::vector<cmCMakePresetsGraph::BuildPreset>& out,
                        const Json::Value* value, cmJSONState* state);

bool TestPresetsHelper(std::vector<cmCMakePresetsGraph::TestPreset>& out,
                       const Json::Value* value, cmJSONState* state);

bool PackagePresetsHelper(std::vector<cmCMakePresetsGraph::PackagePreset>& out,
                          const Json::Value* value, cmJSONState* state);

bool WorkflowPresetsHelper(
  std::vector<cmCMakePresetsGraph::WorkflowPreset>& out,
  const Json::Value* value, cmJSONState* state);

cmJSONHelper<std::nullptr_t> VendorHelper(const ErrorGenerator& error);

bool PresetConditionHelper(
  std::shared_ptr<cmCMakePresetsGraph::Condition>& out,
  const Json::Value* value, cmJSONState* state);

bool PresetVectorOneOrMoreStringHelper(std::vector<std::string>& out,
                                       const Json::Value* value,
                                       cmJSONState* state);

bool EnvironmentMapHelper(
  std::map<std::string, cm::optional<std::string>>& out,
  const Json::Value* value, cmJSONState* state);

cmJSONHelper<std::nullptr_t> SchemaHelper();
}
