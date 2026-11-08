// SPDX-License-Identifier: GPL-3.0-or-later
const fs = require('node:fs');
const path = require('node:path');
const vm = require('node:vm');
const assert = require('node:assert/strict');
const source = fs.readFileSync(path.join(__dirname, '../runtime/taskbar-pins-migration.js'), 'utf8');
function widget(state, type = 'org.kde.plasma.icontasks') {
  return {
    type, currentConfigGroup: ['Previous'],
    readConfig: (key, fallback) => state[key] === undefined ? fallback : state[key],
    writeConfig: (key, value) => state[key] = Array.isArray(value) ? Array.from(value) : value,
  };
}
function run(widgets, tools = false, extras = false) {
  const messages = [];
  vm.runInNewContext(source, {
    panels: () => [{ widgets: () => widgets }], print: text => messages.push(text),
    kdeDesktopToolsAvailable: tools, kdeDesktopExtrasAvailable: extras,
  });
  return messages;
}
const state = {launchers: ['applications:org.kde.discover.desktop', 'applications:systemsettings.desktop', 'applications:custom.desktop']};
const tasks = widget(state);
run([tasks], true, false);
assert.deepEqual(state.launchers, ['applications:custom.desktop', 'applications:org.kde.konsole.desktop', 'applications:org.nixbridge.processmonitor.desktop']);
assert.deepEqual(tasks.currentConfigGroup, ['Previous']);
// Users may unpin terminal/monitor before the next cumulative upgrade.
state.launchers = ['applications:custom.desktop'];
run([tasks], true, true);
assert.deepEqual(state.launchers, ['applications:custom.desktop', 'applications:org.nixbridge.texteditor.desktop', 'applications:org.nixbridge.systemsettings.desktop']);
state.launchers = ['applications:custom.desktop'];
run([tasks], true, true);
assert.deepEqual(state.launchers, ['applications:custom.desktop']);
const original = {};
run([widget(original)]);
assert.deepEqual(original.launchers, ['preferred://filemanager', 'preferred://browser']);
const untouched = {launchers: ['applications:systemsettings.desktop']};
run([widget(untouched, 'org.kde.plasma.systemtray')], true, true);
assert.deepEqual(untouched.launchers, ['applications:systemsettings.desktop']);
assert.equal(run([])[0], 'WAITING_TASKBAR=0');
console.log('PASS: old defaults repaired, custom pins preserved, tools/extras added once, user unpinning preserved, groups restored, missing widgets retried.');
