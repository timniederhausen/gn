# GN Frequently Asked Questions

[TOC]

## Where is the GN documentation?

GN has extensive built-in help, so you can run `gn help`, but you can
also see all of the help on [the reference page](reference.md). See
also the [quick start](quick_start.md) guide and the [language and
operation details](language.md).

## Can I generate XCode or Visual Studio projects?

You can generate skeleton (or wrapper) projects for Xcode, Visual Studio,
QTCreator, and Eclipse that will list the files and targets in the
build, but use Ninja to do the actual build. You cannot generate "real"
projects that look like native ones like GYP could.

Run `gn help gen` for more details.

## How do I generate common build variants?

In GN, args go with a build directory rather than being global in the
environment. To edit the args for your `out/Default` build directory:

```
gn args out/Default
```

You can set variables in that file:

  * The default is a debug build. To do a release build add
    `is_debug = false`
  * The default is a static build. To do a component build add
    `is_component_build = true`
  * The default is a developer build. To do an official build, set
    `is_official_build = true`
  * The default is Chromium branding. To do Chrome branding, set
    `is_chrome_branded = true`

## How do I do cross-compiles?

GN has robust support for doing cross compiles and building things for
multiple architectures in a single build.

See [GNCrossCompiles](cross_compiles.md) for more info.

## Can I control what targets are built by default?

Yes! If you create a group target called "default" in the top-level (root)
build file, i.e., "//:default", GN will tell Ninja to build that by
default, rather than building everything.

## Are there any public presentations on GN?

[There's at least one](https://docs.google.com/presentation/d/15Zwb53JcncHfEwHpnG_PoIbbzQ3GQi_cpujYwbpcbZo/edit?usp=sharing), from 2015. There
haven't been big changes since then apart from moving it to a standalone
repo, so it should still be relevant.

## How can I recursively copy a directory as a build step?

Sometimes people want to write a build action that expresses copying all files
(possibly recursively, possily not) from a source directory without specifying
all files in that directory in a BUILD file. This is not possible to express:
correct builds must list all inputs. Most approaches people try to work around
this break in some way for incremental builds, either the build step is run
every time (the build is always "dirty"), file modifications will be missed, or
file additions will be missed.

One thing people try is to write an action that declares an input directory and
an output directory and have it copy all files from the source to the
destination. But incremental builds are likely going to be incorrect if you do
this. Ninja determines if an output is in need of rebuilding by comparing the
last modified date of the source to the last modified date of the destination.
Since almost no filesystems propagate the last modified date of files to their
directory, modifications to files in the source will not trigger an incremental
rebuild.

Beware when testing this: most filesystems update the last modified date of the
parent directory (but not recursive parents) when adding to or removing a file
from that directory so this will appear to work in many cases. But no modern
production filesystems propagate modification times of the contents of the files
to any directories because it would be very slow. The result will be that
modifications to the source files will not be reflected in the output when doing
incremental builds.

Another thing people try is to write all of the source files to a "depfile" (see
`gn help depfile`) and to write a single stamp file that tracks the modified
date of the output. This approach also may appear to work but is subtly wrong:
the additions of new files to the source directory will not trigger the build
step and that addition will not be reflected in an incremental build.
