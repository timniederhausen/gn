// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gn/resolved_target_data.h"

#include "gn/config_values_extractors.h"
#include "gn/resolved_target_deps.h"

using LibInfo = ResolvedTargetData::LibInfo;
using FrameworkInfo = ResolvedTargetData::FrameworkInfo;

namespace {

struct TargetInfo {
  TargetInfo() = default;

  TargetInfo(const Target* target)
      : target(target),
        deps(target->public_deps(),
             target->private_deps(),
             target->data_deps()) {}

  const Target* target = nullptr;
  ResolvedTargetDeps deps;

  bool has_lib_info = false;
  bool has_framework_info = false;
  bool has_hard_deps = false;
  bool has_inherited_libs = false;
  bool has_rust_libs = false;

  // Only valid if |has_lib_info|.
  ImmutableVector<SourceDir> lib_dirs;
  ImmutableVector<LibFile> libs;

  // Only valid if |has_framework_info|.
  ImmutableVector<SourceDir> framework_dirs;
  ImmutableVector<std::string> frameworks;
  ImmutableVector<std::string> weak_frameworks;

  // Only valid if |has_hard_deps|.
  ImmutableVector<const Target*> hard_deps;

  // Only valid if |has_inherited_libs|.
  ImmutableVector<TargetPublicPair> inherited_libs;

  // Only valid if |has_rust_libs|.
  ImmutableVector<TargetPublicPair> rust_inherited_libs;
  ImmutableVector<TargetPublicPair> rust_inheritable_libs;
};

}  // namespace

// Implementation class for ResolvedTargetData.
class ResolvedTargetData::Impl {
 public:
  LibInfo GetLibInfo(const Target* target) const {
    const TargetInfo* info = GetRecursiveTargetLibInfo(target);
    DCHECK(info->has_lib_info);
    return LibInfo{
        info->lib_dirs,
        info->libs,
    };
  }

  ImmutableVectorView<SourceDir> all_lib_dirs(const Target* target) const {
    const TargetInfo* info = GetRecursiveTargetLibInfo(target);
    DCHECK(info->has_lib_info);
    return info->lib_dirs;
  }

  ImmutableVectorView<LibFile> all_libs(const Target* target) const {
    const TargetInfo* info = GetRecursiveTargetLibInfo(target);
    DCHECK(info->has_lib_info);
    return info->libs;
  }

  FrameworkInfo GetFrameworkInfo(const Target* target) const {
    const TargetInfo* info = GetRecursiveTargetFrameworkInfo(target);
    DCHECK(info->has_framework_info);
    return FrameworkInfo{
        info->framework_dirs,
        info->frameworks,
        info->weak_frameworks,
    };
  }

  ImmutableVectorView<SourceDir> all_framework_dirs(
      const Target* target) const {
    const TargetInfo* info = GetRecursiveTargetFrameworkInfo(target);
    DCHECK(info->has_framework_info);
    return info->framework_dirs;
  }

  ImmutableVectorView<std::string> all_frameworks(const Target* target) const {
    const TargetInfo* info = GetRecursiveTargetFrameworkInfo(target);
    DCHECK(info->has_framework_info);
    return info->frameworks;
  }

  ImmutableVectorView<std::string> all_weak_frameworks(
      const Target* target) const {
    const TargetInfo* info = GetRecursiveTargetFrameworkInfo(target);
    DCHECK(info->has_framework_info);
    return info->weak_frameworks;
  }

  TargetSet recursive_hard_deps(const Target* target) const {
    TargetInfo* info = GetInfo(target);
    DCHECK(info->has_hard_deps);
    if (!info->has_hard_deps)
      ComputeHardDeps(info);

    return TargetSet(info->hard_deps.begin(), info->hard_deps.end());
  }

  ImmutableVectorView<TargetPublicPair> inherited_libraries(
      const Target* target) const {
    const TargetInfo* info = GetRecursiveTargetInheritedLibs(target);
    DCHECK(info->has_inherited_libs);
    return info->inherited_libs;
  }

  ImmutableVectorView<TargetPublicPair> rust_transitive_inherited_libs(
      const Target* target) const {
    const TargetInfo* info = GetRecursiveTargetRustLibs(target);
    DCHECK(info->has_rust_libs);
    return info->rust_inherited_libs;
  }

 private:
  const TargetInfo* GetRecursiveTargetLibInfo(const Target* target) const {
    TargetInfo* info = GetInfo(target);
    if (!info->has_lib_info)
      ComputeLibInfo(info);
    return info;
  }

