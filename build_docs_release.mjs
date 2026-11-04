import { cpSync, lstatSync, mkdirSync, readdirSync, readFileSync, rmSync, writeFileSync } from 'node:fs'
import { resolve } from 'node:path'
import { normalizeTextInputs, root, run } from './build_docs_utils.mjs'
import { isReleaseVersion, sortDocsVersions } from './docs_versions.mjs'

function validateSnapshotSource(path) {
  const stat = lstatSync(path)
  if (stat.isDirectory()) {
    for (const name of readdirSync(path)) validateSnapshotSource(resolve(path, name))
  } else if (!stat.isFile()) {
    throw new Error(`Snapshot sources must be regular files or directories: ${path}`)
  }
}

try {
  const args = process.argv.slice(2)
  const version = args[0]
  if (args.length !== 1 || !isReleaseVersion(version)) {
    throw new Error('Usage: build_docs_release <major.minor> (for example: 1.0; no v prefix or leading zeroes)')
  }

  const versionsPath = resolve(root, 'versions.json')
  const originalVersions = readFileSync(versionsPath)
  const versions = sortDocsVersions(JSON.parse(originalVersions.toString('utf8').replace(/^\uFEFF/, '')))
  const target = resolve(root, version)
  if (versions.includes(version) || lstatSync(target, { throwIfNoEntry: false })) {
    throw new Error(`Documentation release ${version} already exists; published snapshots cannot be overwritten.`)
  }
  const trunk = resolve(root, 'trunk')
  if (!lstatSync(trunk).isDirectory()) throw new Error('Expected a trunk content directory.')
  validateSnapshotSource(trunk)
  const normalized = normalizeTextInputs('trunk')
  if (normalized) console.log(`Normalized ${normalized} trunk file(s) to LF without a UTF-8 BOM.`)

  mkdirSync(target)
  try {
    cpSync(trunk, target, { recursive: true, force: false, errorOnExist: true })
    writeFileSync(versionsPath, `${JSON.stringify(sortDocsVersions([...versions, version]))}\n`)
    run(process.execPath, [resolve(root, 'build_docs.mjs')])
  } catch (error) {
    try {
      writeFileSync(versionsPath, originalVersions)
    } finally {
      rmSync(target, { recursive: true, force: true })
    }
    throw error
  }
  console.log(`Created documentation release ${version} from trunk. Changes are ready for review and remain unstaged.`)
} catch (error) {
  console.error(error.message)
  process.exitCode = 1
}
