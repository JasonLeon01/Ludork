import hljs from 'highlight.js/lib/core'
import type { Emitter, HighlightResult } from 'highlight.js'
import bash from 'highlight.js/lib/languages/bash'
import cmake from 'highlight.js/lib/languages/cmake'
import cpp from 'highlight.js/lib/languages/cpp'
import csharp from 'highlight.js/lib/languages/csharp'
import diff from 'highlight.js/lib/languages/diff'
import dos from 'highlight.js/lib/languages/dos'
import glsl from 'highlight.js/lib/languages/glsl'
import ini from 'highlight.js/lib/languages/ini'
import json from 'highlight.js/lib/languages/json'
import lua from 'highlight.js/lib/languages/lua'
import markdown from 'highlight.js/lib/languages/markdown'
import plaintext from 'highlight.js/lib/languages/plaintext'
import powershell from 'highlight.js/lib/languages/powershell'
import xml from 'highlight.js/lib/languages/xml'
import yaml from 'highlight.js/lib/languages/yaml'

type HastText = {
  type: 'text'
  value: string
}

type HastElement = {
  type: 'element'
  tagName: string
  properties: Record<string, unknown>
  children: HastContent[]
}

type HastRoot = {
  type: 'root'
  children: HastContent[]
}

type HastContent = HastElement | HastText

type HastParent = HastRoot | HastElement

const CLASS_PREFIX = 'hljs-'

const highlighter = hljs.newInstance()
highlighter.registerLanguage('bash', bash)
highlighter.registerLanguage('cmake', cmake)
highlighter.registerLanguage('cpp', cpp)
highlighter.registerLanguage('csharp', csharp)
highlighter.registerLanguage('diff', diff)
highlighter.registerLanguage('dos', dos)
highlighter.registerLanguage('glsl', glsl)
highlighter.registerLanguage('ini', ini)
highlighter.registerLanguage('json', json)
highlighter.registerLanguage('lua', lua)
highlighter.registerLanguage('markdown', markdown)
highlighter.registerLanguage('plaintext', plaintext)
highlighter.registerLanguage('powershell', powershell)
highlighter.registerLanguage('xml', xml)
highlighter.registerLanguage('yaml', yaml)

highlighter.registerAliases(['sh', 'shell', 'zsh'], { languageName: 'bash' })
highlighter.registerAliases(['bat', 'cmd'], { languageName: 'dos' })
highlighter.registerAliases(['c++', 'cc', 'h', 'hh', 'hpp', 'hxx'], { languageName: 'cpp' })
highlighter.registerAliases(['cs', 'c#'], { languageName: 'csharp' })
highlighter.registerAliases(['axaml', 'xaml', 'html', 'svg'], { languageName: 'xml' })
highlighter.registerAliases(['yml'], { languageName: 'yaml' })
highlighter.registerAliases(['text', 'txt'], { languageName: 'plaintext' })
highlighter.registerAliases(['ps', 'ps1'], { languageName: 'powershell' })
highlighter.registerAliases(['md'], { languageName: 'markdown' })

class HastEmitter implements Emitter {
  readonly root: HastRoot = { type: 'root', children: [] }
  private readonly stack: HastParent[] = [this.root]

  addText(text: string): void {
    if (!text) return
    const current = this.stack[this.stack.length - 1]
    const last = current.children[current.children.length - 1]
    if (last?.type === 'text') {
      last.value += text
    } else {
      current.children.push({ type: 'text', value: text })
    }
  }

  startScope(name: string): void {
    this.openNode(String(name))
  }

  endScope(): void {
    this.closeNode()
  }

  openNode(name: string): void {
    const className = name.split('.').map((part, index) => (index ? `${part}${'_'.repeat(index)}` : `${CLASS_PREFIX}${part}`))
    const child: HastElement = {
      type: 'element',
      tagName: 'span',
      properties: { className },
      children: [],
    }
    this.stack[this.stack.length - 1].children.push(child)
    this.stack.push(child)
  }

  closeNode(): void {
    if (this.stack.length > 1) {
      this.stack.pop()
    }
  }

  __addSublanguage(other: Emitter, subLanguageName: string): void {
    if (!(other instanceof HastEmitter)) return
    const current = this.stack[this.stack.length - 1]
    if (subLanguageName) {
      current.children.push({
        type: 'element',
        tagName: 'span',
        properties: { className: [subLanguageName] },
        children: other.root.children,
      })
    } else {
      current.children.push(...other.root.children)
    }
  }

  finalize(): void {}

  toHTML(): string {
    return ''
  }
}

highlighter.configure({ __emitter: HastEmitter, classPrefix: CLASS_PREFIX } as Parameters<typeof highlighter.configure>[0])

function extractText(node: HastParent | HastText): string {
  if (node.type === 'text') return node.value
  return node.children.map(extractText).join('')
}

function getCodeLanguage(node: HastElement): string | false | undefined {
  const classList = node.properties.className
  if (!Array.isArray(classList)) return undefined

  let name: string | undefined
  for (const item of classList) {
    const value = String(item)
    if (value === 'no-highlight' || value === 'nohighlight') return false
    if (!name && value.startsWith('language-')) name = value.slice(9)
    else if (!name && value.startsWith('lang-')) name = value.slice(5)
  }
  return name
}

function visitCodeBlocks(node: HastParent, parent: HastParent | null): void {
  if (node.type === 'element' && node.tagName === 'code' && parent?.type === 'element' && parent.tagName === 'pre') {
    const language = getCodeLanguage(node)
    if (!language || !highlighter.getLanguage(language)) return

    const className = Array.isArray(node.properties.className) ? [...node.properties.className] : []
    if (!className.includes('hljs')) className.unshift('hljs')
    node.properties.className = className

    const result = highlighter.highlight(extractText(node), { language, ignoreIllegals: true }) as HighlightResult & {
      _emitter?: HastEmitter
    }
    if (result.errorRaised) return
    const children = result._emitter?.root.children
    if (children && children.length > 0) {
      node.children = children
    }
    return
  }

  for (const child of node.children) {
    if (child.type === 'element') {
      visitCodeBlocks(child, node)
    }
  }
}

export default function rehypeHighlight() {
  return (tree: HastRoot) => {
    visitCodeBlocks(tree, null)
  }
}
