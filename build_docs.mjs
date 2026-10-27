import { cpSync, existsSync, lstatSync, readdirSync, readFileSync, rmSync, writeFileSync } from 'node:fs'
import { basename, extname, resolve } from 'node:path'
import { fileURLToPath } from 'node:url'
import { spawnSync } from 'node:child_process'

const root = fileURLToPath(new URL('.', import.meta.url))
const frontend = resolve(root, '__default__')
const versions = JSON.parse(readFileSync(resolve(root, 'versions.json'), 'utf8'))
const inputs = ['__default__', ...versions, 'versions.json', 'build_docs.mjs', 'build_docs.sh', 'build_docs.bat', '.githooks', '.gitattributes']
const outputs = ['index.html', 'assets', 'docs', 'about', 'notices', 'favicon.svg', '.nojekyll']
const textExtensions = new Set(['.bat', '.cmd', '.css', '.html', '.js', '.jsx', '.json', '.md', '.mjs', '.cjs', '.sh', '.svg', '.ts', '.tsx', '.txt', '.yaml', '.yml'])
const textNames = new Set(['.gitattributes', '.gitignore', '.nojekyll', '.npmrc'])
const ignoredDirectories = new Set(['node_modules', 'dist', '.git', '.cache', '.vite', '.vite-temp', '.tmp', 'coverage'])

function normalizeLineEndings(path) {
  const filename = resolve(root, path)
  const stat = lstatSync(filename)
  if (stat.isSymbolicLink()) return 0
  if (stat.isDirectory()) {
    if (ignoredDirectories.has(basename(filename)) || /^__default__\/docs\/v[^/]+$/.test(path)) return 0
    return readdirSync(filename).reduce((count, name) => count + normalizeLineEndings(`${path}/${name}`), 0)
  }
  if (!stat.isFile() || (!textExtensions.has(extname(filename).toLowerCase())
    && !textNames.has(basename(filename)) && !path.startsWith('.githooks/'))) return 0

  const content = readFileSync(filename)
  if (content.includes(0) || !content.includes(13)) return 0
  let length = 0
  for (let index = 0; index < content.length; index++) {
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

function run(command, args, cwd = root, capture = false) {
  const result = spawnSync(command, args, { cwd, encoding: 'utf8', stdio: capture ? 'pipe' : 'inherit' })
  if (result.error) throw result.error
  if (result.status !== 0) throw new Error(`${command} failed (${result.status})${result.stderr ? `: ${result.stderr}` : ''}`)
  return result.stdout?.trim() ?? ''
}

try {
  const normalized = inputs.reduce((count, path) => count + normalizeLineEndings(path), 0)
  if (normalized) console.log(`Normalized ${normalized} build input file(s) to LF.`)
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
    ...versions.map((version) => `docs/v${version}/index.html`)]) {
    if (!existsSync(resolve(dist, path))) throw new Error(`Missing website output: ${path}`)
  }
  for (const path of outputs) {
    const target = resolve(root, path)
    rmSync(target, { recursive: true, force: true })
    cpSync(resolve(dist, path), target, { recursive: true })
  }
  if (commit) run('git', ['add', '-A', '--', ...outputs])
  console.log('Updated the static website at the gh-pages worktree root.')
} catch (error) {
  console.error(error.message)
  process.exitCode = 1
}
