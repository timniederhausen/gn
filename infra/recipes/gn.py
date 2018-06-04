# Copyright 2018 The Chromium Authors. All rights reserved.
# Use of this source code is governed under the Apache License, Version 2.0
# that can be found in the LICENSE file.
"""Recipe for building GN."""

DEPS = [
    'recipe_engine/path',
    'recipe_engine/platform',
    'recipe_engine/step',
]


def RunSteps(api):
  src_dir = api.path['start_dir'].join('gn')
  api.step('checkout',
           ['git', 'clone', 'https://gn.googlesource.com/gn', src_dir])


def GenTests(api):
  for platform in ('linux', 'mac', 'win'):
    yield api.test(platform) + api.platform.name(platform)
