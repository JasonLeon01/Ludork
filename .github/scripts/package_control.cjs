const fs = require('node:fs');
const path = require('node:path');

const templateAssets = ['source', 'source-ffmpeg', 'standalone', 'standalone-ffmpeg']
  .flatMap(name => [`game-${name}.zip`, `game-${name}.tar.gz`]);
const serverAssetPattern = /^LudorkServer-[0-9a-f]{40}\.tar\.gz$/;

function validateReleaseTag(context) {
  const configuration = fs.readFileSync(path.join(__dirname, '../../versions.conf'), 'utf8');
  const version = /^EDITOR_VERSION=(\d+\.\d+\.\d+)\s*$/m.exec(configuration)?.[1];
  if (!version || context.ref !== `refs/tags/v${version}`) {
    throw new Error('The release tag must match v<EDITOR_VERSION> from versions.conf.');
  }
  return version;
}

async function resolveServerCommit({ github, context, core }) {
  const { data } = await github.rest.repos.getCommit({ ...context.repo, ref: 'Server' });
  if (!/^[0-9a-f]{40}$/.test(data.sha)) throw new Error('Server did not resolve to a commit.');
  core.setOutput('server_sha', data.sha);
  return data.sha;
}

async function shouldRun({ github, context, core }, workflow, markerName, serverSha) {
  if (!/^[0-9a-f]{40}$/.test(serverSha)) {
    throw new Error('A resolved Server commit SHA is required for package decisions.');
  }
  if (context.eventName !== 'schedule') {
    core.setOutput('should_build', true);
    return true;
  }

  const defaultBranch = context.payload.repository.default_branch;
  if (context.ref !== `refs/heads/${defaultBranch}`) {
    throw new Error('Scheduled packages must run on the default branch.');
  }

  let latestPackage;
  for (let page = 1; ; page++) {
    const { data } = await github.rest.actions.listWorkflowRuns({
      ...context.repo,
      workflow_id: workflow,
      per_page: 100,
      page,
    });
    for (const run of data.workflow_runs) {
      if (run.id === context.runId || run.conclusion !== 'success'
          || run.head_branch !== defaultBranch
          || !['schedule', 'workflow_dispatch', 'push'].includes(run.event)) {
        continue;
      }

      let marker;
      for (let jobPage = 1; ; jobPage++) {
        const { data: jobs } = await github.rest.actions.listJobsForWorkflowRun({
          ...context.repo,
          run_id: run.id,
          filter: 'latest',
          per_page: 100,
          page: jobPage,
        });
        marker = jobs.jobs.find(job => job.name.startsWith(`${markerName} (`)
          && job.name.endsWith(')')
          && /^[0-9a-f]{40}$/.test(job.name.slice(markerName.length + 2, -1))
          && job.conclusion === 'success');
        if (marker || jobs.jobs.length < 100) break;
      }
      if (!marker) continue;
      const completedAt = Date.parse(marker.completed_at);
      if (!Number.isFinite(completedAt)) {
        throw new Error(`Successful package marker has no completion timestamp (run ${run.id}).`);
      }
      if (!latestPackage || completedAt > latestPackage.completedAt
          || (completedAt === latestPackage.completedAt && run.id > latestPackage.run.id)) {
        latestPackage = { run, completedAt,
          serverSha: marker.name.slice(markerName.length + 2, -1) };
      }
    }
    if (data.workflow_runs.length < 100) break;
  }

  if (latestPackage) {
    const { run } = latestPackage;
    const build = run.head_sha !== context.sha || latestPackage.serverSha !== serverSha;
    core.info(`Latest successful default-branch package: ${run.head_sha}, Server ${latestPackage.serverSha} (run ${run.id}).`);
    core.setOutput('should_build', build);
    return build;
  }

  core.info('No previous successful default-branch package found.');
  core.setOutput('should_build', true);
  return true;
}

