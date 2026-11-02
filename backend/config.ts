import { readFile } from 'node:fs/promises';
import { fileURLToPath } from 'node:url';
import { join } from 'node:path';
import { hasControlCharacters, projectIdentifier } from './identity.ts';

export interface ProjectConfig {
  keyHash: string;
  passwordSalt: string;
  passwordHash: string;
  ipWriteIntervalSeconds?: number;
  luaEntry?: string;
  hosts?: string[];
}

export interface ServerConfig {
  version: 1;
  projects: Record<string, ProjectConfig>;
}

export const root = fileURLToPath(new URL('../', import.meta.url));
export const configPath = join(root, 'config', 'projects.json');
export const dataPath = join(root, 'data');
export const projectsPath = join(root, 'projects');

export function luaEntry(value: unknown): string {
  const entry = value === undefined ? 'Main.lua' : value;
  if (typeof entry !== 'string' || !entry.isWellFormed() || !entry.endsWith('.lua') || Buffer.byteLength(entry) > 512 || entry.split('/').some(part => !part || part === '.' || part === '..') || entry.includes('\\') || entry.includes(':') || hasControlCharacters(entry)) throw new Error('luaEntry must be a relative Lua file path inside the project directory.');
  return entry;
}

export function ipWriteIntervalSeconds(value: unknown): number {
  const interval = value === undefined ? 180 : value;
  if (typeof interval !== 'number' || !Number.isSafeInteger(interval) || interval < 0) {
    throw new Error('ipWriteIntervalSeconds must be a nonnegative safe integer.');
  }
  return interval;
}

export function hosts(value: unknown): string[] {
  const list = value === undefined ? [] : value;
  const label = /^[a-z0-9](?:[a-z0-9-]{0,61}[a-z0-9])?$/;
  if (!Array.isArray(list) || list.some(host => typeof host !== 'string' || host.length > 253 || !host.split('.').every(part => label.test(part)))) {
    throw new Error('hosts must list lowercase hostnames without a scheme or port.');
  }
  return list;
}

export function projectHosts(config: ServerConfig): Map<string, string> {
  const result = new Map<string, string>();
  for (const [name, project] of Object.entries(config.projects)) for (const host of project.hosts ?? []) result.set(host, name);
  return result;
}

export function hostName(header: string | undefined): string {
  return (header ?? '').toLowerCase().replace(/:\d*$/, '');
}

export async function loadConfig(): Promise<ServerConfig> {
  const parsed: unknown = JSON.parse(await readFile(configPath, 'utf8'));
  if (!parsed || typeof parsed !== 'object' || !('version' in parsed) || parsed.version !== 1 ||
      !('projects' in parsed) || !parsed.projects || typeof parsed.projects !== 'object') {
    throw new Error('Invalid project configuration. Run deploy again.');
  }
  const config = parsed as ServerConfig;
  if (Object.keys(config.projects).length === 0) throw new Error('No projects configured. Run deploy first.');
  const boundHosts = new Set<string>();
  for (const [name, project] of Object.entries(config.projects)) {
    projectIdentifier(name);
    if (!project ||
        !/^[a-f0-9]{64}$/.test(project.keyHash) ||
        !/^[a-f0-9]{32}$/.test(project.passwordSalt) ||
        !/^[a-f0-9]{128}$/.test(project.passwordHash)) {
      throw new Error('Invalid project configuration. Run deploy again.');
    }
    project.ipWriteIntervalSeconds = ipWriteIntervalSeconds(project.ipWriteIntervalSeconds);
    project.luaEntry = luaEntry(project.luaEntry);
    project.hosts = hosts(project.hosts);
    for (const host of project.hosts) {
      if (boundHosts.has(host)) throw new Error('Each host can belong to only one project.');
      boundHosts.add(host);
    }
  }
  return config;
}
