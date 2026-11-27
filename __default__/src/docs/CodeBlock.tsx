import { isValidElement, useEffect, useState, type ReactNode } from 'react'
import { CheckIcon, CopyIcon } from '../components/Icons'
import type { LanguageKey } from '../i18n/languages'
import { LUDORK_SITE_MESSAGES } from '../i18n/messages'

type CodeBlockProps = {
  children: ReactNode
  language: LanguageKey
}

const LANGUAGE_LABELS: Record<string, string> = {
  lua: 'Lua',
  cpp: 'C++',
  'c++': 'C++',
  cc: 'C++',
  h: 'C++',
  hpp: 'C++',
  csharp: 'C#',
  cs: 'C#',
  json: 'JSON',
  bash: 'Bash',
  sh: 'Shell',
  shell: 'Shell',
  zsh: 'Shell',
  bat: 'Batch',
  cmd: 'Batch',
  dos: 'Batch',
  powershell: 'PowerShell',
  ps: 'PowerShell',
  ps1: 'PowerShell',
  xml: 'XML',
  axaml: 'AXAML',
  xaml: 'XAML',
  html: 'HTML',
  svg: 'SVG',
  yaml: 'YAML',
  yml: 'YAML',
  cmake: 'CMake',
  glsl: 'GLSL',
  ini: 'INI',
  diff: 'Diff',
  markdown: 'Markdown',
  md: 'Markdown',
  text: 'Text',
  txt: 'Text',
  plaintext: 'Text',
}

function extractText(node: ReactNode): string {
  if (typeof node === 'string' || typeof node === 'number') return String(node)
  if (Array.isArray(node)) return node.map(extractText).join('')
  if (isValidElement<{ children?: ReactNode }>(node)) return extractText(node.props.children)
  return ''
}

function extractLanguageLabel(children: ReactNode): string | null {
  const child = Array.isArray(children) ? children[0] : children
  if (!isValidElement<{ className?: string }>(child)) return null
  const className = child.props.className ?? ''
  const match = className.match(/\b(?:language|lang)-([a-z0-9_+#.-]+)/i)
  if (!match) return null
  const raw = match[1].toLowerCase()
  return LANGUAGE_LABELS[raw] ?? raw.toUpperCase()
}

async function copyText(text: string): Promise<boolean> {
  try {
    if (navigator.clipboard?.writeText) {
      await navigator.clipboard.writeText(text)
      return true
    }
  } catch {
    // Fall back to execCommand when Clipboard API is unavailable.
  }
  try {
    const textarea = document.createElement('textarea')
    textarea.value = text
    textarea.setAttribute('readonly', '')
    textarea.style.position = 'fixed'
    textarea.style.opacity = '0'
    document.body.appendChild(textarea)
    textarea.select()
    const copied = document.execCommand('copy')
    document.body.removeChild(textarea)
    return copied
  } catch {
    return false
  }
}

export default function CodeBlock({ children, language }: CodeBlockProps) {
  const [copied, setCopied] = useState(false)
  const messages = LUDORK_SITE_MESSAGES[language].docs
  const languageLabel = extractLanguageLabel(children)

  useEffect(() => {
    if (!copied) return
    const timer = window.setTimeout(() => setCopied(false), 1800)
    return () => window.clearTimeout(timer)
  }, [copied])

  const handleCopy = async () => {
    const code = extractText(children).replace(/\n$/, '')
    if (await copyText(code)) {
      setCopied(true)
    }
  }

  return (
    <div className="ludork-code-block">
      <div className="ludork-code-header">
        <span className="ludork-code-language">{languageLabel ?? ''}</span>
        <button
          type="button"
          className={`ludork-code-copy${copied ? ' is-copied' : ''}`}
          onClick={() => void handleCopy()}
          aria-label={copied ? messages.copied : messages.copyCode}
        >
          {copied ? <CheckIcon size={14} /> : <CopyIcon size={14} />}
          <span>{copied ? messages.copied : messages.copyCode}</span>
        </button>
      </div>
      <pre tabIndex={0}>{children}</pre>
    </div>
  )
}
