#!/usr/bin/env python3
"""Check the executable's observable command-line behavior."""
import subprocess
import sys
import tempfile
import wave
from pathlib import Path

binary = sys.argv[1]
for value in ['nan', 'inf', '-inf', '-1', '0', 'broken', '1x', '1e99999']:
    result = subprocess.run([binary, '--seconds', value], capture_output=True, text=True, timeout=3)
    assert result.returncode == 2, (value, result.returncode, result.stderr)
    assert 'positive finite' in result.stderr, (value, result.stderr)
missing = subprocess.run([binary, '--seconds'], capture_output=True, text=True, timeout=3)
assert missing.returncode == 2, missing.stderr
help_result = subprocess.run([binary, '--help'], capture_output=True, text=True, timeout=3)
assert help_result.returncode == 0 and 'Pocket Synth' in help_result.stdout
print('PASS: invalid durations exit cleanly; help is available')

with tempfile.TemporaryDirectory() as folder:
    for option in ['--render-sounds', '--render-loop-demo']:
        path = Path(folder) / 'audio.wav'
        result = subprocess.run([binary, option, str(path)], capture_output=True, text=True, timeout=30)
        assert result.returncode == 0, result.stderr
        with wave.open(str(path), 'rb') as audio:
            assert audio.getnchannels() == 2 and audio.getframerate() == 48000
            assert audio.getsampwidth() == 2 and audio.getnframes() > 0
            assert len(audio.readframes(audio.getnframes())) == audio.getnframes() * 4
print('PASS: sound and loop exports produce complete playable WAV files')
