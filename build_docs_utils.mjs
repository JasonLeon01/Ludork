import { lstatSync, readdirSync, readFileSync, writeFileSync } from 'node:fs'
import { basename, extname, resolve } from 'node:path'
import { fileURLToPath } from 'node:url'
import { spawnSync } from 'node:child_process'

export const root = fileURLToPath(new URL('.', import.meta.url))
export const editableInputs = ['__default__', 'trunk', 'versions.json',
  'build_docs.mjs', 'build_docs.sh', 'build_docs.bat', 'build_docs_utils.mjs',
  'build_docs_release.mjs', 'build_docs_release.sh', 'build_docs_release.bat',
  'docs_versions.mjs', 'docs_versions.d.mts', '.githooks', '.gitattributes', '.gitignore']

const textExtensions = new Set(['.bat', '.cmd', '.css', '.html', '.js', '.jsx', '.json', '.md', '.mjs', '.mts', '.cjs', '.sh', '.svg', '.ts', '.tsx', '.txt', '.yaml', '.yml'])
const textNames = new Set(['.gitattributes', '.gitignore', '.nojekyll', '.npmrc'])
const ignoredDirectories = new Set(['node_modules', 'dist', '.git', '.cache', '.vite', '.vite-temp', '.tmp', 'coverage'])

export function normalizeTextInputs(path) {
  const filename = resolve(root, path)
  const stat = lstatSync(filename)
  if (stat.isSymbolicLink()) return 0
  if (stat.isDirectory()) {
    if (ignoredDirectories.has(basename(filename)) || /^__default__\/docs\/(?:trunk|v[^/]+)$/.test(path)) return 0
    return readdirSync(filename).reduce((count, name) => count + normalizeTextInputs(`${path}/${name}`), 0)
  }
  if (!stat.isFile() || (!textExtensions.has(extname(filename).toLowerCase())
    && !textNames.has(basename(filename)) && !path.startsWith('.githooks/'))) return 0

  const content = readFileSync(filename)
  const start = content[0] === 0xef && content[1] === 0xbb && content[2] === 0xbf ? 3 : 0
  if (content.includes(0) || (!start && !content.includes(13))) return 0
  let length = 0
  for (let index = start; index < content.length; index++) {
    if (content[index] === 13) {
      content[length++] = 10
      if (content[index + 1] === 10) index++
    } else {
      content[length++] = content[index]
    }
  }
  writeFileSync(filename, content.subarray(0, length))
  return 1
}

export function run(command, args, cwd = root, capture = false) {
  const result = spawnSync(command, args, { cwd, encoding: 'utf8', stdio: capture ? 'pipe' : 'inherit' })
  if (result.error) throw result.error
  if (result.status !== 0) throw new Error(`${command} failed (${result.status})${result.stderr ? `: ${result.stderr}` : ''}`)
  return result.stdout?.trim() ?? ''
}
