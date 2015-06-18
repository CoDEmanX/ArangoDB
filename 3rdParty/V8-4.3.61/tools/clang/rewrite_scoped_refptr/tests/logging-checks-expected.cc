// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/logging.h"
#include "base/memory/ref_counted.h"

struct Foo : public base::RefCounted<Foo> {
  int dummy;
};

// Rewriting the logging macros is a bit tricky. The CHECK_OP macros actually
// wrap a function where the actual comparison happens. Make sure that the tool
// is correctly matching the AST nodes generated by the macros and generating
// the appropriate replacements.
void TestFunction() {
  scoped_refptr<Foo> a;
  Foo* b;

  CHECK_EQ(a.get(), b);
  CHECK_EQ(b, a.get());

  CHECK_NE(a.get(), b);
  CHECK_NE(b, a.get());

  CHECK(a.get());
  CHECK(!a.get());
}
