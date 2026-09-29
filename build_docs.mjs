import { cpSync, existsSync, readFileSync, rmSync } from 'node:fs'
import { resolve } from 'node:path'
import { fileURLToPath } from 'node:url'
import { spawnSync } from 'node:child_process'

const root = fileURLToPath(new URL('.', import.meta.url))
const frontend = resolve(root, '__default__')
const versions = JSON.parse(readFileSync(resolve(root, 'versions.json'), 'utf8'))
const outputs = ['index.html', 'assets', 'docs', 'about', 'notices', 'favicon.svg', '.nojekyll']

function run(command, args, cwd = root, capture = false) {
  const result = spawnSync(command, args, { cwd, encoding: 'utf8', stdio: capture ? 'pipe' : 'inherit' })
  if (result.error) throw result.error
  if (result.status !== 0) throw new Error(`${command} failed (${result.status})${result.stderr ? `: ${result.stderr}` : ''}`)
  return result.stdout?.trim() ?? ''
}

try {
  const commit = process.argv.includes('--commit')
  if (commit) {
    const inputs = ['__default__', ...versions, 'versions.json', 'build_docs.mjs', 'build_docs.sh', 'build_docs.bat', '.githooks']
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
