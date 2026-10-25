#!/usr/bin/env python3
"""Validate a supported NAM capture and embed its exact float32 weights.

No retraining, format conversion, upload, or device write is performed.
The supported configuration template comes from RiffSmith (Matt Hartman, MIT).
"""
import argparse
import copy
import hashlib
import json
import math
from pathlib import Path
import struct
import sys

ROOT = Path(__file__).resolve().parents[1]


def shape(config):
    result = copy.deepcopy(config)
    scale = result.pop('head_scale', None)
    if isinstance(scale, bool) or not isinstance(scale, (int, float)) or not math.isfinite(scale):
        raise ValueError('Missing or invalid head_scale')
    return result


def candidates(model, inherited_rate=None):
    if not isinstance(model, dict):
        raise ValueError('NAM model must be a JSON object')
    rate = model.get('sample_rate', inherited_rate)
    if model.get('architecture') == 'SlimmableContainer':
        for entry in model.get('config', {}).get('submodels', []):
            yield from candidates(entry['model'], rate)
    else:
        yield model, rate


def select(model, template):
    for candidate, rate in candidates(model):
        if candidate.get('architecture') != 'WaveNet' or rate != 48000:
            continue
        try:
            if shape(candidate.get('config', {})) != shape(template['config']):
                continue
        except (ValueError, TypeError):
            continue
        weights = candidate.get('weights', [])
        if not isinstance(weights, list) or len(weights) != 1871:
            continue
        if not all(not isinstance(v, bool) and isinstance(v, (int, float)) and math.isfinite(v) for v in weights):
            continue
        try:
            words = [struct.unpack('<f', struct.pack('<f', v))[0] for v in weights]
        except (OverflowError, struct.error):
            continue
        if not all(math.isfinite(v) for v in words):
            continue
        normalized = copy.deepcopy(candidate)
        normalized['sample_rate'] = 48000
        return normalized, words
    raise ValueError('Unsupported capture. Requires the exact 48 kHz, mono A2-Lite topology (1,871 weights). Renaming a file or reducing its weight count cannot convert it.')


def header(words, source_hash):
    lines = ['// User-selected capture; its original license continues to apply.',
             '// Source SHA256: ' + source_hash,
             '#pragma once', '#include <cstddef>', 'namespace legio_nam {',
             'inline constexpr unsigned kCaptureId=4;',
             'inline constexpr std::size_t kLiteWeights=1871;',
             'alignas(4) inline constexpr float kPublicWeights[1871]={']
    for i in range(0, len(words), 6):
        lines.append('  ' + ', '.join(v.hex() + 'f' for v in words[i:i+6]) + ',')
    lines += ['};', 'inline const float* Weights(unsigned) {return kPublicWeights;}', '}']
    return '\n'.join(lines) + '\n'


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('capture', type=Path)
    parser.add_argument('--check', action='store_true', help='Validate without changing files')
    parser.add_argument('--output', type=Path, default=ROOT/'models/riffsmith_clean_embedded.h')
    parser.add_argument('--model-output', type=Path, default=ROOT/'models/custom-capture.nam')
    args = parser.parse_args()
    try:
        raw = args.capture.read_bytes()
        template = json.loads((ROOT/'tools/supported_capture_config.json').read_text())
        normalized, words = select(json.loads(raw), template)
        source_hash = hashlib.sha256(raw).hexdigest()
        if not args.check:
            if args.output.resolve() == args.capture.resolve() or args.model_output.resolve() == args.capture.resolve():
                raise ValueError('Output must not overwrite the original capture')
            args.output.parent.mkdir(parents=True, exist_ok=True)
            args.model_output.parent.mkdir(parents=True, exist_ok=True)
            args.model_output.write_text(json.dumps(normalized, indent=2, allow_nan=False)+'\n')
            args.output.write_text(header(words, source_hash))
        print('Compatible A2-Lite capture: 48 kHz, 1,871 weights. ' + ('No files changed.' if args.check else 'Embedded header written; firmware has not been built or installed.'))
        print('For redistribution, verify the capture license and include its required notices. Compatibility is not a license check.')
    except (OSError, ValueError, TypeError, KeyError, AttributeError, RecursionError) as error:
        print('Capture rejected: ' + str(error), file=sys.stderr)
        return 1
    return 0


if __name__ == '__main__':
    sys.exit(main())
