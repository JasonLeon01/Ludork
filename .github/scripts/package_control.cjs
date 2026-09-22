const fs = require('node:fs');
const path = require('node:path');

async function shouldRun({ github, context, core }) {
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
      workflow_id: 'export-editor.yml',
      per_page: 100,
      page,
    });
    for (const run of data.workflow_runs) {
      if (run.id === context.runId || run.conclusion !== 'success'
          || run.head_branch !== defaultBranch
          || !['schedule', 'workflow_dispatch'].includes(run.event)) {
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
        marker = jobs.jobs.find(job => job.name === 'Record successful package'
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
        latestPackage = { run, completedAt };
      }
    }
    if (data.workflow_runs.length < 100) break;
  }

  if (latestPackage) {
    const { run } = latestPackage;
    const build = run.head_sha !== context.sha;
    core.info(`Latest successful default-branch package: ${run.head_sha} (run ${run.id}).`);
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

  const entries = fs.readdirSync(assetDirectory, { withFileTypes: true });
  if (entries.length !== 2 || entries.some(entry => !entry.isFile())
      || entries.filter(entry => entry.name.endsWith('.msi')).length !== 1
      || entries.filter(entry => entry.name.endsWith('.dmg')).length !== 1) {
    throw new Error('Release assets must contain exactly one Windows MSI and one macOS DMG.');
  }
  const assets = entries.map(entry => {
    const filename = path.join(assetDirectory, entry.name);
    const size = fs.statSync(filename).size;
    if (size === 0) throw new Error(`Release asset is empty: ${entry.name}`);
    fs.accessSync(filename, fs.constants.R_OK);
    return { name: entry.name, filename, size };
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
        && !/^Ludork-.+-(windows-x64\.msi|macos-arm64\.dmg)$/.test(asset.name)) continue;
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
        'content-type': asset.name.endsWith('.msi') ? 'application/x-msi' : 'application/x-apple-diskimage',
        'content-length': asset.size,
      },
      data: fs.createReadStream(asset.filename),
    });
  }
  core.info(`Draft release ready: ${release.html_url}`);
}

module.exports = { shouldRun, publishRelease };