async function publishRelease({ github, context, core }, assetDirectory) {
  if (context.eventName !== 'push' || !context.ref.startsWith('refs/tags/v')) {
    throw new Error('Draft releases require a push of a v-prefixed tag.');
  }
  const version = validateReleaseTag(context);

  const entries = fs.readdirSync(assetDirectory, { withFileTypes: true });
  const expected = new Set([
    `Ludork-${version}-windows-x64.msi`, `Ludork-${version}-windows-x64.exe`,
    `Ludork-${version}-macos-arm64.dmg`, ...templateAssets,
  ]);
  if (entries.length !== expected.size + 1 || entries.some(entry => !entry.isFile())
      || entries.filter(entry => serverAssetPattern.test(entry.name)).length !== 1
      || entries.some(entry => !expected.has(entry.name) && !serverAssetPattern.test(entry.name))) {
    throw new Error('Release assets must contain the matching Windows MSI and EXE, macOS DMG, Server tar.gz and eight project templates.');
  }
  const assets = entries.map(entry => {
    const filename = path.join(assetDirectory, entry.name);
    const size = fs.statSync(filename).size;
    if (size === 0) throw new Error(`Release asset is empty: ${entry.name}`);
    fs.accessSync(filename, fs.constants.R_OK);
    const name = serverAssetPattern.test(entry.name) ? `LudorkServer-${version}.tar.gz` : entry.name;
    return { name, filename, size };
  });

  const tag = context.ref.slice('refs/tags/'.length);
  let release;
  for (let page = 1; ; page++) {
    const { data } = await github.rest.repos.listReleases({
      ...context.repo, per_page: 100, page,
    });
    release = data.find(item => item.tag_name === tag);
    if (release || data.length < 100) break;
  }
  if (release && !release.draft) {
    throw new Error(`Release ${tag} is already published; refusing to modify it.`);
  }
  if (!release) {
    const { data } = await github.rest.repos.createRelease({
      ...context.repo,
      tag_name: tag,
      target_commitish: context.sha,
      name: tag,
      draft: true,
      generate_release_notes: true,
    });
    release = data;
  }

  const ensureDraft = async () => {
    const { data } = await github.rest.repos.getRelease({
      ...context.repo, release_id: release.id,
    });
    if (!data.draft) throw new Error(`Release ${tag} was published; refusing to modify it.`);
  };
  const previousAssets = await github.paginate(github.rest.repos.listReleaseAssets, {
    ...context.repo, release_id: release.id, per_page: 100,
  });
  for (const asset of previousAssets) {
    if (!assets.some(current => current.name === asset.name)
        && !/^Ludork-.+-(windows-x64\.(?:msi|exe)|macos-arm64\.dmg)$/.test(asset.name)
        && !/^LudorkServer-(?:[0-9a-f]{40}|\d+\.\d+\.\d+)\.tar\.gz$/.test(asset.name)
        && !/^LudorkServer-source-(?:[0-9a-f]{40}|\d+\.\d+\.\d+)\.(?:zip|tar\.gz)$/.test(asset.name)) continue;
    await ensureDraft();
    await github.rest.repos.deleteReleaseAsset({ ...context.repo, asset_id: asset.id });
  }
  for (const asset of assets) {
    await ensureDraft();
    await github.rest.repos.uploadReleaseAsset({
      ...context.repo,
      release_id: release.id,
      name: asset.name,
      headers: {
        'content-type': asset.name.endsWith('.zip') ? 'application/zip'
          : asset.name.endsWith('.tar.gz') ? 'application/gzip'
          : asset.name.endsWith('.msi') ? 'application/x-msi'
          : asset.name.endsWith('.exe') ? 'application/vnd.microsoft.portable-executable'
          : 'application/x-apple-diskimage',
        'content-length': asset.size,
      },
      data: fs.createReadStream(asset.filename),
    });
  }
  core.info(`Draft release ready: ${release.html_url}`);
}

module.exports = { shouldRun, publishRelease, resolveServerCommit, validateReleaseTag };
