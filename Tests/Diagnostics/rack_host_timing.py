#!/usr/bin/env python3
"""Compare real VCV Host audio with original, aligned, and direct CV wiring.

macOS ARM64; requires an installed, licensed VCV Host and the official Rack SDK.
Uses temporary user folders: no audio device, GUI, or changes to the user's patch.
"""
import argparse
import array
import base64
import concurrent.futures
import json
import math
import os
from pathlib import Path
import shutil
import struct
import subprocess
import tempfile
import time


def test_state():
    # Host 2.0.4 stores component/controller byte lengths before VST3 state.
    # The component here is JUCE's binary XML representation of APVTS state.
    values = {"model": 3, "attack": 0, "sustain": 1, "release": 0.02,
              "portamento": 0, "mseg1Destination": 0}
    xml = ('<MacroOscState>' + ''.join(
        f'<PARAM id="{key}" value="{value}"/>' for key, value in values.items())
        + '</MacroOscState>').encode()
    component = b'VC2!' + struct.pack('<I', len(xml)) + xml + b'\0'
    return base64.b64encode(struct.pack('<QQ', len(component), 0) + component).decode()


def patch(mode, plugin):
    modules = [
        {"id": 1, "plugin": "MacroOscDiagnostics", "model": "KeyboardFixture", "pos": [0, 0]},
        {"id": 2, "plugin": "ML_modules", "model": "Quantum", "pos": [4, 0],
         "data": {"scale": [1, 0, 1, 0, 1, 0, 0, 1, 0, 1, 0, 0],
                  "mode": 1, "transpose_select": 1, "toggle_mode": 0}},
        {"id": 3, "plugin": "Fundamental", "model": "VCA-1", "pos": [12, 0],
         "params": [{"id": 0, "value": 1}, {"id": 1, "value": 1}]},
        {"id": 4, "plugin": "VCV-Host", "model": "Host", "pos": [16, 0],
         "data": {"pluginPath": str(plugin), "pluginName": "Macro OSC", "subPluginId": 0,
                  "pluginState": test_state(), "parameterIds": [-1] * 16,
                  "busChannelMappings": [[], [[0, 1]]], "blockSize": 256,
                  "worker": False, "bypass": False}},
    ]
    connections = [(4, 0, 1, 0)]
    connections += [(1, 0, 4, 16)] if mode == 'direct' else [(1, 0, 2, 0), (2, 0, 4, 16)]
    connections += [(1, 1, 3, 1), (3, 0, 4, 17)] if mode == 'aligned' else [(1, 1, 4, 17)]
    cables = [dict(id=i + 1, outputModuleId=a, outputId=b, inputModuleId=c,
                   inputId=d, color='#ff5050') for i, (a, b, c, d) in enumerate(connections)]
    return dict(version='2.6.6', modules=modules, cables=cables)


def main():
    args = argparse.ArgumentParser(description=__doc__)
    args.add_argument('--sdk', type=Path, required=True)
    args.add_argument('--plugin', type=Path, required=True)
    args.add_argument('--app', type=Path, default=Path('/Applications/VCV Rack 2 Pro.app'))
    args.add_argument('--user', type=Path, default=Path.home() / 'Library/Application Support/Rack2')
    options = args.parse_args()
    resources = options.app / 'Contents/Resources'
    modes = ['original', 'aligned', 'direct']
    with tempfile.TemporaryDirectory(prefix='macro-osc-rack-') as temporary:
        root = Path(temporary)
        fixture = root / 'MacroOscDiagnostics'
        fixture.mkdir()
        subprocess.run(['clang++', '-std=c++17', '-O2', '-fPIC', '-shared',
                        '-undefined', 'dynamic_lookup', f'-I{options.sdk / "include"}',
                        f'-I{options.sdk / "dep/include"}',
                        str(Path(__file__).with_name('RackHostFixture.cpp')),
                        '-o', str(fixture / 'plugin.dylib')], check=True)
        (fixture / 'plugin.json').write_text(json.dumps({
            'slug': 'MacroOscDiagnostics', 'name': 'Macro OSC diagnostics', 'version': '2.0.0',
            'license': 'proprietary', 'modules': [{'slug': 'KeyboardFixture', 'name': 'Keyboard fixture'}]}))
        for mode in modes:
            folder = root / mode
            (folder / 'autosave').mkdir(parents=True)
            plugins = folder / 'plugins-mac-arm64'
            plugins.mkdir()
            # Real copies prevent Rack's bundled-plugin installer writing
            # through symlinks into the normal user directory.
            for slug in ['VCV-Host', 'ML_modules', 'Fundamental']:
                shutil.copytree(options.user / 'plugins-mac-arm64' / slug, plugins / slug)
            shutil.copytree(fixture, plugins / fixture.name)
            if (options.user / 'licenses').is_dir():
                shutil.copytree(options.user / 'licenses', folder / 'licenses')
            (folder / 'autosave/patch.json').write_text(json.dumps(patch(mode, options.plugin.resolve())))
            (folder / 'settings.json').write_text(json.dumps({'sampleRate': 48000, 'threadCount': 1}))

        def render(mode):
            folder = root / mode
            with (folder / 'console.log').open('w') as log:
                process = subprocess.Popen([str(options.app / 'Contents/MacOS/Rack'), '-h', '-u', str(folder)],
                    stdin=subprocess.PIPE, stdout=log, stderr=log, text=True,
                    env=dict(os.environ, DYLD_LIBRARY_PATH=str(resources)))
                try:
                    deadline = time.monotonic() + 40
                    while not (folder / 'host-result.json').exists():
                        if process.poll() is not None or time.monotonic() >= deadline:
                            raise RuntimeError(f'{mode}: Rack did not finish\n{(folder / "console.log").read_text()}')
                        time.sleep(0.1)
                    process.communicate('\n', timeout=10)
                    if process.returncode != 0:
                        raise RuntimeError(f'{mode}: Rack exited {process.returncode}')
                finally:
                    if process.poll() is None:
                        process.terminate()
                        try:
                            process.wait(timeout=5)
                        except subprocess.TimeoutExpired:
                            process.kill()
                            process.wait()
            recording = array.array('f', (folder / 'audio.f32').read_bytes())
            if len(recording) != 589824 or not all(map(math.isfinite, recording)):
                raise RuntimeError(f'{mode}: invalid recording')
            if max(map(abs, recording)) < 0.001:
                raise RuntimeError(f'{mode}: silent test cannot validate notes')
            return recording

        with concurrent.futures.ThreadPoolExecutor(max_workers=3) as pool:
            audio = dict(zip(modes, pool.map(render, modes)))
        # The aligned path has one extra cable/sample compared with direct.
        differences = {mode: max(abs(a - b) for a, b in zip(audio[mode][1:], audio['direct'][:-1]))
                       for mode in ['original', 'aligned']}
        for mode, peak in differences.items():
            print(f'{mode}: peak audio difference from direct = {peak:.9f} V')
        if differences['original'] < 0.01:
            raise RuntimeError('Original wiring did not reproduce the fault')
        if differences['aligned'] > 1e-6:
            raise RuntimeError('Aligned wiring still differs from direct keyboard audio')
        for mode, samples in audio.items():
            for note in range(16):
                start = 65536 + note * 32768
                if max(map(abs, samples[start + 16000:start + 32768])) > 1e-6:
                    raise RuntimeError(f'{mode}: note {note} continued after release')
        print('PASS: original wiring reproduces; aligned wiring matches direct; all key-ups become silent')


if __name__ == '__main__':
    main()
