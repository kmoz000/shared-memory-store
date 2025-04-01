const store = require('./test-export');
const usersettings = store.createMutableKey('user-config');
store.set(usersettings, { theme: 'light', language: 'ma-FR', notifications: true });