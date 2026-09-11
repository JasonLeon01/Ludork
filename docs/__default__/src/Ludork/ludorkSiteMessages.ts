import type { LanguageKey } from './ludorkLanguages'

export type SiteMessages = {
  navigation: { home: string; docs: string; about: string; repository: string; label: string }
  language: string
  skipToContent: string
  download: string
  home: {
    eyebrow: string
    title: string
    description: string
    imageAlt: string
    editorPlatforms: string
    gamePlatforms: string
  }
  about: { title: string; description: string; eyebrow: string; linksTitle: string; sourceDescription: string }
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
      title: 'A 2D Tilemap\nRPG engine',
      description: 'Create tile-based worlds with visual map editing, Lua scripting, and Blueprints.',
      imageAlt: 'An illustrated tilemap with a river, trees, and houses, floating above its map layers.',
      editorPlatforms: 'Editor platforms',
      gamePlatforms: 'Game platforms',
    },
    about: { title: 'About Ludork', description: 'About Ludork, a 2D RPG editor and native runtime. Find its source, licences, and project resources.', eyebrow: 'The project', linksTitle: 'Explore the project', sourceDescription: 'Find the source, review the terms, or help improve Ludork.' },
    footer: { description: 'Tilemap RPG Engine, but for the next generation.', license: 'Zlib License', notices: 'Third-party notices', feedback: 'Feedback & issues', source: 'Source on GitHub', iconCredits: 'Icon credits' },
    docs: { title: 'Ludork Documentation', description: 'Learn to build 2D RPGs with Ludork: getting started, editor tools, Lua, Blueprints, native development, and plug-ins.', sections: 'Documentation sections', expandSidebar: 'Open document navigation', collapseSidebar: 'Close document navigation', selectDocument: 'Select a document from the sidebar', loading: 'Loading document', loadError: 'Unable to load this document.', retry: 'Try again' },
  },
  zh_CN: {
    navigation: { home: '首页', docs: '文档', about: '关于', repository: 'GitHub 仓库', label: '主导航' },
    language: '语言',
    skipToContent: '跳到主要内容',
    download: '下载 Ludork',
    home: {
      eyebrow: 'Ludork',
      title: '2D Tilemap\nRPG 游戏引擎',
      description: '用可视化地图编辑、Lua 脚本和蓝图编程，搭建基于图块的 RPG 游戏。',
      imageAlt: '由河流、树木和房屋组成的图块地图插画，下方展示分层的地图。',
      editorPlatforms: '编辑器支持的平台',
      gamePlatforms: '可以导出到的平台',
    },
    about: { title: '关于 Ludork', description: '了解 Ludork 2D RPG 编辑器与原生运行时，查看项目源码、许可证与反馈入口。', eyebrow: '项目简介', linksTitle: '了解项目', sourceDescription: '查看源码与使用条款，亦可参与改进 Ludork。' },
    footer: { description: 'Tilemap RPG Engine, but for the next generation.', license: 'Zlib 许可证', notices: '第三方声明', feedback: '问题与建议', source: 'GitHub 源码', iconCredits: '图标来源' },
    docs: { title: 'Ludork 文档', description: '从快速入门、编辑器、Lua、蓝图、原生开发到插件，用 Ludork 制作 2D RPG。', sections: '文档章节', expandSidebar: '打开文档目录', collapseSidebar: '关闭文档目录', selectDocument: '请在左侧选择一篇文档', loading: '正在加载文档', loadError: '这篇文档暂时无法加载。', retry: '再试一次' },
  },
} as const satisfies Record<LanguageKey, SiteMessages>
