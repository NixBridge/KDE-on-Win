// SPDX-License-Identifier: GPL-3.0-or-later
const {readFileSync} = require('node:fs');
const vm = require('node:vm');
const assert = require('node:assert/strict');
const source = readFileSync(require('node:path').join(__dirname, '../native/default-panel.js'), 'utf8');
const tasksId = 'org.kde.plasma.icontasks';

for (const available of [true, false]) {
    const added = [];
    const messages = [];
    const launches = [];
    vm.runInNewContext(source, {
        gridUnit: 8,
        print: message => messages.push(message),
        Panel: function () {
            this.addWidget = id => {
                added.push(id);
                if (id === tasksId) {
                    return available ? {writeConfig: (_, list) => launches.push(...list)} : new Error('missing applet');
                }
            };
        },
    });
    assert.equal(added.length, 7);
    assert.equal(added.at(-1), 'org.kde.plasma.digitalclock');
    assert.equal(messages.length, available ? 0 : 1);
    assert.equal(launches.length, available ? 5 : 0);
}
console.log('PASS: panel setup reaches the clock with and without a working task manager');
