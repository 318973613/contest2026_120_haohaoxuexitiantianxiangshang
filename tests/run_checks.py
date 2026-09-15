#!/usr/bin/env python3
"""Run the real changed C modules with deterministic device/network fixtures."""
import os
from pathlib import Path
import subprocess
import sys

tests = Path(__file__).resolve().parent
repo = tests.parent
# The full openvela tree (nuttx/, packages/, apps/, external/) - override with
# OPENVELA_ROOT when it is not at the default location.
tree = Path(os.environ.get('OPENVELA_ROOT', '/home/openvela/openvela'))
agent = tree / 'packages/ai_agent'
app = repo / 'app/hello_app'
out = tests / 'out'
out.mkdir(exist_ok=True)
cjson = tree / 'apps/netutils/cjson/cJSON'
flags = ['gcc', '-std=c11', '-D_GNU_SOURCE', '-g', '-O1', '-Wall', '-Wextra',
         '-Werror=implicit-function-declaration', '-pthread',
         '-fsanitize=address,undefined', '-fno-omit-frame-pointer', '-no-pie',
         '-I' + str(tests / 'include'), '-I' + str(agent / 'src'),
         '-I' + str(agent / 'include'), '-I' + str(app), '-I' + str(cjson),
         '-DTEST_REAL_AGENT_CONFIG_PATH="' + str(agent / 'include/agent_config.h') + '"']
mbedtls = tree / 'apps/crypto/mbedtls/mbedtls'

def compile_run(name, sources, fixture, extra=()):
    objects = []
    for path in sources:
        obj = out / (name + '-' + path.stem + '.o')
        subprocess.run(flags + list(extra) + ['-c', str(path), '-o', str(obj)],
                       check=True)
        objects.append(str(obj))
    exe = out / name
    subprocess.run(flags + [str(tests / fixture)] + objects + ['-o', str(exe)],
                   check=True)
    env = dict(os.environ, ASAN_OPTIONS='detect_leaks=0:abort_on_error=1',
               UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1')
    result = subprocess.run([str(exe)], cwd=out, env=env, text=True,
                            stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                            timeout=120)
    (out / (name + '.log')).write_text(result.stdout, encoding='utf-8')
    print(result.stdout, end='', flush=True)
    if result.returncode:
        raise SystemExit(result.returncode)

selected = set(sys.argv[1:] or ['voice', 'chat', 'cron', 'focus', 'api'])
if 'voice' in selected:
    compile_run('voice', [agent / 'src/voice/voice_channel.c',
                     agent / 'src/voice/voice_wake.c', app / 'voice_ui_bridge.c'],
            'voice_integration.c', ['-Dgetpid=test_getpid',
                                   '-Dpthread_create=test_pthread_create',
                                   '-Dmalloc=test_malloc'])
if 'chat' in selected:
    compile_run('chat', [app / 'ai_chat_bridge.c'], 'chat_integration.c',
            ['-Dclock_gettime=test_clock_gettime'])
if 'cron' in selected:
    compile_run('cron', [agent / 'src/infra/cron_service.c',
                    agent / 'src/tools/tool_cron.c', cjson / 'cJSON.c'],
            'cron_integration.c', ['-Dfopen=test_fopen', '-Dtime=test_time'])
if 'focus' in selected:
    compile_run('focus', [cjson / 'cJSON.c'], 'focus_stats_test.c')
if 'api' in selected:
    flags += ['-I' + str(mbedtls / 'include'), '-ffunction-sections',
              '-fdata-sections', '-Wl,--gc-sections']
    compile_run('api', [agent / 'src/llm/llm_proxy.c',
                       agent / 'src/llm/llm_parse.c',
                       agent / 'src/voice/mimo_asr.c', cjson / 'cJSON.c',
                       mbedtls / 'library/base64.c',
                       mbedtls / 'library/constant_time.c'],
                'api_integration.c', ['-DTEST_REAL_AGENT_CONFIG',
                    '-DMBEDTLS_CONFIG_FILE="test_mbedtls_config.h"'])
