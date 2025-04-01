const store = require('./test-export');

// Create mutable keys
const userConfigKey = store.createMutableKey('user-config');
const dbConfigKey = store.createMutableKey('database-config');
const sessionKey = store.createMutableKey('session');

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
console.log('Keys (string representation):', store.keys());
// Check if getKeys is available (returns the actual key objects)
if (typeof store.getKeys === 'function') {
  console.log('Original keys:', store.getKeys());
}

console.log('\nChanging key from "user-config" to "user-settings"...');
userConfigKey.value = 'user-settings'; // This updates the key everywhere it's referenced

console.log('\nAfter key change:');
console.log('Keys (string representation):', store.keys());
// Check if getKeys is available
if (typeof store.getKeys === 'function') {
  console.log('Updated keys:', store.getKeys());
}
console.log('User config with new key name:', store.get(userConfigKey));

// Create a mutable key with an object as the initial value
const complexKey = store.createMutableKey({ type: 'config', id: 'api' });
store.set(complexKey, { 
  url: 'https://api.example.com',
  version: 'v2',
  timeout: 5000
});

console.log('\nComplex key:', complexKey.toString());
console.log('Complex key value:', store.get(complexKey));

// Modify the complex key
complexKey.value = { type: 'config', id: 'rest-api' };
console.log('\nModified complex key:', complexKey.value);
console.log('Value with modified key:', store.get(complexKey));

// Debug helper - print direct string representation of keys
console.log('\nKey representations:');
console.log('userConfigKey:', userConfigKey);
console.log('dbConfigKey:', dbConfigKey);
console.log('sessionKey:', sessionKey);
console.log('complexKey:', complexKey);
userConfigKey.value = 'user-config'; // Reset key name for next test
require('./test-mutate');
store.set(complexKey, ()=>{
    console.log('This is a function');
});
// Alternative way to see the raw value after mutation
console.log('\nKey raw values:');
console.log('userConfigKey value:', );
console.log('dbConfigKey value:', dbConfigKey.value);
console.log('sessionKey value:', sessionKey.value);
// calling a function stored in the store
store.get(complexKey)();

// example calling a class stored in the store and calling a method on it
store.set(complexKey, class {
    constructor() {
        this.name = 'John Doe';
    }
    greet(name = this.name) {
        console.log(`Hello, ${name}!`);
    }
});
let MyClass = store.get(complexKey);
let myInstance = new MyClass();
myInstance.greet();

// calling saved instance of class
store.set(complexKey, myInstance);
let myInstance2 = store.get(complexKey);
myInstance2.greet('Nice person');