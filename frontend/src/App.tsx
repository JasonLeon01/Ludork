import { useCallback, useEffect, useState } from 'react';
import { ApiFailure, request } from './api.ts';
import { Login } from './Login.tsx';
import { DataBrowser } from './DataBrowser.tsx';

export function App() {
  let project = '';
  try { project = decodeURIComponent(window.location.pathname.split('/').filter(Boolean)[0] ?? ''); }
  catch { /* Invalid URL escapes are displayed as an unselected project. */ }
  const [signedIn, setSignedIn] = useState(false);
  const [checking, setChecking] = useState(true);
  const [error, setError] = useState('');
  const expired = useCallback(() => { setSignedIn(false); setError('Your session has ended. Sign in again.'); }, []);
  useEffect(() => {
    if (!project) { setChecking(false); return; }
    const controller = new AbortController();
    void request(project, '/session', { signal: controller.signal }).then(() => setSignedIn(true)).catch((reason: unknown) => {
      if (!controller.signal.aborted && !(reason instanceof ApiFailure && reason.status === 401)) setError(reason instanceof Error ? reason.message : 'Could not connect to the server.');
    }).finally(() => { if (!controller.signal.aborted) setChecking(false); });
    return () => controller.abort();
  }, [project]);

  async function logout() {
    try { await request(project, '/logout', { method: 'POST' }); setSignedIn(false); setError(''); }
    catch (reason) { setError(reason instanceof Error ? reason.message : 'Could not sign out.'); }
  }

  return <div className="shell">
    <header className="header">
      <div className="brand"><span className="brand-icon">L</span><div><strong>Ludork Server</strong><small>Simple data server</small></div></div>
      <div className="header-actions"><span className="project-badge">{project || 'No project selected'}</span>{signedIn && <button className="quiet" onClick={() => void logout()}>Sign out</button>}</div>
    </header>
    {!project ? <main className="welcome"><h1>Open a project</h1><p>Use the project name configured during deployment.</p><code>http://host:3333/project/</code></main>
      : checking ? <main className="welcome" role="status">Connecting to the project…</main>
      : signedIn ? <><div className="intro"><div><p className="eyebrow">PROJECT DATA</p><h1>Accounts and files</h1><p>Browse category dictionaries and field values stored on the server.</p></div><span className="readonly"><span />Read-only</span></div>{error && <p role="alert" className="error global-error">{error}</p>}<DataBrowser project={project} onExpired={expired} /></>
      : <Login project={project} initialError={error} onSignedIn={() => { setSignedIn(true); setError(''); }} />}
    <footer>Data queries and storage only · No multiplayer synchronization or RPC</footer>
  </div>;
}
