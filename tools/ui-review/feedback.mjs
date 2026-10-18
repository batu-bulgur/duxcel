// Project-scoped access to the official Agentation queue. No global MCP config required.
const origin = 'http://localhost:4174';
async function request(route, options) {
  const response = await fetch(origin + route, options);
  if (!response.ok) throw new Error(`${response.status}: ${await response.text()}`);
  return response.json();
}
const [command = 'list', issueId, ...words] = process.argv.slice(2);
try {
  const { provenance, issues } = await request('/__review/queue');
  if (command === 'list') {
    console.log(JSON.stringify({ reference: provenance.referenceSha256, issues: issues.map(({ id, annotation: a, context }) => ({ id, status: a.status, comment: a.comment, target: a.element, source: a.sourceFile, state: context?.state })) }, null, 2));
  } else {
    const issue = issues.find(item => item.id === issueId);
    if (!issue) throw new Error(`Unknown issue: ${issueId}`);
    const route = `/__agentation/annotations/${encodeURIComponent(issue.annotation.id)}`;
    if (command === 'show') console.log(JSON.stringify(issue, null, 2));
    else if (command === 'status') {
      if (!['pending', 'acknowledged', 'resolved', 'dismissed'].includes(words[0])) throw new Error('Use pending, acknowledged, resolved, or dismissed');
      console.log(JSON.stringify(await request(route, { method: 'PATCH', headers: { 'Content-Type': 'application/json' }, body: JSON.stringify({ status: words[0] }) }), null, 2));
    } else if (command === 'reply') {
      if (!words.length) throw new Error('A reply is required');
      console.log(JSON.stringify(await request(route + '/thread', { method: 'POST', headers: { 'Content-Type': 'application/json' }, body: JSON.stringify({ role: 'agent', content: words.join(' ') }) }), null, 2));
    } else throw new Error('Commands: list; show UX-001; status UX-001 acknowledged; reply UX-001 "message"');
  }
} catch (error) { console.error(error.message); process.exitCode = 1; }
