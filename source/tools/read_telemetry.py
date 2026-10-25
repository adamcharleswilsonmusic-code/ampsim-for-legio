#!/usr/bin/env python3
"""Read Legio numerical USB diagnostics. Never flashes, sends commands or records audio."""
import argparse
import glob
import json
import math
import os
import select
import struct
import termios
import time

FIELDS = (
    'callbacks cpu_hz app_last_cycles app_max_cycles app_overruns '
    'dma_last_cycles dma_max_cycles dma_overruns dma_calls '
    'heap_high_water heap_failures heap_largest_failure warmup_max_us '
    'amp ready fault bypass drive_db tone output_gain contour '
    'input_left_peak input_right_peak model_input_peak model_output_peak '
    'output_unclamped_peak input_near_rail_frames output_clamped_frames '
    'block_frames sample_rate'
).split()
FLOATS = set(('drive_db tone output_gain input_left_peak input_right_peak '
              'model_input_peak model_output_peak output_unclamped_peak').split())
VERSION = 1
VERSION_FIELDS = {1: FIELDS, 2: FIELDS + ['mid_position', 'swell_mode', 'input_envelope', 'swell_gain', 'swell_armed']}
VERSION_FIELDS[3] = VERSION_FIELDS[2] + ['preset_slot', 'preset_save_state', 'tone_pickup', 'preset_save_count', 'preset_save_errors', 'wet_envelope']
VERSION_FIELDS[4] = VERSION_FIELDS[2] + ['chorus_mode', 'wet_envelope']
VERSION_FIELDS[5] = VERSION_FIELDS[4] + ['clock_locked', 'clock_period_ms', 'clock_edges', 'chorus_rate', 'gate_high', 'pitch_adc', 'top_adc', 'bottom_adc', 'base_drive_db', 'gain_cv_db', 'gain_cv_zero']
VERSION_FIELDS[6] = VERSION_FIELDS[5] + ['chorus_depth', 'output_level_db']
VERSION_FIELDS[7] = VERSION_FIELDS[6]  # Restored gain/tone/output knobs; identical meter schema.
VERSION_FIELDS[8] = [({'gain_cv_db': 'unused_gain_cv_db', 'gain_cv_zero': 'depth_cv_zero'}).get(f, f) for f in VERSION_FIELDS[7]]  # Left CV controls depth; gain is encoder-only.
FLOATS.add('depth_cv_zero')
FLOATS.add('unused_gain_cv_db')
MAGIC = b'LGD1'
PACKETS = {version: struct.Struct('<4sIII' + 'I' * len(fields) + 'I') for version, fields in VERSION_FIELDS.items()}
PACKET = PACKETS[1]
AMPS = ['Deluxe Reverb Clean', 'Marshall Plexi', 'Vox AC30 Edge of Breakup', 'RiffSmith Clean (Matt Hartman, MIT)', 'User-provided capture']
FAULTS = ['none', 'allocation failure', 'model load', 'non-finite output',
          'invalid block size', 'deadline missed']


def checksum(data):
    value = 2166136261
    for byte in data:
        value = ((value ^ byte) * 16777619) & 0xffffffff
    return value


def decode(packet):
    if len(packet) < 16:
        raise ValueError('Wrong packet length')
    magic, version, size, sequence = struct.unpack_from('<4sIII', packet)
    if magic != MAGIC or version not in VERSION_FIELDS or size != len(VERSION_FIELDS[version]) * 4:
        raise ValueError('Unsupported telemetry frame')
    if len(packet) != PACKETS[version].size:
        raise ValueError('Wrong packet length')
    _, _, _, _, *values = PACKETS[version].unpack(packet)
    if checksum(packet[:-4]) != values.pop():
        raise ValueError('Telemetry checksum mismatch')
    result = dict(zip(VERSION_FIELDS[version], values))
    for name in FLOATS | {'input_envelope', 'swell_gain', 'wet_envelope', 'chorus_rate', 'pitch_adc', 'top_adc', 'bottom_adc', 'base_drive_db', 'gain_cv_db', 'gain_cv_zero', 'chorus_depth', 'output_level_db'}:
        if name not in result:
            continue
        result[name] = struct.unpack('<f', struct.pack('<I', result[name]))[0]
        if not math.isfinite(result[name]):
            raise ValueError('Invalid meter value')
    if result['amp'] >= len(AMPS) or result['fault'] >= len(FAULTS):
        raise ValueError('Unsupported amp or fault')
    if result['sample_rate'] != 48000 or result['block_frames'] not in (24, 48):
        raise ValueError('Unexpected audio configuration')
    if not result['cpu_hz']:
        raise ValueError('Missing clock measurement')
    result['sequence'] = sequence
    result['protocol_version'] = version
    result['amp_name'] = AMPS[result['amp']]
    result['fault_name'] = FAULTS[result['fault']]
    budget = result['cpu_hz'] * result['block_frames'] / result['sample_rate']
    result['dma_max_percent'] = result['dma_max_cycles'] / budget * 100
    result['app_max_percent'] = result['app_max_cycles'] / budget * 100
    return result