  void ComputeLibInfo(TargetInfo* info) const {
    UniqueVector<SourceDir> all_lib_dirs;
    UniqueVector<LibFile> all_libs;

    for (ConfigValuesIterator iter(info->target); !iter.done(); iter.Next()) {
      const ConfigValues& cur = iter.cur();
      all_lib_dirs.Append(cur.lib_dirs());
      all_libs.Append(cur.libs());
    }
    for (const Target* dep : info->deps.linked_deps()) {
      if (!dep->IsFinal() || dep->output_type() == Target::STATIC_LIBRARY) {
        const TargetInfo* dep_info = GetRecursiveTargetLibInfo(dep);
        all_lib_dirs.Append(dep_info->lib_dirs);
        all_libs.Append(dep_info->libs);
      }
    }

    info->lib_dirs = ImmutableVector<SourceDir>(all_lib_dirs.release());
    info->libs = ImmutableVector<LibFile>(all_libs.release());
    info->has_lib_info = true;
  }

  const TargetInfo* GetRecursiveTargetFrameworkInfo(
      const Target* target) const {
    TargetInfo* info = GetInfo(target);
    if (!info->has_framework_info)
      ComputeFrameworkInfo(info);
    return info;
  }

  void ComputeFrameworkInfo(TargetInfo* info) const {
    UniqueVector<SourceDir> all_framework_dirs;
    UniqueVector<std::string> all_frameworks;
    UniqueVector<std::string> all_weak_frameworks;

    for (ConfigValuesIterator iter(info->target); !iter.done(); iter.Next()) {
      const ConfigValues& cur = iter.cur();
      all_framework_dirs.Append(cur.framework_dirs());
      all_frameworks.Append(cur.frameworks());
      all_weak_frameworks.Append(cur.weak_frameworks());
    }
    for (const Target* dep : info->deps.linked_deps()) {
      if (!dep->IsFinal() || dep->output_type() == Target::STATIC_LIBRARY) {
        const TargetInfo* dep_info = GetRecursiveTargetLibInfo(dep);
        all_framework_dirs.Append(dep_info->framework_dirs);
        all_frameworks.Append(dep_info->frameworks);
        all_weak_frameworks.Append(dep_info->weak_frameworks);
      }
    }

    info->framework_dirs = ImmutableVector<SourceDir>(all_framework_dirs);
    info->frameworks = ImmutableVector<std::string>(all_frameworks);
    info->weak_frameworks = ImmutableVector<std::string>(all_weak_frameworks);
    info->has_framework_info = true;
  }

  const TargetInfo* GetRecursiveTargetHardDeps(const Target* target) const {
    TargetInfo* info = GetInfo(target);
    if (!info->has_hard_deps)
      ComputeHardDeps(info);
    return info;
  }

  void ComputeHardDeps(TargetInfo* info) const {
    TargetSet all_hard_deps;
    for (const Target* dep : info->deps.linked_deps()) {
      // Direct hard dependencies
      if (info->target->hard_dep() || dep->hard_dep()) {
        all_hard_deps.insert(dep);
        continue;
      }
      // If |dep| is binary target and |dep| has no public header,
      // |this| target does not need to have |dep|'s hard_deps as its
      // hard_deps to start compiles earlier. Unless the target compiles a
      // Swift module (since they also generate a header that can be used
      // by the current target).
      if (dep->IsBinary() && !dep->all_headers_public() &&
          dep->public_headers().empty() && !dep->builds_swift_module()) {
        continue;
      }

      // Recursive hard dependencies of all dependencies.
      const TargetInfo* dep_info = GetRecursiveTargetHardDeps(dep);
      all_hard_deps.insert(dep_info->hard_deps.begin(),
                           dep_info->hard_deps.end());
    }
    info->hard_deps = ImmutableVector<const Target*>(all_hard_deps);
    info->has_hard_deps = true;
  }

  const TargetInfo* GetRecursiveTargetInheritedLibs(
      const Target* target) const {
    TargetInfo* info = GetInfo(target);
    if (!info->has_inherited_libs)
      ComputeInheritedLibs(info);
    return info;
  }

  void ComputeInheritedLibs(TargetInfo* info) const {
    TargetPublicPairListBuilder inherited_libraries;

    ComputeInheritedLibsFor(info->deps.public_deps(), true,
                            &inherited_libraries);
    ComputeInheritedLibsFor(info->deps.private_deps(), false,
                            &inherited_libraries);

    info->has_inherited_libs = true;
    info->inherited_libs = inherited_libraries.Build();
  }

