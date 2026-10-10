import { useCallback, useEffect, useRef, useState } from 'react';
import { ApiFailure, formattedJson, request, segment } from './api.ts';

export function DataBrowser({ project, onExpired }: { project: string; onExpired: () => void }) {
  const [accounts, setAccounts] = useState<string[]>([]);
  const [categories, setCategories] = useState<string[]>([]);
  const [account, setAccount] = useState('');
  const [category, setCategory] = useState('');
  const [filter, setFilter] = useState('');
  const [content, setContent] = useState('');
  const [error, setError] = useState('');
  const [loading, setLoading] = useState('accounts');
  const [revision, setRevision] = useState(0);
  const selection = useRef({ account, category });
  selection.current = { account, category };

  const failed = useCallback((reason: unknown, signal: AbortSignal) => {
    if (signal.aborted) return;
    if (reason instanceof ApiFailure && reason.status === 401) { onExpired(); return; }
    setError(reason instanceof Error ? reason.message : 'The request failed. Refresh to try again.');
  }, [onExpired]);

  useEffect(() => {
    const controller = new AbortController();
    setLoading('accounts'); setError('');
    void request(project, '/accounts', { signal: controller.signal }).then(text => {
      if (controller.signal.aborted) return;
      const next = (JSON.parse(text) as { accounts: string[] }).accounts;
      setAccounts(next);
      if (!next.includes(selection.current.account)) { setAccount(next[0] ?? ''); setCategory(''); }
    }).catch(reason => failed(reason, controller.signal)).finally(() => { if (!controller.signal.aborted) setLoading(''); });
    return () => controller.abort();
  }, [project, revision, failed]);

  useEffect(() => {
    setCategories([]); setContent('');
    if (!account) return;
    const controller = new AbortController();
    setLoading('categories'); setError('');
    void request(project, `/accounts/${segment(account)}/categories`, { signal: controller.signal }).then(text => {
      if (controller.signal.aborted) return;
      const next = (JSON.parse(text) as { categories: string[] }).categories;
      setCategories(next);
      if (!next.includes(selection.current.category)) setCategory(next[0] ?? '');
    }).catch(reason => failed(reason, controller.signal)).finally(() => { if (!controller.signal.aborted) setLoading(''); });
    return () => controller.abort();
  }, [project, account, revision, failed]);

  useEffect(() => {
    setContent('');
    if (!account || !category) return;
    const controller = new AbortController();
    setLoading('content'); setError('');
    void request(project, `/accounts/${segment(account)}/categories/${segment(category)}`, { signal: controller.signal })
      .then(text => { if (!controller.signal.aborted) setContent(formattedJson(text)); }).catch(reason => failed(reason, controller.signal))
      .finally(() => { if (!controller.signal.aborted) setLoading(''); });
    return () => controller.abort();
  }, [project, account, category, revision, failed]);

  const filtered = accounts.filter(value => value.toLocaleLowerCase().includes(filter.toLocaleLowerCase()));
  return <main className="browser">
    <aside className="account-pane"><div className="pane-title"><h2>Accounts</h2><span>{accounts.length}</span></div>
      <label className="sr-only" htmlFor="account-search">Filter accounts</label><input id="account-search" className="search" placeholder="Filter accounts…" value={filter} onChange={event => setFilter(event.target.value)} />
      <div className="account-list">{filtered.map(value => <button key={value} title={value} className={`account ${value === account ? 'selected' : ''}`} onClick={() => { if (value !== account) { setAccount(value); setCategory(''); } }}><span className="avatar">{Array.from(value)[0]?.toLocaleUpperCase()}</span><span>{value}</span></button>)}
        {filtered.length === 0 && <p className="empty">{loading ? 'Loading…' : accounts.length ? 'No matching accounts.' : 'No accounts yet. The first client write will create one.'}</p>}
      </div>
    </aside>
    <section className="file-pane"><div className="file-toolbar"><div><span className="eyebrow">{account || 'No account selected'}</span><h2>Category files</h2></div><button onClick={() => setRevision(value => value + 1)} disabled={Boolean(loading)}>Refresh</button></div>
      <div className="file-tabs" aria-label="Category files">{categories.map(value => <button key={value} title={value} className={value === category ? 'active' : ''} onClick={() => setCategory(value)}>{value}.json</button>)}</div>
      {error && <p role="alert" className="error file-error">{error}</p>}
      {content ? <><div className="code-title"><span>{category}.json</span><span>JSON · Read-only</span></div><pre className="json-content" tabIndex={0}><code>{content}</code></pre></>
        : <div className="file-empty" role="status"><span className="document-icon">{'{ }'}</span><h3>{loading ? 'Reading…' : 'Select a category file'}</h3><p>The file contents will appear here.</p></div>}
    </section>
  </main>;
}
