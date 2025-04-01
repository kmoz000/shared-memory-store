const MemoryStore = require('./index');

// Create a memory store
const store = new MemoryStore({ cleanupInterval: 5000 });

// Create mutable keys
const userConfigKey = store.createKey('user-config');
const dbConfigKey = store.createKey('database-config');
const sessionKey = store.createKey('session');

// Store values using the mutable keys
store.set(userConfigKey, { 
  theme: 'dark',
  language: 'en-US',
  notifications: true
});

store.set(dbConfigKey, {
  host: 'localhost',
  port: 5432,
  user: 'admin'
});

store.set(sessionKey, { 
  userId: 123,
  token: 'abc123'
}, { 
  isPermanent: false, 
  maxAgeMs: 30000  // 30 seconds
});

// Access values using the mutable keys
console.log('User config:', store.get(userConfigKey));
console.log('DB config:', store.get(dbConfigKey));
console.log('Session:', store.get(sessionKey));

// Use the same key in different places
function getUserSettings(store, key) {
  return store.get(key);
}

function updateUser(store, key) {
  const config = store.get(key);
  config.lastAccess = new Date();
  store.set(key, config);
}

console.log('\nAccessing with function:');
console.log(getUserSettings(store, userConfigKey));
updateUser(store, userConfigKey);
console.log(getUserSettings(store, userConfigKey));

// Change a key - the update will be reflected everywhere
console.log('\nBefore key change:');
console.log('Keys:', store.keys());

console.log('\nChanging key from "user-config" to "user-settings"...');
userConfigKey.value = 'user-settings'; // This updates the key everywhere it's referenced

console.log('\nAfter key change:');
console.log('Keys:', store.keys());
console.log('User config with new key name:', store.get(userConfigKey));

// Create a mutable key with an object as the initial value
const complexKey = store.createKey({ type: 'config', id: 'api' });
store.set(complexKey, { 
  url: 'https://api.example.com',
  version: 'v2',
  timeout: 5000
});

console.log('\nComplex key:', complexKey.toString());
console.log('Complex key value:', store.get(complexKey));

// Modify the complex key
complexKey.value = { type: 'config', id: 'rest-api' };
console.log('\nModified complex key:', complexKey.toString());
console.log('Value with modified key:', store.get(complexKey));