import js from '@eslint/js';
import ts from 'typescript-eslint';

export default ts.config(
  { ignores: ['node_modules/**', 'frontend/.vite/**'] },
  js.configs.recommended,
  ...ts.configs.recommended,
  {
    files: ['**/*.{mjs,ts,tsx}'],
    rules: { '@typescript-eslint/no-unused-vars': ['error', { argsIgnorePattern: '^_' }] },
    languageOptions: {
      globals: {
        process: 'readonly', console: 'readonly', Buffer: 'readonly',
        setTimeout: 'readonly', clearTimeout: 'readonly',
        setInterval: 'readonly', clearInterval: 'readonly', URL: 'readonly',
        fetch: 'readonly', window: 'readonly', document: 'readonly',
        FormData: 'readonly', AbortController: 'readonly',
        RequestInit: 'readonly', React: 'readonly'
      }
    }
  }
);
