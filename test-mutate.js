const store = require('./test-export');
const usersettings = store.createMutableKey('user-config');
store.set(usersettings, { theme: 'mid-day', language: 'ma-FR', notifications: true });