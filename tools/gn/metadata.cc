// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tools/gn/metadata.h"

#include "tools/gn/filesystem_utils.h"

bool Metadata::WalkStep(const BuildSettings* settings,
                        const std::vector<std::string>& keys_to_extract,
                        const std::vector<std::string>& keys_to_walk,
                        const SourceDir& rebase_dir,
                        std::vector<Value>* next_walk_keys,
                        std::vector<Value>* result,
                        Err* err) const {
  // If there's no metadata, there's nothing to find, so quick exit.
  if (contents_.empty()) {
    next_walk_keys->emplace_back(nullptr, "");
    return true;
  }

  // Pull the data from each specified key.
  for (const auto& key : keys_to_extract) {
    auto iter = contents_.find(key);
    if (iter == contents_.end())
      continue;
    assert(iter->second.type() == Value::LIST);

    if (!rebase_dir.is_null()) {
      for (const auto& val : iter->second.list_value()) {
        std::pair<Value, bool> pair =
            this->RebaseValue(settings, rebase_dir, val, err);
        if (!pair.second)
          return false;
        result->push_back(pair.first);
      }
    } else {
      result->insert(result->end(), iter->second.list_value().begin(),
                     iter->second.list_value().end());
    }
  }

  // Get the targets to look at next. If no keys_to_walk are present, we push
  // the empty string to the list so that the target knows to include its deps
  // and data_deps. The values used here must be lists of strings.
  bool found_walk_key = false;
  for (const auto& key : keys_to_walk) {
    auto iter = contents_.find(key);
    if (iter != contents_.end()) {
      found_walk_key = true;
      assert(iter->second.type() == Value::LIST);
      for (const auto& val : iter->second.list_value()) {
        if (!val.VerifyTypeIs(Value::STRING, err))
          return false;
        next_walk_keys->emplace_back(val);
      }
    }
  }

  if (!found_walk_key)
    next_walk_keys->emplace_back(nullptr, "");

  return true;
}

std::pair<Value, bool> Metadata::RebaseValue(const BuildSettings* settings,
                                             const SourceDir& rebase_dir,
                                             const Value& value,
                                             Err* err) const {
  switch (value.type()) {
    case Value::STRING:
      return this->RebaseStringValue(settings, rebase_dir, value, err);
    case Value::LIST:
      return this->RebaseListValue(settings, rebase_dir, value, err);
    case Value::SCOPE:
      return this->RebaseScopeValue(settings, rebase_dir, value, err);
    default:
      return std::make_pair(value, true);
  }
}

std::pair<Value, bool> Metadata::RebaseStringValue(
    const BuildSettings* settings,
    const SourceDir& rebase_dir,
    const Value& value,
    Err* err) const {
  if (!value.VerifyTypeIs(Value::STRING, err))
    return std::make_pair(value, false);
  std::string filename = source_dir_.ResolveRelativeAs(
      /*as_file = */ true, value, err, settings->root_path_utf8());
  if (err->has_error())
    return std::make_pair(value, false);
  Value rebased_value(value.origin(), RebasePath(filename, rebase_dir,
                                                 settings->root_path_utf8()));
  return std::make_pair(rebased_value, true);
}

std::pair<Value, bool> Metadata::RebaseListValue(const BuildSettings* settings,
                                                 const SourceDir& rebase_dir,
                                                 const Value& value,
                                                 Err* err) const {
  if (!value.VerifyTypeIs(Value::LIST, err))
    return std::make_pair(value, false);

  Value rebased_list_value(value.origin(), Value::LIST);
  for (auto& val : value.list_value()) {
    std::pair<Value, bool> pair = RebaseValue(settings, rebase_dir, val, err);
    if (!pair.second)
      return std::make_pair(value, false);
    rebased_list_value.list_value().push_back(pair.first);
  }
  return std::make_pair(rebased_list_value, true);
}

std::pair<Value, bool> Metadata::RebaseScopeValue(const BuildSettings* settings,
                                                  const SourceDir& rebase_dir,
                                                  const Value& value,
                                                  Err* err) const {
  if (!value.VerifyTypeIs(Value::SCOPE, err))
    return std::make_pair(value, false);
  Value rebased_scope_value(value);
  Scope::KeyValueMap scope_values;
  value.scope_value()->GetCurrentScopeValues(&scope_values);
  for (auto& value_pair : scope_values) {
    std::pair<Value, bool> pair =
        RebaseValue(settings, rebase_dir, value_pair.second, err);
    if (!pair.second)
      return std::make_pair(value, false);

    rebased_scope_value.scope_value()->SetValue(value_pair.first, pair.first,
                                                value.origin());
  }
  return std::make_pair(rebased_scope_value, true);
}