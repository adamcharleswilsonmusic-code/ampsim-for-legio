#!/usr/bin/env python3
"""One explicit platform adaptation to the current upstream DSP base class."""
import pathlib
root=pathlib.Path(__file__).resolve().parents[1]
source=root/"host-research/NeuralAmpModelerCore-current/NAM/dsp.cpp"
text=source.read_text()
needle="thread_local bool gPrewarmOnResetDefault = true;"
assert text.count(needle)==1
# Bare-metal single main thread; prewarm default is never changed by the ISR.
# Avoid an unavailable ARM bare-metal thread pointer/TLS runtime.
text=text.replace(needle,"bool gPrewarmOnResetDefault = true; // Embedded: main-thread only.")
out=root/"firmware/generated/nam_dsp.cpp"
out.parent.mkdir(parents=True,exist_ok=True)
out.write_text("// Generated from upstream NAM/dsp.cpp; MIT license retained in upstream/LICENSE.\n"+text)
print("Generated NAM DSP base; only adaptation: main-thread prewarm default replaces TLS")
fast=root/"host-research/NeuralAmpModelerCore-current/NAM/wavenet/a2_fast.cpp"
text=fast.read_text()
needle='''    std::stringstream ss;
    ss << "A2FastModel: weight stream has " << std::distance(it, end) << " trailing bytes";
    throw std::runtime_error(ss.str());'''
assert text.count(needle)==1
# Preserve the validation/exception; do not link the desktop locale and stream
# formatter merely to print a trailing-weight count. Inference is unchanged.
text=text.replace(needle,'    throw std::runtime_error("A2FastModel: trailing weights");')
# Keep include paths unambiguous for a generated copy outside the upstream tree.
text=text.replace('#include "a2_fast.h"','#include "NAM/wavenet/a2_fast.h"')
text=text.replace('#include "../dsp.h"','#include "NAM/dsp.h"')
(out.parent/"a2_fast_embedded.cpp").write_text(
    "// Generated from upstream a2_fast.cpp; MIT license retained in upstream/LICENSE.\n"+text)
print("Generated upstream A2 inference; only runtime change: compact trailing-weight error message")
