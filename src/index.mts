export const T2XNU_CAS_INDEX_URL =
  process.env.B8G_GHOST_INDEX_URL ??
  'https://raw.githubusercontent.com/awesome-os/b8g-ghost/main/manifests/index.json';

export const XNULINUX_MIRROR_BRANCH = 'mirror/xnulinux';

export interface T2XnuHostStatus {
  platform: NodeJS.Platform;
  arch: string;
  arm64LinuxReady: boolean;
  mirrorBranch: string;
  casIndexUrl: string;
}

export function getT2XnuHostStatus(
  platform: NodeJS.Platform = process.platform,
  arch: string = process.arch,
): T2XnuHostStatus {
  return {
    platform,
    arch,
    arm64LinuxReady: platform === 'linux' && arch === 'arm64',
    mirrorBranch: XNULINUX_MIRROR_BRANCH,
    casIndexUrl: T2XNU_CAS_INDEX_URL,
  };
}

if (process.argv.includes('--status')) {
  console.log(JSON.stringify(getT2XnuHostStatus(), null, 2));
}