  void ComputeInheritedLibsFor(
      base::span<const Target*> deps,
      bool is_public,
      TargetPublicPairListBuilder* inherited_libraries) const {
    for (const Target* dep : deps) {
      // Direct dependent libraries.
      if (dep->output_type() == Target::STATIC_LIBRARY ||
          dep->output_type() == Target::SHARED_LIBRARY ||
          dep->output_type() == Target::RUST_LIBRARY ||
          dep->output_type() == Target::SOURCE_SET ||
          (dep->output_type() == Target::CREATE_BUNDLE &&
           dep->bundle_data().is_framework())) {
        inherited_libraries->Append(dep, is_public);
      }
      if (dep->output_type() == Target::SHARED_LIBRARY) {
        // Shared library dependendencies are inherited across public shared
        // library boundaries.
        //
        // In this case:
        //   EXE -> INTERMEDIATE_SHLIB --[public]--> FINAL_SHLIB
        // The EXE will also link to to FINAL_SHLIB. The public dependency means
        // that the EXE can use the headers in FINAL_SHLIB so the FINAL_SHLIB
        // will need to appear on EXE's link line.
        //
        // However, if the dependency is private:
        //   EXE -> INTERMEDIATE_SHLIB --[private]--> FINAL_SHLIB
        // the dependency will not be propagated because INTERMEDIATE_SHLIB is
        // not granting permission to call functions from FINAL_SHLIB. If EXE
        // wants to use functions (and link to) FINAL_SHLIB, it will need to do
        // so explicitly.
        //
        // Static libraries and source sets aren't inherited across shared
        // library boundaries because they will be linked into the shared
        // library. Rust dylib deps are handled above and transitive deps are
        // resolved by the compiler.
        const TargetInfo* dep_info = GetRecursiveTargetInheritedLibs(dep);
        for (const auto& pair : dep_info->inherited_libs) {
          if (pair.target()->output_type() == Target::SHARED_LIBRARY &&
              pair.is_public()) {
            inherited_libraries->Append(pair.target(), is_public);
          }
        }
      } else if (!dep->IsFinal()) {
        // The current target isn't linked, so propagate linked deps and
        // libraries up the dependency tree.
        const TargetInfo* dep_info = GetRecursiveTargetInheritedLibs(dep);
        for (const auto& pair : dep_info->inherited_libs) {
          // Proc macros are not linked into targets that depend on them, so do
          // not get inherited; they are consumed by the Rust compiler and only
          // need to be specified in --extern.
          if (pair.target()->output_type() != Target::RUST_PROC_MACRO)
            inherited_libraries->Append(pair.target(),
                                        is_public && pair.is_public());
        }
      } else if (dep->complete_static_lib()) {
        // Inherit only final targets through _complete_ static libraries.
        //
        // Inherited final libraries aren't linked into complete static
        // libraries. They are forwarded here so that targets that depend on
        // complete static libraries can link them in. Conversely, since
        // complete static libraries link in non-final targets they shouldn't be
        // inherited.
        const TargetInfo* dep_info = GetRecursiveTargetInheritedLibs(dep);
        for (const auto& pair : dep_info->inherited_libs) {
          if (pair.target()->IsFinal())
            inherited_libraries->Append(pair.target(),
                                        is_public && pair.is_public());
        }
      }
    }
  }

  const TargetInfo* GetRecursiveTargetRustLibs(const Target* target) const {
    TargetInfo* info = GetInfo(target);
    if (!info->has_rust_libs)
      ComputeRustLibs(info);
    return info;
  }

  struct RustLibsBuilder {
    TargetPublicPairListBuilder inherited;
    TargetPublicPairListBuilder inheritable;
  };

  void ComputeRustLibs(TargetInfo* info) const {
    RustLibsBuilder rust_libs;

    ComputeRustLibsFor(info->deps.public_deps(), true, &rust_libs);
    ComputeRustLibsFor(info->deps.private_deps(), false, &rust_libs);

    info->has_rust_libs = true;
    info->rust_inherited_libs = rust_libs.inherited.Build();
    info->rust_inheritable_libs = rust_libs.inheritable.Build();
  }

