import { readFile } from 'node:fs/promises';
import { fileURLToPath } from 'node:url';
import { join } from 'node:path';
import { projectIdentifier } from './identity.ts';

export interface ProjectConfig {
  keyHash: string;
  passwordSalt: string;
  passwordHash: string;
  ipWriteIntervalSeconds?: number;
}

export interface ServerConfig {
  version: 1;
  projects: Record<string, ProjectConfig>;
}

export const root = fileURLToPath(new URL('../', import.meta.url));
export const configPath = join(root, 'config', 'projects.json');
export const dataPath = join(root, 'data');

export function ipWriteIntervalSeconds(value: unknown): number {
  const interval = value === undefined ? 180 : value;
  if (typeof interval !== 'number' || !Number.isSafeInteger(interval) || interval < 0) {
    throw new Error('ipWriteIntervalSeconds must be a nonnegative safe integer.');
  }
  return interval;
}

export async function loadConfig(): Promise<ServerConfig> {
  const parsed: unknown = JSON.parse(await readFile(configPath, 'utf8'));
  if (!parsed || typeof parsed !== 'object' || !('version' in parsed) || parsed.version !== 1 ||
      !('projects' in parsed) || !parsed.projects || typeof parsed.projects !== 'object') {
    throw new Error('Invalid project configuration. Run deploy again.');
  }
  const config = parsed as ServerConfig;
  if (Object.keys(config.projects).length === 0) throw new Error('No projects configured. Run deploy first.');
  for (const [name, project] of Object.entries(config.projects)) {
    projectIdentifier(name);
    if (!project ||
        !/^[a-f0-9]{64}$/.test(project.keyHash) ||
        !/^[a-f0-9]{32}$/.test(project.passwordSalt) ||
        !/^[a-f0-9]{128}$/.test(project.passwordHash)) {
      throw new Error('Invalid project configuration. Run deploy again.');
    }
    project.ipWriteIntervalSeconds = ipWriteIntervalSeconds(project.ipWriteIntervalSeconds);
  }
  return config;
}
