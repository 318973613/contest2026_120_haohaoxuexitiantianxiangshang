#!/usr/bin/env python3
"""Render the actual board-side UI with host LVGL and deterministic services."""
import os
from pathlib import Path
import subprocess

tests = Path(__file__).resolve().parent
tree = os.environ.get('OPENVELA_ROOT', '/home/openvela/openvela')
build = tests / 'out/ui-build'
renders = tests / 'out/renders'
renders.mkdir(parents=True, exist_ok=True)
env = dict(os.environ, OPENVELA_ROOT=tree,
           ASAN_OPTIONS='detect_leaks=0:abort_on_error=1',
           UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1')
with (renders / 'build.log').open('w') as log:
    subprocess.run(['cmake', '-S', str(tests / 'ui'), '-B', str(build)],
                   stdout=log, stderr=subprocess.STDOUT, check=True)
    subprocess.run(['cmake', '--build', str(build), '-j8'],
                   stdout=log, stderr=subprocess.STDOUT, check=True)
for width, height in [(480, 320), (1280, 800)]:
    result = subprocess.run([str(build / 'ui'), str(width), str(height)],
                            cwd=renders, env=env, text=True,
                            stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                            timeout=120)
    (renders / f'ui-{width}x{height}.log').write_text(result.stdout)
    print(result.stdout, end='', flush=True)
    try:
        from PIL import Image
    except ImportError:
        print('PPM snapshots retained; use the Windows bundled Pillow for PNG conversion.')
    else:
        for path in renders.glob('*.ppm'):
            Image.open(path).save(path.with_suffix('.png'))
    if result.returncode:
        raise SystemExit(result.returncode)
