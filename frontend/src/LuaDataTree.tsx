import { useState } from 'react';
import type { WireValue } from '../../backend/wire.ts';

function quotedString(encoded: string): string {
  const bytes = Uint8Array.from(atob(encoded), character => character.charCodeAt(0));
  let text: string;
  try { text = new TextDecoder('utf-8', { fatal: true, ignoreBOM: true }).decode(bytes); }
  catch {
    return '"' + Array.from(bytes, byte => byte >= 32 && byte < 127 && byte !== 34 && byte !== 92
      ? String.fromCharCode(byte) : `\\${String(byte).padStart(3, '0')}`).join('') + '"';
  }
  return '"' + Array.from(text, character => {
    if (character === '"' || character === '\\') return '\\' + character;
    const code = character.codePointAt(0)!;
    if (code < 32 || code === 127) return `\\${String(code).padStart(3, '0')}`;
    if (code >= 128 && code <= 159 || code === 0x2028 || code === 0x2029) return `\\u{${code.toString(16)}}`;
    return character;
  }).join('') + '"';
}

function scalar(value: WireValue): string {
  switch (value[0]) {
    case 'nil': return 'nil';
    case 'bool': return String(value[1]);
    case 'int': return value[1];
    case 'float': return /[.eE]/.test(value[1]) ? value[1] : value[1] + '.0';
    case 'string': return quotedString(value[1]);
    case 'table': return '{}';
  }
}

function LuaValue({ value, label, root = false }: { value: WireValue; label?: string; root?: boolean }) {
  const [expanded, setExpanded] = useState(root);
  const [visible, setVisible] = useState(100);
  if (value[0] !== 'table') return <div className="lua-field"><code>{label}{scalar(value)}</code><span className="lua-type">{value[0]}</span></div>;
  const entries = value[1];
  return <details className="lua-table" open={expanded} onToggle={event => setExpanded(event.currentTarget.open)}>
    <summary><code>{label}{'{ }'}</code><span className="lua-type">table · {entries.length} {entries.length === 1 ? 'entry' : 'entries'}</span></summary>
    {expanded && <div className="lua-entries">
      {entries.slice(0, visible).map(([key, child], index) => <LuaValue key={index} value={child} label={`[${scalar(key)}] = `} />)}
      {entries.length === 0 && <span className="lua-empty">Empty table</span>}
      {visible < entries.length && <button className="lua-more" onClick={() => setVisible(count => count + 100)}>Show next {Math.min(100, entries.length - visible)} entries</button>}
    </div>}
  </details>;
}

export function LuaDataTree({ value }: { value: WireValue }) {
  return <div className="lua-content" aria-label="Lua data" tabIndex={0}><LuaValue value={value} root /></div>;
}
