import { useState } from 'react';
import type { FormEvent } from 'react';
import { request } from './api.ts';

export function Login({ project, initialError, onSignedIn }: { project: string; initialError: string; onSignedIn: () => void }) {
  const [error, setError] = useState(initialError);
  const [busy, setBusy] = useState(false);
  async function submit(event: FormEvent<HTMLFormElement>) {
    event.preventDefault();
    const form = event.currentTarget;
    const values = new FormData(form);
    setBusy(true); setError('');
    try {
      await request(project, '/login', { method: 'POST', headers: { 'Content-Type': 'application/json' }, body: JSON.stringify({ key: values.get('key'), password: values.get('password') }) });
      form.reset(); onSignedIn();
    } catch (reason) { setError(reason instanceof Error ? reason.message : 'Could not sign in.'); }
    finally { setBusy(false); }
  }
  return <main className="login-wrap"><section className="login-card"><p className="eyebrow">PROJECT ACCESS</p><h1>View project data</h1><p>Enter the project key and management password for <strong>{project}</strong>.</p>
    <form onSubmit={event => void submit(event)}>
      <label htmlFor="project-key">Project key</label><input id="project-key" name="key" type="password" autoComplete="off" required disabled={busy} autoFocus />
      <label htmlFor="project-password">Management password</label><input id="project-password" name="password" type="password" autoComplete="current-password" required disabled={busy} />
      {error && <p role="alert" className="error">{error}</p>}
      <button className="primary" disabled={busy} type="submit">{busy ? 'Signing in…' : 'Open project'}</button>
    </form><p className="login-note">Sign in to browse accounts, category files and JSON content.</p>
  </section></main>;
}
