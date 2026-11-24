import assert from 'node:assert/strict';
import test from 'node:test';
import { getT2XnuHostStatus, XNULINUX_MIRROR_BRANCH } from '../src/index.mts';

test('getT2XnuHostStatus detects ARM64 Linux readiness cleanly', () => {
  const arm = getT2XnuHostStatus('linux', 'arm64');
  assert.equal(arm.arm64LinuxReady, true);
  assert.equal(arm.mirrorBranch, XNULINUX_MIRROR_BRANCH);

  const x64 = getT2XnuHostStatus('linux', 'x64');
  assert.equal(x64.arm64LinuxReady, false);

  const win = getT2XnuHostStatus('win32', 'x64');
  assert.equal(win.arm64LinuxReady, false);
});
