import { cpSync, existsSync, mkdirSync, mkdtempSync, readFileSync, renameSync, rmSync } from 'node:fs'
import { resolve } from 'node:path'
import { editableInputs, normalizeTextInputs, root, run } from './build_docs_utils.mjs'
import { getDocsRoute, sortDocsVersions } from './docs_versions.mjs'

const frontend = resolve(root, '__default__')
const outputs = ['index.html', 'assets', 'docs', 'about', 'notices', 'favicon.svg', '.nojekyll']

function publishOutputs(dist) {
  const staging = mkdtempSync(resolve(root, '.docs-build-'))
  const next = resolve(staging, 'next')
  const previous = resolve(staging, 'previous')
  const replaced = []
  let preserveBackup = false
  try {
    mkdirSync(next)
    mkdirSync(previous)
    for (const path of outputs) cpSync(resolve(dist, path), resolve(next, path), { recursive: true })
    try {
      for (const path of outputs) {
        const target = resolve(root, path)
        const entry = { path, backedUp: false, installed: false }
        replaced.push(entry)
        if (existsSync(target)) {
          renameSync(target, resolve(previous, path))
          entry.backedUp = true
        }
        renameSync(resolve(next, path), target)
        entry.installed = true
      }
    } catch (error) {
      const failures = []
      for (const entry of replaced.reverse()) {
        try {
          const target = resolve(root, entry.path)
          if (entry.installed) rmSync(target, { recursive: true, force: true })
          if (entry.backedUp) renameSync(resolve(previous, entry.path), target)
        } catch (restoreError) {
          failures.push(restoreError.message)
        }
      }
      if (failures.length) {
        preserveBackup = true
        throw new Error(`${error.message}. Could not restore all website outputs: ${failures.join('; ')}. Backup retained at ${staging}`)
      }
      throw error
    }
  } finally {
    if (!preserveBackup) {
      try {
        rmSync(staging, { recursive: true, force: true })
      } catch (error) {
        console.warn(`Could not remove build staging directory ${staging}: ${error.message}`)
      }
    }
  }
}

try {
  let normalized = normalizeTextInputs('versions.json')
  const versions = sortDocsVersions(JSON.parse(readFileSync(resolve(root, 'versions.json'), 'utf8')))
  const inputs = [...editableInputs, ...versions.filter((version) => version !== 'trunk')]
  normalized += editableInputs.reduce((count, path) => count + normalizeTextInputs(path), 0)
  if (normalized) console.log(`Normalized ${normalized} build input file(s) to LF without a UTF-8 BOM.`)
  const commit = process.argv.includes('--commit')
  if (commit) {
    if (run('git', ['diff', '--name-only', '--', ...inputs], root, true)
      || run('git', ['ls-files', '--others', '--exclude-standard', '--', ...inputs], root, true)) {
      throw new Error('Stage all documentation and frontend build inputs before committing. Unstaged inputs would produce a different website.')
    }
  }

  if (process.platform === 'win32') run('cmd.exe', ['/d', '/c', 'npm run build'], frontend)
  else run('npm', ['run', 'build'], frontend)

  const dist = resolve(frontend, 'dist')
  for (const path of ['index.html', 'assets', 'about/index.html', 'notices/index.html', 'favicon.svg', '.nojekyll',
    ...versions.map((version) => `${getDocsRoute(version)}index.html`)]) {
    if (!existsSync(resolve(dist, path))) throw new Error(`Missing website output: ${path}`)
  }
  publishOutputs(dist)
  if (commit) run('git', ['add', '-A', '--', ...outputs])
  console.log('Updated the static website at the gh-pages worktree root.')
} catch (error) {
  console.error(error.message)
  process.exitCode = 1
}
