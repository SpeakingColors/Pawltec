#!/usr/bin/env python3
"""Read every control position, label, colour token and tooltip from the approved mockup
(docs/mockup/Pawltec.html) with headless Chromium and write docs/mockup/layout.json.
tools/gen_layout.py turns that file into src/editor/Layout.h, so the native window follows the mockup.
Run from the repo root. Needs playwright (chromium). Fonts come from assets/fonts so text boxes match."""
import json, os, sys
from playwright.sync_api import sync_playwright

root = os.getcwd()
src = open('docs/mockup/Pawltec.html').read()
face = ''.join('@font-face{font-family:"%s";font-weight:%s;font-style:%s;src:url("file://%s/assets/fonts/%s")}' % (fam, w, st, root, f)
               for fam, w, st, f in [('Barlow Condensed', 500, 'normal', 'BarlowCondensed-Medium.ttf'), ('Barlow Condensed', 600, 'normal', 'BarlowCondensed-SemiBold.ttf'),
                                     ('Barlow Condensed', 700, 'normal', 'BarlowCondensed-Bold.ttf'), ('Barlow Condensed', 800, 'italic', 'BarlowCondensed-ExtraBoldItalic.ttf'),
                                     ('Barlow Condensed', 700, 'italic', 'BarlowCondensed-ExtraBoldItalic.ttf'),
                                     ('JetBrains Mono', 400, 'normal', 'JetBrainsMono-Regular.ttf'), ('JetBrains Mono', 600, 'normal', 'JetBrainsMono-Regular.ttf')])
# the mockup links Google Fonts; use the local files instead so the dump works offline
import re
src = re.sub(r'<link[^>]*googleapis[^>]*>', '', src)
open('/tmp/pawltec_wrapped.html', 'w').write('<!doctype html><html><head><meta charset="utf-8"><style>' + face + '</style></head><body>' + src + '</body></html>')

JS = r"""
() => {
  const st = document.querySelector('#stage').getBoundingClientRect();
  const R = e => { const r = e.getBoundingClientRect(); return [Math.round((r.left - st.left) * 100) / 100, Math.round((r.top - st.top) * 100) / 100, Math.round(r.width * 100) / 100, Math.round(r.height * 100) / 100]; };
  const q = s => document.querySelector(s);
  const o = {};
  const ids = {cheekL:'#cheekL', cheekR:'#cheekR', main:'.main', hdr:'.hdr', body:'.body', disp:'.disp', knobs:'#knobs', rig:'.rig', meters:'.meters', foot:'.foot',
    logo:'.brand svg', name:'.brand .name', s87:'.brand .s87', brand:'.brand', pPrev:'#pPrev', pName:'#pName', pNext:'#pNext', pSave:'#pSave', bBypass:'#bBypass', bGuide:'#bGuide', bSettings:'#bSettings', bAbout:'#bAbout',
    cvResp:'#cvResp', cvSpec:'#cvSpec', cvMet:'#cvMet', pkIn:'#pkIn', pkOut:'#pkOut', winsz:'#winsz'};
  for (const [k, s] of Object.entries(ids)) { const e = q(s); o[k] = R(e); }
  o.tips = {};
  for (const [k, s] of Object.entries(ids)) { const e = q(s); if (e && e.dataset.tip) o.tips[k] = e.dataset.tip; }
  o.footTexts = [...document.querySelectorAll('.foot span')].map(e => ({ t: e.textContent, r: R(e) }));
  o.caps = [...document.querySelectorAll('.dcell .cap')].map(c => ({ title: c.children[0].textContent, titleR: R(c.children[0]), capR: R(c),
     leg: [...c.querySelectorAll('.leg > span')].map(s => ({ t: s.textContent.replace(/^\s*▬\s*/, '').trim(), r: R(s), color: s.style.color || '' })) }));
  o.mcaps = [...document.querySelectorAll('.mcaps span')].map(e => ({ t: e.textContent, r: R(e) }));
  o.groups = [...document.querySelectorAll('#knobs .grp')].map(g => ({ title: g.querySelector('.gt').textContent, titleR: R(g.querySelector('.gt')), r: R(g), tip: g.dataset.tip,
     cells: [...g.querySelectorAll('.kcell')].map(c => { const kn = c.querySelector('.knob'); return { lab: c.querySelector('.lab').textContent, labR: R(c.querySelector('.lab')), knobR: R(kn), valR: R(c.querySelector('.kval')), tip: kn.dataset.tip, aria: kn.getAttribute('aria-label') }; }) }));
  o.knobsR = R(q('#knobs'));
  const cs = (s) => { const c = getComputedStyle(q(s)); return { fs: c.fontSize, fw: c.fontWeight, ls: c.letterSpacing, ff: c.fontFamily, color: c.color }; };
  o.styles = { name: cs('.brand .name'), s87: cs('.brand .s87'), lab: cs('.kcell .lab'), gt: cs('.grp .gt'), cap: cs('.dcell .cap span'), foot: cs('.foot span'), kval: cs('.kval'), mcap: cs('.mcaps span'), pk: cs('#pkIn') };
  return o;
}
"""
with sync_playwright() as p:
    b = p.chromium.launch()
    pg = b.new_page(viewport={'width': 1400, 'height': 900})
    pg.goto('file:///tmp/pawltec_wrapped.html')
    pg.wait_for_timeout(1500)
    d = pg.evaluate(JS)
    json.dump(d, open('docs/mockup/layout.json', 'w'), indent=1)
    pg.screenshot(path='/tmp/pawltec_mockup_ref.png', clip={'x': 0, 'y': 0, 'width': 1400, 'height': 720})
    print(json.dumps({k: d[k] for k in ['cheekL', 'main', 'hdr', 'body', 'disp', 'knobs', 'rig', 'meters', 'foot', 'cvResp', 'cvSpec', 'cvMet', 'logo', 'name', 's87', 'pPrev', 'pName', 'pNext', 'pSave', 'bBypass', 'bGuide', 'bSettings', 'bAbout', 'pkIn', 'pkOut', 'winsz']}))
    print(d['styles'])
    for g in d['groups']: print(g['title'], g['r'], [c['knobR'] for c in g['cells']])
    b.close()