  void ComputeRustLibsFor(base::span<const Target*> deps,
                          bool is_public,
                          RustLibsBuilder* rust_libs) const {
    for (const Target* dep : deps) {
      // Collect Rust libraries that are accessible from the current target, or
      // transitively part of the current target.
      if (dep->output_type() == Target::STATIC_LIBRARY ||
          dep->output_type() == Target::SHARED_LIBRARY ||
          dep->output_type() == Target::SOURCE_SET ||
          dep->output_type() == Target::RUST_LIBRARY ||
          dep->output_type() == Target::GROUP) {
        // Here we have: `this` --[depends-on]--> `dep`
        //
        // The `this` target has direct access to `dep` since its a direct
        // dependency, regardless of the edge being a public_dep or not, so we
        // pass true for public-ness. Whereas, anything depending on `this` can
        // only gain direct access to `dep` if the edge between `this` and `dep`
        // is public, so we pass `is_public`.
        //
        // TODO(danakj): We should only need to track Rust rlibs or dylibs here,
        // as it's used for passing to rustc with --extern. We currently track
        // everything then drop non-Rust libs in
        // ninja_rust_binary_target_writer.cc.
        rust_libs->inherited.Append(dep, true);
        rust_libs->inheritable.Append(dep, is_public);

        const TargetInfo* dep_info = GetRecursiveTargetRustLibs(dep);
        rust_libs->inherited.AppendInherited(dep_info->rust_inheritable_libs,
                                             true);
        rust_libs->inheritable.AppendInherited(dep_info->rust_inheritable_libs,
                                               is_public);
      } else if (dep->output_type() == Target::RUST_PROC_MACRO) {
        // Proc-macros are inherited as a transitive dependency, but the things
        // they depend on can't be used elsewhere, as the proc macro is not
        // linked into the target (as it's only used during compilation).
        rust_libs->inherited.Append(dep, true);
        rust_libs->inheritable.Append(dep, is_public);
      }
    }
  }

  TargetInfo* GetInfo(const Target* target) const {
    auto ret = targets_.PushBackWithIndex(target);
    if (!ret.first)
      return infos_[ret.second].get();

    infos_.push_back(std::make_unique<TargetInfo>(target));
    return infos_.back().get();
  }

  // A { target -> TargetInfo } map that will create entries
  // on demand. Implemented with a UniqueVector<> and a parallel
  // vector of unique TargetInfo instances for best performance.
  mutable UniqueVector<const Target*> targets_;
  mutable std::vector<std::unique_ptr<TargetInfo>> infos_;
};

ResolvedTargetData::ResolvedTargetData() = default;

ResolvedTargetData::~ResolvedTargetData() = default;

ResolvedTargetData::ResolvedTargetData(ResolvedTargetData&&) noexcept = default;
ResolvedTargetData& ResolvedTargetData::operator=(ResolvedTargetData&&) =
    default;

ResolvedTargetData::Impl* ResolvedTargetData::GetImpl() const {
  if (!impl_)
    impl_ = std::make_unique<ResolvedTargetData::Impl>();
  return impl_.get();
}

LibInfo ResolvedTargetData::GetLibInfo(const Target* target) const {
  return GetImpl()->GetLibInfo(target);
}

ImmutableVectorView<SourceDir> ResolvedTargetData::all_lib_dirs(
    const Target* target) const {
  return GetImpl()->all_lib_dirs(target);
}

ImmutableVectorView<LibFile> ResolvedTargetData::all_libs(
    const Target* target) const {
  return GetImpl()->all_libs(target);
}

FrameworkInfo ResolvedTargetData::GetFrameworkInfo(const Target* target) const {
  return GetImpl()->GetFrameworkInfo(target);
}

ImmutableVectorView<SourceDir> ResolvedTargetData::all_framework_dirs(
    const Target* target) const {
  return GetImpl()->all_framework_dirs(target);
}

ImmutableVectorView<std::string> ResolvedTargetData::all_frameworks(
    const Target* target) const {
  return GetImpl()->all_frameworks(target);
}

ImmutableVectorView<std::string> ResolvedTargetData::all_weak_frameworks(
    const Target* target) const {
  return GetImpl()->all_weak_frameworks(target);
}

TargetSet ResolvedTargetData::recursive_hard_deps(const Target* target) const {
  return GetImpl()->recursive_hard_deps(target);
}

TargetPublicPairList ResolvedTargetData::inherited_libraries(
    const Target* target) const {
  return GetImpl()->inherited_libraries(target);
}

TargetPublicPairList ResolvedTargetData::rust_transitive_inherited_libs(
    const Target* target) const {
  return GetImpl()->rust_transitive_inherited_libs(target);
}
