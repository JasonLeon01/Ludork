import type { LanguageKey } from './ludorkLanguages'

const HOME_SLOGAN = [
  'Dream something new',
  'Bring something fresh',
  'Make something out of imagination',
] as const

const FOOTER_DESCRIPTION = 'A tilemap RPG Engine for the next generation'

export type SiteMessages = {
  navigation: { home: string; docs: string; about: string; repository: string; label: string }
  language: string
  skipToContent: string
  download: string
  home: {
    eyebrow: string
    description: string
    slogan: readonly [string, string, string]
    imageAlt: string
    imageEditorLabel: string
    imageGameLabel: string
    editorPlatforms: string
    gamePlatforms: string
  }
  about: { title: string; description: string; eyebrow: string }
  notices: { title: string; description: string }
  acknowledgements: { title: string }
  footer: { description: string; license: string; notices: string; feedback: string; source: string; iconCredits: string }
  docs: {
    title: string
    description: string
    sections: string
    expandSidebar: string
    collapseSidebar: string
    selectDocument: string
    loading: string
    loadError: string
    retry: string
  }
}

export const LUDORK_SITE_MESSAGES = {
  en_GB: {
    navigation: { home: 'Home', docs: 'Documentation', about: 'About', repository: 'GitHub repository', label: 'Main navigation' },
    language: 'Language',
    skipToContent: 'Skip to content',
    download: 'Download Ludork',
    home: {
      eyebrow: 'Ludork',
      description: 'Designed to turn your inspiration into a virtual world.',
      slogan: HOME_SLOGAN,
      imageAlt: 'The same RPG scene divided from top left to bottom right: the editor on the left and the running game on the right.',
      imageEditorLabel: 'Show editor view',
      imageGameLabel: 'Show game view',
      editorPlatforms: 'Editor platforms',
      gamePlatforms: 'Game platforms',
    },
    about: { title: 'About Ludork', description: 'About Ludork, a 2D RPG editor and native runtime. Find its source, licences, and project resources.', eyebrow: 'The project' },
    notices: { title: 'Ludork Licences and Third-Party Notices', description: 'Licences, attribution, and third-party notices for Ludork and its bundled components and assets.' },
    acknowledgements: { title: 'Acknowledgements' },
    footer: { description: FOOTER_DESCRIPTION, license: 'Zlib License', notices: 'Third-party notices', feedback: 'Feedback & issues', source: 'Source on GitHub', iconCredits: 'Icon credits' },
    docs: { title: 'Ludork Documentation', description: 'Learn to build 2D RPGs with Ludork: getting started, editor tools, Lua, Blueprints, native development, and plug-ins.', sections: 'Documentation sections', expandSidebar: 'Open document navigation', collapseSidebar: 'Close document navigation', selectDocument: 'Select a document from the sidebar', loading: 'Loading document', loadError: 'Unable to load this document.', retry: 'Try again' },
  },
  zh_CN: {
    navigation: { home: '首页', docs: '文档', about: '关于', repository: 'GitHub 仓库', label: '主导航' },
    language: '语言',
    skipToContent: '跳到主要内容',
    download: '下载 Ludork',
    home: {
      eyebrow: 'Ludork',
      description: '让想象中的世界，在你手中成形。',
      slogan: HOME_SLOGAN,
      imageAlt: '同一 RPG 场景沿左上至右下的斜线分隔：左侧为编辑器，右侧为游戏运行效果。',
      imageEditorLabel: '展示编辑器画面',
      imageGameLabel: '展示游戏画面',
      editorPlatforms: '编辑器支持的平台',
      gamePlatforms: '可以导出到的平台',
    },
    about: { title: '关于 Ludork', description: '了解 Ludork 2D RPG 编辑器与原生运行时，查看项目源码、许可证与反馈入口。', eyebrow: '项目简介' },
    notices: { title: 'Ludork 许可证与第三方声明', description: 'Ludork 及随附组件和资产的许可证、来源与第三方声明。' },
    acknowledgements: { title: '致谢' },
    footer: { description: FOOTER_DESCRIPTION, license: 'Zlib 许可证', notices: '第三方声明', feedback: '问题与建议', source: 'GitHub 源码', iconCredits: '图标来源' },
    docs: { title: 'Ludork 文档', description: '从快速入门、编辑器、Lua、蓝图、原生开发到插件，用 Ludork 制作 2D RPG。', sections: '文档章节', expandSidebar: '打开文档目录', collapseSidebar: '关闭文档目录', selectDocument: '请在左侧选择一篇文档', loading: '正在加载文档', loadError: '这篇文档暂时无法加载。', retry: '再试一次' },
  },
} as const satisfies Record<LanguageKey, SiteMessages>