class Decoder:
    """Bounded parser, resynchronizes after partial reads or corrupted bytes."""
    def __init__(self):
        self.buffer = bytearray()

    def feed(self, data):
        self.buffer.extend(data)
        reports = []
        while True:
            start = self.buffer.find(MAGIC)
            if start < 0:
                self.buffer[:] = self.buffer[-3:]
                break
            del self.buffer[:start]
            if len(self.buffer) < 16:
                break
            _, version, size, _ = struct.unpack_from('<4sIII', self.buffer)
            if version not in VERSION_FIELDS or size != len(VERSION_FIELDS[version]) * 4:
                del self.buffer[0]
                continue
            packet_size = PACKETS[version].size
            if len(self.buffer) < packet_size:
                break
            try:
                reports.append(decode(self.buffer[:packet_size]))
            except ValueError:
                del self.buffer[0]
                continue
            del self.buffer[:packet_size]
        return reports


def dbfs(peak):
    return '-inf' if peak <= 0 else f'{20 * math.log10(peak):.1f}'


def summary(report):
    state = ('FAULT: ' + report['fault_name'] if report['fault'] else
             'bypassed' if report['bypass'] else 'engaged' if report['ready'] else 'loading')
    return (f"{report['amp_name']} | {state} | DMA max {report['dma_max_percent']:.1f}% | "
            f"input L peak {dbfs(report['input_left_peak'])} dBFS | "
            f"input near-rail frames {report['input_near_rail_frames']} | "
            f"output clamped frames {report['output_clamped_frames']} | "
            f"heap {report['heap_high_water']}/229376 bytes | "
            f"overruns {report['dma_overruns']}")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--list', action='store_true', help='List candidate serial ports only')
    parser.add_argument('--port', help='Explicit serial port to read')
    parser.add_argument('--seconds', type=float, default=10, help='Bounded read duration (default 10)')
    parser.add_argument('--json', action='store_true', help='Print all measured fields as JSON lines')
    args = parser.parse_args()
    if args.list:
        print('\n'.join(sorted(glob.glob('/dev/cu.usbmodem*') + glob.glob('/dev/ttyACM*')))
              or 'No candidate USB serial ports found.')
        return
    if not args.port:
        parser.error('Choose an explicit --port; use --list to see candidates')
    if not math.isfinite(args.seconds) or not 0 < args.seconds <= 3600:
        parser.error('--seconds must be greater than zero and at most 3600')
    descriptor = os.open(args.port, os.O_RDONLY | os.O_NOCTTY | os.O_NONBLOCK)
    original = None
    count = 0
    decoder = Decoder()
    try:
        original = termios.tcgetattr(descriptor)
        raw = original[:]
        raw[0] = raw[1] = raw[3] = 0
        raw[2] = termios.CS8 | termios.CREAD | termios.CLOCAL
        raw[4] = raw[5] = termios.B115200
        raw[6] = original[6][:]
        raw[6][termios.VMIN] = 0
        raw[6][termios.VTIME] = 0
        termios.tcsetattr(descriptor, termios.TCSANOW, raw)
        until = time.monotonic() + args.seconds
        while time.monotonic() < until:
            available, _, _ = select.select([descriptor], [], [], min(.5, max(0, until-time.monotonic())))
            if not available:
                continue
            try:
                data = os.read(descriptor, 4096)
            except BlockingIOError:
                continue
            if not data:
                raise OSError('USB serial connection closed')
            for report in decoder.feed(data):
                count += 1
                print(json.dumps(report) if args.json else summary(report), flush=True)
    finally:
        try:
            if original is not None:
                termios.tcsetattr(descriptor, termios.TCSANOW, original)
        finally:
            os.close(descriptor)
    if not count:
        raise SystemExit('No valid diagnostic reports received. This does not establish firmware execution.')


if __name__ == '__main__':
    try:
        main()
    except (OSError, termios.error) as error:
        raise SystemExit(f'Unable to read diagnostics: {error}')
